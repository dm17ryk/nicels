#pragma once

#include <filesystem>
#include <string>

namespace nls {

// Initialize resource search paths (idempotent). Should be called early.
void init_resource_paths(const char* argv0);

// Locate a resource file under the yaml directory search paths.
// Returns empty path if not found.
std::filesystem::path find_resource(const std::string& name);

} // namespace nls
