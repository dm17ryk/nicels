#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <iomanip>
#include <utility>
#include <cctype>
#include <chrono>
#include <array>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winioctl.h>

#ifndef REPARSE_DATA_BUFFER_HEADER_SIZE
typedef struct _REPARSE_DATA_BUFFER {
    DWORD ReparseTag;
    WORD ReparseDataLength;
    WORD Reserved;
    union {
        struct {
            WORD SubstituteNameOffset;
            WORD SubstituteNameLength;
            WORD PrintNameOffset;
            WORD PrintNameLength;
            ULONG Flags;
            WCHAR PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            WORD SubstituteNameOffset;
            WORD SubstituteNameLength;
            WORD PrintNameOffset;
            WORD PrintNameLength;
            WCHAR PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            BYTE DataBuffer[1];
        } GenericReparseBuffer;
    };
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_SIZE FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#endif
#endif

#include "console.h"
#include "util.h"
#include "icons.h"
#include "git_status.h"
#include "colors.h"
#include "resources.h"

namespace fs = std::filesystem;
using std::string;
using std::vector;

namespace nls {

struct Entry {
    FileInfo info;
};

struct TreeItem {
    Entry entry;
    std::vector<TreeItem> children;
};

#ifdef _WIN32
struct WindowsLinkInfo {
    bool is_link = false;
    bool has_target = false;
    fs::path target;
};

static std::wstring read_reparse_string(const WCHAR* path_buffer, USHORT offset, USHORT length) {
    if (length == 0) return {};
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(path_buffer);
    const WCHAR* start = reinterpret_cast<const WCHAR*>(bytes + offset);
    size_t count = length / sizeof(WCHAR);
    return std::wstring(start, start + count);
}

static std::wstring clean_windows_reparse_target(std::wstring target) {
    const std::wstring prefix = L"\\??\\";
    if (target.rfind(prefix, 0) == 0) {
        target.erase(0, prefix.size());
        const std::wstring unc_prefix = L"UNC\\";
        if (target.rfind(unc_prefix, 0) == 0) {
            target.erase(0, unc_prefix.size());
            target.insert(0, L"\\\\");
        }
    }
    return target;
}

static WindowsLinkInfo query_windows_link(const fs::path& path) {
    WindowsLinkInfo info;
    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return info;
    }
    if ((attrs & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        return info;
    }

    HANDLE handle = CreateFileW(path.c_str(), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                nullptr, OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        info.is_link = true;
        return info;
    }

    std::array<unsigned char, MAXIMUM_REPARSE_DATA_BUFFER_SIZE> buffer{};
    DWORD bytes_returned = 0;
    BOOL ok = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, nullptr, 0,
                              buffer.data(), static_cast<DWORD>(buffer.size()),
                              &bytes_returned, nullptr);
    CloseHandle(handle);
    if (!ok) {
        info.is_link = true;
        return info;
    }

    auto* header = reinterpret_cast<REPARSE_DATA_BUFFER*>(buffer.data());
    switch (header->ReparseTag) {
        case IO_REPARSE_TAG_SYMLINK: {
            info.is_link = true;
            const auto& data = header->SymbolicLinkReparseBuffer;
            std::wstring target = read_reparse_string(data.PathBuffer, data.PrintNameOffset, data.PrintNameLength);
            if (target.empty()) {
                target = read_reparse_string(data.PathBuffer, data.SubstituteNameOffset, data.SubstituteNameLength);
            }
            target = clean_windows_reparse_target(std::move(target));
            if (!target.empty()) {
                info.target = fs::path(target);
                info.has_target = true;
            }
            break;
        }
        case IO_REPARSE_TAG_MOUNT_POINT: {
            info.is_link = true;
            const auto& data = header->MountPointReparseBuffer;
            std::wstring target = read_reparse_string(data.PathBuffer, data.PrintNameOffset, data.PrintNameLength);
            if (target.empty()) {
                target = read_reparse_string(data.PathBuffer, data.SubstituteNameOffset, data.SubstituteNameLength);
            }
            target = clean_windows_reparse_target(std::move(target));
            if (!target.empty()) {
                info.target = fs::path(target);
                info.has_target = true;
            }
            break;
        }
        default:
            break;
    }
    return info;
}
#endif

struct ReportStats {
    size_t total = 0;
    size_t folders = 0;
    size_t recognized_files = 0;
    size_t unrecognized_files = 0;
    size_t links = 0;
    size_t dead_links = 0;

    size_t files() const { return recognized_files + unrecognized_files; }
};

static ReportStats compute_report_stats(const std::vector<Entry>& items) {
    ReportStats stats;
    stats.total = items.size();
    for (const auto& e : items) {
        bool is_directory = e.info.is_dir && !e.info.is_symlink;
        if (is_directory) {
            ++stats.folders;
        } else {
            if (e.info.has_recognized_icon) {
                ++stats.recognized_files;
            } else {
                ++stats.unrecognized_files;
            }
        }
        if (e.info.is_symlink) {
            ++stats.links;
            if (e.info.is_broken_symlink) {
                ++stats.dead_links;
            }
        }
    }
    return stats;
}

static void print_report_short(const ReportStats& stats) {
    std::cout << "    Folders: " << stats.folders
              << ", Files: " << stats.files() << ".\n\n";
}

static void print_report_long(const ReportStats& stats) {
    std::cout << "    Found " << stats.total << ' '
              << (stats.total == 1 ? "item" : "items")
              << " in total.\n\n";
    std::cout << "        Folders                 : " << stats.folders << "\n";
    std::cout << "        Recognized files        : " << stats.recognized_files << "\n";
    std::cout << "        Unrecognized files      : " << stats.unrecognized_files << "\n";
    std::cout << "        Links                   : " << stats.links << "\n";
    std::cout << "        Dead links              : " << stats.dead_links << "\n\n";
}

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

    const ThemeColors& theme_colors = active_theme();

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
        auto status = de.symlink_status(info_ec);
        if (!info_ec) {
            e.info.is_socket = fs::is_socket(status);
            e.info.is_block_device = fs::is_block_file(status);
            e.info.is_char_device = fs::is_character_file(status);
            e.info.is_symlink = fs::is_symlink(status);
        }
        info_ec.clear();
        if (!e.info.is_symlink) {
            e.info.is_symlink = de.is_symlink(info_ec);
        }
        info_ec.clear();
        e.info.is_dir = de.is_directory(info_ec);
        info_ec.clear();

        auto fill_symlink_target = [&]() {
            if (!e.info.is_symlink || e.info.has_symlink_target) return;
            std::error_code link_ec;
            auto target = fs::read_symlink(e.info.path, link_ec);
            if (!link_ec) {
                e.info.symlink_target = std::move(target);
                e.info.has_symlink_target = true;
            }
        };

        fill_symlink_target();
#ifdef _WIN32
        if (!e.info.is_symlink || !e.info.has_symlink_target) {
            WindowsLinkInfo link_info = query_windows_link(de.path());
            if (link_info.is_link) {
                e.info.is_symlink = true;
                if (!e.info.has_symlink_target && link_info.has_target) {
                    e.info.symlink_target = std::move(link_info.target);
                    e.info.has_symlink_target = true;
                }
            }
        }
        fill_symlink_target();
#endif

        if (opt.dirs_only && !e.info.is_dir) return;
        if (opt.files_only && e.info.is_dir) return;

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
        e.info.is_hidden = is_hidden(e.info.name);
        std::error_code exists_ec;
        bool exists = fs::exists(e.info.path, exists_ec);
        e.info.is_broken_symlink = e.info.is_symlink && (!exists || exists_ec);
        fill_owner_group(e.info);
        IconResult icon = icon_for(e.info.name, e.info.is_dir, e.info.is_exec);
        if (!opt.no_icons) {
            e.info.icon = icon.icon;
        }
        e.info.has_recognized_icon = icon.recognized && !e.info.is_dir;

        if (opt.no_color) {
            e.info.color_fg.clear();
            e.info.color_reset.clear();
        } else {
            std::string color;
            /*if (e.info.is_symlink) {
                color = e.info.is_broken_symlink ? theme_colors.get("dead_link") : theme_colors.get("link");
            } else*/ if (e.info.is_socket) {
                color = theme_colors.get("socket");
            } else if (e.info.is_block_device) {
                color = theme_colors.get("blockdev");
            } else if (e.info.is_char_device) {
                color = theme_colors.get("chardev");
            } else if (e.info.is_dir) {
                color = e.info.is_hidden ? theme_colors.get("hidden_dir") : theme_colors.get("dir");
            } else if (e.info.is_hidden) {
                color = theme_colors.get("hidden");
            } else if (e.info.is_exec) {
                color = theme_colors.get("executable_file");
            } else if (e.info.has_recognized_icon) {
                color = theme_colors.get("recognized_file");
            } else {
                color = theme_colors.get("unrecognized_file");
            }
            e.info.color_fg = std::move(color);
            e.info.color_reset = theme_colors.reset;
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

static std::string format_git_prefix(bool has_repo,
                                     const std::set<std::string>* modes,
                                     bool is_dir,
                                     bool is_empty_dir,
                                     bool no_color) {
    if (!has_repo) return {};
    bool saw_code = false;
    bool saw_visible = false;
    std::set<char> glyphs;
    const ThemeColors& theme = active_theme();
    std::string col_add = theme.color_or("addition", "\x1b[32m");
    std::string col_mod = theme.color_or("modification", "\x1b[33m");
    std::string col_del = theme.color_or("deletion", "\x1b[31m");
    std::string col_untracked = theme.color_or("untracked", "\x1b[35m");
    std::string col_clean = theme.color_or("unchanged", "\x1b[32m");
    std::string col_conflict = theme.color_or("error", "\x1b[31m");

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
        if (!has_repo) {
            return {};
        }
        if (is_dir && is_empty_dir) return std::string(4, ' ');
        std::string clean = "  \xe2\x9c\x93 ";
        if (no_color || col_clean.empty()) return clean;
        return col_clean + clean + theme.reset;
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
        const std::string* col = nullptr;
        switch (ch) {
            case '?':
                col = &col_untracked; break;
            case 'A':
                col = &col_add; break;
            case 'M':
                col = &col_mod; break;
            case 'D':
                col = &col_del; break;
            case 'R':
            case 'T':
                col = &col_mod; break;
            case 'U':
                col = &col_conflict; break;
            default: break;
        }
        if (col) {
            if (!col->empty()) {
                out += *col;
                out.push_back(ch);
                out += theme.reset;
            } else {
                out.push_back(ch);
            }
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

        e.info.git_prefix = format_git_prefix(status.repository_found, modes,
                                             e.info.is_dir, is_empty_dir, opt.no_color);
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
    if (opt.sort_files_first) {
        std::stable_sort(v.begin(), v.end(), [](const Entry& a, const Entry& b){
            return !a.info.is_dir && b.info.is_dir;
        });
    }
    if (opt.dots_first) {
        std::stable_sort(v.begin(), v.end(), [](const Entry& a, const Entry& b){
            bool da = is_hidden(a.info.name);
            bool db = is_hidden(b.info.name);
            return da && !db;
        });
    }
}

static std::vector<TreeItem> build_tree_items(const fs::path& dir,
                                             const Options& opt,
                                             std::size_t depth,
                                             std::vector<Entry>& flat) {
    std::vector<TreeItem> nodes;
    std::vector<Entry> items;
    collect_entries(dir, opt, items);
    apply_git_status(items, dir, opt);
    sort_entries(items, opt);

    nodes.reserve(items.size());
    for (const auto& item : items) {
        TreeItem node;
        node.entry = item;
        flat.push_back(item);

        bool is_dir = node.entry.info.is_dir && !node.entry.info.is_symlink;
        bool is_self = (node.entry.info.name == "." || node.entry.info.name == "..");
        bool within_limit = true;
        if (opt.tree_depth.has_value()) {
            within_limit = depth + 1 < *opt.tree_depth;
        }
        if (is_dir && within_limit && !is_self) {
            node.children = build_tree_items(node.entry.info.path, opt, depth + 1, flat);
        }

        nodes.push_back(std::move(node));
    }

    return nodes;
}

static std::chrono::system_clock::time_point to_system_clock(const fs::file_time_type& tp) {
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(tp - fs::file_time_type::clock::now() + system_clock::now());
}

static std::string age_color(const fs::file_time_type& tp, const ThemeColors& theme) {
    auto file_time = to_system_clock(tp);
    auto now = std::chrono::system_clock::now();
    auto diff = now - file_time;
    if (diff <= std::chrono::hours(1)) {
        return theme.get("hour_old");
    }
    if (diff <= std::chrono::hours(24)) {
        return theme.get("day_old");
    }
    return theme.get("no_modifier");
}

static std::string size_color(uintmax_t size, const ThemeColors& theme) {
    constexpr uintmax_t MEDIUM_THRESHOLD = 1ull * 1024ull * 1024ull;      // 1 MiB
    constexpr uintmax_t LARGE_THRESHOLD = 100ull * 1024ull * 1024ull;     // 100 MiB
    if (size >= LARGE_THRESHOLD) {
        return theme.get("file_large");
    }
    if (size >= MEDIUM_THRESHOLD) {
        return theme.get("file_medium");
    }
    return theme.get("file_small");
}

static std::string base_display_name(const Entry& e, const Options& opt) {
    std::string name = e.info.name;
    if (opt.indicator == Options::IndicatorStyle::Slash && e.info.is_dir) name.push_back('/');
    if (!e.info.icon.empty()) {
        return e.info.icon + std::string(" ") + name;
    }
    return name;
}

static std::string percent_encode(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    const char* hex = "0123456789ABCDEF";
    for (unsigned char ch : input) {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~' || ch == '/') {
            out.push_back(static_cast<char>(ch));
        } else {
            out.push_back('%');
            out.push_back(hex[(ch >> 4) & 0x0F]);
            out.push_back(hex[ch & 0x0F]);
        }
    }
    return out;
}

static std::string file_uri(const fs::path& path) {
    std::error_code ec;
    fs::path abs = fs::absolute(path, ec);
    if (ec) abs = path;
    abs = abs.lexically_normal();
    std::string generic = abs.generic_string();
#ifdef _WIN32
    if (generic.size() >= 2 && generic[1] == ':') {
        generic.insert(generic.begin(), '/');
    }
#endif
    return std::string("file://") + percent_encode(generic);
}

static std::string styled_name(const Entry& e, const Options& opt) {
    std::string label = base_display_name(e, opt);
    std::string out;
    const ThemeColors& theme = active_theme();
    if (opt.hyperlink) {
        out += "\x1b]8;;";
        out += file_uri(e.info.path);
        out += "\x1b\\";
    }
    if (!opt.no_color && !e.info.color_fg.empty()) out += e.info.color_fg;
    out += label;
    if (!opt.no_color && !e.info.color_fg.empty()) {
        if (!e.info.color_reset.empty()) {
            out += e.info.color_reset;
        } else {
            out += theme.reset;
        }
    }
    if (opt.hyperlink) {
        out += "\x1b]8;;\x1b\\";
    }
    return out;
}

static size_t printable_width(const std::string& s) {
    // Heuristic: count UTF-8 code points, skipping ANSI and OSC escapes.
    size_t w = 0;
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c == 0x1b) { // ESC
            size_t j = i + 1;
            if (j < s.size()) {
                unsigned char next = static_cast<unsigned char>(s[j]);
                if (next == '[') {
                    ++j;
                    while (j < s.size() && s[j] != 'm') ++j;
                    if (j < s.size()) ++j;
                    i = j;
                    continue;
                } else if (next == ']') {
                    ++j;
                    while (j < s.size()) {
                        if (s[j] == '\x07') { ++j; break; }
                        if (s[j] == '\x1b' && j + 1 < s.size() && s[j + 1] == '\\') { j += 2; break; }
                        ++j;
                    }
                    i = j;
                    continue;
                }
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

static size_t compute_inode_width(const std::vector<Entry>& v, const Options& opt) {
    if (!opt.show_inode) return 0;
    size_t width = 0;
    for (const auto& e : v) {
        std::string s = std::to_string(e.info.inode);
        width = std::max(width, s.size());
    }
    return width;
}

static std::string format_entry_cell(const Entry& e, const Options& opt, size_t inode_width, bool include_git_prefix) {
    std::string out;
    if (opt.show_inode) {
        std::string inode = std::to_string(e.info.inode);
        if (inode_width > inode.size()) out.append(inode_width - inode.size(), ' ');
        out += inode;
        out.push_back(' ');
    }
    if (include_git_prefix && opt.git_status && !e.info.git_prefix.empty()) {
        out += e.info.git_prefix;
        out.push_back(' ');
    }
    out += styled_name(e, opt);
    return out;
}

static std::string tree_prefix(const std::vector<bool>& branches, bool is_last) {
    std::string prefix;
    prefix.reserve(branches.size() * 4 + 5);
    for (bool branch : branches) {
        prefix += branch ? " │  " : "    ";
    }
    prefix += is_last ? " └── " : " ├── ";
    return prefix;
}

static void print_tree_nodes(const std::vector<TreeItem>& nodes,
                             const Options& opt,
                             size_t inode_width,
                             std::vector<bool>& branch_stack,
                             const ThemeColors& theme) {
    for (size_t i = 0; i < nodes.size(); ++i) {
        const TreeItem& node = nodes[i];
        bool is_last = (i + 1 == nodes.size());
        std::string prefix = tree_prefix(branch_stack, is_last);
        std::cout << apply_color(theme.get("tree"), prefix, theme, opt.no_color);
        std::cout << format_entry_cell(node.entry, opt, inode_width, true) << "\n";
        if (!node.children.empty()) {
            branch_stack.push_back(!is_last);
            print_tree_nodes(node.children, opt, inode_width, branch_stack, theme);
            branch_stack.pop_back();
        }
    }
}

static void print_tree_view(const std::vector<TreeItem>& nodes,
                            const Options& opt,
                            size_t inode_width) {
    std::vector<bool> branch_stack;
    const ThemeColors& theme = active_theme();
    print_tree_nodes(nodes, opt, inode_width, branch_stack, theme);
}

static void print_long(const std::vector<Entry>& v, const Options& opt, size_t inode_width) {
    constexpr size_t perm_width = 10;
    const ThemeColors& theme = active_theme();

    // Determine column widths for metadata
    size_t w_owner = 0, w_group = 0, w_size = 0, w_nlink = 0, w_time = 0, w_git = 0;
    for (const auto& e : v) {
        if (opt.show_owner) w_owner = std::max(w_owner, e.info.owner.size());
        if (opt.show_group) w_group = std::max(w_group, e.info.group.size());
        w_nlink = std::max(w_nlink, std::to_string(e.info.nlink).size());
        std::string size_str = opt.bytes ? std::to_string(e.info.size) : human_size(e.info.size);
        w_size = std::max(w_size, size_str.size());
        std::string time_str = format_time(e.info.mtime, opt);
        w_time = std::max(w_time, time_str.size());
        if (opt.git_status) {
            w_git = std::max(w_git, printable_width(e.info.git_prefix));
        }
    }

    const std::string owner_color = opt.no_color ? std::string() : theme.get("owned");
    const std::string group_color = opt.no_color ? std::string() : theme.get("group");

    const std::string size_header = opt.bytes ? "Length" : "Size";
    const std::string links_header = "Links";
    const std::string owner_header = "Owner";
    const std::string group_header = "Group";
    const std::string time_header = "LastWriteTime";
    const std::string inode_header = "Inode";
    const std::string git_header = "Git";
    const std::string name_header = "Name";

    enum class HeaderAlign { Left, Right };
    const std::string& header_color = theme.get("header_names");
    auto format_header_cell = [&](const std::string& text, size_t width, HeaderAlign align) {
        std::ostringstream oss;
        if (align == HeaderAlign::Left) {
            oss << std::left;
        } else {
            oss << std::right;
        }
        if (width > 0) {
            oss << std::setw(static_cast<int>(width));
        }
        oss << text;
        return apply_color(header_color, oss.str(), theme, opt.no_color);
    };
    auto format_simple_header = [&](const std::string& text) {
        return apply_color(header_color, text, theme, opt.no_color);
    };

    if (opt.header) {
        if (opt.show_inode) inode_width = std::max(inode_width, inode_header.size());
        w_nlink = std::max(w_nlink, links_header.size());
        if (opt.show_owner) w_owner = std::max(w_owner, owner_header.size());
        if (opt.show_group) w_group = std::max(w_group, group_header.size());
        w_size = std::max(w_size, size_header.size());
        w_time = std::max(w_time, time_header.size());
        if (opt.git_status) w_git = std::max(w_git, git_header.size());
    }

    if (opt.header) {
        if (opt.show_inode) {
            std::cout << format_header_cell(inode_header, inode_width, HeaderAlign::Right) << ' ';
        }
        std::cout << format_header_cell("Mode", perm_width, HeaderAlign::Left) << ' ';
        std::cout << format_header_cell(links_header, w_nlink, HeaderAlign::Right) << ' ';
        if (opt.show_owner) {
            std::cout << format_header_cell(owner_header, w_owner, HeaderAlign::Left) << ' ';
        }
        if (opt.show_group) {
            std::cout << format_header_cell(group_header, w_group, HeaderAlign::Left) << ' ';
        }
        std::cout << format_header_cell(size_header, w_size, HeaderAlign::Right) << ' ';
        std::cout << format_header_cell(time_header, w_time, HeaderAlign::Left) << ' ';
        if (opt.git_status) {
            std::cout << format_header_cell(git_header, w_git, HeaderAlign::Left) << ' ';
        }
        std::cout << format_simple_header(name_header) << "\n";

        if (opt.show_inode) {
            std::cout << std::string(inode_width, '-') << ' ';
        }
        std::cout << std::string(perm_width, '-') << ' ';
        std::cout << std::string(w_nlink, '-') << ' ';
        if (opt.show_owner) std::cout << std::string(w_owner, '-') << ' ';
        if (opt.show_group) std::cout << std::string(w_group, '-') << ' ';
        std::cout << std::string(w_size, '-') << ' ';
        std::cout << std::string(w_time, '-') << ' ';
        if (opt.git_status) std::cout << std::string(w_git, '-') << ' ';
        std::cout << std::string(name_header.size(), '-') << "\n";
    }

    for (const auto& e : v) {
        if (opt.show_inode) {
            std::cout << std::right << std::setw(static_cast<int>(inode_width)) << e.info.inode << ' ';
        }

        std::string perm = perm_string(fs::directory_entry(e.info.path));
        std::cout << colorize_perm(perm, opt.no_color) << ' ';

        std::cout << std::right << std::setw(static_cast<int>(w_nlink)) << e.info.nlink << ' ';

        if (opt.show_owner) {
            if (!owner_color.empty()) std::cout << owner_color;
            std::cout << std::left << std::setw(static_cast<int>(w_owner)) << e.info.owner;
            if (!owner_color.empty()) std::cout << theme.reset;
            std::cout << ' ';
        }
        if (opt.show_group) {
            if (!group_color.empty()) std::cout << group_color;
            std::cout << std::left << std::setw(static_cast<int>(w_group)) << e.info.group;
            if (!group_color.empty()) std::cout << theme.reset;
            std::cout << ' ';
        }

        std::string size_str = opt.bytes ? std::to_string(e.info.size) : human_size(e.info.size);
        std::string size_col = opt.no_color ? std::string() : size_color(e.info.size, theme);
        if (!size_col.empty()) std::cout << size_col;
        std::cout << std::right << std::setw(static_cast<int>(w_size)) << size_str;
        if (!size_col.empty()) std::cout << theme.reset;
        std::cout << ' ';

        std::string time_str = format_time(e.info.mtime, opt);
        std::string time_col = opt.no_color ? std::string() : age_color(e.info.mtime, theme);
        if (!time_col.empty()) std::cout << time_col;
        if (opt.header) {
            std::cout << std::left << std::setw(static_cast<int>(w_time)) << time_str;
        } else {
            std::cout << time_str;
        }
        if (!time_col.empty()) std::cout << theme.reset;
        std::cout << ' ';

        if (opt.git_status) {
            if (opt.header) {
                std::cout << e.info.git_prefix;
                size_t git_width = printable_width(e.info.git_prefix);
                if (w_git > git_width) {
                    std::cout << std::string(w_git - git_width, ' ');
                }
                std::cout << ' ';
            } else if (!e.info.git_prefix.empty()) {
                std::cout << e.info.git_prefix << ' ';
            }
        }

        std::cout << styled_name(e, opt);

        // symlink target
        if (e.info.is_symlink) {
            std::string target_str;
            if (e.info.has_symlink_target) {
                target_str = e.info.symlink_target.string();
            } else {
                std::error_code ec;
                auto target = fs::read_symlink(e.info.path, ec);
                if (!ec) target_str = target.string();
            }
            if (!target_str.empty()) {
                const char* arrow = "  \xE2\x87\x92 ";
                bool broken = e.info.is_broken_symlink;
                bool use_color = !opt.no_color;
                std::string link_color;
                if (use_color) {
                    link_color = broken ? theme.get("dead_link") : theme.get("link");
                    if (link_color.empty()) use_color = false;
                }

                if (use_color) std::cout << link_color;
                std::cout << arrow;
                std::cout << target_str;
                if (broken) {
                    std::cout << " [Dead link]";
                }
                if (use_color) std::cout << theme.reset;
            }
        }

        std::cout << "\n";
    }
}

static void print_columns(const std::vector<Entry>& v, const Options& opt, size_t inode_width) {
    struct Cell { std::string text; size_t width; };
    std::vector<Cell> cells;
    cells.reserve(v.size());

    size_t maxw = 0;
    for (const auto& e : v) {
        std::string cell = format_entry_cell(e, opt, inode_width, true);
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
            size_t idx;
            if (opt.format == Options::Format::ColumnsHorizontal) {
                idx = r * per_row + c;
            } else {
                idx = c * rows + r;
            }
            if (idx >= cells.size()) break;
            const auto& cell = cells[idx];
            std::cout << cell.text;

            size_t next;
            if (opt.format == Options::Format::ColumnsHorizontal) {
                next = r * per_row + (c + 1);
            } else {
                next = (c + 1) * rows + r;
            }
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
    std::error_code dir_ec;
    bool is_directory = fs::is_directory(p, dir_ec);
    const ThemeColors& theme = active_theme();

    if (opt.tree) {
        std::vector<Entry> flat;
        if (is_directory) {
            if (opt.paths.size() > 1) {
                std::cout << p.string() << ":\n";
            }
            auto nodes = build_tree_items(p, opt, 0, flat);
            size_t inode_width = compute_inode_width(flat, opt);
            print_tree_view(nodes, opt, inode_width);
        } else {
            std::vector<Entry> single;
            collect_entries(p, opt, single);
            apply_git_status(single, is_directory ? p : p.parent_path(), opt);
            sort_entries(single, opt);
            flat = single;
            size_t inode_width = compute_inode_width(flat, opt);
            for (const auto& e : single) {
                std::cout << format_entry_cell(e, opt, inode_width, true) << "\n";
            }
        }

        if (opt.report != Options::Report::None) {
            ReportStats stats = compute_report_stats(flat);
            std::cout << "\n";
            if (opt.report == Options::Report::Long) {
                print_report_long(stats);
            } else {
                print_report_short(stats);
            }
        }

        if (opt.paths.size() > 1) std::cout << "\n";
        return 0;
    }

    std::vector<Entry> items;
    collect_entries(p, opt, items);
    apply_git_status(items, is_directory ? p : p.parent_path(), opt);
    sort_entries(items, opt);

    if (opt.header && opt.format == Options::Format::Long) {
        std::error_code ec;
        fs::path absolute_path = fs::absolute(p, ec);
        fs::path header_path;
        if (!ec) {
            header_path = is_directory ? absolute_path : absolute_path.parent_path();
        }
        if (header_path.empty()) {
            header_path = is_directory ? p : p.parent_path();
        }
        if (header_path.empty()) {
            std::error_code cwd_ec;
            fs::path cwd = fs::current_path(cwd_ec);
            if (!cwd_ec) header_path = cwd;
        }
        if (!header_path.empty()) {
            header_path = header_path.lexically_normal();
        }
        std::string header_str = header_path.string();
        if (!header_str.empty()) {
            std::string root_str = header_path.root_path().string();
            while (header_str.size() > 1 && (header_str.back() == '/' || header_str.back() == '\\') && header_str != root_str) {
                header_str.pop_back();
            }
        }
        std::string colored_header = apply_color(theme.get("header_directory"), header_str, theme, opt.no_color);
        std::cout << "\nDirectory: " << colored_header << "\n\n";
    } else if (opt.paths.size() > 1 && is_directory) {
        std::cout << p.string() << ":\n";
    }

    size_t inode_width = compute_inode_width(items, opt);
    switch (opt.format) {
        case Options::Format::Long:
            print_long(items, opt, inode_width);
            break;
        case Options::Format::SingleColumn:
            for (const auto& e : items) {
                std::cout << format_entry_cell(e, opt, inode_width, true) << "\n";
            }
            break;
        case Options::Format::ColumnsHorizontal:
        case Options::Format::ColumnsVertical:
        default:
            print_columns(items, opt, inode_width);
            break;
    }

    if (opt.report != Options::Report::None) {
        ReportStats stats = compute_report_stats(items);
        std::cout << "\n";
        if (opt.report == Options::Report::Long) {
            print_report_long(stats);
        } else {
            print_report_short(stats);
        }
    }

    if (opt.paths.size() > 1) std::cout << "\n";
    return 0;
}

} // namespace nls

int main(int argc, char** argv) {
    using namespace nls;
    enable_virtual_terminal();
    init_resource_paths(argc > 0 ? argv[0] : nullptr);
    load_color_themes();
    Options opt = parse_args(argc, argv);
    ColorScheme scheme = ColorScheme::Dark;
    switch (opt.color_theme) {
        case Options::ColorTheme::Light:
            scheme = ColorScheme::Light;
            break;
        case Options::ColorTheme::Dark:
        case Options::ColorTheme::Default:
        default:
            scheme = ColorScheme::Dark;
            break;
    }
    set_active_theme(scheme);
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
