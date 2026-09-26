#include "Game/EscapeHerd.hpp"
#include "Game/SheepTrotMotion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
namespace Game = LiberateTheSheep::Game;
namespace Motion = Game::SheepTrotMotion;
namespace Animation = Game::SheepRunAnimation;

static_assert(static_cast<float>(Motion::kStrideInCells) == Game::kEscapeRunStride,
              "Rebake the trot atlas if runtime stride changes");

void check(bool condition, const char *message) {
    if (!condition) { throw std::runtime_error(message); }
}

void near(double first, double second, double tolerance, const char *message) {
    check(std::abs(first - second) <= tolerance, message);
}

void testContactAndLanes() {
    for (const auto leg : Motion::kLegs) {
        const double offset = Motion::kPhaseOffsets[static_cast<std::size_t>(leg)];
        double smallest = 1e10, largest = -1e10;
        for (int step = 0; step <= 10000; ++step) {
            const double phase = static_cast<double>(step) / 10000.0;
            const auto pose = Motion::samplePhase(phase - offset, leg);
            near(pose.x, 0.0, 0.0, "A hoof leaves its fixed lateral lane");
            check(std::isfinite(pose.y) && std::isfinite(pose.lift), "Non-finite hoof pose");
            check(pose.lift >= 0.0 && pose.lift <= 1.0, "Swing lift outside unit range");
            smallest = std::min(smallest, pose.y);
            largest = std::max(largest, pose.y);
            if (pose.stance) {
                near(pose.lift, 0.0, 0.0, "Planted hoof lifts off ground");
                // Forward world position = body travel - local pixel Y.
                const double bodyTravel = phase * Motion::kStrideInCells;
                const double hoofForward = bodyTravel - pose.y *
                    Motion::kQuadHeightInCells / Motion::kCellHeight;
                // The phase=1 sample wraps to the next contact, which has a
                // different planted world position; test this contact only.
                if (phase < Motion::kStanceDuty) {
                    near(hoofForward, Motion::kStrideInCells * Motion::kStanceDuty * 0.5,
                         1e-12, "Planted hoof slides instead of cancelling body travel");
                }
            }
        }
        check(smallest < -80.0 && smallest > -82.0 && largest > 80.0 && largest < 82.0,
              "Hermite swing footprint changed: update baker bounds before changing the motion");
    }
}

void testCycleSeams() {
    constexpr double epsilon = 1e-6;
    for (const auto leg : Motion::kLegs) {
        const double offset = Motion::kPhaseOffsets[static_cast<std::size_t>(leg)];
        for (const double seam : {0.0, Motion::kStanceDuty, 1.0}) {
            const auto before = Motion::samplePhase(seam - offset - epsilon, leg);
            const auto at = Motion::samplePhase(seam - offset, leg);
            const auto after = Motion::samplePhase(seam - offset + epsilon, leg);
            near(before.y, after.y, 0.001, "Hoof position pops at contact/cycle seam");
            near((at.y - before.y) / epsilon, Motion::kPixelsPerCycle, 0.01,
                 "Incoming hoof velocity is discontinuous");
            near((after.y - at.y) / epsilon, Motion::kPixelsPerCycle, 0.01,
                 "Outgoing hoof velocity is discontinuous");
            near(before.lift, after.lift, 1e-9, "Lift pops at contact/cycle seam");
            near((after.lift - before.lift) / (2.0 * epsilon), 0.0, 0.001,
                 "Lift velocity pops at contact/cycle seam");
        }
    }
}

void testDiagonalTimingAndAtlasFrames() {
    for (int frame = 0; frame < Animation::kRunPoseCount; ++frame) {
        const auto fl = Motion::sampleFrame(frame, Motion::Leg::FrontLeft);
        const auto fr = Motion::sampleFrame(frame, Motion::Leg::FrontRight);
        const auto hl = Motion::sampleFrame(frame, Motion::Leg::HindLeft);
        const auto hr = Motion::sampleFrame(frame, Motion::Leg::HindRight);
        near(fl.y, hr.y, 0.0, "Front-left and hind-right diagonal lost sync");
        near(fr.y, hl.y, 0.0, "Front-right and hind-left diagonal lost sync");
        near(fl.lift, hr.lift, 0.0, "Diagonal lift lost sync");
        near(fr.lift, hl.lift, 0.0, "Diagonal lift lost sync");
        near(Motion::wrapPhase(fl.phase - fr.phase), 0.5, 1e-12,
             "Opposite leg pairs no longer half-cycle apart");
        check(!(fl.stance && fr.stance), "Short running contact overlaps both diagonal pairs");
        for (const auto leg : Motion::kLegs) {
            const auto pose = Motion::sampleFrame(frame, leg);
            const auto repeated = Motion::sampleFrame(frame + Animation::kRunPoseCount, leg);
            near(pose.y, repeated.y, 1e-10, "Atlas cycle does not repeat exactly");
            near(pose.lift, repeated.lift, 1e-12, "Atlas lift does not repeat exactly");
        }
    }
    check(Motion::kLanePixels[0] < Motion::kLanePixels[1] &&
              Motion::kLanePixels[2] < Motion::kLanePixels[3], "Hoof lanes cross");
}

