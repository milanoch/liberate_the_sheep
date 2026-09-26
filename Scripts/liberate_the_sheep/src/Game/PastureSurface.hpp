#pragma once

#include "Game/BoardLayout.hpp"
#include "Game/PastureWear.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace LiberateTheSheep::Game {

inline constexpr int kPastureSurfaceMaxDimension = 2048;
inline constexpr std::size_t kPastureSurfaceMaxPixels = 2u * 1024u * 1024u;
inline constexpr float kPastureTexturePeriodCells = 6.f;
inline constexpr float kPastureMaximumSoilBlend = 0.78f;

// One complete, opaque background. Rows follow image/PNG order (top first).
// RGB bytes are sRGB albedo; a renderer using a linear runtime texture must
// decode them before upload. No material shading or transparency is baked in.
struct PastureSurface {
    int width{};
    int height{};
    std::vector<std::uint8_t> rgba;

    [[nodiscard]] bool valid() const noexcept;
};

// Atlas: exactly two adjacent square cells, dense grass on the left and warm
// sparse grass / earth on the right. Every input alpha byte must be 255.
// Native source detail is anchored in board-cell coordinates, independent of
// viewport size. The continuous mask uses the initial flock, never live sheep.
// Called on level load or viewport/layout change, not during frame updates.
[[nodiscard]] PastureSurface composePastureSurface(
    std::span<const std::uint8_t> atlasRgba,
    int atlasWidth,
    int atlasHeight,
    const BoardLayout &layout,
    const PastureWear &initialWear,
    std::uint64_t seed
);

} // namespace LiberateTheSheep::Game
