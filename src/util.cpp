#include "util.h"
#include "colors.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>

#include <argparse/argparse.hpp>

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include <windows.h>
#include <Aclapi.h>
#endif

namespace fs = std::filesystem;

namespace nls {

static std::optional<Options::QuotingStyle> parse_quoting_style_word(std::string word) {
    word = to_lower(std::move(word));
    if (word == "literal") return Options::QuotingStyle::Literal;
    if (word == "locale") return Options::QuotingStyle::Locale;
    if (word == "shell") return Options::QuotingStyle::Shell;
    if (word == "shell-always") return Options::QuotingStyle::ShellAlways;
    if (word == "shell-escape") return Options::QuotingStyle::ShellEscape;
    if (word == "shell-escape-always") return Options::QuotingStyle::ShellEscapeAlways;
    if (word == "c") return Options::QuotingStyle::C;
    if (word == "escape") return Options::QuotingStyle::Escape;
    return std::nullopt;
}

Options parse_args(int argc, char** argv) {
    Options opt;

    if (const char* env = std::getenv("QUOTING_STYLE")) {
        if (auto style = parse_quoting_style_word(env)) {
            opt.quoting_style = *style;
        }
    }

    std::vector<std::string> raw_args(argv, argv + argc);
    std::vector<std::string> normalized_args;
    normalized_args.reserve(raw_args.size() * 2);

    bool passthrough = false;
    for (size_t i = 0; i < raw_args.size(); ++i) {
        const std::string& token = raw_args[i];
        if (i == 0) {
            normalized_args.push_back(token);
            continue;
        }
        if (passthrough) {
            normalized_args.push_back(token);
            continue;
        }
        if (token.size() > 2 && token[0] == '-' && token[1] == '-') {
            auto eq = token.find('=');
            if (eq != std::string::npos) {
                std::string name = token.substr(0, eq);
                std::string value = token.substr(eq + 1);
                normalized_args.push_back(name);
                normalized_args.push_back(value);
                continue;
            }
        }
        if (token == "--color") {
            normalized_args.push_back(token);
            size_t next = i + 1;
            bool needs_default = (next >= raw_args.size());
            if (!needs_default) {
                const std::string& nxt = raw_args[next];
                needs_default = nxt.empty() || nxt[0] == '-';
            }
            if (needs_default) {
                normalized_args.emplace_back("always");
            }
            continue;
        }
        if (token == "--") {
            passthrough = true;
            continue;
        }
        if (token == "-1") {
            normalized_args.emplace_back("--one-per-line");
            continue;
        }
        if (token.size() > 2 && token[0] == '-' && token[1] != '-' && token.find('1', 1) != std::string::npos) {
            for (size_t j = 1; j < token.size(); ++j) {
                char ch = token[j];
                if (ch == '1') {
                    normalized_args.emplace_back("--one-per-line");
                } else {
                    std::string opt;
                    opt.reserve(2);
                    opt.push_back('-');
                    opt.push_back(ch);
                    normalized_args.push_back(opt);
                }
            }
            continue;
        }
        normalized_args.push_back(token);
    }

    argparse::ArgumentParser program("nls");
    program.add_description("NextLS — a colorful ls clone");

    program.add_argument("-l", "--long")
        .help("use a long listing format")
        .flag()
        .action([&](auto&&){ opt.format = Options::Format::Long; });

    program.add_argument("-1", "--one-per-line")
        .help("list one file per line")
        .flag()
        .action([&](auto&&){ opt.format = Options::Format::SingleColumn; });

    program.add_argument("-x")
        .help("list entries by lines instead of by columns")
        .flag()
        .action([&](auto&&){ opt.format = Options::Format::ColumnsHorizontal; });

    program.add_argument("-C")
        .help("list entries by columns instead of by lines")
        .flag()
        .action([&](auto&&){ opt.format = Options::Format::ColumnsVertical; });

    program.add_argument("--format")
        .help("use format: across (-x), horizontal (-x), long (-l), single-column (-1), vertical (-C)")
        .metavar("WORD")
        .action([&](const std::string& value) {
            std::string word = to_lower(value);
            if (word == "long" || word == "l") {
                opt.format = Options::Format::Long;
            } else if (word == "single-column" || word == "single" || word == "1") {
                opt.format = Options::Format::SingleColumn;
            } else if (word == "across" || word == "horizontal" || word == "x") {
                opt.format = Options::Format::ColumnsHorizontal;
            } else if (word == "vertical" || word == "columns" || word == "column" || word == "c") {
                opt.format = Options::Format::ColumnsVertical;
            } else {
                throw std::runtime_error("invalid value for --format: " + value);
            }
        });

    program.add_argument("--header")
        .help("print directory header and column names in long listing")
        .flag()
        .action([&](auto&&){ opt.header = true; });

    program.add_argument("-a", "--all")
        .help("do not ignore entries starting with .")
        .flag()
        .action([&](auto&&){ opt.all = true; });

    program.add_argument("-A", "--almost-all")
        .help("do not list . and ..")
        .flag()
        .action([&](auto&&){ opt.almost_all = true; });

    program.add_argument("-d", "--dirs")
        .help("show only directories")
        .flag()
        .action([&](auto&&){
            opt.dirs_only = true;
            opt.files_only = false;
        });

    program.add_argument("-f", "--files")
        .help("show only files")
        .flag()
        .action([&](auto&&){
            opt.files_only = true;
            opt.dirs_only = false;
        });

    program.add_argument("-p")
        .help("append / indicator to directories")
        .flag()
        .action([&](auto&&){ opt.indicator = Options::IndicatorStyle::Slash; });

    program.add_argument("-i", "--inode")
        .help("show inode number")
        .flag()
        .action([&](auto&&){ opt.show_inode = true; });

    program.add_argument("-o")
        .help("use a long listing format without group information")
        .flag()
        .action([&](auto&&){
            opt.format = Options::Format::Long;
            opt.show_group = false;
        });

    program.add_argument("-g")
        .help("use a long listing format without owner information")
        .flag()
        .action([&](auto&&){
            opt.format = Options::Format::Long;
            opt.show_owner = false;
        });

    program.add_argument("-G", "--no-group")
        .help("show no group information in a long listing")
        .flag()
        .action([&](auto&&){ opt.show_group = false; });

    program.add_argument("-n", "--numeric-uid-gid")
        .help("like -l, but list numeric user and group IDs")
        .flag()
        .action([&](auto&&){
            opt.format = Options::Format::Long;
            opt.numeric_uid_gid = true;
        });

    program.add_argument("-t")
        .help("sort by modification time, newest first")
        .flag()
        .action([&](auto&&){ opt.sort = Options::Sort::Time; });

    program.add_argument("-S")
        .help("sort by file size, largest first")
        .flag()
        .action([&](auto&&){ opt.sort = Options::Sort::Size; });

    program.add_argument("-X")
        .help("sort by file extension")
        .flag()
        .action([&](auto&&){ opt.sort = Options::Sort::Extension; });

    program.add_argument("-U")
        .help("do not sort; list entries in directory order")
        .flag()
        .action([&](auto&&){ opt.sort = Options::Sort::None; });

    program.add_argument("-r", "--reverse")
        .help("reverse order while sorting")
        .flag()
        .action([&](auto&&){ opt.reverse = true; });

    program.add_argument("--sort")
        .help("sort by WORD instead of name: none, size, time, extension")
        .metavar("WORD")
        .action([&](const std::string& value) {
            std::string word = to_lower(value);
            if (word == "none") {
                opt.sort = Options::Sort::None;
            } else if (word == "time" || word == "mtime") {
                opt.sort = Options::Sort::Time;
            } else if (word == "size") {
                opt.sort = Options::Sort::Size;
            } else if (word == "extension" || word == "ext") {
                opt.sort = Options::Sort::Extension;
            } else if (word == "name") {
                opt.sort = Options::Sort::Name;
            } else {
                throw std::runtime_error("invalid value for --sort: " + value);
            }
        });

    program.add_argument("--gs", "--git-status")
        .help("show git status for each file")
        .flag()
        .action([&](auto&&){ opt.git_status = true; });

    program.add_argument("--group-directories-first", "--sd", "--sort-dirs")
        .help("sort directories before files")
        .flag()
        .action([&](auto&&){
            opt.group_dirs_first = true;
            opt.sort_files_first = false;
        });

    program.add_argument("--sf", "--sort-files")
        .help("sort files first")
        .flag()
        .action([&](auto&&){
            opt.sort_files_first = true;
            opt.group_dirs_first = false;
        });

    program.add_argument("--df", "--dots-first")
        .help("sort dot-files and dot-folders first")
        .flag()
        .action([&](auto&&){ opt.dots_first = true; });

    program.add_argument("--no-icons", "--without-icons")
        .help("disable icons in output")
        .flag()
        .action([&](auto&&){ opt.no_icons = true; });

    program.add_argument("--no-color")
        .help("disable ANSI colors")
        .flag()
        .action([&](auto&&){ opt.no_color = true; });

    program.add_argument("--color")
        .help("colorize the output: auto, always, never")
        .default_value(std::string("auto"));

    program.add_argument("--light")
        .help("use light color scheme")
        .flag()
        .action([&](auto&&){ opt.color_theme = Options::ColorTheme::Light; });

    program.add_argument("--dark")
        .help("use dark color scheme")
        .flag()
        .action([&](auto&&){ opt.color_theme = Options::ColorTheme::Dark; });

    program.add_argument("--indicator-style")
        .help("append indicator with style STYLE to entry names: none, slash (-p)")
        .metavar("STYLE")
        .action([&](const std::string& value) {
            std::string word = to_lower(value);
            if (word == "slash" || word == "slashes") {
                opt.indicator = Options::IndicatorStyle::Slash;
            } else if (word == "none" || word == "off") {
                opt.indicator = Options::IndicatorStyle::None;
            } else {
                throw std::runtime_error("invalid value for --indicator-style: " + value);
            }
        });

    program.add_argument("-Q", "--quote-name")
        .help("enclose entry names in double quotes")
        .flag()
        .action([&](auto&&){ opt.quoting_style = Options::QuotingStyle::C; });

    program.add_argument("--quoting-style")
        .help("use quoting style WORD for entry names: literal, locale, shell, shell-always, shell-escape, shell-escape-always, c, escape")
        .metavar("WORD")
        .action([&](const std::string& value) {
            auto style = parse_quoting_style_word(value);
            if (!style) {
                throw std::runtime_error("invalid value for --quoting-style: " + value);
            }
            opt.quoting_style = *style;
        });

    program.add_argument("--time-style")
        .help("use time display format: default, locale, long-iso, full-iso, iso, iso8601, +FORMAT (default: locale)")
        .metavar("FORMAT")
        .action([&](const std::string& value){ opt.time_style = value; });

    program.add_argument("-L", "--dereference")
        .help("when showing file information for a symbolic link, show information for the file the link references")
        .flag()
        .action([&](auto&&){ opt.dereference = true; });

    program.add_argument("--bytes", "--non-human-readable")
        .help("show file sizes in bytes")
        .flag()
        .action([&](auto&&){ opt.bytes = true; });

    program.add_argument("--hyperlink")
        .help("emit hyperlinks for entries")
        .flag()
        .action([&](auto&&){ opt.hyperlink = true; });

    program.add_argument("--tree")
        .help("show tree view of directories, optionally limited to DEPTH")
        .metavar("DEPTH")
        .default_value(std::string(""))
        .implicit_value(std::string(""))
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("--report")
        .help("show summary report: short, long (default: long)")
        .metavar("WORD")
        .default_value(std::string(""))
        .implicit_value(std::string("long"))
        .nargs(argparse::nargs_pattern::optional);

    program.add_argument("paths")
        .help("paths to list")
        .remaining();

    try {
        program.parse_args(normalized_args);
    } catch (const std::exception& err) {
        std::cerr << "nls: " << err.what() << "\n";
        std::cerr << program << '\n';
        std::exit(2);
    }

    if (program.is_used("--color")) {
        std::string value = program.get<std::string>("--color");
        std::string word = to_lower(value);
        if (word.empty()) word = "always";
        if (word == "never") {
            opt.no_color = true;
        } else if (word == "always" || word == "auto") {
            opt.no_color = false;
        } else {
            std::cerr << "nls: invalid value for --color: " << value << "\n";
            std::cerr << program << '\n';
            std::exit(2);
        }
    }

    if (program.is_used("--tree")) {
        opt.tree = true;
        std::string value = program.get<std::string>("--tree");
        if (!value.empty()) {
            try {
                std::size_t idx = 0;
                unsigned long parsed = std::stoul(value, &idx);
                if (idx != value.size() || parsed == 0) {
                    throw std::invalid_argument("invalid depth");
                }
                opt.tree_depth = static_cast<std::size_t>(parsed);
            } catch (const std::exception&) {
                std::cerr << "nls: invalid value for --tree: " << value << "\n";
                std::cerr << program << '\n';
                std::exit(2);
            }
        }
    }

    if (program.is_used("--report")) {
        std::string value = program.get<std::string>("--report");
        std::string word = to_lower(value);
        if (word.empty() || word == "long") {
            opt.report = Options::Report::Long;
        } else if (word == "short") {
            opt.report = Options::Report::Short;
        } else {
            std::cerr << "nls: invalid value for --report: " << value << "\n";
            std::cerr << program << '\n';
            std::exit(2);
        }
    }

    try {
        opt.paths = program.get<std::vector<std::string>>("paths");
    } catch (const std::logic_error&) {
        // no positional paths provided
    }

    if (opt.paths.empty()) opt.paths.push_back(".");
    if (opt.all) opt.almost_all = false;
    return opt;
}

bool is_hidden(const std::string& name) {
    return !name.empty() && name[0] == '.';
}

bool iequals(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
}

std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string human_size(uintmax_t bytes) {
    static const char* units[] = {"B","KiB","MiB","GiB","TiB","PiB"};
    double v = static_cast<double>(bytes);
    int idx = 0;
    while (v >= 1024.0 && idx < 5) { v /= 1024.0; ++idx; }
    std::ostringstream oss;
    if (idx == 0) oss << (uintmax_t)v << ' ' << units[idx];
    else oss << std::fixed << std::setprecision(v < 10 ? 1 : 0) << v << ' ' << units[idx];
    return oss.str();
}

// Convert filesystem time to time_t (C++17 portable trick)
static std::time_t to_time_t(fs::file_time_type tp) {
    using namespace std::chrono;
    auto sctp = time_point_cast<system_clock::duration>(
        tp - fs::file_time_type::clock::now() + system_clock::now());
    return system_clock::to_time_t(sctp);
}

static std::string resolve_time_format(const Options& opt) {
    if (opt.time_style.empty() || opt.time_style == "locale" || opt.time_style == "default") {
        return "%a %b %d %H:%M:%S %Y";
    }

    std::string word = to_lower(opt.time_style);
    if (word == "long-iso") {
        return "%Y-%m-%d %H:%M";
    }
    if (word == "full-iso") {
        return "%Y-%m-%d %H:%M:%S %z";
    }
    if (word == "iso" || word == "iso8601") {
        return "%Y-%m-%d";
    }
    if (!opt.time_style.empty() && opt.time_style[0] == '+') {
        return opt.time_style.substr(1);
    }
    return opt.time_style;
}

std::string format_time(const fs::file_time_type& tp, const Options& opt) {
    std::time_t t = to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[128];
    std::string fmt = resolve_time_format(opt);
    if (fmt.empty()) fmt = "%a %b %d %H:%M:%S %Y";
    if (std::strftime(buf, sizeof(buf), fmt.c_str(), &tm) == 0) {
        std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm);
    }
    return buf;
}

