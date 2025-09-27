#include "nicels/logger.hpp"

#include <iostream>

namespace nicels {

Logger& Logger::instance() {
    static Logger logger;
    static bool initialized = [] {
        logger.set_output_stream(&std::cerr);
        return true;
    }();
    (void)initialized;
    return logger;
}

void Logger::set_level(Level level) noexcept { level_.store(level, std::memory_order_relaxed); }

Logger::Level Logger::level() const noexcept { return level_.load(std::memory_order_relaxed); }

void Logger::set_output_stream(std::ostream* stream) { stream_ = stream; }

} // namespace nicels
