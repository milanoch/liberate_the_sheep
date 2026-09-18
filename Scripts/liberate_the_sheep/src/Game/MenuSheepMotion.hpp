#pragma once

namespace LiberateTheSheep::Game::MenuSheepMotion {

inline constexpr float kRunSpeed = 1.75f;
// Preserve the original full-animal gait's cadence. Extra in-between frames
// make the same stride smoother; they must not make the legs cycle faster.
inline constexpr float kStrideLength = 1.75f;
inline constexpr float kGroundOffset = 0.26f;
inline constexpr float kClearance = 0.10f;
inline constexpr int kRunFrames = 48;

struct JumpArc {
    float airborneHalfWidth{};
    float transitionDistance{};
    float height{};
    float airborneSpeed{};
    float transitionDuration{};

    [[nodiscard]] float windowHalfWidth() const noexcept {
        return airborneHalfWidth + transitionDistance;
    }

    [[nodiscard]] float airborneDuration() const noexcept {
        return 2.f * airborneHalfWidth / airborneSpeed;
    }
};

struct Pose {
    // Reserved for a hoof-pivot correction. All articulation is baked in art.
    float forwardOffset{};
    float rise{};
    float scaleX{1.f};
    float scaleY{1.f};
    // Relative to the right-facing art. Mirror the angle with travel direction.
    float pitchDegrees{};
    float runFramePosition{};
    // Negative while running; otherwise position in the ordered jump sequence.
    float jumpFramePosition{-1.f};
};

// obstacleRise is the hay's top relative to the resting sheep centre. Bounds
// conservatively cover the complete atlas silhouette. The Sprite itself is
// neither rotated nor stretched at runtime.
[[nodiscard]] JumpArc makeJumpArc(
    float obstacleHalfWidth, float obstacleRise, float sheepScaleX, float sheepScaleY
) noexcept;

// Signed time/distance along travel, with the centre of the hay at zero.
// The inverse pair adds only a modest 10% approach speed before takeoff, uses
// constant forward speed in flight, then eases back during the landing stance.
[[nodiscard]] float routeDistance(float signedTime, const JumpArc &arc) noexcept;
[[nodiscard]] float routeTime(float signedDistance, const JumpArc &arc) noexcept;

// signedDistance increases along the sheep's direction of travel, with hay at 0.
[[nodiscard]] Pose sample(float signedDistance, const JumpArc &arc, float sheepScaleY) noexcept;

} // namespace LiberateTheSheep::Game::MenuSheepMotion