std::string perm_string(const fs::directory_entry& de, bool is_symlink_hint, bool dereference) {
    std::error_code ec;
    auto link_status = de.symlink_status(ec);
    if (ec) return "???????????";
    bool is_link = fs::is_symlink(link_status) || is_symlink_hint;

    fs::file_status used_status = link_status;
    bool followed = false;
    if (dereference) {
        std::error_code follow_ec;
        auto follow = de.status(follow_ec);
        if (!follow_ec) {
            used_status = follow;
            followed = true;
        }
    }

    char type = '-';
    if (!followed && is_link) type = 'l';
    else if (fs::is_directory(used_status)) type = 'd';
    else if (fs::is_character_file(used_status)) type = 'c';
    else if (fs::is_block_file(used_status)) type = 'b';
    else if (fs::is_fifo(used_status)) type = 'p';
    else if (fs::is_socket(used_status)) type = 's';
    auto p = used_status.permissions();
    std::string out;
    out += type;
    if (p == fs::perms::unknown) {
        out.append(9, '?');
        return out;
    }

    auto has = [&](fs::perms mask) {
        return (p & mask) != fs::perms::none;
    };

    std::array<bool, 3> can_read = {
        has(fs::perms::owner_read),
        has(fs::perms::group_read),
        has(fs::perms::others_read)
    };
    std::array<bool, 3> can_write = {
        has(fs::perms::owner_write),
        has(fs::perms::group_write),
        has(fs::perms::others_write)
    };
    std::array<bool, 3> can_exec = {
        has(fs::perms::owner_exec),
        has(fs::perms::group_exec),
        has(fs::perms::others_exec)
    };

#ifdef _WIN32
    const auto native_path = de.path();
    DWORD attrs = GetFileAttributesW(native_path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if ((attrs & FILE_ATTRIBUTE_READONLY) != 0) {
            can_write.fill(false);
        } else {
            // Windows reports coarse permission bits where write access for
            // group/others mirrors the owner bit.  Mirror colorls behaviour
            // by suppressing those write flags unless the entry is read-only.
            can_write[1] = false;
            can_write[2] = false;
        }
    }
#endif

    auto append_triplet = [&](int idx, bool special, char special_char) {
        out += can_read[idx] ? 'r' : '-';
        bool writable = can_write[idx];
        out += writable ? 'w' : '-';
        if (special) {
            out += can_exec[idx]
                ? special_char
                : static_cast<char>(std::toupper(static_cast<unsigned char>(special_char)));
        } else {
            out += can_exec[idx] ? 'x' : '-';
        }
    };

    append_triplet(0, has(fs::perms::set_uid), 's');
    append_triplet(1, has(fs::perms::set_gid), 's');
    append_triplet(2, has(fs::perms::sticky_bit), 't');
    return out;
}

