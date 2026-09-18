#include "Game/MenuSheepMotion.hpp"

#include <algorithm>
#include <cmath>

namespace LiberateTheSheep::Game::MenuSheepMotion {
namespace {

constexpr float kAirborneSpeedFactor = 1.10f;
constexpr float kTransitionDuration = 0.16f;

// Integrate v(q) = airborneSpeed + (runSpeed - airborneSpeed) * smoothstep(q).
// Its endpoint speeds and accelerations meet the adjacent constant-speed legs.
float landingDistance(float time, const JumpArc &arc) noexcept {
    const float phase = time / arc.transitionDuration;
    const float integral = phase * phase * phase * (1.f - 0.5f * phase);
    return arc.airborneSpeed * time +
        (kRunSpeed - arc.airborneSpeed) * arc.transitionDuration * integral;
}

} // namespace

JumpArc makeJumpArc(
    float obstacleHalfWidth, float obstacleRise, float sheepScaleX, float sheepScaleY
) noexcept {
    // Atlas alpha bounds: |x| <= .355, downward extent <= .30. These already
    // enclose every baked pose; there is no second runtime rotation/stretch.
    // Inflating them for a nonexistent transform made the leap too long.
    const float halfWidth = 0.355f * sheepScaleX;
    const float downExtent = 0.30f * sheepScaleY;
    const float overlapHalfWidth = obstacleHalfWidth + halfWidth + 0.04f;
    const float requiredRise = std::max(
        0.f, obstacleRise + downExtent + kClearance
    );

    JumpArc arc;
    arc.height = std::max(0.90f * sheepScaleY, requiredRise + 0.22f * sheepScaleY);
    // A real parabola gives a constant downward acceleration, not a flattened
    // apex. Solve H * (1 - (C/A)^2) >= R at the silhouette/hay overlap edges.
    const float remainingHeight = 1.f - requiredRise / arc.height;
    arc.airborneHalfWidth = std::max(
        1.25f * sheepScaleX,
        overlapHalfWidth / std::sqrt(remainingHeight)
    );
    // Never force an arbitrary short flight time by multiplying forward speed.
    // The last version crossed this arc at almost three times running speed.
    // Choose the route's safe geometry first, then keep movement close to the
    // approach speed. Wider obstacles require a longer leap, not a dash.
    arc.airborneSpeed = kRunSpeed * kAirborneSpeedFactor;
    arc.transitionDuration = kTransitionDuration;
    arc.transitionDistance = (arc.airborneSpeed + kRunSpeed) * 0.5f * arc.transitionDuration;
    return arc;
}

float routeDistance(float signedTime, const JumpArc &arc) noexcept {
    const float time = std::abs(signedTime);
    const float airborneHalfTime = arc.airborneHalfWidth / arc.airborneSpeed;
    float distance;
    if (time <= airborneHalfTime) {
        distance = time * arc.airborneSpeed;
    } else if (time < airborneHalfTime + arc.transitionDuration) {
        distance = arc.airborneHalfWidth + landingDistance(time - airborneHalfTime, arc);
    } else {
        distance = arc.windowHalfWidth() +
            kRunSpeed * (time - airborneHalfTime - arc.transitionDuration);
    }
    return std::copysign(distance, signedTime);
}

float routeTime(float signedDistance, const JumpArc &arc) noexcept {
    const float distance = std::abs(signedDistance);
    const float airborneHalfTime = arc.airborneHalfWidth / arc.airborneSpeed;
    float time;
    if (distance <= arc.airborneHalfWidth) {
        time = distance / arc.airborneSpeed;
    } else if (distance >= arc.windowHalfWidth()) {
        time = airborneHalfTime + arc.transitionDuration +
            (distance - arc.windowHalfWidth()) / kRunSpeed;
    } else {
        // The integrated smoothstep is strictly increasing. Bisection avoids
        // unstable Newton steps when the airborne/run speeds differ greatly.
        float lower = 0.f;
        float upper = arc.transitionDuration;
        for (int iteration = 0; iteration < 24; ++iteration) {
            const float middle = (lower + upper) * 0.5f;
            if (landingDistance(middle, arc) < distance - arc.airborneHalfWidth) {
                lower = middle;
            } else {
                upper = middle;
            }
        }
        time = airborneHalfTime + (lower + upper) * 0.5f;
    }
    return std::copysign(time, signedDistance);
}

Pose sample(float signedDistance, const JumpArc &arc, float /* sheepScaleY */) noexcept {
    Pose pose;
    const float halfWindow = arc.windowHalfWidth();
    if (std::abs(signedDistance) >= halfWindow) {
        const float distance = signedDistance < 0.f
            ? signedDistance + halfWindow
            : signedDistance - halfWindow;
        const float cycles = distance / kStrideLength;
        pose.runFramePosition = (cycles - std::floor(cycles)) * kRunFrames;
        // Full-animal poses and their in-betweens contain the motion already.
        // An extra runtime bob or scaling would distort that original gait.
        return pose;
    }

    if (signedDistance < -arc.airborneHalfWidth) {
        const float phase = std::clamp(
            (routeTime(signedDistance, arc) + arc.airborneDuration() * 0.5f +
                arc.transitionDuration) / arc.transitionDuration,
            0.f, 1.f
        );
        pose.jumpFramePosition = phase * 4.f;
    } else if (signedDistance > arc.airborneHalfWidth) {
        const float phase = std::clamp(
            (routeTime(signedDistance, arc) - arc.airborneDuration() * 0.5f) /
                arc.transitionDuration,
            0.f, 1.f
        );
        pose.jumpFramePosition = 20.f + phase * 3.f;
    } else {
        const float normalized = signedDistance / arc.airborneHalfWidth;
        const float phase = (normalized + 1.f) * 0.5f;
        const float squared = normalized * normalized;
        pose.rise = arc.height * (1.f - squared);
        pose.jumpFramePosition = 4.f + phase * 16.f;
    }
    // Anticipation, leg tuck and landing poses come from the original artwork.
    // Only translate the intact silhouette along the arc.
    return pose;
}

} // namespace LiberateTheSheep::Game::MenuSheepMotion
