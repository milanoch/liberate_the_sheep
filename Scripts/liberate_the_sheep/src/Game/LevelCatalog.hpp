#pragma once

#include "Game/BoardGenerator.hpp"

#include <span>
#include <string>
#include <vector>

namespace LiberateTheSheep::Game {

struct LevelDefinition {
    int number{};
    GenerationConfig generation;
};

[[nodiscard]] std::span<const LevelDefinition> levelCatalog() noexcept;
[[nodiscard]] const LevelDefinition *findLevel(int levelNumber) noexcept;

struct LogicSelfValidationReport {
    std::size_t levelsChecked{};
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const noexcept {
        return errors.empty();
    }
};

// Runs small rule checks and regenerates every catalog entry twice. This is
// intentionally framework-free so it can be called from a unit test, a debug
// startup check, or a standalone diagnostic executable.
[[nodiscard]] LogicSelfValidationReport runBoardLogicSelfValidation();

} // namespace LiberateTheSheep::Game
