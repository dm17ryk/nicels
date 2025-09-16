#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <iomanip>
#include <utility>

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

    auto add_one = [&](const fs::directory_entry& de, std::string override_name = {}) {
        std::string name = override_name.empty() ? de.path().filename().string() : std::move(override_name);

        if ((name == "." || name == "..")) {
            if (!opt.all) return;
        } else {
            if (!opt.all && !opt.almost_all && is_hidden(name)) return;
        }
        if (opt.almost_all && (name == "." || name == "..")) return;

        Entry e{};
        e.info.path = de.path();
        e.info.name = std::move(name);

        std::error_code info_ec;
        e.info.is_dir = de.is_directory(info_ec);
        info_ec.clear();
        e.info.is_symlink = de.is_symlink(info_ec);
        info_ec.clear();
        bool is_reg = de.is_regular_file(info_ec);
        if (info_ec) {
            is_reg = false;
            info_ec.clear();
        }
        e.info.size = e.info.is_dir ? 0 : (is_reg ? de.file_size(info_ec) : 0);
        if (info_ec) {
            e.info.size = 0;
            info_ec.clear();
        }
        e.info.mtime = de.last_write_time(info_ec);
        e.info.is_exec = is_executable_posix(de);
        fill_owner_group(e.info);
        if (!opt.no_icons) e.info.icon = icon_for(e.info.name, e.info.is_dir, e.info.is_exec);
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
        if (opt.all) {
            std::error_code self_ec;
            fs::directory_entry self(dir, self_ec);
            if (!self_ec) add_one(self, ".");

            std::error_code parent_ec;
            fs::directory_entry parent(dir / "..", parent_ec);
            if (!parent_ec) add_one(parent, "..");
        }

        for (auto it = fs::directory_iterator(dir, ec); !ec && it != fs::end(it); ++it) {
            add_one(*it);
        }
    } else {
        // single file path
        fs::directory_entry de(dir, ec);
        if (!ec) add_one(de);
    }
}

static const std::set<std::string>* status_modes_for(const GitStatusResult& status,
                                                     const std::string& rel) {
    std::string key = rel;
    auto slash = key.find('/');
    if (slash != std::string::npos) key = key.substr(0, slash);
    if (key.empty()) {
        return status.default_modes.empty() ? nullptr : &status.default_modes;
    }
    auto it = status.entries.find(key);
    if (it != status.entries.end()) return &it->second;
    return nullptr;
}

