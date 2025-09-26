#pragma once

#include "nicels/options.hpp"

#include <CLI/CLI.hpp>

#include <ostream>

namespace nicels {

class Cli {
public:
    Cli();

    Options parse(int argc, const char* const argv[]);
    void write_markdown(std::ostream& os) const;

private:
    CLI::App app_;

    void configure();
};

} // namespace nicels
