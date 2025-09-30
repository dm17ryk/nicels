#include "permission_formatter.h"

#include <array>
#include <sstream>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

#include "theme.h"

namespace nls {

char PermissionFormatter::SymbolForPermissions(
    bool read, bool write, bool execute,
    std::filesystem::perms special, std::filesystem::perms mask,
    char special_char_lower, char special_char_upper) const
{
    if ((special & mask) != std::filesystem::perms::none) {
        if (execute) {
            return special_char_lower;
        }
        if (read || write) {
            return special_char_upper;
        }
    }
    return execute ? 'x' : '-';
}

std::string PermissionFormatter::Format(const std::filesystem::directory_entry& entry,
    bool is_symlink_hint,
    bool dereference) const
{
    std::error_code status_error;
    auto symlink_status = entry.symlink_status(status_error);
    if (status_error) {
        return "???????????";
    }

    const bool is_link = std::filesystem::is_symlink(symlink_status) || is_symlink_hint;
    std::filesystem::file_status status_to_use = symlink_status;
    bool followed = false;
    if (dereference) {
        std::error_code follow_error;
        auto resolved_status = entry.status(follow_error);
        if (!follow_error) {
            status_to_use = resolved_status;
            followed = true;
        }
    }

    char type_symbol = '-';
    if (!followed && is_link) {
        type_symbol = 'l';
    } else if (std::filesystem::is_directory(status_to_use)) {
        type_symbol = 'd';
    } else if (std::filesystem::is_character_file(status_to_use)) {
        type_symbol = 'c';
    } else if (std::filesystem::is_block_file(status_to_use)) {
        type_symbol = 'b';
    } else if (std::filesystem::is_fifo(status_to_use)) {
        type_symbol = 'p';
    } else if (std::filesystem::is_socket(status_to_use)) {
        type_symbol = 's';
    }

    const auto permissions = status_to_use.permissions();
    std::string result;
    result.reserve(10);
    result.push_back(type_symbol);
    if (permissions == std::filesystem::perms::unknown) {
        result.append(9, '?');
        return result;
    }

    auto has = [&](std::filesystem::perms mask) {
        return (permissions & mask) != std::filesystem::perms::none;
    };

    std::array<bool, 3> can_read = {
        has(std::filesystem::perms::owner_read),
        has(std::filesystem::perms::group_read),
        has(std::filesystem::perms::others_read)};

    std::array<bool, 3> can_write = {
        has(std::filesystem::perms::owner_write),
        has(std::filesystem::perms::group_write),
        has(std::filesystem::perms::others_write)};

    std::array<bool, 3> can_exec = {
        has(std::filesystem::perms::owner_exec),
        has(std::filesystem::perms::group_exec),
        has(std::filesystem::perms::others_exec)};

#ifdef _WIN32
    const auto native_path = entry.path();
    DWORD attrs = GetFileAttributesW(native_path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES) {
        if ((attrs & FILE_ATTRIBUTE_READONLY) != 0) {
            can_write.fill(false);
        } else {
            can_write[1] = false;
            can_write[2] = false;
        }
    }
#endif

    for (int i = 0; i < 3; ++i) {
        result.push_back(can_read[i] ? 'r' : '-');
        result.push_back(can_write[i] ? 'w' : '-');

        char exec_symbol = can_exec[i] ? 'x' : '-';
        if (i == 0) {
            exec_symbol = SymbolForPermissions(can_read[i], can_write[i], can_exec[i],
                permissions, std::filesystem::perms::set_uid, 's', 'S');
        } else if (i == 1) {
            exec_symbol = SymbolForPermissions(can_read[i], can_write[i], can_exec[i],
                permissions, std::filesystem::perms::set_gid, 's', 'S');
        } else if (i == 2) {
            exec_symbol = SymbolForPermissions(can_read[i], can_write[i], can_exec[i],
                permissions, std::filesystem::perms::sticky_bit, 't', 'T');
        }
        result.push_back(exec_symbol);
    }
    return result;
}

std::string PermissionFormatter::Colorize(const std::string& permissions, bool disable_color) const {
    if (disable_color || permissions.empty()) {
        return permissions;
    }

    const ThemeColors& theme = Theme::instance().colors();
    const std::string color_read = theme.color_or("read", "\x1b[32m");
    const std::string color_write = theme.color_or("write", "\x1b[31m");
    const std::string color_exec = theme.color_or("exec", "\x1b[33m");
    const std::string color_dir = theme.color_or("dir", "\x1b[34m");
    const std::string color_link = theme.color_or("link", "\x1b[36m");

    std::ostringstream stream;
    for (size_t index = 0; index < permissions.size(); ++index) {
        const char symbol = permissions[index];
        if (index == 0) {
            if (symbol == 'd' && !color_dir.empty()) {
                stream << color_dir << symbol << theme.reset;
            } else if (symbol == 'l' && !color_link.empty()) {
                stream << color_link << symbol << theme.reset;
            } else {
                stream << symbol;
            }
            continue;
        }

        if (symbol == 'r' && !color_read.empty()) {
            stream << color_read << 'r' << theme.reset;
        } else if (symbol == 'w' && !color_write.empty()) {
            stream << color_write << 'w' << theme.reset;
        } else if ((symbol == 'x' || symbol == 's' || symbol == 'S' || symbol == 't' || symbol == 'T') && !color_exec.empty()) {
            stream << color_exec << symbol << theme.reset;
        } else {
            stream << symbol;
        }
    }

    return stream.str();
}

} // namespace nls
