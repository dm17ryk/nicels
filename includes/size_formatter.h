#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nls {

class Config;

class SizeFormatter {
public:
    enum class UnitSystem { Binary, Decimal };

    struct Options {
        bool bytes = false;
        bool show_block_size = false;
        bool block_size_specified = false;
        bool block_size_show_suffix = false;
        uintmax_t block_size = 0;
        std::string block_size_suffix;
        UnitSystem unit_system = UnitSystem::Binary;
    };

    SizeFormatter() = default;
    explicit SizeFormatter(Options options);
    explicit SizeFormatter(const Config& config);

    std::string FormatSize(uintmax_t size) const;
    std::string FormatBlocks(uintmax_t logical_size,
                             std::optional<uintmax_t> allocated_size) const;
    uintmax_t BlockUnit() const;
    bool ShowsBlocks() const;

    static std::string FormatHumanReadable(
        uintmax_t bytes,
        UnitSystem system = UnitSystem::Binary);

private:
    Options options_{};
};

}  // namespace nls
