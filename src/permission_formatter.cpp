#include "permission_formatter.h"

#include <array>
#include <sstream>
#include <string_view>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#endif

#include "config.h"
#include "theme.h"

namespace nls {

PermissionFormatter::PermissionFormatter(Options options)
    : options_(options) {}

PermissionFormatter::PermissionFormatter(const Config& config)
    : PermissionFormatter(Options{.dereference = config.dereference()}) {}

char PermissionFormatter::SymbolForPermissions(bool read,
                                               bool write,
                                               bool execute,
                                               std::filesystem::perms special,
                                               std::filesystem::perms mask,
                                               char special_char_lower,
                                               char special_char_upper) {
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

const std::filesystem::file_status* PermissionFormatter::StatusFor(const FileInfo& info) const {
    if (options_.dereference && info.has_target_status) {
        return &info.target_status;
    }
    if (info.has_symlink_status) {
        return &info.symlink_status;
    }
    return nullptr;
}

char PermissionFormatter::TypeSymbol(const FileInfo& info,
                                     const std::filesystem::file_status& status) const {
    using std::filesystem::file_type;

    if (info.is_broken_symlink || (info.is_symlink && !options_.dereference)) return 'l';

    switch (status.type()) {
        case file_type::symlink:
            return 'l';
        case file_type::character:
            return 'c';
        case file_type::block:
            return 'b';
        case file_type::fifo:
            return 'p';
        case file_type::socket:
            return 's';
        case file_type::directory:
            return 'd';
        case file_type::regular:
            return '-';
        default:
            break;
    }

    if (info.is_dir) return 'd';
    if (info.is_socket) return 's';
    if (info.is_block_device) return 'b';
    if (info.is_char_device) return 'c';
    return '-';
}

std::string PermissionFormatter::Format(const FileInfo& info) const {
    const std::filesystem::file_status* status = StatusFor(info);
    if (!status) {
        return "???????????";
    }

    std::string result;
    result.reserve(10);
    result.push_back(TypeSymbol(info, *status));

    auto permissions = status->permissions();
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
    DWORD attrs = GetFileAttributesW(info.path.c_str());
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
                                               permissions, std::filesystem::perms::set_uid,
                                               's', 'S');
        } else if (i == 1) {
            exec_symbol = SymbolForPermissions(can_read[i], can_write[i], can_exec[i],
                                               permissions, std::filesystem::perms::set_gid,
                                               's', 'S');
        } else if (i == 2) {
            exec_symbol = SymbolForPermissions(can_read[i], can_write[i], can_exec[i],
                                               permissions, std::filesystem::perms::sticky_bit,
                                               't', 'T');
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
        } else if ((symbol == 'x' || symbol == 's' || symbol == 'S' || symbol == 't' || symbol == 'T') &&
                   !color_exec.empty()) {
            stream << color_exec << symbol << theme.reset;
        } else {
            stream << symbol;
        }
    }

    return stream.str();
}

}  // namespace nls
