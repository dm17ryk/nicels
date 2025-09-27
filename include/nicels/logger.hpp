#pragma once

#include <atomic>
#include <chrono>
#include <format>
#include <mutex>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace nicels {

class Logger {
public:
    enum class Level {
        Error = 0,
        Warn,
        Info,
        Debug,
        Trace,
    };

    static Logger& instance();

    void set_level(Level level) noexcept;
    [[nodiscard]] Level level() const noexcept;

    void set_output_stream(std::ostream* stream);

    template <typename... Args>
    void log(Level level, std::string_view fmt, Args&&... args) {
        if (level > level_.load(std::memory_order_relaxed)) {
            return;
        }

        auto formatted = std::vformat(fmt, std::make_format_args(std::forward<Args>(args)...));
        std::scoped_lock lock(mutex_);
        if (!stream_) {
            return;
        }
        *stream_ << '[' << to_string(level) << "] " << formatted << '\n';
    }

private:
    Logger() = default;

    [[nodiscard]] static constexpr std::string_view to_string(Level level) noexcept {
        switch (level) {
        case Level::Error: return "ERROR";
        case Level::Warn: return "WARN";
        case Level::Info: return "INFO";
        case Level::Debug: return "DEBUG";
        case Level::Trace: return "TRACE";
        }
        return "INFO";
    }

    std::atomic<Level> level_ { Level::Error };
    std::ostream* stream_ { nullptr };
    std::mutex mutex_;
};

} // namespace nicels
