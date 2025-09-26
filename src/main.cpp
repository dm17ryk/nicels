#include <exception>
#include <iostream>

#include "nicels/app.h"
#include "nicels/logger.h"

int main(int argc, char** argv) {
    try {
        nicels::Logger::instance().set_level(nicels::Logger::Level::Error);
        nicels::App app;
        return app.run(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "nicels: " << ex.what() << '\n';
        return 1;
    }
}
