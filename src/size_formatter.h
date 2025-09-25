#pragma once

#include <cstdint>
#include <string>

namespace nls {

class SizeFormatter {
public:
    std::string FormatHumanReadable(uintmax_t bytes) const;
};

} // namespace nls
