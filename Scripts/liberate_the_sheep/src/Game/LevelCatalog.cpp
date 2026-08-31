#include "Game/LevelCatalog.hpp"

#include <array>
#include <unordered_set>

namespace LiberateTheSheep::Game {
namespace {

    constexpr DirectionMask kVerticalDirections =
        directionBit(Direction::Up) | directionBit(Direction::Down);

    constexpr std::array<LevelDefinition, 9> kLevels{{
        // level, size, density, seed, directions, attempts, pool,
        // preferred initial moves, preferred dependency waves
        {1, {4, 4, 0.40, 0x6c74735f00000001ULL, directionBit(Direction::Up), 64, 4, 1, 2, 2, 4}},
        {2, {4, 4, 0.60, 0x6c74735f00000002ULL, kVerticalDirections, 72, 5, 1, 3, 2, 6}},
        {3, {4, 4, 0.75, 0x6c74735f00000003ULL, kAllDirections, 80, 5, 1, 3, 3, 8}},

        {4, {4, 6, 0.55, 0x6c74735f00000004ULL, kAllDirections, 80, 5, 1, 3, 3, 10}},
        {5, {4, 6, 0.70, 0x6c74735f00000005ULL, kAllDirections, 96, 6, 1, 3, 4, 12}},
        {6, {4, 6, 0.85, 0x6c74735f00000006ULL, kAllDirections, 112, 6, 1, 3, 4, 14}},

        {7, {8, 12, 0.55, 0x6c74735f00000007ULL, kAllDirections, 64, 7, 1, 6, 5, 24}},
        {8, {8, 12, 0.70, 0x6c74735f00000008ULL, kAllDirections, 80, 3, 1, 6, 6, 32}},
        {9, {8, 12, 0.85, 0x6c74735f00000009ULL, kAllDirections, 128, 3, 1, 12, 18, 48}},
    }};

    void addError(LogicSelfValidationReport &report, std::string error) {
        report.errors.push_back(std::move(error));
    }

    void validateCoreRules(LogicSelfValidationReport &report) {
        BoardModel board(4, 4);
        const Sheep lower{1, {1, 0}, Direction::Up};
        const Sheep blocker{2, {0, 2}, Direction::Right};

        if (!board.addSheep(lower) || !board.addSheep(blocker)) {
            addError(report, "core rule test could not place its sheep");
            return;
        }
        if (board.addSheep({3, {1, 1}, Direction::Up})) {
            addError(report, "overlapping placement was incorrectly accepted");
        }

        const MoveCheck blocked = board.inspectMove(lower.id);
        if (blocked.state != MoveState::Blocked || blocked.blocker != blocker.id ||
            blocked.blockerCell != Cell{1, 2}) {
            addError(report, "nearest blocker detection failed");
        }
        if (board.tryEscape(lower.id) != EscapeResult::Blocked) {
            addError(report, "a blocked sheep escaped");
        }
        if (board.tryEscape(blocker.id) != EscapeResult::Escaped ||
            board.tryEscape(lower.id) != EscapeResult::Escaped || !board.empty()) {
            addError(report, "a valid two-step solution did not clear the board");
        }
        if (!board.validateStructure().valid()) {
            addError(report, "board structure became invalid after normal moves");
        }
    }

} // namespace

std::span<const LevelDefinition> levelCatalog() noexcept {
    return kLevels;
}

const LevelDefinition *findLevel(int levelNumber) noexcept {
    for (const LevelDefinition &level : kLevels) {
        if (level.number == levelNumber) {
            return &level;
        }
    }
    return nullptr;
}

LogicSelfValidationReport runBoardLogicSelfValidation() {
    LogicSelfValidationReport report;
    validateCoreRules(report);

    std::unordered_set<std::uint64_t> seeds;
    int expectedLevelNumber = 1;
    for (const LevelDefinition &level : kLevels) {
        ++report.levelsChecked;
        const std::string prefix = "level " + std::to_string(level.number) + ": ";
        if (level.number != expectedLevelNumber++) {
            addError(report, prefix + "level numbers must be contiguous and start at one");
        }
        if (!seeds.insert(level.generation.seed).second) {
            addError(report, prefix + "seed is duplicated");
        }

        const GenerationResult first = BoardGenerator::generate(level.generation);
        if (!first.succeeded()) {
            addError(report, prefix + "generation failed: " + first.error);
            continue;
        }

        const GenerationValidationReport validation =
            BoardGenerator::validateResult(level.generation, first);
        for (const std::string &error : validation.errors) {
            addError(report, prefix + error);
        }
        if (!first.metrics.meetsDifficultyTargets) {
            addError(report, prefix + "generated layout misses its preferred difficulty range");
        }

        const GenerationResult second = BoardGenerator::generate(level.generation);
        if (!second.succeeded() || !first.board || !second.board ||
            first.board->layoutFingerprint() != second.board->layoutFingerprint() ||
            first.guaranteedSolution != second.guaranteedSolution) {
            addError(report, prefix + "generation is not deterministic");
        }
    }
    return report;
}

} // namespace LiberateTheSheep::Game
