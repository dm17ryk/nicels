#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nicels {

class Config {
public:
    enum class LayoutFormat {
        ColumnsVertical,
        ColumnsHorizontal,
        Long,
        SingleColumn,
        CommaSeparated,
    };

    enum class IndicatorStyle {
        None,
        Slash,
    };

    enum class ColorTheme {
        Default,
        Light,
        Dark,
    };

    enum class SortMode {
        Name,
        Time,
        Size,
        Extension,
        None,
    };

    enum class ReportMode {
        None,
        Short,
        Long,
    };

    enum class QuotingStyle {
        Literal,
        Locale,
        Shell,
        ShellAlways,
        ShellEscape,
        ShellEscapeAlways,
        C,
        Escape,
    };

    struct Data {
        LayoutFormat layout_format { LayoutFormat::ColumnsVertical };
        IndicatorStyle indicator { IndicatorStyle::Slash };
        ColorTheme color_theme { ColorTheme::Default };
        SortMode sort_mode { SortMode::Name };
        ReportMode report_mode { ReportMode::None };
        QuotingStyle quoting_style { QuotingStyle::Literal };

        bool reverse { false };
        bool group_dirs_first { false };
        bool sort_files_first { false };
        bool dots_first { false };
        bool all { false };
        bool almost_all { false };
        bool git_status { false };
        bool dirs_only { false };
        bool files_only { false };
        bool no_icons { false };
        bool no_color { false };
        bool bytes { false };
        bool show_inode { false };
        bool show_owner { true };
        bool show_group { true };
        bool hyperlink { false };
        bool header { false };
        bool tree { false };
        bool numeric_uid_gid { false };
        bool dereference { false };
        bool ignore_backups { false };
        bool hide_control_chars { false };
        bool zero_terminate { false };
        bool show_block_size { false };

        std::optional<std::size_t> tree_depth {};
        std::optional<int> output_width {};
        std::optional<unsigned> tab_size { 8u };
        std::optional<std::uintmax_t> block_size {};
        bool block_size_show_suffix { false };
        std::string block_size_suffix {};

        std::string time_style {};
        std::vector<std::string> hide_patterns {};
        std::vector<std::string> ignore_patterns {};
        std::vector<std::string> paths {};
    };

    Config() = default;

    [[nodiscard]] const Data& data() const noexcept { return data_; }
    [[nodiscard]] Data& data() noexcept { return data_; }

    void paths(std::vector<std::string> paths) { data_.paths = std::move(paths); }

private:
    Data data_ {};
};

} // namespace nicels
