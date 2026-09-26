#include "Game/PastureSurface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace LiberateTheSheep::Game {
namespace {

constexpr std::size_t kMaxSourcePixels = 8u * 1024u * 1024u;
constexpr int kDensityGridMaxSide = 257;
constexpr double kPi = 3.14159265358979323846;

double smooth(double value) noexcept {
    value = std::clamp(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

double mix(double a, double b, double amount) noexcept {
    return a + (b - a) * amount;
}

std::uint64_t hash(std::uint64_t value) noexcept {
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31u);
}

double noiseCorner(std::int64_t x, std::int64_t y, std::uint64_t seed) noexcept {
    const auto value = hash(seed ^ hash(static_cast<std::uint64_t>(x)) ^
                            hash(static_cast<std::uint64_t>(y) + 0x9e3779b97f4a7c15ULL));
    return static_cast<double>(value >> 11u) * (1.0 / 9007199254740992.0);
}

// This noise only breaks up the wear mask at a broad, four-cell scale. All
// visible grass blades and soil detail come from the authored source image.
double broadNoise(double x, double y, std::uint64_t seed) noexcept {
    x *= 0.25;
    y *= 0.25;
    const double floorX = std::floor(x);
    const double floorY = std::floor(y);
    const auto ix = static_cast<std::int64_t>(floorX);
    const auto iy = static_cast<std::int64_t>(floorY);
    const double fx = smooth(x - floorX);
    const double fy = smooth(y - floorY);
    return mix(mix(noiseCorner(ix, iy, seed), noiseCorner(ix + 1, iy, seed), fx),
               mix(noiseCorner(ix, iy + 1, seed), noiseCorner(ix + 1, iy + 1, seed), fx), fy);
}

struct DensityGrid {
    double origin{-3.0};
    double step{0.25};
    int width{};
    int height{};
    std::vector<float> values;

    DensityGrid(const BoardLayout &layout, const PastureWear &wear) {
        const double extent = std::max(layout.columns, layout.rows) + 6.0;
        step = std::max(step, extent / (kDensityGridMaxSide - 1));
        width = std::min(kDensityGridMaxSide,
                         static_cast<int>(std::ceil((layout.columns + 6.0) / step)) + 1);
        height = std::min(kDensityGridMaxSide,
                          static_cast<int>(std::ceil((layout.rows + 6.0) / step)) + 1);
        values.resize(static_cast<std::size_t>(width) * height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                values[static_cast<std::size_t>(y) * width + x] = wear.densityAt(
                    static_cast<float>(origin + x * step),
                    static_cast<float>(origin + y * step));
            }
        }
    }

    [[nodiscard]] double at(double x, double y) const noexcept {
        const double gx = (x - origin) / step;
        const double gy = (y - origin) / step;
        if (gx < 0.0 || gy < 0.0 || gx >= width - 1 || gy >= height - 1) {
            return 0.0;
        }
        const int ix = static_cast<int>(gx);
        const int iy = static_cast<int>(gy);
        const auto row = static_cast<std::size_t>(iy) * width + ix;
        return mix(mix(values[row], values[row + 1], gx - ix),
                   mix(values[row + width], values[row + width + 1], gx - ix), gy - iy);
    }
};

struct Color {
    std::array<double, 3> channels{};
};

struct SourceAtlas {
    std::span<const std::uint8_t> bytes;
    int width{};
    int tileSide{};

    [[nodiscard]] Color sample(int cell, double u, double v) const noexcept {
        const double px = u * (tileSide - 1);
        const double py = v * (tileSide - 1);
        const int x = static_cast<int>(px);
        const int y = static_cast<int>(py);
        const int nx = std::min(x + 1, tileSide - 1);
        const int ny = std::min(y + 1, tileSide - 1);
        const auto offset = [this, cell](int xx, int yy) {
            return (static_cast<std::size_t>(yy) * width + cell * tileSide + xx) * 4;
        };
        const std::array<std::size_t, 4> p{offset(x, y), offset(nx, y),
                                           offset(x, ny), offset(nx, ny)};
        Color color;
        for (std::size_t channel = 0; channel < color.channels.size(); ++channel) {
            color.channels[channel] = mix(
                mix(bytes[p[0] + channel], bytes[p[1] + channel], px - x),
                mix(bytes[p[2] + channel], bytes[p[3] + channel], px - x), py - y);
        }
        return color;
    }

    // Two periodically shifted samples per axis. Each sample's weight reaches
    // zero with zero derivative at its own edge, so even nonmatching source
    // edges cannot create a line. No mirroring, Sprite tiles or decal borders.
    [[nodiscard]] Color periodic(int cell, double x, double y) const noexcept {
        const double rawU = x / kPastureTexturePeriodCells;
        const double rawV = -y / kPastureTexturePeriodCells;
        const double u = rawU - std::floor(rawU);
        const double v = rawV - std::floor(rawV);
        const double shiftedU = u < 0.5 ? u + 0.5 : u - 0.5;
        const double shiftedV = v < 0.5 ? v + 0.5 : v - 0.5;
        const double sineU = std::sin(kPi * u);
        const double sineV = std::sin(kPi * v);
        const double wu = sineU * sineU;
        const double wv = sineV * sineV;
        const std::array<Color, 4> samples{sample(cell, u, v), sample(cell, shiftedU, v),
                                           sample(cell, u, shiftedV), sample(cell, shiftedU, shiftedV)};
        Color color;
        for (std::size_t channel = 0; channel < color.channels.size(); ++channel) {
            color.channels[channel] = mix(
                mix(samples[3].channels[channel], samples[2].channels[channel], wu),
                mix(samples[1].channels[channel], samples[0].channels[channel], wu), wv);
        }
        return color;
    }
};

bool suitableLayout(const BoardLayout &layout) noexcept {
    return layout.valid() && layout.columns > 0 && layout.rows > 0 &&
           layout.viewportWidth > 0 && layout.viewportHeight > 0 &&
           std::isfinite(layout.cameraOrthoSize) && layout.cameraOrthoSize > 0.f &&
           std::isfinite(layout.cellWorldSize) && layout.cellWorldSize > 0.f &&
           std::isfinite(layout.cameraPosition.x) && std::isfinite(layout.cameraPosition.y) &&
           std::isfinite(layout.worldRect.x) && std::isfinite(layout.worldRect.y);
}

} // namespace

