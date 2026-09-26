#include "Game/BoardLayout.hpp"
#include "Game/PastureGround.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

using namespace LiberateTheSheep::Game;

bool covers(const std::vector<GrassTile> &tiles, float x, float y) {
    return std::any_of(tiles.begin(), tiles.end(), [x, y](GrassTile tile) {
        return std::abs(x - tile.x) <= tile.side * 0.5f + 0.00001f &&
               std::abs(y - tile.y) <= tile.side * 0.5f + 0.00001f;
    });
}

bool viewportCoverageChecks() {
    for (const Cell viewport : {Cell{1920, 1080}, Cell{2520, 1080}, Cell{1080, 1920}}) {
        for (const Cell board : {Cell{4, 4}, Cell{4, 6}, Cell{12, 18}}) {
            const BoardLayout layout = calculatePastureLayout(
                viewport.x, viewport.y, board.x, board.y, 9.f,
                {1.25f, -2.5f}, {170.f, 18.f, 24.f, 18.f});
            if (!layout.valid()) {
                return false;
            }
            const float halfHeight = layout.cameraOrthoSize * 0.5f;
            const float halfWidth = halfHeight * viewport.x / viewport.y;
            const float left = layout.cameraPosition.x - halfWidth;
            const float right = layout.cameraPosition.x + halfWidth;
            const float bottom = layout.cameraPosition.y - halfHeight;
            const float top = layout.cameraPosition.y + halfHeight;
            const auto tiles = groundTiles(left, bottom, right, top, layout.cellWorldSize,
                                           layout.worldRect.x, layout.worldRect.y);
            if (tiles.empty() || tiles.size() > kMaxGrassTiles) {
                return false;
            }
            for (const GrassTile tile : tiles) {
                // Normal viewport sizes always retain six-cell grass scale.
                if (std::abs(tile.side / layout.cellWorldSize -
                             (kGrassTileCells + kGrassTileOverlapCells)) > 0.00001f) {
                    return false;
                }
            }
            for (int y = 0; y <= 40; ++y) {
                for (int x = 0; x <= 40; ++x) {
                    if (!covers(tiles, left + (right - left) * x / 40.f,
                                bottom + (top - bottom) * y / 40.f)) {
                        return false;
                    }
                }
            }
            // The center of every shared tile remains fixed when bounds grow.
            const auto expanded = groundTiles(left - 2.f, bottom - 2.f,
                                              right + 2.f, top + 2.f,
                                              layout.cellWorldSize,
                                              layout.worldRect.x, layout.worldRect.y);
            for (const GrassTile tile : tiles) {
                if (std::find(expanded.begin(), expanded.end(), tile) == expanded.end()) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool negativeCoordinatesAndSeamChecks() {
    constexpr float kCell = 0.5f;
    constexpr float kPitch = kGrassTileCells * kCell;
    constexpr float kAnchorX = 0.75f;
    constexpr float kAnchorY = -1.25f;
    const auto tiles = groundTiles(-10.f, -10.f, 10.f, 10.f, kCell, kAnchorX, kAnchorY);
    for (const GrassTile tile : tiles) {
        const int ix = static_cast<int>(std::round((tile.x - kAnchorX) / kPitch - 0.5f));
        const int iy = static_cast<int>(std::round((tile.y - kAnchorY) / kPitch - 0.5f));
        if (std::abs(tile.x - (kAnchorX + (ix + 0.5f) * kPitch)) > 0.00001f ||
            std::abs(tile.y - (kAnchorY + (iy + 0.5f) * kPitch)) > 0.00001f) {
            return false;
        }
        for (const GrassTile adjacent : tiles) {
            if (std::abs(adjacent.x - tile.x - kPitch) < 0.00001f && adjacent.y == tile.y) {
                if (tile.x + tile.side * 0.5f <= adjacent.x - adjacent.side * 0.5f) {
                    return false;
                }
            }
            if (std::abs(adjacent.y - tile.y - kPitch) < 0.00001f && adjacent.x == tile.x) {
                if (tile.y + tile.side * 0.5f <= adjacent.y - adjacent.side * 0.5f) {
                    return false;
                }
            }
        }
    }
    return !tiles.empty();
}

bool pathologicalAndInvalidChecks() {
    for (const LayoutRect bounds : {LayoutRect{-10000.f, -0.5f, 20000.f, 1.f},
                                     LayoutRect{-0.5f, -10000.f, 1.f, 20000.f},
                                     LayoutRect{-10000.f, -10000.f, 20000.f, 20000.f}}) {
        const auto tiles = groundTiles(bounds.x, bounds.y, bounds.x + bounds.width,
                                       bounds.y + bounds.height, 0.01f, 0.f, 0.f);
        if (tiles.empty() || tiles.size() > kMaxGrassTiles ||
            tiles.front().side <= (kGrassTileCells + kGrassTileOverlapCells) * 0.01f) {
            return false;
        }
        for (const GrassTile tile : tiles) {
            if (tile.side != tiles.front().side || !std::isfinite(tile.x) ||
                !std::isfinite(tile.y) || !std::isfinite(tile.side)) {
                return false;
            }
        }
        for (int y = 0; y <= 40; ++y) {
            for (int x = 0; x <= 40; ++x) {
                if (!covers(tiles, bounds.x + bounds.width * x / 40.f,
                            bounds.y + bounds.height * y / 40.f)) {
                    return false;
                }
            }
        }
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    if (!groundTiles(0.f, 0.f, 1.f, 1.f, 0.f, 0.f, 0.f).empty() ||
        !groundTiles(0.f, 0.f, 1.f, 1.f, -1.f, 0.f, 0.f).empty() ||
        !groundTiles(1.f, 0.f, 0.f, 1.f, 1.f, 0.f, 0.f).empty() ||
        !groundTiles(0.f, 1.f, 1.f, 0.f, 1.f, 0.f, 0.f).empty() ||
        !groundTiles(0.f, 0.f, 1.f, 1.f, 1.f, nan, 0.f).empty() ||
        !groundTiles(0.f, 0.f, infinity, 1.f, 1.f, 0.f, 0.f).empty() ||
        !groundTiles(nan, 0.f, 1.f, 1.f, 1.f, 0.f, 0.f).empty() ||
        !groundTiles(0.f, 0.f, 1.f, 1.f, infinity, 0.f, 0.f).empty()) {
        return false;
    }
    return true;
}

} // namespace

int main() {
    if (!viewportCoverageChecks()) {
        std::cerr << "Pasture ground viewport coverage/scale checks failed.\n";
        return 1;
    }
    if (!negativeCoordinatesAndSeamChecks()) {
        std::cerr << "Pasture ground negative-coordinate/seam checks failed.\n";
        return 1;
    }
    if (!pathologicalAndInvalidChecks()) {
        std::cerr << "Pasture ground cap/invalid-input checks failed.\n";
        return 1;
    }
    std::cout << "Pasture ground coverage, scale, anchor and pool bounds passed.\n";
    return 0;
}
