#include "nicels/config.hpp"

#include <mutex>

namespace nicels {

namespace {
Config* g_instance = nullptr;
std::once_flag g_init_flag;
}

Config& Config::instance() {
    std::call_once(g_init_flag, [] { g_instance = new Config(); });
    return *g_instance;
}

void Config::set_listing_options(ListingOptions options) {
    listing_ = std::move(options);
}

const ListingOptions& Config::listing() const noexcept {
    return listing_;
}

void Config::set_paths(std::vector<ListingPath> paths) {
    paths_ = std::move(paths);
}

const std::vector<ListingPath>& Config::paths() const noexcept {
    return paths_;
}

void Config::set_locale(std::string locale) {
    locale_ = std::move(locale);
}

std::string_view Config::locale() const noexcept {
    return locale_;
}

} // namespace nicels
