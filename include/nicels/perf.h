#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <utility>

#include "nicels/logger.h"

namespace nicels::perf {

class ScopedTimer {
public:
    explicit ScopedTimer(std::string label)
        : label_{std::move(label)}, start_{std::chrono::steady_clock::now()} {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        Logger::instance().debug("{} took {} ms", label_, ms);
    }

private:
    std::string label_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace nicels::perf
