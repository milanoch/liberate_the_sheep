#include "Game/LevelCatalog.hpp"

#include <array>
#include <unordered_set>

namespace LiberateTheSheep::Game {
namespace {

    constexpr DirectionMask kVerticalDirections =
        directionBit(Direction::Up) | directionBit(Direction::Down);

    constexpr std::array<LevelDefinition, kCampaignLevelCount> kLevels{{
        // level, size, density, seed, directions, attempts, pool,
        // preferred initial moves, preferred dependency waves
        {1, {4, 4, 0.40, 0x6c74735f00000001ULL, directionBit(Direction::Up), 64, 4, 1, 2, 2, 4}},
        {2, {4, 4, 0.65, 0x6c74735f00000002ULL, kVerticalDirections, 72, 5, 1, 2, 3, 6, true}},
        {3, {4, 4, 0.85, 0x6c74735f00000003ULL, kAllDirections, 80, 5, 1, 2, 4, 8, true}},

        {4, {4, 6, 0.65, 0x6c74735f00000004ULL, kAllDirections, 80, 5, 1, 2, 5, 10, true}},
        {5, {4, 6, 0.80, 0x6c74735f00000005ULL, kAllDirections, 96, 5, 1, 2, 6, 12, true}},
        {6, {4, 6, 0.90, 0x6c74735f00000006ULL, kAllDirections, 112, 5, 1, 2, 7, 14, true}},

        {7, {8, 12, 0.70, 0x6c74735f00000007ULL, kAllDirections, 80, 6, 1, 3, 10, 32, true}},
        {8, {8, 12, 0.82, 0x6c74735f00000008ULL, kAllDirections, 96, 5, 1, 3, 13, 40, true}},
        {9, {8, 12, 0.88, 0x6c74735f00000009ULL, kAllDirections, 128, 5, 1, 2, 17, 48, true}},

        // 10-15: timed red bomb only, with progressively deeper blockers.
        {10, {8, 12, 0.78, 0x6c74735f0000000aULL, kAllDirections, 96, 6, 1, 3, 12, 40, true}, 26.f, 4},
        {11, {8, 12, 0.805, 0x6c74735f0000000bULL, kAllDirections, 96, 6, 1, 3, 14, 42, true}, 25.5f, 5},
        {12, {8, 12, 0.83, 0x6c74735f0000000cULL, kAllDirections, 112, 5, 1, 2, 16, 44, true}, 25.f, 6},
        {13, {8, 12, 0.85, 0x6c74735f0000000dULL, kAllDirections, 112, 5, 1, 2, 18, 46, true}, 24.5f, 7},
        {14, {8, 12, 0.875, 0x6c74735f0000000eULL, kAllDirections, 128, 5, 1, 2, 20, 48, true}, 24.f, 8},
        {15, {8, 12, 0.90, 0x6c74735f0000000fULL, kAllDirections, 128, 5, 1, 2, 22, 48, true}, 23.5f, 9},

        // 16-20: impact black bomb only. The first field teaches the mechanic
        // without a clock; subsequent fields tighten its collision allowance.
        {16, {10, 15, 0.62, 0x6c74735f00000010ULL, kAllDirections, 80, 6, 1, 10, 8, 48}, 0.f, 1, 3, 2, 1},
        {17, {10, 15, 0.78, 0x6c74735f00000011ULL, kAllDirections, 112, 6, 1, 3, 17, 58, true}, 0.f, 1, 3, 4, 1},
        {18, {10, 15, 0.82, 0x6c74735f00000012ULL, kAllDirections, 112, 5, 1, 3, 19, 60, true}, 0.f, 1, 2, 6, 1},
        {19, {10, 15, 0.86, 0x6c74735f00000013ULL, kAllDirections, 112, 5, 1, 2, 22, 64, true}, 0.f, 1, 2, 8, 1},
        {20, {10, 15, 0.89, 0x6c74735f00000014ULL, kAllDirections, 128, 6, 1, 2, 25, 68, true}, 0.f, 1, 1, 10, 1},

        // 21-25: both bomb mechanics on every field.
        {21, {10, 15, 0.80, 0x6c74735f00000015ULL, kAllDirections, 112, 6, 1, 3, 19, 60, true}, 26.f, 7, 3, 4, 1},
        {22, {10, 15, 0.81, 0x6c74735f00000016ULL, kAllDirections, 112, 6, 1, 3, 20, 62, true}, 25.5f, 8, 3, 5, 1},
        {23, {10, 15, 0.82, 0x6c74735f00000017ULL, kAllDirections, 112, 5, 1, 2, 21, 64, true}, 25.f, 9, 3, 6, 1},
        {24, {10, 15, 0.83, 0x6c74735f00000018ULL, kAllDirections, 112, 5, 1, 2, 22, 64, true}, 24.5f, 9, 3, 6, 1},
        {25, {10, 15, 0.84, 0x6c74735f00000019ULL, kAllDirections, 112, 5, 1, 2, 23, 66, true}, 24.f, 10, 3, 7, 1},

        // 26-30: a red deadline and two independent black counters.
        {26, {10, 15, 0.845, 0x6c74735f0000001aULL, kAllDirections, 112, 6, 1, 2, 24, 66, true}, 23.75f, 10, 2, 7, 2},
        {27, {10, 15, 0.85, 0x6c74735f0000001bULL, kAllDirections, 112, 6, 1, 2, 25, 68, true}, 23.5f, 11, 2, 8, 2},
        {28, {10, 15, 0.855, 0x6c74735f0000001cULL, kAllDirections, 128, 6, 1, 2, 26, 68, true}, 23.25f, 11, 2, 8, 2},
        {29, {10, 15, 0.86, 0x6c74735f0000001dULL, kAllDirections, 128, 6, 1, 2, 27, 70, true}, 23.f, 12, 2, 9, 2},
        {30, {10, 15, 0.865, 0x6c74735f0000001eULL, kAllDirections, 128, 6, 1, 2, 28, 70, true}, 22.75f, 12, 2, 9, 2},

        // 31-40: either black bomb detonates on its first direct hit.
        {31, {10, 15, 0.87, 0x6c74735f0000001fULL, kAllDirections, 128, 6, 1, 2, 29, 72, true}, 22.5f, 12, 1, 10, 2},
        {32, {10, 15, 0.875, 0x6c74735f00000020ULL, kAllDirections, 128, 6, 1, 2, 30, 72, true}, 22.25f, 13, 1, 10, 2},
        {33, {10, 15, 0.88, 0x6c74735f00000021ULL, kAllDirections, 128, 6, 1, 2, 31, 72, true}, 22.f, 13, 1, 11, 2},
        {34, {10, 15, 0.885, 0x6c74735f00000022ULL, kAllDirections, 128, 6, 1, 2, 32, 74, true}, 21.75f, 14, 1, 11, 2},
        {35, {10, 15, 0.89, 0x6c74735f00000023ULL, kAllDirections, 128, 6, 1, 2, 33, 74, true}, 21.5f, 14, 1, 12, 2},
        {36, {10, 15, 0.892, 0x6c74735f00000024ULL, kAllDirections, 128, 6, 1, 2, 34, 74, true}, 21.25f, 15, 1, 12, 2},
        {37, {10, 15, 0.894, 0x6c74735f00000025ULL, kAllDirections, 128, 6, 1, 2, 35, 74, true}, 21.f, 15, 1, 13, 2},
        {38, {10, 15, 0.896, 0x6c74735f00000026ULL, kAllDirections, 128, 6, 1, 2, 36, 74, true}, 20.75f, 16, 1, 13, 2},
        {39, {10, 15, 0.898, 0x6c74735f00000027ULL, kAllDirections, 128, 6, 1, 2, 37, 74, true}, 20.5f, 16, 1, 14, 2},
        {40, {10, 15, 0.90, 0x6c74735f00000028ULL, kAllDirections, 128, 6, 1, 2, 38, 74, true}, 20.25f, 17, 1, 14, 2},
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
