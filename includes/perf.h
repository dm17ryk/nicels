#pragma once

#include <chrono>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nls::perf {

class Timer;

class Manager {
public:
    static Manager& Instance();

    void set_enabled(bool enabled);
    [[nodiscard]] bool enabled() const { return enabled_; }

    void AddDuration(std::string_view label, std::chrono::steady_clock::duration duration);
    void IncrementCounter(std::string_view name, std::uint64_t delta = 1);
    void Report(std::ostream& os) const;
    void Clear();

private:
    Manager() = default;

    struct TimingData {
        std::chrono::steady_clock::duration total{};
        std::chrono::steady_clock::duration max{};
        std::uint64_t count = 0;
    };

    bool enabled_ = false;
    std::unordered_map<std::string, TimingData> timings_;
    std::unordered_map<std::string, std::uint64_t> counters_;
};

class Timer {
public:
    explicit Timer(std::string label);
    ~Timer();

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    Timer(Timer&& other) noexcept;
    Timer& operator=(Timer&& other) noexcept;

    void Stop();

private:
    std::string label_;
    std::chrono::steady_clock::time_point start_{};
    bool active_ = false;
    bool enabled_ = false;
};

}  // namespace nls::perf

