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
        {2, {4, 4, 0.60, 0x6c74735f00000002ULL, kVerticalDirections, 72, 5, 1, 3, 2, 6}},
        {3, {4, 4, 0.75, 0x6c74735f00000003ULL, kAllDirections, 80, 5, 1, 3, 3, 8}},

        {4, {4, 6, 0.55, 0x6c74735f00000004ULL, kAllDirections, 80, 5, 1, 3, 3, 10}},
        {5, {4, 6, 0.70, 0x6c74735f00000005ULL, kAllDirections, 96, 6, 1, 3, 4, 12}},
        {6, {4, 6, 0.85, 0x6c74735f00000006ULL, kAllDirections, 112, 6, 1, 3, 4, 14}},

        {7, {8, 12, 0.55, 0x6c74735f00000007ULL, kAllDirections, 64, 7, 1, 6, 5, 24}},
        {8, {8, 12, 0.70, 0x6c74735f00000008ULL, kAllDirections, 80, 3, 1, 6, 6, 32}},
        {9, {8, 12, 0.85, 0x6c74735f00000009ULL, kAllDirections, 128, 3, 1, 12, 18, 48}},

        // Finish the original-sized pasture before introducing the timer.
        {10, {8, 12, 0.86, 0x6c74735f0000000aULL, kAllDirections, 128, 3, 1, 12, 18, 48}},

        // A lighter flock and an early dependency wave give time to learn the
        // new sheep. Later levels tighten the deadline without changing speed.
        {11, {8, 12, 0.62, 0x6c74735f0000000bULL, kAllDirections, 80, 6, 1, 8, 6, 32}, 30.f, 2},
        {12, {8, 12, 0.66, 0x6c74735f0000000cULL, kAllDirections, 80, 5, 1, 8, 7, 36}, 28.f, 2},
        {13, {8, 12, 0.72, 0x6c74735f0000000dULL, kAllDirections, 96, 5, 1, 8, 8, 40}, 28.f, 3},
        {14, {8, 12, 0.78, 0x6c74735f0000000eULL, kAllDirections, 112, 4, 1, 10, 10, 44}, 27.f, 3},
        {15, {8, 12, 0.84, 0x6c74735f0000000fULL, kAllDirections, 128, 4, 1, 12, 12, 48}, 26.f, 3},

        // Keep the 2:3 portrait shape. A lower density offsets the longer trip
        // to the exit when the pasture first grows to 10 by 15 cells.
        {16, {10, 15, 0.62, 0x6c74735f00000010ULL, kAllDirections, 80, 6, 1, 10, 8, 48}, 32.f, 3},
        {17, {10, 15, 0.66, 0x6c74735f00000011ULL, kAllDirections, 80, 5, 1, 10, 10, 52}, 31.f, 3},
        {18, {10, 15, 0.70, 0x6c74735f00000012ULL, kAllDirections, 96, 5, 1, 10, 12, 56}, 30.f, 4},
        {19, {10, 15, 0.74, 0x6c74735f00000013ULL, kAllDirections, 96, 4, 1, 12, 14, 60}, 29.f, 4},
        {20, {10, 15, 0.78, 0x6c74735f00000014ULL, kAllDirections, 112, 4, 1, 12, 16, 64}, 28.f, 4},
        {21, {10, 15, 0.80, 0x6c74735f00000015ULL, kAllDirections, 112, 4, 1, 12, 18, 64}, 27.f, 4},
        {22, {10, 15, 0.82, 0x6c74735f00000016ULL, kAllDirections, 128, 3, 1, 14, 18, 68}, 27.f, 5},
        {23, {10, 15, 0.84, 0x6c74735f00000017ULL, kAllDirections, 128, 3, 1, 14, 20, 72}, 26.f, 5},
        {24, {10, 15, 0.86, 0x6c74735f00000018ULL, kAllDirections, 128, 3, 1, 14, 20, 72}, 25.f, 5},

        // Learn the black impact bomb without a simultaneous deadline. Three
        // charges permit two safe bumps before the third causes an explosion.
        {25, {10, 15, 0.62, 0x6c74735f00000019ULL, kAllDirections, 80, 6, 1, 10, 8, 48}, 0.f, 1, 3, 2},
        {26, {10, 15, 0.64, 0x6c74735f0000001aULL, kAllDirections, 80, 6, 1, 10, 8, 48}, 0.f, 1, 3, 2},
        {27, {10, 15, 0.66, 0x6c74735f0000001bULL, kAllDirections, 80, 5, 1, 10, 10, 52}, 0.f, 1, 3, 2},
        {28, {10, 15, 0.69, 0x6c74735f0000001cULL, kAllDirections, 96, 5, 1, 10, 10, 52}, 0.f, 1, 3, 3},
        {29, {10, 15, 0.72, 0x6c74735f0000001dULL, kAllDirections, 96, 5, 1, 10, 12, 56}, 0.f, 1, 3, 3},

        // A short density reset introduces each smaller allowance before the
        // flock becomes denser again. Existing movement and board sizes stay.
        {30, {10, 15, 0.64, 0x6c74735f0000001eULL, kAllDirections, 80, 6, 1, 10, 8, 48}, 0.f, 1, 2, 2},
        {31, {10, 15, 0.67, 0x6c74735f0000001fULL, kAllDirections, 96, 5, 1, 10, 10, 52}, 0.f, 1, 2, 3},
        {32, {10, 15, 0.70, 0x6c74735f00000020ULL, kAllDirections, 96, 5, 1, 10, 12, 56}, 0.f, 1, 2, 3},
        {33, {10, 15, 0.74, 0x6c74735f00000021ULL, kAllDirections, 96, 4, 1, 12, 14, 60}, 0.f, 1, 2, 4},
        {34, {10, 15, 0.77, 0x6c74735f00000022ULL, kAllDirections, 112, 4, 1, 12, 16, 64}, 0.f, 1, 2, 4},
        {35, {10, 15, 0.80, 0x6c74735f00000023ULL, kAllDirections, 112, 4, 1, 12, 18, 64}, 0.f, 1, 2, 4},

        // A single remaining charge makes the first direct collision fatal.
        {36, {10, 15, 0.70, 0x6c74735f00000024ULL, kAllDirections, 96, 5, 1, 10, 12, 56}, 0.f, 1, 1, 3},
        {37, {10, 15, 0.76, 0x6c74735f00000025ULL, kAllDirections, 112, 4, 1, 12, 14, 60}, 0.f, 1, 1, 4},
        {38, {10, 15, 0.82, 0x6c74735f00000026ULL, kAllDirections, 128, 3, 1, 14, 18, 68}, 0.f, 1, 1, 5},
        {39, {10, 15, 0.86, 0x6c74735f00000027ULL, kAllDirections, 128, 3, 1, 14, 20, 72}, 0.f, 1, 1, 5},
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
