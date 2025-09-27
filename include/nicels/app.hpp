#pragma once

#include "nicels/cli.hpp"
#include "nicels/config.hpp"

namespace nicels {

class App {
public:
    App();
    int run(int argc, char** argv);

private:
    Cli cli_;
};

} // namespace nicels
