#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <iomanip>

#include "console.h"
#include "util.h"
#include "icons.h"
#include "git_status.h"

namespace fs = std::filesystem;
using std::string;
using std::vector;

namespace nls {

static const char* FG_RESET = "\x1b[0m";
static const char* FG_DIR   = "\x1b[34m"; // blue
static const char* FG_LINK  = "\x1b[36m"; // cyan
static const char* FG_EXEC  = "\x1b[32m"; // green
static const char* FG_FILE  = "\x1b[37m"; // light gray

// Git colors
static const char* FG_GIT_CLEAN = "\x1b[32m";     // ✓
static const char* FG_GIT_MOD   = "\x1b[33m";     // modified
static const char* FG_GIT_NEW   = "\x1b[35m";     // new/untracked
static const char* FG_GIT_DEL   = "\x1b[31m";     // deleted
static const char* FG_GIT_REN   = "\x1b[36m";     // renamed
static const char* FG_GIT_TYP   = "\x1b[34m";     // typechange
static const char* FG_GIT_IGN   = "\x1b[90m";     // ignored/unreadable
static const char* FG_GIT_CON   = "\x1b[91m";     // conflicted (bright red)

struct Entry {
    FileInfo info;
    std::string display_name; // icon + name (+ slash for dir)
};

static bool is_executable_posix(const fs::directory_entry& de) {
#ifdef _WIN32
    // simple heuristic: .exe/.bat/.cmd/.ps1
    std::string n = de.path().filename().string();
    auto pos = n.rfind('.');
    if (pos != std::string::npos) {
        std::string ext = n.substr(pos+1);
        for (auto& c: ext) c = (char)tolower((unsigned char)c);
        return (ext == "exe" || ext == "bat" || ext == "cmd" || ext == "ps1");
    }
    return false;
#else
    std::error_code ec;
    auto p = de.status(ec).permissions();
    (void)ec;
    return ( (p & fs::perms::owner_exec) != fs::perms::none ||
             (p & fs::perms::group_exec) != fs::perms::none ||
             (p & fs::perms::others_exec) != fs::perms::none );
#endif
}

static void collect_entries(const fs::path& dir, const Options& opt, std::vector<Entry>& out) {
    std::error_code ec;
    if (!fs::exists(dir, ec)) {
        std::cerr << "nls: " << dir.string() << ": No such file or directory\n";
        return;
    }
    auto add_one = [&](const fs::directory_entry& de) {
        std::string name = de.path().filename().string();
        if (!opt.all && !opt.almost_all && is_hidden(name)) return;
        if (opt.almost_all && (name == "." || name == "..")) return;
        Entry e{};
        e.info.path = de.path();
        e.info.name = name;
        e.info.is_dir = de.is_directory(ec);
        e.info.is_symlink = de.is_symlink(ec);
        e.info.size = e.info.is_dir ? 0 : (de.is_regular_file(ec) ? de.file_size(ec) : 0);
        e.info.mtime = de.last_write_time(ec);
        e.info.is_exec = is_executable_posix(de);
        fill_owner_group(e.info);
        if (!opt.no_icons) e.info.icon = icon_for(name, e.info.is_dir, e.info.is_exec);
        // color
        if (opt.no_color) {
            e.info.color_fg.clear();
            e.info.color_reset.clear();
        } else {
            if (e.info.is_symlink) e.info.color_fg = FG_LINK;
            else if (e.info.is_dir) e.info.color_fg = FG_DIR;
            else if (e.info.is_exec) e.info.color_fg = FG_EXEC;
            else e.info.color_fg = FG_FILE;
            e.info.color_reset = FG_RESET;
        }
        out.push_back(std::move(e));
    };

    if (fs::is_directory(dir, ec)) {
        for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::end(it); ++it) {
            add_one(*it);
        }
    } else {
        // single file path
        fs::directory_entry de(dir, ec);
        if (!ec) add_one(de);
    }
}

