#include "nicels/logger.hpp"

#include <iostream>
#include <mutex>

namespace nicels {

namespace {
Logger* g_instance = nullptr;
std::once_flag g_logger_once;
}

Logger::Logger()
    : stream_(&std::clog) {}

Logger& Logger::instance() {
    std::call_once(g_logger_once, [] { g_instance = new Logger(); });
    return *g_instance;
}

void Logger::set_level(LogLevel level) noexcept {
    level_ = level;
}

LogLevel Logger::level() const noexcept {
    return level_;
}

void Logger::set_stream(std::ostream& os) {
    std::lock_guard<std::mutex> lock(mutex_);
    stream_ = &os;
}

std::string_view Logger::level_to_string(LogLevel level) {
    switch (level) {
    case LogLevel::Error:
        return "ERROR";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Trace:
        return "TRACE";
    }
    return "UNKNOWN";
}

} // namespace nicels
