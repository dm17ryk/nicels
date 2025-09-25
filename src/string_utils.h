#pragma once

#include <string>

namespace nls {

class StringUtils {
public:
    static bool IsHidden(const std::string& name);
    static bool EqualsIgnoreCase(char a, char b);
    static std::string ToLower(std::string value);
};

} // namespace nls
