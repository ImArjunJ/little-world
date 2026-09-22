#pragma once
#include "sengine/drawing.hpp"
#include <numbers>
namespace terrarium {
inline constexpr float pi = std::numbers::pi_v<float>;
inline sengine::drawing::color rgb(int r, int g, int b, int a = 255) {
    return {static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b),
            static_cast<unsigned char>(a)};
}
}
