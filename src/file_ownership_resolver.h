#pragma once

#include "file_info.h"
#include "symlink_resolver.h"

namespace nls {

class FileOwnershipResolver {
public:
    void Populate(FileInfo& file_info, bool dereference) const;
};

} // namespace nls
