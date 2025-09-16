#pragma once
#include <string>
#include <string_view>

namespace nls {

// Minimal built-in icon map (UTF-8). Extend via YAML later.
std::string icon_for(std::string_view name, bool is_dir, bool is_exec);

} // namespace nls
