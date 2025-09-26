#include "nicels/theme.hpp"

#include <system_error>
#include <unordered_map>

namespace nicels {

Theme::Theme(bool use_color, bool use_icons)
    : use_color_{use_color}
    , use_icons_{use_icons} {}

bool Theme::use_color() const noexcept {
    return use_color_;
}

bool Theme::use_icons() const noexcept {
    return use_icons_;
}

std::string Theme::color_for(const std::filesystem::directory_entry& entry) const {
    if (!use_color_) {
        return {};
    }
    std::error_code ec;
    if (entry.is_directory(ec)) {
        return "\033[34m"; // blue
    }
    ec.clear();
    if (entry.is_symlink(ec)) {
        return "\033[36m"; // cyan
    }
    ec.clear();
    if (entry.is_regular_file(ec)) {
        return "\033[37m"; // white
    }
    return "\033[35m"; // magenta for others
}

std::string Theme::icon_for(const std::filesystem::directory_entry& entry) const {
    if (!use_icons_) {
        return {};
    }
    std::error_code ec;
    if (entry.is_directory(ec)) {
        return "\uf07b"; // folder
    }
    ec.clear();
    if (entry.is_symlink(ec)) {
        return "\ue0b0"; // link glyph
    }
    ec.clear();
    if (entry.is_regular_file(ec)) {
        return "\uf15b"; // file icon
    }
    return "\uf128"; // question mark icon
}

} // namespace nicels
