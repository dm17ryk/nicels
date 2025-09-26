#pragma once

#include <chrono>
#include <string>

namespace nicels {

class ScopedTimer {
public:
    using Clock = std::chrono::steady_clock;

    ScopedTimer(std::string name, bool enabled = false) noexcept;
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    std::string name_;
    Clock::time_point start_;
    bool enabled_;
};

} // namespace nicels
