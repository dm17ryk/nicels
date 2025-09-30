#include "perf.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <ios>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nls::perf {

namespace {

double DurationToMilliseconds(std::chrono::steady_clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

}  // namespace

Manager& Manager::Instance()
{
    static Manager instance;
    return instance;
}

void Manager::set_enabled(bool enabled)
{
    enabled_ = enabled;
    Clear();
}

void Manager::AddDuration(std::string_view label, std::chrono::steady_clock::duration duration)
{
    if (!enabled_) return;
    auto& entry = timings_[std::string(label)];
    entry.total += duration;
    entry.count += 1;
    if (duration > entry.max) {
        entry.max = duration;
    }
}

void Manager::IncrementCounter(std::string_view name, std::uint64_t delta)
{
    if (!enabled_) return;
    counters_[std::string(name)] += delta;
}

void Manager::Report(std::ostream& os) const
{
    if (!enabled_) return;
    if (timings_.empty() && counters_.empty()) return;

    if (!timings_.empty()) {
        os << "[perf] Timings (ms)\n";
        std::vector<std::pair<std::string, TimingData>> timings;
        timings.reserve(timings_.size());
        for (const auto& [label, data] : timings_) {
            timings.emplace_back(label, data);
        }
        std::sort(timings.begin(), timings.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        const auto previous_flags = os.flags();
        const auto previous_precision = os.precision();
        for (const auto& [label, data] : timings) {
            const double total = DurationToMilliseconds(data.total);
            const double avg = data.count > 0 ? total / static_cast<double>(data.count) : 0.0;
            const double max = DurationToMilliseconds(data.max);
            os.setf(std::ios::fixed, std::ios::floatfield);
            os << "  " << label << ": total=" << std::setprecision(3) << total
               << " avg=" << avg << " max=" << max << " count=" << data.count << '\n';
        }
        os.flags(previous_flags);
        os.precision(previous_precision);
    }

    if (!counters_.empty()) {
        os << "[perf] Counters\n";
        std::vector<std::pair<std::string, std::uint64_t>> counters;
        counters.reserve(counters_.size());
        for (const auto& [name, value] : counters_) {
            counters.emplace_back(name, value);
        }
        std::sort(counters.begin(), counters.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& [name, value] : counters) {
            os << "  " << name << ": " << value << '\n';
        }
    }
}

void Manager::Clear()
{
    timings_.clear();
    counters_.clear();
}

Timer::Timer(std::string label)
    : enabled_(Manager::Instance().enabled())
{
    if (!enabled_) return;
    label_ = std::move(label);
    start_ = std::chrono::steady_clock::now();
    active_ = true;
}

Timer::~Timer()
{
    Stop();
}

Timer::Timer(Timer&& other) noexcept
{
    *this = std::move(other);
}

Timer& Timer::operator=(Timer&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    Stop();

    label_ = std::move(other.label_);
    start_ = other.start_;
    active_ = other.active_;
    enabled_ = other.enabled_;

    other.active_ = false;
    other.enabled_ = false;

    return *this;
}

void Timer::Stop()
{
    if (!enabled_ || !active_) {
        return;
    }
    active_ = false;
    auto end = std::chrono::steady_clock::now();
    Manager::Instance().AddDuration(label_, end - start_);
}

}  // namespace nls::perf

