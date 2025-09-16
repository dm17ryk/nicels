#pragma once
#include <string>
#include <string_view>

namespace nls {

struct IconResult {
    std::string icon;
    bool recognized = false;
};

IconResult icon_for(std::string_view name, bool is_dir, bool is_exec);

} // namespace nls
