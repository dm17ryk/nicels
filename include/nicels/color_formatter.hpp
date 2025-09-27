#pragma once

#include <CLI/CLI.hpp>

namespace nicels {

class ColorFormatter final : public CLI::Formatter {
public:
    ColorFormatter() = default;
    std::string make_usage(const CLI::App* app, std::string name) const override;
};

} // namespace nicels