static const char* color_for_git_code(const std::string& code) {
    if (code == "!!" || code.find('!') != std::string::npos) return FG_GIT_IGN;
    if (code.find('U') != std::string::npos) return FG_GIT_CON;
    if (code.find('D') != std::string::npos) return FG_GIT_DEL;
    if (code.find('M') != std::string::npos) return FG_GIT_MOD;
    if (code.find('R') != std::string::npos) return FG_GIT_REN;
    if (code.find('T') != std::string::npos) return FG_GIT_TYP;
    if (code.find('A') != std::string::npos || code.find('?') != std::string::npos) return FG_GIT_NEW;
    return FG_GIT_MOD;
}

// Aggregate porcelain code for a directory from all descendant entries in map
static std::string aggregate_dir_code(const std::string& rel_dir, const GitStatusMap& map) {
    auto starts_with = [](const std::string& s, const std::string& p) -> bool {
        return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
    };
    std::string prefix = rel_dir;
    if (!prefix.empty() && prefix.back() != '/') prefix += '/';

    bool any = false;
    bool any_non_ignored = false;
    bool has_conflict = false;

    // Index flags
    bool xA=false, xM=false, xD=false, xR=false, xT=false;
    // Worktree flags
    bool yQ=false, yM=false, yD=false, yR=false, yT=false, yBang=false;
    // Ignored-only?
    bool saw_ignored = false;

    auto consider = [&](const std::string& code) {
        any = true;
        if (code == "!!") { saw_ignored = true; return; }
        any_non_ignored = true;
        if (code == "UU") { has_conflict = true; return; }

        char X = code.size() > 0 ? code[0] : ' ';
        char Y = code.size() > 1 ? code[1] : ' ';

        switch (X) {
            case 'A': xA = true; break;
            case 'M': xM = true; break;
            case 'D': xD = true; break;
            case 'R': xR = true; break;
            case 'T': xT = true; break;
            default: break;
        }
        switch (Y) {
            case '?': yQ = true; break;
            case 'M': yM = true; break;
            case 'D': yD = true; break;
            case 'R': yR = true; break;
            case 'T': yT = true; break;
            case '!': yBang = true; break;
            default: break;
        }
    };

    // Consider direct entry (rare but happens for ignored or untracked dirs)
    auto it_dir = map.find(rel_dir);
    if (it_dir != map.end()) consider(it_dir->second);

    // Consider descendants
    for (const auto& kv : map) {
        const auto& k = kv.first;
        if (k == rel_dir || (!prefix.empty() && starts_with(k, prefix))) {
            consider(kv.second);
        }
    }

    if (!any) return "";                // No info for this dir
    if (has_conflict) return "UU";
    if (!any_non_ignored && saw_ignored) return "!!"; // entirely ignored

    // Compose X with precedence: D > R > M > A > T
    char X = ' ';
    if      (xD) X = 'D';
    else if (xR) X = 'R';
    else if (xM) X = 'M';
    else if (xA) X = 'A';
    else if (xT) X = 'T';

    // Compose Y with precedence: ! > D > R > M > T > ?
    char Y = ' ';
    if      (yBang) Y = '!';
    else if (yD)    Y = 'D';
    else if (yR)    Y = 'R';
    else if (yM)    Y = 'M';
    else if (yT)    Y = 'T';
    else if (yQ)    Y = '?';

    if (X == ' ' && Y == '?') return "??";
    if (X == ' ' && Y == ' ') return ""; // treat as clean ✓
    return std::string() + X + Y;
}

