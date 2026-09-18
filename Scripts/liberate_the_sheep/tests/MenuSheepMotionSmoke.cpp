#include "Game/MenuSheepMotion.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace {

namespace Motion = LiberateTheSheep::Game::MenuSheepMotion;

constexpr float kPi = 3.14159265359f;

struct Scene {
    float hayHalfWidth;
    float hayTop;
    float sheepScaleX;
    float sheepScaleY;
};

struct Bounds {
    float left{std::numeric_limits<float>::max()};
    float right{std::numeric_limits<float>::lowest()};
    float bottom{std::numeric_limits<float>::max()};
};

// Conservative alpha bounds of every atlas pose, transformed just like Sprite.
// Test the full silhouette's lowest point, not only the centre or the hooves.
Bounds sheepBounds(float distance, int direction, const Scene &scene, const Motion::Pose &pose) {
    Bounds bounds;
    const float radians = pose.pitchDegrees * static_cast<float>(direction) * kPi / 180.f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    for (const float x : {-0.355f, 0.355f}) {
        for (const float y : {-0.30f, 0.30f}) {
            const float localX = x * scene.sheepScaleX * pose.scaleX * static_cast<float>(direction);
            const float localY = y * scene.sheepScaleY * pose.scaleY;
            const float worldX = (distance + pose.forwardOffset) * static_cast<float>(direction) +
                localX * cosine - localY * sine;
            const float worldY = Motion::kGroundOffset * scene.sheepScaleY + pose.rise +
                localX * sine + localY * cosine;
            bounds.left = std::min(bounds.left, worldX);
            bounds.right = std::max(bounds.right, worldX);
            bounds.bottom = std::min(bounds.bottom, worldY);
        }
    }
    return bounds;
}

bool validPose(const Motion::Pose &pose) {
    return std::isfinite(pose.forwardOffset) && std::isfinite(pose.rise) && std::isfinite(pose.scaleX) &&
        std::isfinite(pose.scaleY) && std::isfinite(pose.pitchDegrees) &&
        std::isfinite(pose.runFramePosition) && std::isfinite(pose.jumpFramePosition) &&
        pose.forwardOffset == 0.f && pose.scaleX == 1.f && pose.scaleY == 1.f &&
        pose.pitchDegrees == 0.f && pose.rise >= 0.f &&
        pose.runFramePosition >= 0.f && pose.runFramePosition < Motion::kRunFrames &&
        (pose.jumpFramePosition == -1.f ||
            (pose.jumpFramePosition >= 0.f && pose.jumpFramePosition <= 23.f));
}

