#pragma once

#include <filesystem>
#include <optional>
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

struct IconResult {
    std::string icon;
    bool recognized = false;
};

struct IconTheme {
    std::unordered_map<std::string, std::string> files;
    std::unordered_map<std::string, std::string> folders;
    std::unordered_map<std::string, std::string> file_aliases;
    std::unordered_map<std::string, std::string> folder_aliases;
};

class Theme {
public:
    static Theme& instance();

    void initialize(ColorScheme scheme, std::optional<std::string> custom_theme = std::nullopt);

    void set_active_scheme(ColorScheme scheme);
    ColorScheme active_scheme() const;

    const ThemeColors& colors();
    const ThemeColors& colors(ColorScheme scheme);
    std::string color_or(std::string_view key, std::string_view fallback = {});
    const std::string& color(std::string_view key);

    IconResult get_file_icon(std::string_view filename, bool is_executable);
    IconResult get_folder_icon(std::string_view folder_name);
    IconResult get_icon(std::string_view name, bool is_dir, bool is_executable);

    static std::string ApplyColor(const std::string& color,
                                  std::string_view text,
                                  const ThemeColors& theme,
                                  bool no_color);

private:
    Theme() = default;

    void ensure_loaded();

    IconResult folder_icon(std::string_view name);
    IconResult file_icon(std::string_view name, bool is_exec);

    bool loaded_ = false;
    ColorScheme active_scheme_ = ColorScheme::Dark;
    ThemeColors fallback_;
    ThemeColors dark_;
    ThemeColors light_;
    ThemeColors custom_theme_;
    std::optional<std::string> custom_theme_name_;
    IconTheme icons_;
    std::filesystem::path database_path_;
};

} // namespace nls
