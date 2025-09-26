#pragma once

#include <memory>

namespace nicels {

class App {
public:
    App();
    ~App();
    int run(int argc, char** argv);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nicels
