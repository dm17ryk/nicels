#pragma once

#include "nicels/config.hpp"

#include <map>
#include <optional>
#include <string>

namespace nicels {

class Cli {
public:
    Cli();
    Config parse(int argc, char** argv) const;

private:
    struct SizeSpec {
        std::uintmax_t value { 0 };
        bool show_suffix { false };
        std::string suffix;
    };

    [[nodiscard]] static std::map<std::string, Config::QuotingStyle> quoting_style_map();
    [[nodiscard]] static std::optional<Config::QuotingStyle> parse_quoting_style(std::string word);
    [[nodiscard]] static bool multiply_with_overflow(std::uintmax_t a, std::uintmax_t b, std::uintmax_t& result);
    [[nodiscard]] static bool pow_with_overflow(std::uintmax_t base, unsigned exponent, std::uintmax_t& result);
    [[nodiscard]] static std::optional<SizeSpec> parse_size_spec(const std::string& text);
};

} // namespace nicels
