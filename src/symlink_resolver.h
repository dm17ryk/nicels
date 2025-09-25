#pragma once

#include <filesystem>
#include <optional>

#include "file_info.h"

namespace nls {

class SymlinkResolver {
public:
    std::optional<std::filesystem::path> ResolveTarget(const FileInfo& file_info) const;
};

} // namespace nls
