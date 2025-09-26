#include "nicels/theme.hpp"

#include "nicels/platform.hpp"

#include <unordered_map>

namespace nicels {

namespace {

[[nodiscard]] std::string color_for(const FileEntry& entry) {
    if (entry.is_directory) {
        return "\033[1;34m"; // bright blue
    }
    if (entry.is_symlink) {
        return "\033[1;36m"; // bright cyan
    }
    if (entry.is_broken_symlink) {
        return "\033[1;31m"; // bright red
    }
    if (entry.is_executable) {
        return "\033[1;32m"; // bright green
    }
    return "\033[0m";
}

[[nodiscard]] std::string icon_for_path(const FileEntry& entry) {
    static const std::unordered_map<std::string, std::string> icon_map{
        {".cpp", ""},
        {".hpp", ""},
        {".h", ""},
        {".c", ""},
        {".py", ""},
        {".md", ""},
        {".json", ""},
        {".yml", ""},
        {".yaml", ""},
        {".sh", ""},
        {".txt", ""},
        {".png", ""},
        {".jpg", ""},
        {".jpeg", ""},
        {".gif", ""},
    };

    if (entry.is_directory) {
        return "";
    }
    if (entry.is_symlink) {
        return "";
    }
    if (entry.is_executable) {
        return "";
    }
    const auto ext = entry.path.extension().string();
    if (const auto it = icon_map.find(ext); it != icon_map.end()) {
        return it->second;
    }
    return "";
}

} // namespace

Theme::Theme(const Options& options)
    : colors_{supports_color(options.color_policy)}, icons_{options.icons} {}

std::string Theme::apply(const FileEntry& entry, std::string_view text) const {
    if (!colors_) {
        return std::string{text};
    }
    return color_for(entry) + std::string{text} + "\033[0m";
}

std::string Theme::icon_for(const FileEntry& entry) const {
    if (!icons_) {
        return {};
    }
    return icon_for_path(entry) + " ";
}

std::string Theme::reset() const {
    return colors_ ? "\033[0m" : std::string{};
}

} // namespace nicels
