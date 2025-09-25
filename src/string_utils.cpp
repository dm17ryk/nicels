#include "string_utils.h"

#include <cctype>

namespace nls {

bool StringUtils::IsHidden(const std::string& name) {
    return !name.empty() && name.front() == '.';
}

bool StringUtils::EqualsIgnoreCase(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
        std::tolower(static_cast<unsigned char>(b));
}

std::string StringUtils::ToLower(std::string value) {
    for (auto& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

} // namespace nls
