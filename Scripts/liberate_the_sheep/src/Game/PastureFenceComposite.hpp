#pragma once

#include "Game/PastureSurface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <span>

namespace LiberateTheSheep::Game {

struct PastureFencePanel {
    float x{};
    float y{};
    float width{};
    float height{};
    float rotationDegrees{};
    float tint{1.f};
};

// Both images must contain linear RGB bytes, with straight source alpha and an
// opaque destination. Rows are top-first; positive rotation is counterclockwise
// in world space. The source and destination must not overlap in memory.
// Validate the entire batch before writing, so false leaves the ground intact.
// Fully offscreen panels are successful no-ops. This bakes the fence behind all
// moving Sprites; their routes must keep clear of the timber and gate posts.
[[nodiscard]] inline bool compositePastureFence(
    PastureSurface &surface,
    std::span<const std::uint8_t> fenceLinearRgba,
    int fenceWidth,
    int fenceHeight,
    const BoardLayout &layout,
    std::span<const PastureFencePanel> panels
) {
    constexpr std::uint64_t kMaxFencePixels = 8u * 1024u * 1024u;
    if (!surface.valid() || !layout.valid() || fenceWidth <= 0 || fenceHeight <= 0 ||
        layout.viewportWidth <= 0 || layout.viewportHeight <= 0 ||
        !std::isfinite(layout.cameraOrthoSize) || layout.cameraOrthoSize <= 0.f ||
        !std::isfinite(layout.cameraPosition.x) || !std::isfinite(layout.cameraPosition.y)) {
        return false;
    }
    const auto sourcePixels = static_cast<std::uint64_t>(fenceWidth) * fenceHeight;
    if (sourcePixels > kMaxFencePixels || fenceLinearRgba.size() != sourcePixels * 4) {
        return false;
    }
    const std::less<const std::uint8_t *> before;
    if (before(fenceLinearRgba.data(), surface.rgba.data() + surface.rgba.size()) &&
        before(surface.rgba.data(), fenceLinearRgba.data() + fenceLinearRgba.size())) {
        return false;
    }
    for (const auto &panel : panels) {
        if (!std::isfinite(panel.x) || !std::isfinite(panel.y) ||
            !std::isfinite(panel.width) || panel.width <= 0.f ||
            !std::isfinite(panel.height) || panel.height <= 0.f ||
            !std::isfinite(panel.rotationDegrees) ||
            !std::isfinite(panel.tint) || panel.tint < 0.f) {
            return false;
        }
    }

    const double viewHeight = layout.cameraOrthoSize;
    const double viewWidth = viewHeight * layout.viewportWidth / layout.viewportHeight;
    const double worldLeft = layout.cameraPosition.x - viewWidth * 0.5;
    const double worldTop = layout.cameraPosition.y + viewHeight * 0.5;
    const double pixelWidth = viewWidth / surface.width;
    const double pixelHeight = viewHeight / surface.height;
    constexpr double kRadiansPerDegree = 0.01745329251994329577;
    constexpr std::array<double, 2> kSampleOffsets{0.25, 0.75};

    for (const auto &panel : panels) {
        const double angle = panel.rotationDegrees * kRadiansPerDegree;
        const double cosine = std::cos(angle);
        const double sine = std::sin(angle);
        const double halfWidth = panel.width * 0.5;
        const double halfHeight = panel.height * 0.5;
        const double extentX = std::abs(cosine) * halfWidth + std::abs(sine) * halfHeight;
        const double extentY = std::abs(sine) * halfWidth + std::abs(cosine) * halfHeight;
        // Clamp in floating point before converting possibly offscreen world
        // coordinates to integers. Only the rotated quad's AABB is visited.
        const int left = static_cast<int>(std::clamp(std::floor(
            (panel.x - extentX - worldLeft) / pixelWidth), 0.0,
            static_cast<double>(surface.width)));
        const int right = static_cast<int>(std::clamp(std::ceil(
            (panel.x + extentX - worldLeft) / pixelWidth), 0.0,
            static_cast<double>(surface.width)));
        const int top = static_cast<int>(std::clamp(std::floor(
            (worldTop - panel.y - extentY) / pixelHeight), 0.0,
            static_cast<double>(surface.height)));
        const int bottom = static_cast<int>(std::clamp(std::ceil(
            (worldTop - panel.y + extentY) / pixelHeight), 0.0,
            static_cast<double>(surface.height)));

        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                // Average four coverage samples. RGB is premultiplied before
                // filtering, so padded transparent RGB cannot tint the edge.
                std::array<double, 3> premultiplied{};
                double coverage = 0.0;
                for (const double offsetY : kSampleOffsets) {
                    const double dy = worldTop - (y + offsetY) * pixelHeight - panel.y;
                    for (const double offsetX : kSampleOffsets) {
                        const double dx = worldLeft + (x + offsetX) * pixelWidth - panel.x;
                        const double localX = cosine * dx + sine * dy;
                        const double localY = -sine * dx + cosine * dy;
                        if (std::abs(localX) > halfWidth || std::abs(localY) > halfHeight) {
                            continue;
                        }
                        const double u = localX / panel.width + 0.5;
                        const double v = 0.5 - localY / panel.height;
                        const double sourceX = std::clamp(u * fenceWidth - 0.5,
                            0.0, static_cast<double>(fenceWidth - 1));
                        const double sourceY = std::clamp(v * fenceHeight - 0.5,
                            0.0, static_cast<double>(fenceHeight - 1));
                        const int ix = static_cast<int>(sourceX);
                        const int iy = static_cast<int>(sourceY);
                        const int nx = std::min(ix + 1, fenceWidth - 1);
                        const int ny = std::min(iy + 1, fenceHeight - 1);
                        const double fx = sourceX - ix;
                        const double fy = sourceY - iy;
                        const std::array<int, 4> texelX{ix, nx, ix, nx};
                        const std::array<int, 4> texelY{iy, iy, ny, ny};
                        const std::array<double, 4> weights{
                            (1.0 - fx) * (1.0 - fy), fx * (1.0 - fy),
                            (1.0 - fx) * fy, fx * fy
                        };
                        for (std::size_t sample = 0; sample < weights.size(); ++sample) {
                            const auto source = (static_cast<std::size_t>(texelY[sample]) *
                                fenceWidth + texelX[sample]) * 4;
                            const double alphaWeight = weights[sample] *
                                fenceLinearRgba[source + 3] / 255.0 * 0.25;
                            coverage += alphaWeight;
                            for (std::size_t channel = 0; channel < 3; ++channel) {
                                premultiplied[channel] +=
                                    fenceLinearRgba[source + channel] * alphaWeight;
                            }
                        }
                    }
                }
                if (coverage <= 0.0) { continue; }
                coverage = std::clamp(coverage, 0.0, 1.0);
                const auto destination = (static_cast<std::size_t>(y) * surface.width + x) * 4;
                for (std::size_t channel = 0; channel < 3; ++channel) {
                    const double color = premultiplied[channel] * panel.tint +
                        surface.rgba[destination + channel] * (1.0 - coverage);
                    surface.rgba[destination + channel] = static_cast<std::uint8_t>(
                        std::lround(std::clamp(color, 0.0, 255.0)));
                }
                surface.rgba[destination + 3] = 255;
            }
        }
    }
    return true;
}

} // namespace LiberateTheSheep::Game
