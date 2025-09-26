#include "nicels/config.hpp"

namespace nicels {

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

Config::Config() = default;

void Config::set_options(Options opts) {
    std::lock_guard lock(mutex_);
    options_ = std::move(opts);
}

const Options& Config::options() const noexcept {
    std::lock_guard lock(mutex_);
    return options_;
}

} // namespace nicels
