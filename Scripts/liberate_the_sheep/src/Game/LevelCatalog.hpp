#pragma once

#include "Game/BoardGenerator.hpp"

#include <span>
#include <string>
#include <vector>

namespace LiberateTheSheep::Game {

inline constexpr int kCampaignLevelCount = 39;

struct LevelDefinition {
    int number{};
    GenerationConfig generation;
    // Zero keeps the original untimed rules. Waves are one-based and describe
    // the preferred depth of the bomb sheep's blockers, not a fixed sheep ID.
    float bombSeconds{};
    int bombPreferredWave{1};
    // Impact bombs have no clock. Their counter decreases only when another
    // sheep hits them as the nearest blocker; zero disables this mechanic.
    int impactBombHits{};
    int impactBombPreferredWave{2};
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
