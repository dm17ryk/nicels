#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "nicels/config.h"
#include "nicels/fs.h"

namespace nicels {

class Theme {
public:
    explicit Theme(const Config::Options& options);

    std::string icon_for(const FileEntry& entry) const;
    std::string colorize(const FileEntry& entry, std::string_view text) const;
    std::string git_status_color(std::string_view status) const;
    std::string reset() const;

private:
    void load_default_icons();
    void load_default_colors();

    const Config::Options& options_;
    std::unordered_map<std::string, std::string> icon_map_;
    std::unordered_map<std::string, std::string> color_map_;
};

} // namespace nicels
