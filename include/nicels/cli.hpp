#pragma once

#include <CLI/App.hpp>

#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace nicels {

class Config;

class Cli {
public:
    Cli();
    ~Cli();

    int parse(int argc, char** argv, Config& config);

    void print_markdown(std::ostream& os) const;

private:
    void configure(Config& config);

    std::unique_ptr<CLI::App> app_;
    CLI::Option* verbosity_option_{nullptr};
};

} // namespace nicels
