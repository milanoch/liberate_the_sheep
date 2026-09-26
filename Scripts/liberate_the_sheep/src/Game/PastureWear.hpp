#pragma once

#include "Game/BoardModel.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace LiberateTheSheep::Game {

// All lengths are in board cells; (0, 0) is the board's bottom-left corner.
// The renderer converts these to world space using the current board layout.
struct GrassWearPatch {
    float x{};
    float y{};
    float diameter{};
    float opacity{};
    float rotationDegrees{};
    int variant{};

    friend constexpr bool operator==(const GrassWearPatch &, const GrassWearPatch &) = default;
};

// An initial-flock snapshot, not a live occupancy visualization. Worn ground
// stays worn as sheep leave; only loading/restarting a level captures it again.
class PastureWear {
public:
    static constexpr std::size_t kMaxPatchCount = 96;
    static constexpr float kDensityRadius = 2.4f;

    void capture(const BoardModel &board, std::uint64_t seed) {
        occupiedCells_.clear();
        patches_.clear();
        occupiedCells_.reserve(static_cast<std::size_t>(board.occupiedCellCount()));
        // Copy in spatial order, independent of sheep insertion/removal order.
        for (int y = 0; y < board.height(); ++y) {
            for (int x = 0; x < board.width(); ++x) {
                if (board.isOccupied({x, y})) {
                    occupiedCells_.push_back({x, y});
                }
            }
        }
        if (occupiedCells_.empty()) {
            return;
        }

        Random random(seed);
        struct Site { float x; float y; };
        std::vector<Site> sites;
        // Random, separated sites avoid a visible board-cell/checker pattern.
        // Generate sites independently of occupancy: adding sheep can only
        // increase wear at an existing site, never shuffle the ground beneath it.
        const auto siteCount = std::min(kMaxPatchCount, static_cast<std::size_t>(
            std::ceil(static_cast<double>(board.cellCount()) / 3.8)));
        sites.reserve(siteCount);
        patches_.reserve(siteCount);
        constexpr float kSiteSeparationSquared = 1.2f * 1.2f;
        for (std::size_t attempt = 0;
             attempt < siteCount * 40 && sites.size() < siteCount; ++attempt) {
            const float x = 0.2f + random.unit() * (static_cast<float>(board.width()) - 0.4f);
            const float y = 0.2f + random.unit() * (static_cast<float>(board.height()) - 0.4f);
            const bool overlaps = std::any_of(sites.begin(), sites.end(), [x, y](Site site) {
                const float dx = x - site.x;
                const float dy = y - site.y;
                return dx * dx + dy * dy < kSiteSeparationSquared;
            });
            if (overlaps) {
                continue;
            }
            sites.push_back({x, y});
            // Consume the same random samples even at sites with no wear.
            const float sizeVariation = random.unit();
            const float strengthVariation = random.unit();
            const float rotation = random.unit() * 360.f;
            const int variant = static_cast<int>(random.next() % 3u);
            const float density = densityAt(x, y);
            constexpr float kWearThreshold = 0.34f;
            if (density <= kWearThreshold) {
                continue;
            }
            const float t = std::clamp((density - kWearThreshold) / (0.90f - kWearThreshold),
                                       0.f, 1.f);
            const float wear = t * t * (3.f - 2.f * t);
            patches_.push_back({
                x, y,
                2.f + 1.2f * density + 0.65f * sizeVariation,
                0.15f + 0.72f * wear * (0.85f + 0.15f * strengthVariation),
                rotation, variant,
            });
        }
    }

    [[nodiscard]] const std::vector<GrassWearPatch> &patches() const noexcept {
        return patches_;
    }

    // Smooth local occupancy in [0, 1], not the board's global sheep count.
    // A compact C1 kernel avoids a sharply bounded dirt circle around each sheep.
    [[nodiscard]] float densityAt(float x, float y) const noexcept {
        if (occupiedCells_.empty() || !std::isfinite(x) || !std::isfinite(y)) {
            return 0.f;
        }
        float occupiedWeight = 0.f;
        for (const Cell cell : occupiedCells_) {
            occupiedWeight += weight(static_cast<float>(cell.x) + 0.5f - x,
                                     static_cast<float>(cell.y) + 0.5f - y);
        }
        if (occupiedWeight == 0.f) {
            return 0.f;
        }
        // Normalize against a full, unbounded lattice rather than clipping the
        // denominator at the board edge. One isolated corner sheep must not
        // appear as crowded as a whole flock simply because it is at an edge.
        // Fractional coordinates keep the denominator independent of board size
        // and avoid float-to-int overflow for arbitrary finite queries.
        const float fractionX = x - std::floor(x);
        const float fractionY = y - std::floor(y);
        float fullWeight = 0.f;
        for (int iy = -3; iy <= 3; ++iy) {
            for (int ix = -3; ix <= 3; ++ix) {
                fullWeight += weight(static_cast<float>(ix) + 0.5f - fractionX,
                                     static_cast<float>(iy) + 0.5f - fractionY);
            }
        }
        return std::clamp(occupiedWeight / fullWeight, 0.f, 1.f);
    }

private:
    struct Random {
        explicit Random(std::uint64_t seed) : state(seed) {}

        std::uint64_t next() noexcept {
            // SplitMix64: deterministic across platforms, including seed zero.
            std::uint64_t value = (state += 0x9e3779b97f4a7c15ULL);
            value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31u);
        }

        float unit() noexcept {
            return static_cast<float>(next() >> 40u) * (1.f / 16777216.f);
        }

        std::uint64_t state;
    };

    [[nodiscard]] static float weight(float dx, float dy) noexcept {
        const float remaining = 1.f - (dx * dx + dy * dy) /
                                          (kDensityRadius * kDensityRadius);
        return remaining > 0.f ? remaining * remaining : 0.f;
    }

    std::vector<Cell> occupiedCells_;
    std::vector<GrassWearPatch> patches_;
};

} // namespace LiberateTheSheep::Game
