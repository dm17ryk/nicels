#pragma once

#include <filesystem>
#include <string>

#include "options.h"

namespace nls {

class TimeFormatter {
public:
    std::string Format(
        const std::filesystem::file_time_type& timestamp,
        const Options& options) const;

private:
    std::time_t ToTimeT(std::filesystem::file_time_type timestamp) const;
    std::string ResolveFormat(const Options& options) const;
};

} // namespace nls
