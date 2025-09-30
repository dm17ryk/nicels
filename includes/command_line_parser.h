#pragma once

#include <map>
#include <string>
#include <optional>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "config.h"
namespace nls {

class CommandLineParser {
public:
    Config& Parse(int argc, char** argv);
private:
    struct SizeSpec {
        uintmax_t value = 0;
        bool show_suffix = false;
        std::string suffix;
    };
    const std::map<std::string, Config::QuotingStyle>& QuotingStyleMap() const;
    std::optional<Config::QuotingStyle> ParseQuotingStyleWord(std::string word) const;
    bool MultiplyWithOverflow(uintmax_t a, uintmax_t b, uintmax_t& result) const;
    bool PowWithOverflow(uintmax_t base, unsigned exponent, uintmax_t& result) const;
    std::optional<SizeSpec> ParseSizeSpec(const std::string& text) const;
};

} // namespace nls