// Bootstrap color scheme for permission bits: r=green, w=red, x=yellow.
// Type char: blue for dir, cyan for link.
std::string colorize_perm(const std::string& perm, bool no_color) {
    if (no_color) return perm;
    const auto& theme = active_theme();
    std::string c_r = theme.color_or("read", "\x1b[32m");
    std::string c_w = theme.color_or("write", "\x1b[31m");
    std::string c_x = theme.color_or("exec", "\x1b[33m");
    std::string c_dir = theme.color_or("dir", "\x1b[34m");
    std::string c_link = theme.color_or("link", "\x1b[36m");

    std::string out;
    out.reserve(perm.size() * 5);
    for (size_t i = 0; i < perm.size(); ++i) {
        char ch = perm[i];
        if (i == 0) {
            if (ch == 'd' && !c_dir.empty()) {
                out += c_dir;
                out += ch;
                out += theme.reset;
            } else if (ch == 'l' && !c_link.empty()) {
                out += c_link;
                out += ch;
                out += theme.reset;
            } else {
                out += ch;
            }
        } else if (ch == 'r' && !c_r.empty()) {
            out += c_r;
            out += 'r';
            out += theme.reset;
        } else if (ch == 'w' && !c_w.empty()) {
            out += c_w;
            out += 'w';
            out += theme.reset;
        } else if ((ch == 'x' || ch == 's' || ch == 'S' || ch == 't' || ch == 'T') && !c_x.empty()) {
            out += c_x;
            out += ch;
            out += theme.reset;
        } else {
            out += ch;
        }
    }
    return out;
}