static std::string format_git_prefix(const std::set<std::string>* modes,
                                     bool is_dir,
                                     bool is_empty_dir,
                                     bool no_color) {
    bool saw_code = false;
    bool saw_visible = false;
    std::set<char> glyphs;

    if (modes) {
        for (const auto& code : *modes) {
            if (!code.empty()) saw_code = true;
            for (char ch : code) {
                if (ch == ' ' || ch == '!') continue;
                saw_visible = true;
                glyphs.insert(ch);
            }
        }
    }

    if (!saw_code) {
        if (is_dir && is_empty_dir) return std::string(4, ' ');
        std::string clean = "  \xe2\x9c\x93 ";
        if (no_color) return clean;
        return std::string(FG_GIT_CLEAN) + clean + FG_RESET;
    }

    if (!saw_visible) {
        return std::string(4, ' ');
    }

    std::string symbols;
    for (char ch : glyphs) symbols.push_back(ch);
    if (symbols.size() < 3) symbols.insert(symbols.begin(), 3 - symbols.size(), ' ');
    symbols.push_back(' ');

    if (no_color) return symbols;

    std::string out;
    out.reserve(symbols.size() + 16);
    for (char ch : symbols) {
        if (ch == ' ') {
            out.push_back(' ');
            continue;
        }
        const char* col = nullptr;
        switch (ch) {
            case '?':
            case 'A': col = FG_GIT_NEW; break;
            case 'M': col = FG_GIT_MOD; break;
            case 'D': col = FG_GIT_DEL; break;
            case 'R': col = FG_GIT_REN; break;
            case 'T': col = FG_GIT_TYP; break;
            case 'U': col = FG_GIT_CON; break;
            default: break;
        }
        if (col) {
            out += col;
            out.push_back(ch);
            out += FG_RESET;
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

static void apply_git_status(std::vector<Entry>& items, const fs::path& dir, const Options& opt) {
    if (!opt.git_status) return;

    auto status = get_git_status_for_dir(dir);
    for (auto& e : items) {
        std::error_code ec;
        fs::path base = fs::is_directory(dir) ? dir : dir.parent_path();
        fs::path relp = fs::relative(e.info.path, base, ec);
        std::string rel = ec ? e.info.path.filename().generic_string() : relp.generic_string();

        const std::set<std::string>* modes = status_modes_for(status, rel);
        bool is_empty_dir = false;
        if (e.info.is_dir) {
            is_empty_dir = fs::is_empty(e.info.path, ec);
            if (ec) is_empty_dir = false;
        }

        e.info.git_prefix = format_git_prefix(modes, e.info.is_dir, is_empty_dir, opt.no_color);
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

    switch (opt.sort) {
        case Options::Sort::Time: std::stable_sort(v.begin(), v.end(), cmp_time); break;
        case Options::Sort::Size: std::stable_sort(v.begin(), v.end(), cmp_size); break;
        case Options::Sort::Extension: std::stable_sort(v.begin(), v.end(), cmp_ext); break;
        case Options::Sort::None: /* keep directory order */ break;
        case Options::Sort::Name: default: std::stable_sort(v.begin(), v.end(), cmp_name); break;
    }
    if (opt.reverse) std::reverse(v.begin(), v.end());

    if (opt.group_dirs_first) {
        std::stable_sort(v.begin(), v.end(), [](const Entry& a, const Entry& b){
            return a.info.is_dir && !b.info.is_dir;
        });
    }
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
    // Heuristic: count UTF-8 code points, skipping ANSI escapes.
    size_t w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0x1b) { // ESC
            size_t j = i + 1;
            if (j < s.size() && s[j] == '[') {
                ++j;
                while (j < s.size() && s[j] != 'm') ++j;
                if (j < s.size()) ++j;
                i = j;
                continue;
            }
            ++i;
            continue;
        }

        size_t adv = 1;
        if      ((c & 0x80u) == 0x00u) adv = 1;
        else if ((c & 0xE0u) == 0xC0u && i + 1 < s.size()) adv = 2;
        else if ((c & 0xF0u) == 0xE0u && i + 2 < s.size()) adv = 3;
        else if ((c & 0xF8u) == 0xF0u && i + 3 < s.size()) adv = 4;
        i += adv;
        w += 1; // count as width 1
    }
    return w;
}

static void print_long(const std::vector<Entry>& v, const Options& opt) {
    // Determine column widths for metadata
    size_t w_owner = 0, w_group = 0, w_size = 0;
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
    struct Cell { std::string text; size_t width; };
    std::vector<Cell> cells;
    cells.reserve(v.size());

    size_t maxw = 0;
    for (const auto& e : v) {
        std::string cell;
        if (opt.git_status && !e.info.git_prefix.empty()) {
            cell += e.info.git_prefix;
            cell.push_back(' ');
        }
        if (!opt.no_color && !e.info.color_fg.empty()) cell += e.info.color_fg;
        cell += make_display_name(e);
        if (!opt.no_color && !e.info.color_fg.empty()) cell += FG_RESET;

        size_t w = printable_width(cell);
        maxw = std::max(maxw, w);
        cells.push_back({std::move(cell), w});
    }

    if (cells.empty()) return;

    const size_t gutter = 2;
    size_t per_row = 1;
    if (maxw > 0) {
        int cols = terminal_width();
        size_t denom = maxw + gutter;
        if (denom == 0) denom = 1;
        per_row = std::max<size_t>(1, static_cast<size_t>(std::max(1, cols)) / denom);
    }
    size_t rows = (cells.size() + per_row - 1) / per_row;

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < per_row; ++c) {
            size_t idx = c * rows + r;
            if (idx >= cells.size()) break;
            const auto& cell = cells[idx];
            std::cout << cell.text;

            size_t next = (c + 1) * rows + r;
            if (next < cells.size()) {
                size_t pad = gutter;
                if (cell.width < maxw) pad += (maxw - cell.width);
                std::cout << std::string(pad, ' ');
            }
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
