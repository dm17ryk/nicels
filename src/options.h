#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace nls {

class Options {
public:
    enum class Format {
        ColumnsVertical,
        ColumnsHorizontal,
        Long,
        SingleColumn,
        CommaSeparated
    };

    enum class IndicatorStyle { None, Slash };
    enum class ColorTheme { Default, Light, Dark };
    enum class Sort { Name, Time, Size, Extension, None };
    enum class Report { None, Short, Long };
    enum class QuotingStyle {
        Literal,
        Locale,
        Shell,
        ShellAlways,
        ShellEscape,
        ShellEscapeAlways,
        C,
        Escape
    };

    Options() = default;

    Format format = Format::ColumnsVertical;
    IndicatorStyle indicator = IndicatorStyle::Slash;
    ColorTheme color_theme = ColorTheme::Default;
    Sort sort = Sort::Name;
    Report report = Report::None;
    QuotingStyle quoting_style = QuotingStyle::Literal;

    bool all = false;
    bool almost_all = false;
    bool git_status = false;
    bool group_dirs_first = false;
    bool sort_files_first = false;
    bool dots_first = false;
    bool no_icons = false;
    bool no_color = false;
    bool reverse = false;
    bool bytes = false;
    bool dirs_only = false;
    bool files_only = false;
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

    std::optional<std::size_t> tree_depth;
    std::optional<int> output_width;

    std::string time_style;
    std::vector<std::string> hide_patterns;
    std::vector<std::string> ignore_patterns;
    std::vector<std::string> paths;

    int tab_size = 8;
    uintmax_t block_size = 0;
    bool block_size_specified = false;
    bool block_size_show_suffix = false;
    std::string block_size_suffix;
};

} // namespace nls
