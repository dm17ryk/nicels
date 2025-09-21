#pragma once
#include <string>
#include <filesystem>
#include <vector>

namespace nls {

struct Options {
    enum class Format { ColumnsVertical, ColumnsHorizontal, Long, SingleColumn } format = Format::ColumnsVertical;
    enum class IndicatorStyle { None, Slash } indicator = IndicatorStyle::Slash;
    enum class ColorTheme { Default, Light, Dark } color_theme = ColorTheme::Default;
    enum class Sort { Name, Time, Size, Extension, None } sort = Sort::Name;
    enum class Report { None, Short, Long } report = Report::None;

    bool all = false;
    bool almost_all = false;
    bool git_status = false;
    bool group_dirs_first = false;
    bool sort_files_first = false;
    bool dots_first = false;
    bool no_icons = false;
    bool no_color = false;
    bool reverse = false;
    bool bytes = false;        // show raw bytes instead of human-readable sizes
    bool dirs_only = false;    // show only directories
    bool files_only = false;   // show only files
    bool show_inode = false;
    bool show_owner = true;
    bool show_group = true;
    bool hyperlink = false;
    bool header = false;

    std::string time_style;    // strftime-style pattern or keyword
    std::vector<std::string> paths;
};

struct FileInfo {
    std::filesystem::path path;
    std::string name;
    bool is_dir = false;
    bool is_symlink = false;
    bool is_exec = false;
    bool is_hidden = false;
    bool is_broken_symlink = false;
    bool is_socket = false;
    bool is_block_device = false;
    bool is_char_device = false;
    bool has_recognized_icon = false;
    uintmax_t inode = 0;
    uintmax_t size = 0;
    std::filesystem::file_time_type mtime{};
    std::filesystem::path symlink_target;
    bool has_symlink_target = false;
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
std::string format_time(const std::filesystem::file_time_type& tp, const Options& opt);
std::string perm_string(const std::filesystem::directory_entry& de);
std::string colorize_perm(const std::string& perm, bool no_color);
void fill_owner_group(FileInfo& fi);

// Sorting helpers
std::string to_lower(std::string s);

} // namespace nls
