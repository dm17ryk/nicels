#pragma once

#include <filesystem>
#include <string>

#include "options.h"

namespace nls {

class PermissionFormatter {
private:
    char SymbolForPermissions(
        bool read, bool write, bool execute,
        std::filesystem::perms special, std::filesystem::perms mask,
        char special_char_lower, char special_char_upper) const;
public:
    std::string Format(const std::filesystem::directory_entry& entry,
        bool is_symlink_hint,
        bool dereference) const;

    std::string Colorize(const std::string& permissions, bool disable_color) const;
};

} // namespace nls
