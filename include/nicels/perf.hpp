#pragma once

#include <chrono>
#include <string>

#include "nicels/logger.hpp"

namespace nicels {

class ScopedTimer {
public:
    explicit ScopedTimer(std::string label, LogLevel level = LogLevel::Debug)
        : label_(std::move(label))
        , level_(level)
        , start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
        Logger::instance().log(level_, "{} took {} us", label_, elapsed.count());
    }

private:
    std::string label_;
    LogLevel level_;
    std::chrono::steady_clock::time_point start_;
};

} // namespace nicels
