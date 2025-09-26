#pragma once

#include <chrono>
#include <filesystem>
#include <format>
#include <optional>
#include <string>

namespace nicels::formatter {

inline std::string permissions(std::filesystem::perms p) {
    auto bit = [p](std::filesystem::perms mask, char c) {
        return (p & mask) != std::filesystem::perms::none ? c : '-';
    };
    std::string out;
    out.reserve(10);
    out.push_back(bit(std::filesystem::perms::owner_read, 'r'));
    out.push_back(bit(std::filesystem::perms::owner_write, 'w'));
    out.push_back(bit(std::filesystem::perms::owner_exec, 'x'));
    out.push_back(bit(std::filesystem::perms::group_read, 'r'));
    out.push_back(bit(std::filesystem::perms::group_write, 'w'));
    out.push_back(bit(std::filesystem::perms::group_exec, 'x'));
    out.push_back(bit(std::filesystem::perms::others_read, 'r'));
    out.push_back(bit(std::filesystem::perms::others_write, 'w'));
    out.push_back(bit(std::filesystem::perms::others_exec, 'x'));
    return out;
}

inline std::string file_size(uintmax_t size, std::string_view mode) {
    constexpr auto k1024 = 1024.0;
    constexpr auto k1000 = 1000.0;
    const double base = mode == "si" ? k1000 : k1024;
    const char* suffixes = mode == "si" ? "kMGTPE" : "KMGTPE";
    double value = static_cast<double>(size);
    std::size_t idx = 0;
    while (value >= base && idx < 6) {
        value /= base;
        ++idx;
    }
    if (idx == 0) {
        return std::format("{}", size);
    }
    return std::format("{:.1f}{}", value, suffixes[idx - 1]);
}

inline std::string timestamp(std::filesystem::file_time_type tp, std::string_view style) {
    using clock = std::chrono::system_clock;
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(tp - std::filesystem::file_time_type::clock::now()
        + clock::now());
    if (style == "full-iso" || style == "iso") {
        return std::format("{:%FT%TZ}", sctp);
    }
    if (style == "long-iso") {
        return std::format("{:%F %H:%M}", sctp);
    }
    return std::format("{:%F %R}", sctp);
}

inline std::string owner_group(const std::optional<std::string>& owner, const std::optional<std::string>& group) {
    return std::format("{}:{}", owner.value_or("?"), group.value_or("?"));
}

} // namespace nicels::formatter
