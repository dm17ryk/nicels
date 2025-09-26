#include "nicels/formatter.h"

#include <array>
#include <chrono>
#include <cmath>
#include <format>
#include <iomanip>
#include <locale>
#include <sstream>
#include <string>

namespace nicels::formatter {
namespace {

char file_type_char(const FileEntry& entry) {
    switch (entry.type) {
        case FileEntry::Type::Directory:
            return 'd';
        case FileEntry::Type::Symlink:
            return 'l';
        case FileEntry::Type::Block:
            return 'b';
        case FileEntry::Type::Character:
            return 'c';
        case FileEntry::Type::Fifo:
            return 'p';
        case FileEntry::Type::Socket:
            return 's';
        case FileEntry::Type::Regular:
            return '-';
        default:
            return '?';
    }
}

char permission_char(std::filesystem::perms perms, std::filesystem::perms mask, char letter) {
    return (perms & mask) != std::filesystem::perms::none ? letter : '-';
}

std::string human_readable_size(std::uintmax_t value) {
    static constexpr std::array units{"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    double size = static_cast<double>(value);
    std::size_t unit = 0;
    while (size >= 1024.0 && unit + 1 < units.size()) {
        size /= 1024.0;
        ++unit;
    }
    return std::format("{:.1f} {}", size, units[unit]);
}

std::string format_time(std::chrono::system_clock::time_point tp, const Config::Options& options) {
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    if (options.time_style == "full-iso") {
        return std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                           tm.tm_min, tm.tm_sec);
    }
    if (options.time_style == "long-iso") {
        return std::format("{:04}-{:02}-{:02} {:02}:{:02}", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
                           tm.tm_min);
    }
    if (options.time_style == "iso") {
        return std::format("{:02}-{:02} {:02}:{:02}", tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
    }

    std::ostringstream oss;
    try {
        oss.imbue(std::locale(""));
    } catch (...) {
        // fallback to classic locale
    }
    oss << std::put_time(&tm, "%b %e %H:%M");
    return oss.str();
}

} // namespace

std::string permissions(const FileEntry& entry) {
    std::string result(10, '-');
    result[0] = file_type_char(entry);

    const auto perms = entry.permissions;
    result[1] = permission_char(perms, std::filesystem::perms::owner_read, 'r');
    result[2] = permission_char(perms, std::filesystem::perms::owner_write, 'w');
    result[3] = permission_char(perms, std::filesystem::perms::owner_exec, 'x');
    result[4] = permission_char(perms, std::filesystem::perms::group_read, 'r');
    result[5] = permission_char(perms, std::filesystem::perms::group_write, 'w');
    result[6] = permission_char(perms, std::filesystem::perms::group_exec, 'x');
    result[7] = permission_char(perms, std::filesystem::perms::others_read, 'r');
    result[8] = permission_char(perms, std::filesystem::perms::others_write, 'w');
    result[9] = permission_char(perms, std::filesystem::perms::others_exec, 'x');
    return result;
}

std::string link_count(const FileEntry& entry) {
    return std::format("{}", entry.hardlink_count);
}

std::string owner(const FileEntry& entry, const Config::Options& options) {
    if (!options.show_owner) {
        return {};
    }
    if (options.numeric_uid_gid) {
        return std::format("{}", entry.owner_id);
    }
    if (!entry.owner_available) {
        return {};
    }
    return entry.owner;
}

std::string group(const FileEntry& entry, const Config::Options& options) {
    if (!options.show_group) {
        return {};
    }
    if (options.numeric_uid_gid) {
        return std::format("{}", entry.group_id);
    }
    if (!entry.group_available) {
        return {};
    }
    return entry.group;
}

std::string size(const FileEntry& entry, const Config::Options& options) {
    std::uintmax_t value = entry.size;
    if (options.show_block_size && entry.has_allocated_size) {
        value = entry.allocated_size;
    }
    if (options.show_bytes || value < 1024) {
        return std::format("{}", value);
    }
    return human_readable_size(value);
}

std::string modified_time(const FileEntry& entry, const Config::Options& options) {
    auto sys_time = std::chrono::clock_cast<std::chrono::system_clock>(entry.modified);
    return format_time(sys_time, options);
}

std::string inode(const FileEntry& entry) {
    if (!entry.has_inode) {
        return {};
    }
    return std::format("{}", entry.inode);
}

std::string git_status(const FileEntry& entry, const Theme& theme) {
    if (entry.git_status.empty()) {
        return {};
    }
    auto color = theme.git_status_color(entry.git_status);
    if (color.empty()) {
        return entry.git_status;
    }
    return color + entry.git_status + theme.reset();
}

} // namespace nicels::formatter
