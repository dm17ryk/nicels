#pragma once
#include <string>
#include <filesystem>
#include <vector>
#include <optional>

namespace nls {

struct Options {
    enum class Format {
        ColumnsVertical,
        ColumnsHorizontal,
        Long,
        SingleColumn,
        CommaSeparated
    } format = Format::ColumnsVertical;
    enum class IndicatorStyle { None, Slash } indicator = IndicatorStyle::Slash;
    enum class ColorTheme { Default, Light, Dark } color_theme = ColorTheme::Default;
    enum class Sort { Name, Time, Size, Extension, None } sort = Sort::Name;
    enum class Report { None, Short, Long } report = Report::None;
    enum class QuotingStyle {
        Literal,
        Locale,
        Shell,
        ShellAlways,
        ShellEscape,
        ShellEscapeAlways,
        C,
        Escape
    } quoting_style = QuotingStyle::Literal;

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
    bool tree = false;
    bool numeric_uid_gid = false;
    bool dereference = false;
    bool ignore_backups = false;
    bool hide_control_chars = false;
    bool zero_terminate = false;
    bool show_block_size = false;

    std::optional<std::size_t> tree_depth;   // max tree depth (levels of children)
    std::optional<int> output_width;         // desired terminal width override

    std::string time_style;    // strftime-style pattern or keyword
    std::vector<std::string> hide_patterns;
    std::vector<std::string> ignore_patterns;
    std::vector<std::string> paths;

    int tab_size = 8;
    uintmax_t block_size = 0;
    bool block_size_specified = false;
    bool block_size_show_suffix = false;
    std::string block_size_suffix;
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
    uintmax_t owner_id = 0;
    uintmax_t group_id = 0;
    bool has_owner_id = false;
    bool has_group_id = false;
    std::string owner_numeric;
    std::string group_numeric;
    bool has_owner_numeric = false;
    bool has_group_numeric = false;
    uintmax_t link_size = 0;
    bool has_link_size = false;
    uintmax_t allocated_size = 0;
    bool has_allocated_size = false;
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
std::string perm_string(const std::filesystem::directory_entry& de, bool is_symlink_hint, bool dereference);
std::string colorize_perm(const std::string& perm, bool no_color);
std::optional<std::filesystem::path> resolve_symlink_target_path(const FileInfo& fi);
void fill_owner_group(FileInfo& fi, bool dereference);

// Sorting helpers
std::string to_lower(std::string s);

} // namespace nls
