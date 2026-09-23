#include "application.hpp"
#include <exception>
#include <iostream>

int main() {
    try {
        terrarium::application{}.run();
    } catch (const std::exception& error) {
        std::cerr << "Little World: " << error.what() << '\n';
        return 1;
    }
}