bool checkFlight(const Scene &scene) {
    const Motion::JumpArc arc = Motion::makeJumpArc(
        scene.hayHalfWidth,
        scene.hayTop - Motion::kGroundOffset * scene.sheepScaleY,
        scene.sheepScaleX,
        scene.sheepScaleY
    );
    if (!std::isfinite(arc.height) || !std::isfinite(arc.airborneHalfWidth) ||
        arc.height <= 0.f || arc.airborneHalfWidth <= 0.f || arc.transitionDistance <= 0.f ||
        !std::isfinite(arc.airborneSpeed) || arc.airborneSpeed < Motion::kRunSpeed ||
        arc.airborneSpeed > Motion::kRunSpeed * 1.10f + 0.00001f ||
        arc.transitionDuration <= 0.f) {
        std::cerr << "Invalid jump dimensions\n";
        return false;
    }

    const float range = arc.windowHalfWidth() + 0.4f;
    const int steps = static_cast<int>(std::ceil(range * 2.f / 0.0015f));
    for (const int direction : {-1, 1}) {
        float previousJumpFrame = -1.f;
        int overlappingSamples = 0;
        for (int step = 0; step <= steps; ++step) {
            const float distance = -range + range * 2.f * static_cast<float>(step) / static_cast<float>(steps);
            const Motion::Pose pose = Motion::sample(distance, arc, scene.sheepScaleY);
            if (!validPose(pose)) {
                std::cerr << "Invalid pose at distance " << distance << '\n';
                return false;
            }
            if (pose.jumpFramePosition >= 0.f) {
                if (pose.jumpFramePosition < previousJumpFrame) {
                    std::cerr << "Jump frames moved backwards\n";
                    return false;
                }
                previousJumpFrame = pose.jumpFramePosition;
            }

            const Bounds bounds = sheepBounds(distance, direction, scene, pose);
            if (bounds.right >= -scene.hayHalfWidth && bounds.left <= scene.hayHalfWidth) {
                ++overlappingSamples;
                if (bounds.bottom < scene.hayTop + 0.05f) {
                    std::cerr << "Sheep clips hay: direction=" << direction << ", distance=" << distance
                              << ", clearance=" << bounds.bottom - scene.hayTop << '\n';
                    return false;
                }
            }
        }
        if (overlappingSamples == 0 || previousJumpFrame < 22.9f) {
            std::cerr << "Flight sampling did not cover the hay or entire animation\n";
            return false;
        }
    }

    // Both transition/flight boundaries and both run/jump boundaries must
    // preserve position, size and orientation. The atlas changes banks only
    // at the outer boundaries, where its first/last poses match the run pose.
    constexpr float epsilon = 0.00001f;
    for (const float seam : {
             -arc.windowHalfWidth(), -arc.airborneHalfWidth,
             arc.airborneHalfWidth, arc.windowHalfWidth()}) {
        const Motion::Pose before = Motion::sample(seam - epsilon, arc, scene.sheepScaleY);
        const Motion::Pose after = Motion::sample(seam + epsilon, arc, scene.sheepScaleY);
        if (std::abs(before.forwardOffset - after.forwardOffset) > 0.001f ||
            std::abs(before.rise - after.rise) > 0.001f ||
            std::abs(before.scaleX - after.scaleX) > 0.001f ||
            std::abs(before.scaleY - after.scaleY) > 0.001f ||
            std::abs(before.pitchDegrees - after.pitchDegrees) > 0.001f ||
            (before.jumpFramePosition >= 0.f && after.jumpFramePosition >= 0.f &&
                std::abs(before.jumpFramePosition - after.jumpFramePosition) > 0.001f)) {
            std::cerr << "Discontinuous transform or frame at seam " << seam << '\n';
            return false;
        }
    }

    const float halfFlightTime = arc.airborneDuration() * 0.5f;
    const float halfWindowTime = halfFlightTime + arc.transitionDuration;
    const float timeRange = halfWindowTime + 1.f;
    constexpr int timeSteps = 10000;
    float previousDistance = Motion::routeDistance(-timeRange, arc);
    for (int step = 0; step <= timeSteps; ++step) {
        const float time = -timeRange + 2.f * timeRange * static_cast<float>(step) / timeSteps;
        const float distance = Motion::routeDistance(time, arc);
        if (!std::isfinite(distance) || distance < previousDistance ||
            std::abs(Motion::routeTime(distance, arc) - time) > 0.000005f ||
            std::abs(Motion::routeDistance(-time, arc) + distance) > 0.000005f) {
            std::cerr << "Route is not monotonic, symmetric or invertible at time " << time << '\n';
            return false;
        }
        previousDistance = distance;
    }

    // Check the entire approach/landing easing too: matching endpoint speeds
    // alone would miss a hidden forward-speed spike between the endpoints.
    constexpr float velocityStep = 0.001f;
    for (int step = 0; step <= 200; ++step) {
        const float time = -timeRange + 2.f * timeRange * static_cast<float>(step) / 200.f;
        const float speed = (Motion::routeDistance(time + velocityStep, arc) -
            Motion::routeDistance(time - velocityStep, arc)) / (2.f * velocityStep);
        if (speed < Motion::kRunSpeed - 0.002f ||
            speed > Motion::kRunSpeed * 1.10f + 0.002f) {
            std::cerr << "Jump produces an excessive horizontal speed change\n";
            return false;
        }
    }

    // No horizontal velocity snap at takeoff/landing or ramp/run boundaries.
    constexpr float speedStep = 0.0005f;
    for (const float seam : {-halfWindowTime, -halfFlightTime, halfFlightTime, halfWindowTime}) {
        const float at = Motion::routeDistance(seam, arc);
        const float beforeSpeed = (at - Motion::routeDistance(seam - speedStep, arc)) / speedStep;
        const float afterSpeed = (Motion::routeDistance(seam + speedStep, arc) - at) / speedStep;
        const float expectedSpeed = std::abs(seam) == halfFlightTime
            ? arc.airborneSpeed : Motion::kRunSpeed;
        if (std::abs(beforeSpeed - afterSpeed) > 0.01f ||
            std::abs(beforeSpeed - expectedSpeed) > 0.01f ||
            std::abs(afterSpeed - expectedSpeed) > 0.01f) {
            std::cerr << "Forward velocity snaps at time " << seam << '\n';
            return false;
        }
    }

    // The apex obeys the same downward acceleration as ascent and descent:
    // a low jump should not spend extra time hanging at the top of its arc.
    const float expectedGravity = 8.f * arc.height /
        (arc.airborneDuration() * arc.airborneDuration());
    constexpr float gravityStep = 0.025f;
    const auto heightAt = [&](float time) {
        return Motion::sample(Motion::routeDistance(time, arc), arc, scene.sheepScaleY).rise;
    };
    for (const float time : {-halfFlightTime * 0.6f, 0.f, halfFlightTime * 0.6f}) {
        const float gravity = (2.f * heightAt(time) - heightAt(time - gravityStep) -
            heightAt(time + gravityStep)) / (gravityStep * gravityStep);
        if (std::abs(gravity - expectedGravity) > 0.005f) {
            std::cerr << "Flight has a flattened apex or variable gravity\n";
            return false;
        }
    }
    return true;
}

