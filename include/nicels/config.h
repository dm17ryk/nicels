#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nicels {

class Config {
public:
    enum class FormatStyle {
        Columns,
        ColumnsHorizontal,
        Long,
        SingleColumn,
        CommaSeparated
    };

    enum class SortMode {
        Name,
        Time,
        Size,
        Extension,
        None
    };

    enum class ReportMode {
        None,
        Short,
        Long
    };

    enum class ColorTheme {
        Default,
        Light,
        Dark
    };

    struct Options {
        FormatStyle format = FormatStyle::Columns;
        SortMode sort_mode = SortMode::Name;
        ReportMode report = ReportMode::None;
        ColorTheme color_theme = ColorTheme::Default;

        bool show_all = false;
        bool show_almost_all = false;
        bool show_git_status = false;
        bool group_directories_first = false;
        bool sort_files_first = false;
        bool dots_first = false;
        bool icons_enabled = true;
        bool color_enabled = true;
        bool reverse = false;
        bool show_bytes = false;
        bool directories_only = false;
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
        bool dump_markdown = false;

        std::optional<std::size_t> tree_depth;
        std::optional<int> output_width;

        std::string time_style;
        std::vector<std::string> hide_patterns;
        std::vector<std::string> ignore_patterns;
        std::vector<std::filesystem::path> paths;

        int tab_size = 8;
        std::uintmax_t block_size = 0;
        bool block_size_specified = false;
        bool block_size_show_suffix = false;
        std::string block_size_suffix;
    };

    static Config& instance();

    void set_options(Options options);
    const Options& options() const noexcept;

    void set_program_name(std::string_view name);
    std::string_view program_name() const noexcept;

private:
    Config() = default;

    Options options_{};
    std::string program_name_;
};

} // namespace nicels
