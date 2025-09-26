#include "nicels/formatters.hpp"

#include <array>
#include <chrono>
#include <format>
#include <sstream>

namespace nicels {

namespace {

[[nodiscard]] char perm_char(std::filesystem::perms perms, std::filesystem::perms bit, char ch) {
    return (perms & bit) == std::filesystem::perms::none ? '-' : ch;
}

} // namespace

std::string format_permissions(const FileEntry& entry) {
    using std::filesystem::perms;
    const auto p = entry.status.permissions();
    std::string result;
    result.reserve(10);
    result.push_back(entry.is_directory ? 'd' : entry.is_symlink ? 'l' : '-');
    result.push_back(perm_char(p, perms::owner_read, 'r'));
    result.push_back(perm_char(p, perms::owner_write, 'w'));
    result.push_back(perm_char(p, perms::owner_exec, 'x'));
    result.push_back(perm_char(p, perms::group_read, 'r'));
    result.push_back(perm_char(p, perms::group_write, 'w'));
    result.push_back(perm_char(p, perms::group_exec, 'x'));
    result.push_back(perm_char(p, perms::others_read, 'r'));
    result.push_back(perm_char(p, perms::others_write, 'w'));
    result.push_back(perm_char(p, perms::others_exec, 'x'));
    return result;
}

std::string format_size(const FileEntry& entry, const Options& options) {
    constexpr std::array<const char*, 5> suffixes{"B", "K", "M", "G", "T"};
    std::uintmax_t value = entry.size;
    if (options.show_block_size && options.block_size_specified && options.block_size > 0) {
        const auto blocks = (entry.size + options.block_size - 1) / options.block_size;
        return std::to_string(blocks) + options.block_size_suffix;
    }
    double size = static_cast<double>(value);
    std::size_t suffix_index = 0;
    while (size >= 1024.0 && suffix_index + 1 < suffixes.size()) {
        size /= 1024.0;
        ++suffix_index;
    }
    std::ostringstream ss;
    if (suffix_index == 0) {
        ss << static_cast<std::uintmax_t>(size) << suffixes[suffix_index];
    } else {
        ss.setf(std::ios::fixed);
        ss.precision(1);
        ss << size << suffixes[suffix_index];
    }
    return ss.str();
}

std::string format_time(const FileEntry& entry, const Options& options) {
    (void)options;
    using namespace std::chrono;
    const auto sys_time = clock_cast<std::chrono::system_clock>(entry.last_write);
    const auto now = std::chrono::system_clock::now();
    const auto six_months = std::chrono::hours(24 * 30 * 6);
    if (now - sys_time > six_months || sys_time > now) {
        return std::format("{:%b %e  %Y}", std::chrono::floor<std::chrono::seconds>(sys_time));
    }
    return std::format("{:%b %e %H:%M}", std::chrono::floor<std::chrono::seconds>(sys_time));
}

} // namespace nicels
