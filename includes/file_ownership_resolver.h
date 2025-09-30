#pragma once

#include "file_info.h"
#include "symlink_resolver.h"

namespace nls {

class FileOwnershipResolver {
private:
#ifndef _WIN32
    bool MultiplyWithOverflow(uintmax_t a, uintmax_t b, uintmax_t& result) const;
#endif
public:
    void Populate(FileInfo& file_info, bool dereference) const;
};

} // namespace nls
