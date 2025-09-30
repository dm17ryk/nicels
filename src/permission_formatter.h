#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "file_info.h"

namespace nls {

class Config;

class PermissionFormatter {
public:
    struct Options {
        bool dereference = false;
    };

    PermissionFormatter() = default;
    explicit PermissionFormatter(Options options);
    explicit PermissionFormatter(const Config& config);

    std::string Format(const FileInfo& info) const;
    std::string Colorize(const std::string& permissions, bool disable_color) const;

private:
    Options options_{};

    static char SymbolForPermissions(bool read,
                                     bool write,
                                     bool execute,
                                     std::filesystem::perms special,
                                     std::filesystem::perms mask,
                                     char special_char_lower,
                                     char special_char_upper);
    const std::filesystem::file_status* StatusFor(const FileInfo& info) const;
    char TypeSymbol(const FileInfo& info, const std::filesystem::file_status& status) const;
};

}  // namespace nls
