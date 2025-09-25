#include "symlink_resolver.h"

namespace nls {

std::optional<std::filesystem::path> SymlinkResolver::ResolveTarget(const FileInfo& file_info) const {
    if (!file_info.is_symlink || !file_info.has_symlink_target) {
        return std::nullopt;
    }

    std::filesystem::path target = file_info.symlink_target;
    if (target.empty()) {
        return std::nullopt;
    }

    if (!target.is_absolute()) {
        std::filesystem::path base = file_info.path.parent_path();
        if (base.empty()) {
            target = target.lexically_normal();
        } else {
            target = (base / target).lexically_normal();
        }
    } else {
        target = target.lexically_normal();
    }

    return target;
}

} // namespace nls