float frameDifference(float actual, float expected) {
    const float difference = std::abs(actual - expected);
    return std::min(difference, static_cast<float>(Motion::kRunFrames) - difference);
}

bool checkRunning() {
    const Motion::JumpArc arc = Motion::makeJumpArc(0.475f, 0.423671875f - Motion::kGroundOffset, 1.05f, 1.f);
    if (Motion::kRunSpeed <= 0.f || Motion::kStrideLength <= 0.f) {
        return false;
    }
    // The menu hay's visible alpha occupies at most .425 of its Sprite's
    // width on either side. Preserve a low, moderate-duration leap with only
    // a slight forward speed change, not the previous .74-second dash.
    const Motion::JumpArc visibleHayArc = Motion::makeJumpArc(
        0.425f * 0.95f, 0.423671875f - Motion::kGroundOffset, 1.05f, 1.f
    );
    const Motion::Pose apex = Motion::sample(0.f, arc, 1.f);
    if (apex.rise > 0.90f || arc.airborneDuration() < 1.35f || arc.airborneDuration() > 1.60f ||
        visibleHayArc.airborneDuration() < 1.30f || visibleHayArc.airborneDuration() > 1.45f) {
        std::cerr << "Normal jump is too high, too fast or stays airborne too long\n";
        return false;
    }
    const float strideDuration = Motion::kStrideLength / Motion::kRunSpeed;
    if (strideDuration < 0.85f || strideDuration > 1.10f) {
        std::cerr << "Extra run frames changed the original gait's cadence\n";
        return false;
    }

    // At the actual travel speed, each frame interval must move the animal
    // through its share of a stride, including wraparound and negative X.
    const float frameTime = Motion::kStrideLength / (Motion::kRunSpeed * Motion::kRunFrames);
    for (const int side : {-1, 1}) {
        const float start = static_cast<float>(side) * (arc.windowHalfWidth() + 3.f * Motion::kStrideLength);
        for (int frame = 0; frame <= Motion::kRunFrames * 2; ++frame) {
            const float distance = start + Motion::kRunSpeed * frameTime * static_cast<float>(frame);
            const Motion::Pose pose = Motion::sample(distance, arc, 1.f);
            if (!validPose(pose) || pose.jumpFramePosition >= 0.f ||
                frameDifference(pose.runFramePosition, static_cast<float>(frame % Motion::kRunFrames)) > 0.0001f) {
                std::cerr << "Run frames do not track distance through a complete stride\n";
                return false;
            }
        }
    }

    // The full-animal atlas carries the motion. Runtime bobbing/scaling must
    // not distort the silhouette or add a second, unrelated body animation.
    for (int slot = 0; slot < Motion::kRunFrames * 4; ++slot) {
        const float distance = arc.windowHalfWidth() +
            (2.f + static_cast<float>(slot) / (Motion::kRunFrames * 4)) * Motion::kStrideLength;
        const Motion::Pose pose = Motion::sample(distance, arc, 1.f);
        if (pose.forwardOffset != 0.f || pose.rise != 0.f || pose.pitchDegrees != 0.f ||
            pose.scaleX != 1.f || pose.scaleY != 1.f) {
            std::cerr << "Runtime transforms duplicate the baked run animation\n";
            return false;
        }
    }
    return true;
}

