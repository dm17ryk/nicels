#include "nicels/logger.h"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace nicels {

namespace {
std::string_view level_name(Logger::Level level) {
    switch (level) {
        case Logger::Level::Error:
            return "error";
        case Logger::Level::Warning:
            return "warn";
        case Logger::Level::Info:
            return "info";
        case Logger::Level::Debug:
            return "debug";
        case Logger::Level::Trace:
            return "trace";
    }
    return "unknown";
}
} // namespace

Logger::Logger()
    : stream_{&std::clog}, level_{Level::Error} {}

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(Level level) noexcept {
    level_ = level;
}

Logger::Level Logger::level() const noexcept {
    return level_;
}

void Logger::set_output(std::ostream* stream) noexcept {
    std::scoped_lock lock{mutex_};
    stream_ = stream != nullptr ? stream : &std::clog;
}

void Logger::write(Level level, std::string_view message) {
    std::scoped_lock lock{mutex_};
    if (!stream_) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto seconds = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &seconds);
#else
    localtime_r(&seconds, &tm);
#endif
    (*stream_) << std::put_time(&tm, "%H:%M:%S") << ' ' << level_name(level) << " | " << message << '\n';
}

} // namespace nicels
