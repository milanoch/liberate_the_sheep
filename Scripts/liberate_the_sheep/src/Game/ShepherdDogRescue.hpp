#pragma once

#include "Game/BoardModel.hpp"
#include "Game/PenEscapePath.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace LiberateTheSheep::Game::DogRescue {

using Point = PenEscape::Point;

// Positions, distances and lift use board-cell units. Artwork faces +Y.
enum class Stage { Idle, Approach, Grab, Return, Finished };

struct Pose {
    Point position{};
    float rotationDegrees{};
    int frame{}; // Logical atlas cell; layout is in ShepherdDogAnimation.hpp.
    float speed{};
    float lift{};
    bool carrying{};
    // A completely sealed flock yields visually during this route. The
    // controller moves nearby sheep aside temporarily without changing cells.
    bool clearingFlock{};
    Point bombPosition{};
    float bombRotationDegrees{};
};

// Moving sheep have already left BoardModel but still occupy visible ground
// while the rescue pauses the flock. Supply their current rendered poses.
struct OtherSheepPose {
    SheepId id{kInvalidSheepId};
    Point position{};
    float rotationDegrees{};
};

// An authored dog Sprite consumes this motion. The game controller owns the
// cinematic pause and applies poses; this helper never edits the logical flock.
class Motion final {
public:
    [[nodiscard]] bool begin(const BoardModel &board, SheepId target,
                             Point bombPosition, float bombRotationDegrees,
                             float offscreenY,
                             std::span<const OtherSheepPose> otherSheep = {});
    void advance(float deltaTime);
    // Extend the exit below a resized camera without moving the current pose
    // or resetting its distance-driven gait. Already-long exits are retained.
    void setOffscreenY(float y);
    void clear();

    [[nodiscard]] const Pose &pose() const noexcept { return pose_; }
    [[nodiscard]] Stage stage() const noexcept { return stage_; }
    [[nodiscard]] bool active() const noexcept;
    // A pulse on the advance() call when the jaws take the bomb. The caller
    // can remove the sheep's accessory at exactly this point.
    [[nodiscard]] bool justGrabbed() const noexcept { return justGrabbed_; }

private:
    struct Waypoint {
        Point position{};
        bool leapFromPrevious{};
    };

    void advanceTravel(float deltaTime, bool returning);
    void advanceGrab(float deltaTime);
    void updateCarriedBomb();

    std::vector<Waypoint> route_;
    std::vector<float> cumulative_;
    Stage stage_{Stage::Idle};
    Pose pose_{};
    Point originalBomb_{};
    float originalBombRotation_{};
    float grabHeading_{};
    float distance_{};
    float speed_{};
    float gaitDistance_{};
    float grabElapsed_{};
    float turnElapsed_{};
    bool justGrabbed_{};
};

} // namespace LiberateTheSheep::Game::DogRescue
