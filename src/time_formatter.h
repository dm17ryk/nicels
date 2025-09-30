#pragma once

#include <filesystem>
#include <string>

#include "config.h"

namespace nls {

class TimeFormatter {
public:
    std::string Format(
        const std::filesystem::file_time_type& timestamp,
        const Config& options) const;

private:
    std::time_t ToTimeT(std::filesystem::file_time_type timestamp) const;
    std::string ResolveFormat(const Config& options) const;
};

} // namespace nls
