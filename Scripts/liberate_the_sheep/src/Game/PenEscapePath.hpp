#pragma once

#include "Game/BoardModel.hpp"

#include <vector>

namespace LiberateTheSheep::Game::PenEscape {

// All distances are in board-cell units, independent of camera and viewport.
// The flock occupies [0, columns] x [0, rows]. Keep a full sheep's turning
// envelope between that rectangle and the wooden fence.
inline constexpr float kCorridorOffset = 1.40f;
inline constexpr float kFenceOffset = 2.60f;
inline constexpr float kGateWidth = 2.20f;
inline constexpr float kCornerRadius = 0.35f;
inline constexpr float kOutsideRunout = 4.00f;

struct Point {
    float x{};
    float y{};
};

struct Segment {
    enum class Kind { Line, Arc };

    Kind kind{Kind::Line};
    Point start{};
    Point end{};
    Point center{};
    float radius{};
    float startAngleRadians{};
    float sweepRadians{};
    float length{};
};

struct Path {
    std::vector<Segment> segments;
    Point start{};
    Point end{};
    float length{};

    [[nodiscard]] bool valid() const noexcept;
};

struct Sample {
    Point position{};
    Point tangent{0.f, 1.f};
    // The sheep artwork faces up: up = 0, right = -90, down = +/-180.
    float rotationDegrees{};
    bool finished{false};
};

// Call only after BoardModel::inspectMove says Clear. The first leg follows
// that exact unobstructed ray. Turns begin only after the whole sheep clears
// the flock; the shortest side of the perimeter leads to ONE bottom gate.
// Invalid dimensions, direction or starting placement produce an empty path.
// Extend outsideRunout to put the whole sheep below the current camera view.
// Values below kOutsideRunout use that minimum; non-finite values are invalid.
[[nodiscard]] Path makePath(
    int columns, int rows, const Sheep &sheep, float outsideRunout = kOutsideRunout
);

// Distance parameterization gives constant ground speed, including corners.
// Lines and circular fillets meet with continuous position and heading.
[[nodiscard]] Sample sample(const Path &path, float travelledDistance) noexcept;

} // namespace LiberateTheSheep::Game::PenEscape
