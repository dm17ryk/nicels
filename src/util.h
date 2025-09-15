\
#pragma once
#include <string>
#include <filesystem>
#include <vector>

namespace nls {

struct Options {
    bool all = false;
    bool almost_all = false;
    bool long_format = false;
    bool one_per_line = false;
    bool git_status = false;
    bool group_dirs_first = false;
    bool no_icons = false;
    bool no_color = false;
    bool reverse = false;
    enum class Sort { Name, Time, Size, Extension, None } sort = Sort::Name;
    std::vector<std::string> paths;
};

struct FileInfo {
    std::filesystem::path path;
    std::string name;
    bool is_dir = false;
    bool is_symlink = false;
    bool is_exec = false;
    uintmax_t size = 0;
    std::filesystem::file_time_type mtime{};
#ifdef _WIN32
    unsigned long nlink = 1;
    std::string owner = "";
    std::string group = "";
#else
    unsigned long nlink = 1;
    std::string owner;
    std::string group;
#endif
    std::string icon;       // UTF-8 icon
    std::string color_fg;   // ANSI fg code or ""
    std::string color_reset;
    std::string git_prefix; // e.g. " M", "??", " ✓"
};

// Parsing & helpers
Options parse_args(int argc, char** argv);
bool is_hidden(const std::string& name);
bool iequals(char a, char b);
std::string human_size(uintmax_t bytes);
std::string format_time(const std::filesystem::file_time_type& tp);
std::string perm_string(const std::filesystem::directory_entry& de);
void fill_owner_group(FileInfo& fi);

// Sorting helpers
std::string to_lower(std::string s);

} // namespace nls