void fill_owner_group(FileInfo& fi, bool dereference) {
#ifndef _WIN32
    fi.owner.clear();
    fi.group.clear();
    fi.has_owner_id = false;
    fi.has_group_id = false;
    fi.has_link_size = false;

    auto assign_from_stat = [&](const struct stat& st) {
        fi.nlink = st.st_nlink;
        fi.inode = static_cast<uintmax_t>(st.st_ino);
        fi.owner_id = static_cast<uintmax_t>(st.st_uid);
        fi.group_id = static_cast<uintmax_t>(st.st_gid);
        fi.has_owner_id = true;
        fi.has_group_id = true;
        if (auto* pw = ::getpwuid(st.st_uid)) {
            fi.owner = pw->pw_name;
        } else {
            fi.owner = std::to_string(static_cast<uintmax_t>(st.st_uid));
        }
        if (auto* gr = ::getgrgid(st.st_gid)) {
            fi.group = gr->gr_name;
        } else {
            fi.group = std::to_string(static_cast<uintmax_t>(st.st_gid));
        }
    };

    struct stat st{};
    if (::lstat(fi.path.c_str(), &st) == 0) {
        assign_from_stat(st);
        fi.link_size = static_cast<uintmax_t>(st.st_size);
        fi.has_link_size = true;
    }

    if (dereference) {
        struct stat st_follow{};
        if (::stat(fi.path.c_str(), &st_follow) == 0) {
            assign_from_stat(st_follow);
        }
    }
#else
    fi.nlink = 1;
    fi.owner.clear();
    fi.group.clear();
    fi.inode = 0;
    fi.has_owner_id = false;
    fi.has_group_id = false;
    fi.has_link_size = false;

    std::wstring native_path = fi.path.native();

    DWORD share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (fi.is_symlink) {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    HANDLE handle = ::CreateFileW(native_path.c_str(),
                                  FILE_READ_ATTRIBUTES,
                                  share_mode,
                                  nullptr,
                                  OPEN_EXISTING,
                                  flags,
                                  nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION file_info{};
        if (::GetFileInformationByHandle(handle, &file_info)) {
            fi.nlink = file_info.nNumberOfLinks;
            ULARGE_INTEGER index{};
            index.HighPart = file_info.nFileIndexHigh;
            index.LowPart = file_info.nFileIndexLow;
            fi.inode = static_cast<uintmax_t>(index.QuadPart);
        }
        ::CloseHandle(handle);
    }

    auto wide_to_utf8 = [](const std::wstring& wide) -> std::string {
        if (wide.empty()) return {};
        int required = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 1) return {};
        std::string utf8(static_cast<size_t>(required - 1), '\0');
        int converted = ::WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), required, nullptr, nullptr);
        if (converted <= 0) return {};
        return utf8;
    };

    auto sid_to_account_name = [&](PSID sid) -> std::string {
        if (sid == nullptr || !::IsValidSid(sid)) return {};

        DWORD name_len = 0;
        DWORD domain_len = 0;
        SID_NAME_USE sid_type;
        if (!::LookupAccountSidW(nullptr, sid, nullptr, &name_len, nullptr, &domain_len, &sid_type)) {
            DWORD err = ::GetLastError();
            if (err != ERROR_INSUFFICIENT_BUFFER || name_len == 0) {
                return {};
            }
        }

        std::wstring name(name_len, L'\0');
        std::wstring domain(domain_len, L'\0');
        if (!::LookupAccountSidW(
                nullptr,
                sid,
                name_len ? name.data() : nullptr,
                &name_len,
                domain_len ? domain.data() : nullptr,
                &domain_len,
                &sid_type)) {
            return {};
        }

        name.resize(name_len);
        domain.resize(domain_len);

        if (name.empty()) return {};
        if (!domain.empty()) {
            return wide_to_utf8(domain + L"\\" + name);
        }
        return wide_to_utf8(name);
    };

    PSECURITY_DESCRIPTOR security_descriptor = nullptr;
    PSID owner_sid = nullptr;
    PSID group_sid = nullptr;
    DWORD result = ::GetNamedSecurityInfoW(
        native_path.c_str(),
        SE_FILE_OBJECT,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION,
        &owner_sid,
        &group_sid,
        nullptr,
        nullptr,
        &security_descriptor);

    if (result == ERROR_SUCCESS) {
        fi.owner = sid_to_account_name(owner_sid);
        fi.group = sid_to_account_name(group_sid);
    }

    if (security_descriptor) {
        ::LocalFree(security_descriptor);
    }
#endif
}

} // namespace nls
