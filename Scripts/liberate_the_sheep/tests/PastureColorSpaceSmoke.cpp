#include "Game/PastureColorSpace.hpp"
#include <array>
#include <iostream>

int main() {
    using namespace LiberateTheSheep::Game;
    if (pastureSrgbToLinear(0) != 0 || pastureSrgbToLinear(255) != 255 ||
        pastureSrgbToLinear(128) != 55 || pastureSrgbToLinear(188) != 128) {
        return 1;
    }
    for (int i = 1; i < 256; ++i) {
        if (pastureSrgbToLinear(static_cast<std::uint8_t>(i)) <
            pastureSrgbToLinear(static_cast<std::uint8_t>(i - 1))) { return 1; }
    }
    std::array<std::uint8_t, 8> pixels{128, 188, 255, 255, 0, 10, 11, 255};
    linearizePastureForUpload(pixels);
    if (pixels != std::array<std::uint8_t, 8>{55, 128, 255, 255, 0, 1, 1, 255}) { return 1; }
    std::cout << "Pasture sRGB-to-linear upload and opaque alpha passed.\n";
}
