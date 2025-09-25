#include "time_formatter.h"

#include <chrono>
#include <ctime>

#include "string_utils.h"

namespace nls {

namespace {
std::time_t ToTimeT(std::filesystem::file_time_type timestamp) {
    using namespace std::chrono;
    const auto system_time = time_point_cast<system_clock::duration>(
        timestamp - std::filesystem::file_time_type::clock::now() + system_clock::now());
    return system_clock::to_time_t(system_time);
}
}

std::string TimeFormatter::Format(const std::filesystem::file_time_type& timestamp,
    const Options& options) const
{
    const std::time_t time_value = ToTimeT(timestamp);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &time_value);
#else
    localtime_r(&time_value, &tm);
#endif

    char buffer[128]{};
    std::string format = ResolveFormat(options);
    if (format.empty()) {
        format = "%a %b %d %H:%M:%S %Y";
    }
    if (std::strftime(buffer, sizeof(buffer), format.c_str(), &tm) == 0) {
        std::strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &tm);
    }
    return buffer;
}

std::string TimeFormatter::ResolveFormat(const Options& options) {
    if (options.time_style.empty() || options.time_style == "locale" || options.time_style == "default") {
        return "%a %b %d %H:%M:%S %Y";
    }

    std::string normalized = StringUtils::ToLower(options.time_style);
    if (normalized == "long-iso") {
        return "%Y-%m-%d %H:%M";
    }
    if (normalized == "full-iso") {
        return "%Y-%m-%d %H:%M:%S %z";
    }
    if (normalized == "iso" || normalized == "iso8601") {
        return "%Y-%m-%d";
    }
    if (!options.time_style.empty() && options.time_style.front() == '+') {
        return options.time_style.substr(1);
    }
    return options.time_style;
}

} // namespace nls