bool checkScheduledWaves() {
    // Mirror the route-level contract used by MenuDiorama's schedule: centre
    // times are spaced within each flock; opposite flocks get a complete
    // offscreen traversal plus a hold. Include unequal sheep sizes and hay
    // offsets, because each runner can have a different route mapping.
    struct Runner {
        int direction;
        float halfWidth;
        float scheduleTime{};
        Motion::JumpArc arc;
        float previousX{};
    };
    constexpr float spacing = 2.2f;
    constexpr float offscreenMargin = 1.4f;
    constexpr float offscreenHold = 1.f;
    constexpr int samplesPerCycle = 2000;
    for (const float visibleHalfWidth : {1.5f, 2.53125f, 4.5f, 8.f, 12.f}) {
        for (const float obstacleOffset : {-1.25f, 0.f, 1.25f}) {
            for (const auto counts : {
                     std::array<int, 2>{3, 3}, std::array<int, 2>{1, 4},
                     std::array<int, 2>{3, 0}, std::array<int, 2>{0, 1}}) {
                std::vector<Runner> runners;
                for (int directionIndex = 0; directionIndex < 2; ++directionIndex) {
                    for (int index = 0; index < counts[directionIndex]; ++index) {
                        const float scale = 0.75f + 0.15f * static_cast<float>(index);
                        runners.push_back({
                            directionIndex == 0 ? 1 : -1,
                            0.355f * scale,
                            0.f,
                            Motion::makeJumpArc(
                                0.475f, 0.423671875f - Motion::kGroundOffset * scale,
                                scale, scale
                            ),
                            0.f,
                        });
                    }
                }
                const float resetDistance = visibleHalfWidth + offscreenMargin +
                    std::abs(obstacleOffset);
                float halfRouteTime = 0.f;
                for (const Runner &runner : runners) {
                    halfRouteTime = std::max(halfRouteTime, Motion::routeTime(resetDistance, runner.arc));
                }
                const float switchGap = 2.f * halfRouteTime + offscreenHold;
                float cycle;
                if (counts[0] > 0 && counts[1] > 0) {
                    const float rightSpan = spacing * static_cast<float>(counts[0] - 1);
                    const float leftSpan = spacing * static_cast<float>(counts[1] - 1);
                    cycle = rightSpan + switchGap + leftSpan + switchGap;
                    for (std::size_t index = 0; index < runners.size(); ++index) {
                        runners[index].scheduleTime = index < static_cast<std::size_t>(counts[0])
                            ? spacing * static_cast<float>(index)
                            : rightSpan + switchGap +
                                spacing * static_cast<float>(index - static_cast<std::size_t>(counts[0]));
                    }
                } else {
                    cycle = std::max(spacing * static_cast<float>(runners.size()), switchGap);
                    for (std::size_t index = 0; index < runners.size(); ++index) {
                        runners[index].scheduleTime = cycle * static_cast<float>(index) /
                            static_cast<float>(runners.size());
                    }
                }
                const float timeStep = cycle / samplesPerCycle;
                for (int step = 0; step <= samplesPerCycle * 2; ++step) {
                    const float elapsed = static_cast<float>(step) * timeStep;
                    bool rightVisible = false;
                    bool leftVisible = false;
                    for (Runner &runner : runners) {
                        const float rawTime = elapsed - runner.scheduleTime + cycle * 0.5f;
                        float wrappedTime = std::fmod(rawTime, cycle);
                        if (wrappedTime < 0.f) {
                            wrappedTime += cycle;
                        }
                        const float relativeTime = wrappedTime - cycle * 0.5f;
                        const float x = obstacleOffset + static_cast<float>(runner.direction) *
                            Motion::routeDistance(relativeTime, runner.arc);
                        const bool visible = std::abs(x) <= visibleHalfWidth + runner.halfWidth;
                        const bool previouslyVisible = std::abs(runner.previousX) <=
                            visibleHalfWidth + runner.halfWidth;
                        const float allowedTravel = runner.arc.airborneSpeed * timeStep + 0.0005f;
                        if (step > 0 && std::abs(x - runner.previousX) > allowedTravel &&
                            (visible || previouslyVisible)) {
                            std::cerr << "Scheduled route teleports a sheep while visible\n";
                            return false;
                        }
                        runner.previousX = x;
                        rightVisible = rightVisible || (visible && runner.direction > 0);
                        leftVisible = leftVisible || (visible && runner.direction < 0);
                    }
                    if (rightVisible && leftVisible) {
                        std::cerr << "Opposite flocks overlap in the visible viewport\n";
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

} // namespace

int main() {
    const std::array<Scene, 7> scenes{{
        {0.425f * 0.95f, 0.423671875f, 1.05f, 1.f},
        {0.475f, 0.423671875f, 1.05f, 1.f},
        {0.475f, 0.423671875f, 0.80f, 0.75f},
        {0.475f, 0.423671875f, 1.35f, 1.30f},
        {0.90f, 0.70f, 1.05f, 1.f},
        {0.60f, 1.00f, 0.90f, 1.20f},
        {0.30f, 0.25f, 1.40f, 0.80f},
    }};
    for (const Scene &scene : scenes) {
        if (!checkFlight(scene)) {
            return 1;
        }
    }
    if (!checkRunning() || !checkScheduledWaves()) {
        return 1;
    }
    std::cout << "Menu sheep motion: clearance, moderate ballistic flight, bounded forward speed, 48-frame stride and offscreen waves passed\n";
    return 0;
}
