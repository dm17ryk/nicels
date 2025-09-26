#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace nicels {

class Theme {
public:
    Theme(bool use_color, bool use_icons);

    [[nodiscard]] bool use_color() const noexcept;
    [[nodiscard]] bool use_icons() const noexcept;

    [[nodiscard]] std::string color_for(const std::filesystem::directory_entry& entry) const;
    [[nodiscard]] std::string icon_for(const std::filesystem::directory_entry& entry) const;

    static constexpr std::string_view reset_color() noexcept { return "\033[0m"; }

private:
    bool use_color_{false};
    bool use_icons_{false};
};

} // namespace nicels
