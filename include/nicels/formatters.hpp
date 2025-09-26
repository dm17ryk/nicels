#pragma once

#include "nicels/filesystem_scanner.hpp"
#include "nicels/options.hpp"

#include <string>

namespace nicels {

[[nodiscard]] std::string format_permissions(const FileEntry& entry);
[[nodiscard]] std::string format_size(const FileEntry& entry, const Options& options);
[[nodiscard]] std::string format_time(const FileEntry& entry, const Options& options);

} // namespace nicels
