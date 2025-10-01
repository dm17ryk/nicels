#include "time_formatter.h"

#include <chrono>
#include <ctime>
#include <format>
#include <stdexcept>
#include <string_view>

#include "config.h"
#include "string_utils.h"

namespace nls {
namespace {

constexpr std::string_view kDefaultStrftime = "%a %b %d %H:%M:%S %Y";
constexpr std::string_view kDefaultChrono = "{:%a %b %d %H:%M:%S %Y}";

std::string normalize_style(std::string style) {
    return StringUtils::ToLower(style);
}

}  // namespace

TimeFormatter::TimeFormatter()
    : TimeFormatter(Options{}) {}

TimeFormatter::TimeFormatter(Options options)
    : format_spec_(std::string(kDefaultChrono)),
      fallback_spec_(std::string(kDefaultStrftime)) {
    std::string style = std::move(options.style);
    if (style.empty()) {
        mode_ = Mode::ChronoFormat;
        format_spec_ = "{:%Y-%m-%d %H:%M:%S%z}";
        return;
    }

    std::string normalized = normalize_style(style);
    if (normalized == "local") {
        mode_ = Mode::ChronoFormat;
        format_spec_ = "{:%Y-%m-%d %H:%M:%s}";
        return;
    }

    if (normalized == "default" || normalized == "locale") {
        use_locale_names_ = true;
        format_spec_ = "{:%Y-%m-%d %H:%M:%s}";
        return;
    }

    if (!style.empty() && style.front() == '+') {
        mode_ = Mode::Strftime;
        format_spec_ = style.substr(1);
        return;
    }

    if (normalized == "long-iso") {
        mode_ = Mode::ChronoFormat;
        format_spec_ = "{:%Y-%m-%d %H:%M}";
        return;
    }
    if (normalized == "full-iso") {
        mode_ = Mode::ChronoFormat;
        format_spec_ = "{:%Y-%m-%d %H:%M:%S%z}";
        return;
    }
    if (normalized == "iso" || normalized == "iso8601") {
        mode_ = Mode::ChronoFormat;
        format_spec_ = "{:%Y-%m-%d}";
        return;
    }

    mode_ = Mode::Strftime;
    format_spec_ = std::move(style);
}

TimeFormatter::TimeFormatter(const Config& config)
    : TimeFormatter(Options{.style = config.time_style()}) {}

std::chrono::system_clock::time_point TimeFormatter::ToSystemTime(
    const std::filesystem::file_time_type& timestamp) {
    using namespace std::chrono;
    return time_point_cast<system_clock::duration>(
        timestamp - std::filesystem::file_time_type::clock::now() + system_clock::now());
}

std::tm TimeFormatter::ToLocalTime(std::chrono::system_clock::time_point time) {
    const std::time_t time_value = std::chrono::system_clock::to_time_t(time);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_value);
#else
    localtime_r(&time_value, &tm);
#endif
    return tm;
}

std::string TimeFormatter::Format(const std::filesystem::file_time_type& timestamp) const {
    auto system_time = ToSystemTime(timestamp);
    if (mode_ == Mode::ChronoFormat) {
        std::string formatted = FormatChrono(system_time);
        if (!formatted.empty()) {
            return formatted;
        }
    }
    std::tm tm = ToLocalTime(system_time);
    return FormatStrftime(tm);
}

std::string TimeFormatter::FormatChrono(std::chrono::system_clock::time_point time) const {
    try {
        std::chrono::zoned_time local_time{std::chrono::current_zone(), time};
        if (use_locale_names_) {
            if (!locale_initialized_) {
                try {
                    locale_ = std::locale("");
                } catch (const std::exception&) {
                    locale_ = std::locale::classic();
                }
                locale_initialized_ = true;
            }
            return std::vformat(locale_, format_spec_, std::make_format_args(local_time));
        }
        return std::vformat(format_spec_, std::make_format_args(local_time));
    } catch (const std::format_error&) {
        return {};
    }
}

std::string TimeFormatter::FormatStrftime(const std::tm& time) const {
    const std::string& spec =
        (mode_ == Mode::Strftime && !format_spec_.empty()) ? format_spec_ : fallback_spec_;
    char buffer[256]{};
    if (std::strftime(buffer, sizeof(buffer), spec.c_str(), &time) == 0) {
        if (spec != fallback_spec_) {
            if (std::strftime(buffer, sizeof(buffer), fallback_spec_.c_str(), &time) == 0) {
                return {};
            }
        } else {
            return {};
        }
    }
    return std::string(buffer);
}

}  // namespace nls
