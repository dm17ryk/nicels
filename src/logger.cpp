#include "nicels/logger.hpp"

#include <ctime>
#include <iomanip>
#include <iostream>

namespace nicels {

namespace {
[[nodiscard]] constexpr std::string_view level_tag(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Error: return "ERROR";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Info: return "INFO";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Trace: return "TRACE";
    }
    return "UNKNOWN";
}
} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger()
    : stream_{&std::cerr} {}

void Logger::set_level(LogLevel level) noexcept {
    level_.store(level, std::memory_order_relaxed);
}

LogLevel Logger::level() const noexcept {
    return level_.load(std::memory_order_relaxed);
}

void Logger::log(LogLevel level, std::string_view message) {
    log(level, "nicels", message);
}

void Logger::log(LogLevel level, std::string_view tag, std::string_view message) {
    log_impl(level, tag, message);
}

void Logger::log_impl(LogLevel level, std::string_view tag, std::string_view message) {
    if (level > level_.load(std::memory_order_relaxed)) {
        return;
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif

    std::lock_guard lock(mutex_);
    (*stream_) << std::put_time(&tm, "%H:%M:%S") << ' ' << level_tag(level) << ' ' << tag << " - " << message << '\n';
}

} // namespace nicels
