#include "string_utils.h"

#include <algorithm>
#include <cctype>
#include <string_view>

namespace nls {

std::string StringUtils::ToLower(std::string_view value)
{
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(StringUtils::ascii_to_lower(ch));
    });
    return result;
}

std::string StringUtils::Trim(std::string_view value)
{
    auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    auto begin = std::find_if_not(value.begin(), value.end(), [&](char ch) {
        return is_space(static_cast<unsigned char>(ch));
    });
    auto end = std::find_if_not(value.rbegin(), value.rend(), [&](char ch) {
        return is_space(static_cast<unsigned char>(ch));
    }).base();
    if (begin >= end) {
        return {};
    }
    return std::string(begin, end);
}

} // namespace nls
