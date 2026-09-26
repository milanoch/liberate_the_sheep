#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace LiberateTheSheep::Game {

// Imported colour PNGs contain sRGB bytes, but Panda's runtime RGBA8 texture
// uses linear UNORM (not an sRGB format). Decode exactly once before upload.
[[nodiscard]] inline std::uint8_t pastureSrgbToLinear(std::uint8_t value) {
    static const auto table = [] {
        std::array<std::uint8_t, 256> result{};
        for (int i = 0; i < 256; ++i) {
            const double s = static_cast<double>(i) / 255.;
            const double linear = s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
            result[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(std::lround(linear * 255.));
        }
        return result;
    }();
    return table[value];
}

inline void linearizePastureForUpload(std::span<std::uint8_t> pixels) {
    for (std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
        pixels[i] = pastureSrgbToLinear(pixels[i]);
        pixels[i + 1] = pastureSrgbToLinear(pixels[i + 1]);
        pixels[i + 2] = pastureSrgbToLinear(pixels[i + 2]);
        // Coverage is not a colour channel. The composed ground is all 255.
    }
}

} // namespace LiberateTheSheep::Game
