#include "application.hpp"
#include <exception>
#include <iostream>

int main() {
    try {
        terrarium::run_game();
    } catch (const std::exception& error) {
        std::cerr << "Little World: " << error.what() << '\n';
        return 1;
    }
}
