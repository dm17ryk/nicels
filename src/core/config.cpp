#include "nicels/config.h"

#include <utility>

namespace nicels {

Config& Config::instance() {
    static Config instance;
    return instance;
}

void Config::set_options(Options options) {
    options_ = std::move(options);
}

const Config::Options& Config::options() const noexcept {
    return options_;
}

void Config::set_program_name(std::string_view name) {
    program_name_.assign(name.begin(), name.end());
}

std::string_view Config::program_name() const noexcept {
    return program_name_;
}

} // namespace nicels
