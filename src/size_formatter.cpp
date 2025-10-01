#include "size_formatter.h"

#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "config.h"

namespace nls {
SizeFormatter::SizeFormatter(Options options)
    : options_(std::move(options)) {}

SizeFormatter::SizeFormatter(const Config& config)
    : SizeFormatter(Options{
          .bytes = config.bytes(),
          .show_block_size = config.show_block_size(),
          .block_size_specified = config.block_size_specified(),
          .block_size_show_suffix = config.block_size_show_suffix(),
          .block_size = config.block_size(),
          .block_size_suffix = config.block_size_suffix(),
          .unit_system = UnitSystem::Binary,
      }) {}

std::string SizeFormatter::FormatSize(uintmax_t size) const {
    if (options_.block_size_specified) {
        uintmax_t unit = SanitizeUnit(options_.block_size);
        uintmax_t scaled = unit == 0 ? size : (size + unit - 1) / unit;
        std::string result = std::to_string(scaled);
        if (options_.block_size_show_suffix && !options_.block_size_suffix.empty()) {
            result += options_.block_size_suffix;
        }
        return result;
    }
    if (options_.bytes) {
        return std::to_string(size);
    }
    return FormatHumanReadable(size, options_.unit_system);
}

std::string SizeFormatter::FormatBlocks(uintmax_t logical_size,
                                        std::optional<uintmax_t> allocated_size) const {
    if (!ShowsBlocks()) {
        return {};
    }
    uintmax_t unit = SanitizeUnit(BlockUnit());
    uintmax_t value = allocated_size.value_or(logical_size);
    uintmax_t blocks = unit == 0 ? 0 : (value + unit - 1) / unit;
    std::string text = std::to_string(blocks);
    if (options_.block_size_specified && options_.block_size_show_suffix &&
        !options_.block_size_suffix.empty()) {
        text += options_.block_size_suffix;
    }
    return text;
}

uintmax_t SizeFormatter::BlockUnit() const {
    if (options_.block_size_specified) {
        return SanitizeUnit(options_.block_size);
    }
    return 1024;
}

bool SizeFormatter::ShowsBlocks() const {
    return options_.show_block_size;
}

std::string SizeFormatter::FormatHumanReadable(uintmax_t bytes, UnitSystem system) {
    const auto& units = system == UnitSystem::Binary ? kBinaryUnits : kDecimalUnits;
    const double base = system == UnitSystem::Binary ? 1024.0 : 1000.0;
    double value = static_cast<double>(bytes);
    size_t unit_index = 0;
    while (value >= base && unit_index + 1 < units.size()) {
        value /= base;
        ++unit_index;
    }

    int precision = (unit_index == 0 || value >= 10.0) ? 0 : 1;
    if (precision == 0) {
        return std::format("{:.0f} {}", value, units[unit_index]);
    }
    return std::format("{:.{}f} {}", value, precision, units[unit_index]);
}

}  // namespace nls
