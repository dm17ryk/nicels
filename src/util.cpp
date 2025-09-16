#include "util.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
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
#endif

namespace fs = std::filesystem;

namespace nls {

Options parse_args(int argc, char** argv) {
    Options opt;

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
        .action([&](auto&&){ opt.long_format = true; });

    program.add_argument("-1", "--one-per-line")
        .help("list one file per line")
        .flag()
        .action([&](auto&&){ opt.one_per_line = true; });

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
        .action([&](auto&&){ opt.group_dirs_first = true; });

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

    program.add_argument("--bytes", "--non-human-readable")
        .help("show file sizes in bytes")
        .flag()
        .action([&](auto&&){ opt.bytes = true; });

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

std::string format_time(const fs::file_time_type& tp) {
    std::time_t t = to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%a %b %d %H:%M:%S %Y", &tm);
    return buf;
}

std::string perm_string(const fs::directory_entry& de) {
    std::error_code ec;
    auto s = de.symlink_status(ec);
    if (ec) return "???????????";
    char type = '-';
    if (fs::is_directory(s)) type = 'd';
    else if (fs::is_symlink(s)) type = 'l';
    else if (fs::is_character_file(s)) type = 'c';
    else if (fs::is_block_file(s)) type = 'b';
    else if (fs::is_fifo(s)) type = 'p';
    else if (fs::is_socket(s)) type = 's';
    auto p = s.permissions();
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
    const char* C_RESET = "\x1b[0m";
    const char* C_R = "\x1b[32m"; // green
    const char* C_W = "\x1b[31m"; // red
    const char* C_X = "\x1b[33m"; // yellow
    const char* C_T_DIR = "\x1b[34m";
    const char* C_T_LNK = "\x1b[36m";

    std::string out; out.reserve(perm.size()*5);
    for (size_t i = 0; i < perm.size(); ++i) {
        char ch = perm[i];
        if (i == 0) {
            if      (ch == 'd') { out += C_T_DIR; out += ch; out += C_RESET; }
            else if (ch == 'l') { out += C_T_LNK; out += ch; out += C_RESET; }
            else { out += ch; }
        } else if (ch == 'r') {
            out += C_R; out += 'r'; out += C_RESET;
        } else if (ch == 'w') {
            out += C_W; out += 'w'; out += C_RESET;
        } else if (ch == 'x' || ch == 's' || ch == 'S' || ch == 't' || ch == 'T') {
            out += C_X; out += ch; out += C_RESET;
        } else {
            out += ch;
        }
    }
    return out;
}

void fill_owner_group(FileInfo& fi) {
#ifndef _WIN32
    struct stat st{};
    if (::lstat(fi.path.c_str(), &st) == 0) {
        fi.nlink = st.st_nlink;
        if (auto* pw = ::getpwuid(st.st_uid)) fi.owner = pw->pw_name;
        if (auto* gr = ::getgrgid(st.st_gid)) fi.group = gr->gr_name;
    }
#else
    fi.nlink = 1;
    fi.owner = "";
    fi.group = "";
#endif
}

} // namespace nls
