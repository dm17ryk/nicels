#pragma once

#include "nicels/options.hpp"

#include <mutex>

namespace nicels {

class Config {
public:
    static Config& instance();

    Config();

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void set_options(Options opts);
    [[nodiscard]] const Options& options() const noexcept;

private:
    mutable std::mutex mutex_{};
    Options options_{};
};

} // namespace nicels