static void apply_git_status(std::vector<Entry>& items, const fs::path& dir, const Options& opt) {
    if (!opt.git_status) return;

    auto map = get_git_status_for_dir(dir);
    for (auto& e : items) {
        std::error_code ec;
        fs::path base = fs::is_directory(dir) ? dir : dir.parent_path();
        fs::path relp = fs::relative(e.info.path, base, ec);
        std::string rel = ec ? e.info.path.filename().generic_string() : relp.generic_string();

        std::string code;

        if (e.info.is_dir) {
            // Aggregate from descendants to get a folder-level porcelain code
            code = aggregate_dir_code(rel, map);
        } else {
            // Files: exact match
            auto it = map.find(rel);
            if (it != map.end()) code = it->second;
        }

        if (code.empty()) {
            e.info.git_prefix = std::string("") + (opt.no_color ? "" : FG_GIT_CLEAN) + "✓ " + (opt.no_color ? "" : FG_RESET);
        } else {
            const char* col = opt.no_color ? "" : color_for_git_code(code);
            e.info.git_prefix = std::string(col) + code + (opt.no_color ? "" : FG_RESET);
        }
    }
}

static void sort_entries(std::vector<Entry>& v, const Options& opt) {
    auto cmp_name = [](const Entry& a, const Entry& b) {
        return nls::to_lower(a.info.name) < nls::to_lower(b.info.name);
    };
    auto cmp_time = [](const Entry& a, const Entry& b) {
        return a.info.mtime > b.info.mtime;
    };
    auto cmp_size = [](const Entry& a, const Entry& b) {
        return a.info.size > b.info.size;
    };
    auto cmp_ext = [](const Entry& a, const Entry& b) {
        auto pa = a.info.path.extension().string();
        auto pb = b.info.path.extension().string();
        return nls::to_lower(pa) < nls::to_lower(pb);
    };

    if (opt.group_dirs_first) {
        std::stable_sort(v.begin(), v.end(), [](const Entry& a, const Entry& b){
            return a.info.is_dir && !b.info.is_dir;
        });
    }

    switch (opt.sort) {
        case Options::Sort::Time: std::stable_sort(v.begin(), v.end(), cmp_time); break;
        case Options::Sort::Size: std::stable_sort(v.begin(), v.end(), cmp_size); break;
        case Options::Sort::Extension: std::stable_sort(v.begin(), v.end(), cmp_ext); break;
        case Options::Sort::None: /* keep directory order */ break;
        case Options::Sort::Name: default: std::stable_sort(v.begin(), v.end(), cmp_name); break;
    }
    if (opt.reverse) std::reverse(v.begin(), v.end());
}

static std::string make_display_name(const Entry& e) {
    std::string name = e.info.name;
    if (e.info.is_dir) name += "/";
    if (!e.info.icon.empty()) {
        return e.info.icon + std::string(" ") + name;
    }
    return name;
}

static size_t printable_width(const std::string& s) {
    // Heuristic: count UTF-8 code points (not perfect for double-width glyphs)
    size_t w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = (unsigned char)s[i];
        size_t adv = 1;
        if      ((c & 0x80) == 0x00) adv = 1;
        else if ((c & 0xE0) == 0xC0) adv = 2;
        else if ((c & 0xF0) == 0xE0) adv = 3;
        else if ((c & 0xF8) == 0xF0) adv = 4;
        else adv = 1;
        i += adv;
        w += 1; // count as width 1
    }
    return w;
}

