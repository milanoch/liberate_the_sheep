#pragma once

#include "Game/PenEscapePath.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace LiberateTheSheep::Game::EscapeTraffic {

// An interval conservatively encloses the whole animated sheep at every
// distance from begin to end. It is not merely the sheep's centerline.
struct SweptFootprint {
    float begin{};
    float end{};
    PenEscape::Point center{};
    PenEscape::Point forward{};
    PenEscape::Point sideways{};
    float halfLength{};
    float halfWidth{};
    PenEscape::Point minimum{};
    PenEscape::Point maximum{};
};

struct FootprintRoute {
    std::vector<SweptFootprint> intervals;
    // Stable world-cell buckets, built once with the route and reused for
    // every younger sheep. Keys pack signed x/y coordinates losslessly.
    std::unordered_map<std::uint64_t, std::vector<std::size_t>> buckets;
};

struct Conflict {
    float followerDistance{};
    float leaderClearDistance{};
};

[[nodiscard]] FootprintRoute buildRoute(
    const PenEscape::Path &path, int columns, int rows
);

// The older sheep owns its remaining route. The younger sheep may enter each
// interval only after all overlapping older intervals have been traversed.
// The result is sorted by followerDistance and independent of frame timing.
[[nodiscard]] std::vector<Conflict> reserveAgainst(
    const FootprintRoute &follower, const FootprintRoute &leader
);

// Infinity means that no older reservation remains. Otherwise do not advance
// the younger sheep past this distance (brake before it, never snap backward).
[[nodiscard]] float stopDistance(
    const std::vector<Conflict> &conflicts, float leaderDistance
) noexcept;

} // namespace LiberateTheSheep::Game::EscapeTraffic
