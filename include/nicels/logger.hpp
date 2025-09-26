#pragma once

#include <array>
#include <chrono>
#include <format>
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

    void set_level(LogLevel level) noexcept;
    [[nodiscard]] LogLevel level() const noexcept;

    template <typename... Args>
    void log(LogLevel level, std::string_view fmt, Args&&... args) {
        if (level > level_) {
            return;
        }
        const auto timestamp = std::chrono::system_clock::now();
        const auto secs = std::chrono::time_point_cast<std::chrono::seconds>(timestamp);
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - secs);
        std::lock_guard<std::mutex> lock(mutex_);
        (*stream_) << std::format("[{:%FT%T}.{:03}Z] [{}] ", secs, ms.count(), level_to_string(level));
        (*stream_) << format_message(fmt, std::forward<Args>(args)...);
        (*stream_) << '\n';
    }

    void set_stream(std::ostream& os);

private:
    Logger();

    static std::string_view level_to_string(LogLevel level);
    template <typename... Args>
    static std::string format_message(std::string_view fmt, Args&&... args) {
        if constexpr (sizeof...(Args) == 0) {
            return std::string{fmt};
        } else {
            std::array<std::string, sizeof...(Args)> parts{std::format("{}", std::forward<Args>(args))...};
            std::string result;
            result.reserve(fmt.size() + parts.size() * 8);
            std::size_t arg_index = 0;
            for (std::size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '{' && i + 1 < fmt.size() && fmt[i + 1] == '}' && arg_index < parts.size()) {
                    result += parts[arg_index++];
                    ++i;
                } else {
                    result.push_back(fmt[i]);
                }
            }
            for (; arg_index < parts.size(); ++arg_index) {
                result.push_back(' ');
                result += parts[arg_index];
            }
            return result;
        }
    }

    std::ostream* stream_;
    LogLevel level_{LogLevel::Error};
    std::mutex mutex_;
};

} // namespace nicels
