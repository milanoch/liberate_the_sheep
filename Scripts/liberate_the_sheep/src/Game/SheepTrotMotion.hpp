#pragma once

#include "Game/SheepRunAnimation.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <string_view>

namespace LiberateTheSheep::Game::SheepTrotMotion {

// Offline atlas motion uses the same distance scale as the runtime gait.
// Pixel Y points towards the tail; increasing it cancels forward body travel
// while a hoof is planted. No time-based animation clock is introduced.
inline constexpr double kCellHeight = 512.0;
inline constexpr double kQuadHeightInCells = 1.97;
inline constexpr double kStrideInCells = 1.6;
inline constexpr double kStanceDuty = 0.30;
inline constexpr double kPixelsPerCycle = kStrideInCells * kCellHeight / kQuadHeightInCells;
inline constexpr double kStanceTravelPixels = kPixelsPerCycle * kStanceDuty;
inline constexpr double kPi = 3.1415926535897932384626433832795;

enum class Leg : std::size_t { FrontLeft, FrontRight, HindLeft, HindRight };
inline constexpr std::array kLegs{Leg::FrontLeft, Leg::FrontRight, Leg::HindLeft, Leg::HindRight};
inline constexpr std::array<std::string_view, 4> kLegNames{
    "front_left", "front_right", "hind_left", "hind_right"};
inline constexpr std::array<double, 4> kPhaseOffsets{0.5, 0.0, 0.0, 0.5};
// The baker can place the layered hooves on these lanes without changing the
// motion. Hind feet stay narrow; the pelvis/coat may sway independently.
inline constexpr std::array<double, 4> kLanePixels{73.0, 183.0, 89.0, 167.0};
inline constexpr std::array<double, 4> kNeutralYPixels{215.0, 215.0, 395.0, 395.0};

struct Pose {
    double phase{};
    bool stance{};
    double x{};
    double y{};
    double lift{};
};

[[nodiscard]] inline double wrapPhase(double phase) noexcept {
    if (!std::isfinite(phase)) { return 0.0; }
    return phase - std::floor(phase);
}

[[nodiscard]] inline Pose samplePhase(double cycle, Leg leg) noexcept {
    const double phase = wrapPhase(cycle + kPhaseOffsets[static_cast<std::size_t>(leg)]);
    const double halfTravel = kStanceTravelPixels * 0.5;
    if (phase < kStanceDuty) {
        return {phase, true, 0.0, -halfTravel + phase * kPixelsPerCycle, 0.0};
    }

    // Return the foot forwards without a velocity discontinuity at toe-off or
    // touchdown. The initial/final small overshoot is deliberate: it permits
    // the same positive backward velocity at both ends of the swing.
    const double t = (phase - kStanceDuty) / (1.0 - kStanceDuty);
    const double t2 = t * t, t3 = t2 * t;
    const double tangent = kPixelsPerCycle * (1.0 - kStanceDuty);
    const double y = (2.0 * t3 - 3.0 * t2 + 1.0) * halfTravel
        + (t3 - 2.0 * t2 + t) * tangent
        + (-2.0 * t3 + 3.0 * t2) * -halfTravel
        + (t3 - t2) * tangent;
    const double liftWave = std::sin(kPi * t);
    return {phase, false, 0.0, y, liftWave * liftWave};
}

[[nodiscard]] inline Pose sampleDistance(double distanceInCells, Leg leg) noexcept {
    return samplePhase(distanceInCells / kStrideInCells, leg);
}

[[nodiscard]] inline Pose sampleFrame(int frame, Leg leg) noexcept {
    return samplePhase(static_cast<double>(frame) / SheepRunAnimation::kRunPoseCount, leg);
}

} // namespace LiberateTheSheep::Game::SheepTrotMotion