// The same distance-based curve is sampled during acceleration, braking and
// a stop. Actual distance is analytic here so render cadence cannot affect it.
double travelled(double time) {
    if (time < 1.0) { return Game::kEscapeRunSpeed * 0.5 * time * time; }
    if (time < 2.0) { return Game::kEscapeRunSpeed * (0.5 + time - 1.0); }
    if (time < 3.0) {
        const double t = time - 2.0;
        return Game::kEscapeRunSpeed * (1.5 + t - 0.5 * t * t);
    }
    if (time < 4.0) { return Game::kEscapeRunSpeed * 2.0; }
    return Game::kEscapeRunSpeed * (2.0 + time - 4.0);
}

void testDistanceSampling() {
    for (const double fps : {30.0, 60.0, 120.0}) {
        double previousDistance = 0.0;
        int previousFrame = 0;
        for (int tick = 0; tick <= static_cast<int>(fps * 6.0); ++tick) {
            const double time = tick / fps, distance = travelled(time);
            check(distance >= previousDistance, "Acceleration trace moved backwards");
            const double cycle = distance / Motion::kStrideInCells;
            const int frame = static_cast<int>(std::floor(cycle * Animation::kRunPoseCount));
            check(frame >= previousFrame, "Distance-driven atlas phase moved backwards");
            for (const auto leg : Motion::kLegs) {
                const auto pose = Motion::sampleDistance(distance, leg);
                const auto equivalent = Motion::samplePhase(cycle, leg);
                near(pose.y, equivalent.y, 0.0, "Time or FPS influences hoof phase");
                if (time >= 3.0 && time <= 4.0) {
                    const auto stopped = Motion::sampleDistance(travelled(3.0), leg);
                    near(pose.y, stopped.y, 0.0, "Hooves advance while stopped");
                }
            }
            previousDistance = distance;
            previousFrame = frame;
        }
    }
    // Advancing exactly one stride returns every leg to the same phase.
    for (const auto leg : Motion::kLegs) {
        const auto first = Motion::sampleDistance(0.137, leg);
        const auto next = Motion::sampleDistance(0.137 + Motion::kStrideInCells, leg);
        near(first.y, next.y, 1e-10, "One runtime stride differs from one baked cycle");
    }
}

void testQuantizedStanceSampling() {
    // Runtime selects a whole atlas frame with floor(distance/stride * count).
    // Ground contact can therefore drift by at most ONE frame's travel, not
    // remain perfectly stationary between discrete pose updates.
    constexpr double tolerance = 1e-11;
    constexpr double frameTravel = Motion::kStrideInCells / Animation::kRunPoseCount;
    for (const double fps : {30.0, 60.0, 120.0}) {
        std::array<double, 4> previousHoofWorld{};
        std::array<double, 4> previousContact{};
        std::array<bool, 4> previousStance{};
        double previousDistance = 0.0;
        int checkedContacts = 0;
        for (int tick = 0; tick <= static_cast<int>(fps * 6.0); ++tick) {
            const double distance = travelled(tick / fps);
            const double cycle = distance / Motion::kStrideInCells;
            const int frame = static_cast<int>(std::floor(cycle * Animation::kRunPoseCount));
            for (const auto leg : Motion::kLegs) {
                const auto index = static_cast<std::size_t>(leg);
                const auto actual = Motion::sampleDistance(distance, leg);
                const auto sampled = Motion::sampleFrame(frame, leg);
                const double contact = std::floor(cycle + Motion::kPhaseOffsets[index]);
                const double contactStart =
                    (contact - Motion::kPhaseOffsets[index]) * Motion::kStrideInCells;
                const double idealHoofWorld = contactStart +
                    Motion::kStrideInCells * Motion::kStanceDuty * 0.5;
                const double sampledHoofWorld = distance - sampled.y *
                    Motion::kQuadHeightInCells / Motion::kCellHeight;
                const bool planted = actual.stance && sampled.stance;
                if (planted && distance > previousDistance) {
                    const double residual = sampledHoofWorld - idealHoofWorld;
                    check(residual >= -tolerance && residual <= frameTravel + tolerance,
                          "Floored atlas contact slides more than one frame's travel");
                    if (previousStance[index] && contact == previousContact[index]) {
                        check(std::abs(sampledHoofWorld - previousHoofWorld[index]) <=
                                  frameTravel + tolerance,
                              "Successive rendered stance poses cause excessive hoof sliding");
                    }
                    ++checkedContacts;
                }
                previousHoofWorld[index] = sampledHoofWorld;
                previousContact[index] = contact;
                previousStance[index] = planted;
            }
            previousDistance = distance;
        }
        check(checkedContacts > 20, "Render cadence did not exercise enough moving contacts");
    }
}
} // namespace

int main() {
    try {
        testContactAndLanes();
        testCycleSeams();
        testDiagonalTimingAndAtlasFrames();
        testDistanceSampling();
        testQuantizedStanceSampling();
        std::cout << "Sheep trot: " << Animation::kRunPoseCount
                  << " frames, fixed hoof lanes, no-slip contacts, C1 cycle/contact seams, "
                     "diagonal pairing, 30/60/120 FPS distance phase and floored-frame contact bounds passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Sheep trot failure: " << error.what() << '\n';
        return 1;
    }
}