bool PastureSurface::valid() const noexcept {
    return width > 0 && height > 0 && width <= kPastureSurfaceMaxDimension &&
           height <= kPastureSurfaceMaxDimension &&
           static_cast<std::size_t>(width) * height <= kPastureSurfaceMaxPixels &&
           rgba.size() == static_cast<std::size_t>(width) * height * 4;
}

PastureSurface composePastureSurface(std::span<const std::uint8_t> atlasRgba,
                                     int atlasWidth, int atlasHeight,
                                     const BoardLayout &layout,
                                     const PastureWear &initialWear,
                                     std::uint64_t seed) {
    PastureSurface output;
    if (!suitableLayout(layout) || atlasWidth < 4 || atlasHeight < 2 ||
        atlasWidth / 2 != atlasHeight || atlasWidth % 2 != 0) {
        return output;
    }
    const auto sourcePixels = static_cast<std::uint64_t>(atlasWidth) * atlasHeight;
    if (sourcePixels > kMaxSourcePixels || atlasRgba.size() != sourcePixels * 4) {
        return output;
    }
    for (std::size_t alpha = 3; alpha < atlasRgba.size(); alpha += 4) {
        if (atlasRgba[alpha] != 255) {
            return output;
        }
    }

    const double viewHeight = layout.cameraOrthoSize;
    const double viewWidth = viewHeight * layout.viewportWidth / layout.viewportHeight;
    const double cellsWide = viewWidth / layout.cellWorldSize;
    const double cellsHigh = viewHeight / layout.cellWorldSize;
    const double cellLeft = (layout.cameraPosition.x - viewWidth * 0.5 - layout.worldRect.x) /
                            layout.cellWorldSize;
    const double cellTop = (layout.cameraPosition.y + viewHeight * 0.5 - layout.worldRect.y) /
                           layout.cellWorldSize;
    // Guard unreasonable transforms before converting a noise-grid coordinate
    // to an integer. Ordinary wide/portrait views are far below this limit.
    constexpr double kMaxCellCoordinate = 1.0e9;
    if (!std::isfinite(cellsWide) || !std::isfinite(cellsHigh) ||
        !std::isfinite(cellLeft) || !std::isfinite(cellTop) ||
        std::abs(cellLeft) + cellsWide > kMaxCellCoordinate ||
        std::abs(cellTop) + cellsHigh > kMaxCellCoordinate) {
        return output;
    }
    const double pixelCount = static_cast<double>(layout.viewportWidth) * layout.viewportHeight;
    const double scale = std::min({1.0,
        static_cast<double>(kPastureSurfaceMaxDimension) / layout.viewportWidth,
        static_cast<double>(kPastureSurfaceMaxDimension) / layout.viewportHeight,
        std::sqrt(static_cast<double>(kPastureSurfaceMaxPixels) / pixelCount)});
    output.width = std::max(1, static_cast<int>(layout.viewportWidth * scale));
    output.height = std::max(1, static_cast<int>(layout.viewportHeight * scale));
    output.rgba.resize(static_cast<std::size_t>(output.width) * output.height * 4);

    const DensityGrid density(layout, initialWear);
    const SourceAtlas source{atlasRgba, atlasWidth, atlasHeight};
    for (int y = 0; y < output.height; ++y) {
        const double cellY = cellTop - (y + 0.5) * cellsHigh / output.height;
        for (int x = 0; x < output.width; ++x) {
            const double cellX = cellLeft + (x + 0.5) * cellsWide / output.width;
            const double localDensity = density.at(cellX, cellY);
            const double variation = localDensity > 0.0 ?
                0.90 + 0.20 * broadNoise(cellX, cellY, seed) : 1.0;
            const double soilAmount = kPastureMaximumSoilBlend *
                smooth((localDensity * variation - 0.15) / (0.85 - 0.15));
            const Color grass = source.periodic(0, cellX, cellY);
            const Color earth = soilAmount > 0.0 ? source.periodic(1, cellX, cellY) : grass;
            const auto pixel = (static_cast<std::size_t>(y) * output.width + x) * 4;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                output.rgba[pixel + channel] = static_cast<std::uint8_t>(std::lround(std::clamp(
                    mix(grass.channels[channel], earth.channels[channel], soilAmount), 0.0, 255.0)));
            }
            output.rgba[pixel + 3] = 255;
        }
    }
    return output;
}

} // namespace LiberateTheSheep::Game
