\
#include "util.h"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <system_error>

#ifndef _WIN32
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace nls {

static bool has_flag(const std::string& s, const char* flag) {
    return s == flag;
}

Options parse_args(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-l") opt.long_format = true;
        else if (a == "-1") opt.one_per_line = true;
        else if (a == "-a") opt.all = true;
        else if (a == "-A") opt.almost_all = true;
        else if (a == "-t") opt.sort = Options::Sort::Time;
        else if (a == "-S") opt.sort = Options::Sort::Size;
        else if (a == "-r") opt.reverse = true;
        else if (a == "-X") opt.sort = Options::Sort::Extension;
        else if (a == "-U") opt.sort = Options::Sort::None;
        else if (a == "--gs" || a == "--git-status") opt.git_status = true;
        else if (a == "--group-directories-first") opt.group_dirs_first = true;
        else if (a == "--no-icons") opt.no_icons = true;
        else if (a == "--no-color") opt.no_color = true;
        else if (!a.empty() && a[0] == '-' ) {
            std::cerr << "nls: unknown option: " << a << "\n";
        } else {
            opt.paths.push_back(a);
        }
    }
    if (opt.paths.empty()) opt.paths.push_back(".");
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
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
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
    auto bit = [&](fs::perms b, char c) { return ( (p & b) != fs::perms::none ) ? c : '-'; };
    std::string out;
    out += type;
    out += bit(fs::perms::owner_read,  'r');
    out += bit(fs::perms::owner_write, 'w');
    out += bit(fs::perms::owner_exec,  'x');
    out += bit(fs::perms::group_read,  'r');
    out += bit(fs::perms::group_write, 'w');
    out += bit(fs::perms::group_exec,  'x');
    out += bit(fs::perms::others_read,  'r');
    out += bit(fs::perms::others_write, 'w');
    out += bit(fs::perms::others_exec,  'x');
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
