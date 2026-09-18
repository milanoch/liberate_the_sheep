#pragma once

#include "Game/BoardModel.hpp"
#include "Game/EscapeTraffic.hpp"
#include "Game/PenEscapePath.hpp"
#include "Game/SheepRunAnimation.hpp"

#include <vector>

namespace LiberateTheSheep::Game {

inline constexpr float kEscapeRunSpeed = 5.5f;
inline constexpr float kEscapeRunStride = 1.6f;
inline constexpr float kEscapeAcceleration = 8.f;
inline constexpr float kEscapeBraking = 10.f;
inline constexpr float kEscapeTrafficStep = 1.f / 120.f;

struct EscapingSheep {
    Sheep sheep;
    PenEscape::Path path;
    float distance{};
    float speed{};
    bool yielding{};

    [[nodiscard]] bool finished() const noexcept { return distance >= path.length; }
};

struct EscapeGait {
    int pose{};
    float bodyWave{};
};

// Pose phase uses actual distance; body movement fades with actual speed.
[[nodiscard]] EscapeGait sampleEscapeGait(const EscapingSheep &runner) noexcept;

// Simulation state, independent of Panda entities/UI. A successful launch
// frees the logical cells immediately; the visual keeps running independently.
class EscapeHerd {
public:
    [[nodiscard]] bool launch(BoardModel &board, SheepId id, float outsideRunout);
    void advance(float deltaTime);
    [[nodiscard]] std::vector<SheepId> takeFinished();
    [[nodiscard]] bool rebuildPaths(int columns, int rows, float outsideRunout);
    void clear() noexcept;

    [[nodiscard]] const std::vector<EscapingSheep> &runners() const noexcept { return runners_; }
    [[nodiscard]] bool empty() const noexcept { return runners_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return runners_.size(); }

private:
    struct Reservation {
        std::vector<EscapeTraffic::Conflict> conflicts;
        std::size_t firstPending{};
    };
    struct TrafficState {
        EscapeTraffic::FootprintRoute footprint;
        // One reservation against each earlier still-active runner, in the
        // same order as runners_. Priority never flips during a crossing.
        std::vector<Reservation> earlier;
    };
    std::vector<EscapingSheep> runners_;
    std::vector<TrafficState> traffic_;
    int columns_{};
    int rows_{};

    void step(float deltaTime);
};

} // namespace LiberateTheSheep::Game
