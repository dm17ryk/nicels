#pragma once

#include <chrono>
#include <filesystem>
#include <locale>
#include <string>

namespace nls {

class Config;

class TimeFormatter {
public:
    struct Options {
        std::string style;
    };

    TimeFormatter();
    explicit TimeFormatter(Options options);
    explicit TimeFormatter(const Config& config);

    std::string Format(const std::filesystem::file_time_type& timestamp) const;

private:
    enum class Mode { ChronoFormat, Strftime };

    std::string format_spec_;
    std::string fallback_spec_;
    Mode mode_ = Mode::ChronoFormat;
    bool use_locale_names_ = false;
    mutable bool locale_initialized_ = false;
    mutable std::locale locale_{};

    static std::chrono::system_clock::time_point ToSystemTime(
        const std::filesystem::file_time_type& timestamp);
    static std::tm ToLocalTime(std::chrono::system_clock::time_point time);
    std::string FormatChrono(std::chrono::system_clock::time_point time) const;
    std::string FormatStrftime(const std::tm& time) const;
};

}  // namespace nls
