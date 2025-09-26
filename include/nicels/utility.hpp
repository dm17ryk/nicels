#pragma once

#include <string>
#include <string_view>

namespace nicels {

[[nodiscard]] bool wildcard_match(std::string_view pattern, std::string_view text);
[[nodiscard]] std::string sanitize_control_chars(std::string_view value, bool hide);

} // namespace nicels
