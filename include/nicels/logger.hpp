#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <ostream>
#include <string>
#include <string_view>

namespace nicels {

enum class LogLevel {
    Error = 0,
    Warn,
    Info,
    Debug,
    Trace
};

class Logger {
public:
    static Logger& instance();

    Logger();

    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    void log(LogLevel level, std::string_view message);
    void log(LogLevel level, std::string_view tag, std::string_view message);

private:
    std::mutex mutex_{};
    std::atomic<LogLevel> level_{LogLevel::Error};
    std::ostream* stream_{};

    void log_impl(LogLevel level, std::string_view tag, std::string_view message);
};

} // namespace nicels
