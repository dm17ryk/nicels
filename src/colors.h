#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace nls {

enum class ColorScheme {
    Dark,
    Light
};

struct ThemeColors {
    std::unordered_map<std::string, std::string> values;
    std::string reset = "\x1b[0m";

    void set(std::string key, std::string value);
    const std::string& get(std::string_view key) const;
    std::string color_or(std::string_view key, std::string_view fallback) const;
};

void load_color_themes();
void set_active_theme(ColorScheme scheme);
const ThemeColors& active_theme();
const ThemeColors& theme_for(ColorScheme scheme);

std::string apply_color(const std::string& color,
                        std::string_view text,
                        const ThemeColors& theme,
                        bool no_color);

} // namespace nls