static void print_long(const std::vector<Entry>& v, const Options& opt) {
    // Determine column widths for metadata
    size_t w_perm = 10, w_nlink = 2, w_owner = 0, w_group = 0, w_size = 0;
    for (auto& e : v) {
        w_owner = std::max(w_owner, e.info.owner.size());
        w_group = std::max(w_group, e.info.group.size());
        std::string size_str = opt.bytes ? std::to_string(e.info.size) : human_size(e.info.size);
        w_size = std::max(w_size, size_str.size());
    }

    for (auto& e : v) {
        std::string perm = perm_string(fs::directory_entry(e.info.path));
        std::cout << colorize_perm(perm, opt.no_color) << ' ';
        std::cout << std::setw(2) << e.info.nlink << ' ';
        if (!e.info.owner.empty())
            std::cout << std::left << std::setw((int)w_owner) << e.info.owner << ' ';
        else
            std::cout << std::left << std::setw((int)w_owner) << "" << ' ';
        if (!e.info.group.empty())
            std::cout << std::left << std::setw((int)w_group) << e.info.group << ' ';
        else
            std::cout << std::left << std::setw((int)w_group) << "" << ' ';

        std::string size_str = opt.bytes ? std::to_string(e.info.size) : human_size(e.info.size);
        std::cout << std::right << std::setw((int)w_size) << size_str << ' ';
        std::cout << format_time(e.info.mtime) << ' ';

        if (opt.git_status && !e.info.git_prefix.empty())
            std::cout << e.info.git_prefix << ' ';

        if (!opt.no_color && !e.info.color_fg.empty()) std::cout << e.info.color_fg;
        std::cout << make_display_name(e);
        if (!opt.no_color && !e.info.color_fg.empty()) std::cout << FG_RESET;

        // symlink target
        std::error_code ec;
        if (e.info.is_symlink) {
            auto target = fs::read_symlink(e.info.path, ec);
            if (!ec) std::cout << " -> " << target.string();
        }

        std::cout << "\n";
    }
}

static void print_columns(const std::vector<Entry>& v, const Options& opt) {
    int cols = terminal_width();
    vector<string> cells;
    size_t maxw = 0;
    cells.reserve(v.size());
    for (auto& e : v) {
        std::string cell;
        if (opt.git_status && !e.info.git_prefix.empty())
            cell += e.info.git_prefix + " ";
        if (!opt.no_color && !e.info.color_fg.empty()) cell += e.info.color_fg;
        cell += make_display_name(e);
        if (!opt.no_color && !e.info.color_fg.empty()) cell += FG_RESET;
        cells.push_back(std::move(cell));
    }
    for (auto& c : cells) maxw = std::max(maxw, printable_width(c));
    size_t gutter = 2;
    size_t per_row = std::max<size_t>(1, (cols) / (maxw + gutter));
    size_t rows = (v.size() + per_row - 1) / per_row;
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < per_row; ++c) {
            size_t idx = c*rows + r;
            if (idx >= cells.size()) break;
            std::cout << std::left << std::setw((int)(maxw + gutter)) << cells[idx];
        }
        std::cout << "\n";
    }
}

static int list_path(const fs::path& p, const Options& opt) {
    std::vector<Entry> items;
    collect_entries(p, opt, items);
    apply_git_status(items, fs::is_directory(p) ? p : p.parent_path(), opt);
    sort_entries(items, opt);

    bool print_header = (opt.paths.size() > 1 && fs::is_directory(p));
    if (print_header) std::cout << p.string() << ":\n";

    if (opt.long_format) print_long(items, opt);
    else if (opt.one_per_line) {
        for (auto& e : items) {
            if (opt.git_status && !e.info.git_prefix.empty())
                std::cout << e.info.git_prefix << ' ';
            if (!opt.no_color && !e.info.color_fg.empty()) std::cout << e.info.color_fg;
            std::cout << make_display_name(e);
            if (!opt.no_color && !e.info.color_fg.empty()) std::cout << FG_RESET;
            std::cout << "\n";
        }
    } else {
        print_columns(items, opt);
    }

    if (opt.paths.size() > 1) std::cout << "\n";
    return 0;
}

} // namespace nls

int main(int argc, char** argv) {
    using namespace nls;
    enable_virtual_terminal();
    Options opt = parse_args(argc, argv);
    int rc = 0;
    for (auto& p : opt.paths) {
        try {
            rc |= list_path(std::filesystem::path(p), opt);
        } catch (const std::exception& e) {
            std::cerr << "nls: error: " << e.what() << "\n";
            rc = 1;
        }
    }
    return rc;
}
