#include "nicels/app.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        nicels::App app;
        return app.run(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "nicels: " << ex.what() << '\n';
    } catch (...) {
        std::cerr << "nicels: unknown error" << '\n';
    }
    return 1;
}
