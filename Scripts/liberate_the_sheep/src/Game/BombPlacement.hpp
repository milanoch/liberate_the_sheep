#pragma once

#include "Game/BoardModel.hpp"

#include <vector>

namespace LiberateTheSheep::Game {

struct LevelDefinition;

struct BombPlacement {
    SheepId id{kInvalidSheepId};
    bool impact{};
    float seconds{};
    int hits{};
    // Includes the bomb sheep itself and every distinct sheep which must be
    // removed before its forward lane becomes clear.
    int requiredMoves{};
};

// Selects distinct sheep for one timed bomb and up to two impact bombs.
// Difficulty uses the actual blocker graph, rather than location alone.
[[nodiscard]] std::vector<BombPlacement> planBombPlacement(
    const BoardModel &board, const LevelDefinition &level
);

} // namespace LiberateTheSheep::Game
