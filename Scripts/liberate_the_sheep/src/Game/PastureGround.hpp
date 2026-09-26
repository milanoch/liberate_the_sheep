#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace LiberateTheSheep::Game {

inline constexpr float kGrassTileCells = 6.f;
inline constexpr float kGrassTileOverlapCells = 0.002f;
inline constexpr std::size_t kMaxGrassTiles = 384;

struct GrassTile {
    // Sprite center and full side length, in world units. side includes the
    // tiny filtering overlap; the lattice pitch does not include that overlap.
    float x{};
    float y{};
    float side{};

    friend constexpr bool operator==(const GrassTile &, const GrassTile &) = default;
};

// A board-anchored texture lattice, not a texture stretched to the viewport.
// Repeat the uniformly lit texture without mirroring: reflection creates
// artificial diamond/kaleidoscope patterns in photographic grass.
// Resizing only adds/removes tiles as long as cellSize and anchor stay fixed.
[[nodiscard]] inline std::vector<GrassTile> groundTiles(
    float left, float bottom, float right, float top,
    float cellSize, float anchorX, float anchorY
) {
    for (const float value : {left, bottom, right, top, cellSize, anchorX, anchorY}) {
        if (!std::isfinite(value)) {
            return {};
        }
    }
    if (left >= right || bottom >= top || cellSize <= 0.f) {
        return {};
    }

    double pitch = static_cast<double>(cellSize) * kGrassTileCells;
    double firstX{};
    double firstY{};
    double lastX{};
    double lastY{};
    std::size_t columns{};
    std::size_t rows{};
    // Even the ratio of largest to smallest positive finite floats needs fewer
    // than 300 doublings. The limit also guarantees no unbounded input loop.
    for (int attempt = 0; attempt < 512; ++attempt) {
        firstX = std::floor((static_cast<double>(left) - anchorX) / pitch);
        firstY = std::floor((static_cast<double>(bottom) - anchorY) / pitch);
        lastX = std::floor((static_cast<double>(right) - anchorX) / pitch);
        lastY = std::floor((static_cast<double>(top) - anchorY) / pitch);
        // Include the tile starting exactly on the far viewport boundary. This
        // harmless extra tile avoids boundary omissions from rounding.
        const double columnCount = lastX - firstX + 1.;
        const double rowCount = lastY - firstY + 1.;
        // Preserve reliable integer indices and center coordinates for extreme
        // anchor/cell ratios. Ordinary game layouts are far below this limit.
        constexpr double kReliableIndex = 1099511627776.; // 2^40
        const bool reliableIndices = std::max({std::abs(firstX), std::abs(firstY),
                                                std::abs(lastX), std::abs(lastY)}) <=
                                     kReliableIndex;
        if (reliableIndices && columnCount >= 1. && rowCount >= 1. &&
            columnCount <= static_cast<double>(kMaxGrassTiles) &&
            rowCount <= static_cast<double>(kMaxGrassTiles) &&
            columnCount * rowCount <= static_cast<double>(kMaxGrassTiles)) {
            columns = static_cast<std::size_t>(columnCount);
            rows = static_cast<std::size_t>(rowCount);
            break;
        }
        // Very wide/narrow pathological viewports still have complete coverage
        // within the authored pool. All tiles grow uniformly; no clipped strip.
        pitch *= 2.;
    }
    const double side = pitch + static_cast<double>(cellSize) * kGrassTileOverlapCells;
    constexpr double kMaximumFloat = std::numeric_limits<float>::max();
    if (columns == 0 || rows == 0 || !std::isfinite(side) || side > kMaximumFloat) {
        return {};
    }

    std::vector<GrassTile> tiles;
    tiles.reserve(columns * rows);
    for (std::size_t row = 0; row < rows; ++row) {
        const double iy = firstY + static_cast<double>(row);
        const double y = static_cast<double>(anchorY) + (iy + 0.5) * pitch;
        for (std::size_t column = 0; column < columns; ++column) {
            const double ix = firstX + static_cast<double>(column);
            const double x = static_cast<double>(anchorX) + (ix + 0.5) * pitch;
            if (!std::isfinite(x) || !std::isfinite(y) ||
                std::abs(x) > kMaximumFloat || std::abs(y) > kMaximumFloat) {
                return {};
            }
            tiles.push_back({static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(side)});
        }
    }
    return tiles;
}

} // namespace LiberateTheSheep::Game
