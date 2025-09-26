#pragma once

#include "nicels/filesystem_scanner.hpp"
#include "nicels/options.hpp"

#include <string>
#include <string_view>

namespace nicels {

class Theme {
public:
    explicit Theme(const Options& options);

    [[nodiscard]] std::string apply(const FileEntry& entry, std::string_view text) const;
    [[nodiscard]] std::string icon_for(const FileEntry& entry) const;
    [[nodiscard]] std::string reset() const;

private:
    bool colors_{false};
    bool icons_{false};
};

} // namespace nicels
