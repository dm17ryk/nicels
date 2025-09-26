#pragma once

#include <chrono>
#include <format>
#include <mutex>
#include <ostream>
#include <string_view>
#include <tuple>
#include <utility>

namespace nicels {

class Logger {
public:
    enum class Level {
        Error = 0,
        Warning,
        Info,
        Debug,
        Trace
    };

    static Logger& instance();

    void set_level(Level level) noexcept;
    Level level() const noexcept;

    template <typename... Args>
    void log(Level level, std::string_view fmt, Args&&... args) {
        if (level > level_) {
            return;
        }
        if constexpr (sizeof...(Args) == 0) {
            write(level, std::string{fmt});
        } else {
            auto tuple_args = std::make_tuple(std::forward<Args>(args)...);
            auto formatted = std::apply(
                [&](auto&... unpacked) {
                    return std::vformat(fmt, std::make_format_args(unpacked...));
                },
                tuple_args);
            write(level, formatted);
        }
    }

    template <typename... Args>
    void debug(std::string_view fmt, Args&&... args) {
        log(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void info(std::string_view fmt, Args&&... args) {
        log(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void warn(std::string_view fmt, Args&&... args) {
        log(Level::Warning, fmt, std::forward<Args>(args)...);
    }

    template <typename... Args>
    void error(std::string_view fmt, Args&&... args) {
        log(Level::Error, fmt, std::forward<Args>(args)...);
    }

    void set_output(std::ostream* stream) noexcept;

private:
    Logger();
    void write(Level level, std::string_view message);

    std::ostream* stream_;
    Level level_;
    std::mutex mutex_;
};

} // namespace nicels
