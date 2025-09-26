#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace nicels {

enum class GitStatusMode {
    Auto,
    Always,
    Never
};

struct ListingPath {
    std::filesystem::path path;
    bool explicit_path{false};
};

struct ListingOptions {
    bool long_format{false};
    bool single_column{false};
    bool include_all{false};
    bool almost_all{false};
    bool directories_only{false};
    bool files_only{false};
    bool sort_time{false};
    bool sort_size{false};
    bool reverse_sort{false};
    bool group_directories_first{false};
    bool enable_icons{true};
    bool enable_color{true};
    bool tree{false};
    bool classify{false};
    bool hide_control_chars{true};
    std::optional<int> tree_depth;
    std::string time_style{"default"};
    std::string size_style{"binary"};
    GitStatusMode git_status{GitStatusMode::Auto};
    bool hyperlink_paths{false};
    std::optional<std::string> report_mode;
};

class Config {
public:
    static Config& instance();

    void set_listing_options(ListingOptions options);
    [[nodiscard]] const ListingOptions& listing() const noexcept;

    void set_paths(std::vector<ListingPath> paths);
    [[nodiscard]] const std::vector<ListingPath>& paths() const noexcept;

    void set_locale(std::string locale);
    [[nodiscard]] std::string_view locale() const noexcept;

private:
    Config() = default;

    ListingOptions listing_{};
    std::vector<ListingPath> paths_{};
    std::string locale_{""};
};

} // namespace nicels
