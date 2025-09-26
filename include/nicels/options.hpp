#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nicels {

enum class FormatStyle {
    Columns,
    Long,
    Single,
    Tree
};

enum class SortField {
    Name,
    Time,
    Size,
    Extension,
    None
};

enum class ColorPolicy {
    Auto,
    Always,
    Never
};

struct Options {
    std::vector<std::string> paths{};

    FormatStyle format{FormatStyle::Columns};
    SortField sort{SortField::Name};
    bool reverse{false};

    bool all{false};
    bool almost_all{false};
    bool directories_only{false};
    bool files_only{false};
    bool group_directories_first{false};
    bool sort_directories_last{false};

    bool git_status{false};
    bool icons{true};
    bool colors{true};
    bool hyperlink{false};
    bool show_header{false};
    bool show_inode{false};
    bool show_owner{true};
    bool show_group{true};
    bool numeric_ids{false};
    bool follow_symlinks{false};
    bool hide_control_chars{false};
    bool zero_terminate{false};
    bool show_block_size{false};
    bool ignore_backups{false};

    std::optional<std::size_t> tree_depth{};
    std::optional<int> output_width{};

    std::chrono::seconds recent_threshold{std::chrono::hours(24)};

    ColorPolicy color_policy{ColorPolicy::Auto};

    std::vector<std::string> hide_patterns{};
    std::vector<std::string> ignore_patterns{};

    std::string time_style{"default"};
    std::string block_size_suffix{};
    std::uintmax_t block_size{0};
    bool block_size_specified{false};
    bool block_size_show_suffix{false};

    int tab_size{8};
};

} // namespace nicels
