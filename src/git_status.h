\
#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>

namespace nls {

// Map of relative path -> two-char status (like " M", "??", "A ", etc.);
// or empty string for clean (✓). This bootstrap returns empty for now unless USE_LIBGIT2=1.
using GitStatusMap = std::unordered_map<std::string, std::string>;

GitStatusMap get_git_status_for_dir(const std::filesystem::path& dir);

} // namespace nls
