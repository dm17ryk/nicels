#include "nicels/perf.hpp"

#include "nicels/logger.hpp"

namespace nicels {

ScopedTimer::ScopedTimer(std::string name, bool enabled) noexcept
    : name_(std::move(name)), start_(Clock::now()), enabled_(enabled) {}

ScopedTimer::~ScopedTimer() {
    if (!enabled_) {
        return;
    }
    const auto end = Clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
    Logger::instance().log(LogLevel::Debug, "perf", name_ + " took " + std::to_string(duration.count()) + "µs");
}

} // namespace nicels
