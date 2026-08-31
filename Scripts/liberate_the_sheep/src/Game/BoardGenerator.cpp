#include "Game/BoardGenerator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace LiberateTheSheep::Game {
namespace {

    constexpr std::array<Direction, 4> kDirections{
        Direction::Up,
        Direction::Right,
        Direction::Down,
        Direction::Left,
    };

    class StableRandom {
    public:
        explicit StableRandom(std::uint64_t seed)
            : state_(seed) {}

        [[nodiscard]] std::uint64_t next() noexcept {
            state_ += 0x9e3779b97f4a7c15ULL;
            std::uint64_t value = state_;
            value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31u);
        }

        [[nodiscard]] std::uint64_t bounded(std::uint64_t upperExclusive) noexcept {
            if (upperExclusive <= 1) {
                return 0;
            }

            // Rejection sampling avoids modulo bias and is identical across standard
            // library implementations.
            const std::uint64_t threshold = (0ULL - upperExclusive) % upperExclusive;
            while (true) {
                const std::uint64_t value = next();
                if (value >= threshold) {
                    return value % upperExclusive;
                }
            }
        }

    private:
        std::uint64_t state_{};
    };

    [[nodiscard]] std::uint64_t attemptSeed(std::uint64_t baseSeed, int attempt) noexcept {
        StableRandom mixer(
            baseSeed ^ (0xd1b54a32d192ed03ULL * static_cast<std::uint64_t>(attempt + 1))
        );
        return mixer.next();
    }

    [[nodiscard]] bool cellIsAheadOf(const Cell cell, const Sheep &sheep) noexcept {
        const Cell head = sheep.head();
        switch (sheep.direction) {
            case Direction::Up:
                return cell.x == head.x && cell.y > head.y;
            case Direction::Right:
                return cell.y == head.y && cell.x > head.x;
            case Direction::Down:
                return cell.x == head.x && cell.y < head.y;
            case Direction::Left:
                return cell.y == head.y && cell.x < head.x;
        }
        return false;
    }

    [[nodiscard]] int rayLengthBeyondHead(const BoardModel &board, const Sheep &sheep) noexcept {
        const Cell head = sheep.head();
        switch (sheep.direction) {
            case Direction::Up:
                return board.height() - head.y - 1;
            case Direction::Right:
                return board.width() - head.x - 1;
            case Direction::Down:
                return head.y;
            case Direction::Left:
                return head.x;
        }
        return 0;
    }

    [[nodiscard]] int occupiedNeighbours(const BoardModel &board, const Sheep &candidate) noexcept {
        constexpr std::array<Cell, 4> kNeighbours{{{0, 1}, {1, 0}, {0, -1}, {-1, 0}}};
        const auto [tail, head] = candidate.occupiedCells();
        int result = 0;
        for (const Cell occupied : {tail, head}) {
            for (const Cell neighbour : kNeighbours) {
                const Cell adjacent{occupied.x + neighbour.x, occupied.y + neighbour.y};
                if (board.isOccupied(adjacent)) {
                    ++result;
                }
            }
        }
        return result;
    }

    struct CandidatePlacement {
        Sheep sheep;
        int score{};
        std::uint64_t tieBreaker{};
    };

    [[nodiscard]] std::vector<CandidatePlacement> enumerateCandidates(
        const BoardModel &board, SheepId id, DirectionMask directions, StableRandom &random
    ) {
        std::vector<CandidatePlacement> candidates;
        for (const Direction direction : kDirections) {
            if (!containsDirection(directions, direction)) {
                continue;
            }

            for (int y = 0; y < board.height(); ++y) {
                for (int x = 0; x < board.width(); ++x) {
                    const Sheep candidate{id, {x, y}, direction};
                    if (!board.inspectPlacement(candidate).valid()) {
                        continue;
                    }
                    if (!board.inspectRay(candidate.head(), candidate.direction).canEscape()) {
                        continue;
                    }

                    int blockedSheep = 0;
                    int newlyBlockedSheep = 0;
                    const auto [tail, head] = candidate.occupiedCells();
                    for (const Sheep &existing : board.sheep()) {
                        if (!cellIsAheadOf(tail, existing) && !cellIsAheadOf(head, existing)) {
                            continue;
                        }
                        ++blockedSheep;
                        if (board.inspectMove(existing.id).canEscape()) {
                            ++newlyBlockedSheep;
                        }
                    }

                    // Dependencies dominate the choice. Forward space makes it more
                    // likely that a later insertion can block this sheep in turn.
                    const int score = newlyBlockedSheep * 500 + blockedSheep * 90 +
                                      rayLengthBeyondHead(board, candidate) * 14 +
                                      occupiedNeighbours(board, candidate) * 5;
                    candidates.push_back({candidate, score, random.next()});
                }
            }
        }

        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const CandidatePlacement &left, const CandidatePlacement &right) {
                if (left.score != right.score) {
                    return left.score > right.score;
                }
                return left.tieBreaker < right.tieBreaker;
            }
        );
        return candidates;
    }

    [[nodiscard]] int distanceOutsideRange(std::size_t value, int minimum, int maximum) noexcept {
        const std::size_t minValue = static_cast<std::size_t>(std::max(0, minimum));
        const std::size_t maxValue = static_cast<std::size_t>(std::max(0, maximum));
        if (value < minValue) {
            return static_cast<int>(minValue - value);
        }
        if (value > maxValue) {
            return static_cast<int>(value - maxValue);
        }
        return 0;
    }

    [[nodiscard]] bool meetsDifficultyTargets(
        const GenerationConfig &config, const SolvabilityReport &analysis
    ) noexcept {
        return distanceOutsideRange(
                   analysis.initialEscapableCount,
                   config.minInitialEscapable,
                   config.maxInitialEscapable
               ) == 0 &&
               distanceOutsideRange(
                   analysis.solutionWaveCount, config.minSolutionWaves, config.maxSolutionWaves
               ) == 0;
    }

    [[nodiscard]] int
    candidateQuality(const GenerationConfig &config, const SolvabilityReport &analysis) noexcept {
        const int initialPenalty = distanceOutsideRange(
            analysis.initialEscapableCount, config.minInitialEscapable, config.maxInitialEscapable
        );
        const int wavePenalty = distanceOutsideRange(
            analysis.solutionWaveCount, config.minSolutionWaves, config.maxSolutionWaves
        );

        // A candidate inside both requested ranges always beats one outside them.
        // Within the ranges, more dependency waves and fewer initial choices win.
        return 1'000'000 - initialPenalty * 50'000 - wavePenalty * 50'000 +
               static_cast<int>(analysis.solutionWaveCount) * 1'000 -
               static_cast<int>(analysis.initialEscapableCount) * 25 -
               static_cast<int>(analysis.widestSolutionWave) * 2;
    }

    struct CandidateBoard {
        BoardModel board;
        std::vector<SheepId> constructionOrder;
        std::vector<SheepId> guaranteedSolution;
        SolvabilityReport analysis;
        int attempt{};
        std::uint64_t seed{};
        int quality{};

        CandidateBoard(int width, int height)
            : board(width, height) {}
    };

    [[nodiscard]] CandidateBoard
    buildCandidate(const GenerationConfig &config, int targetCount, int attempt) {
        CandidateBoard candidate(config.width, config.height);
        candidate.attempt = attempt;
        candidate.seed = attemptSeed(config.seed, attempt);
        StableRandom random(candidate.seed);

        for (int index = 0; index < targetCount; ++index) {
            const SheepId nextId = static_cast<SheepId>(index + 1);
            std::vector<CandidatePlacement> placements =
                enumerateCandidates(candidate.board, nextId, config.allowedDirections, random);
            if (placements.empty()) {
                break;
            }

            const std::size_t pool = std::min(
                placements.size(), static_cast<std::size_t>(std::max(1, config.candidateChoicePool))
            );
            // Strong candidates remain more likely, while choosing across the pool
            // gives retries meaningfully different layouts.
            const std::uint64_t triangularCount = pool * (pool + 1) / 2;
            std::uint64_t pick = random.bounded(triangularCount);
            std::size_t selected = 0;
            for (; selected < pool; ++selected) {
                const std::uint64_t weight = pool - selected;
                if (pick < weight) {
                    break;
                }
                pick -= weight;
            }
            selected = std::min(selected, pool - 1);

            const Sheep sheep = placements[selected].sheep;
            if (!candidate.board.addSheep(sheep)) {
                break;
            }
            candidate.constructionOrder.push_back(sheep.id);
        }

        candidate.guaranteedSolution.assign(
            candidate.constructionOrder.rbegin(), candidate.constructionOrder.rend()
        );
        candidate.analysis = candidate.board.analyzeSolvability();
        candidate.quality = candidateQuality(config, candidate.analysis);
        return candidate;
    }

    [[nodiscard]] GenerationMetrics makeMetrics(
        const GenerationConfig &config,
        int targetCount,
        const CandidateBoard &candidate,
        int attemptsEvaluated
    ) {
        GenerationMetrics metrics;
        metrics.targetSheepCount = targetCount;
        metrics.generatedSheepCount = static_cast<int>(candidate.board.sheepCount());
        metrics.targetDensity = config.targetDensity;
        metrics.achievedDensity = candidate.board.density();
        metrics.initialEscapableCount = candidate.analysis.initialEscapableCount;
        metrics.solutionWaveCount = candidate.analysis.solutionWaveCount;
        metrics.widestSolutionWave = candidate.analysis.widestSolutionWave;
        metrics.meetsDifficultyTargets = meetsDifficultyTargets(config, candidate.analysis);
        metrics.qualityScore = candidate.quality;
        metrics.attemptsEvaluated = attemptsEvaluated;
        metrics.selectedAttempt = candidate.attempt;
        metrics.selectedAttemptSeed = candidate.seed;
        metrics.layoutFingerprint = candidate.board.layoutFingerprint();
        return metrics;
    }

    [[nodiscard]] std::string joinErrors(const std::vector<std::string> &errors) {
        std::ostringstream stream;
        for (std::size_t index = 0; index < errors.size(); ++index) {
            if (index != 0) {
                stream << "; ";
            }
            stream << errors[index];
        }
        return stream.str();
    }

} // namespace

GenerationResult BoardGenerator::generate(const GenerationConfig &config) {
    GenerationResult result;
    const std::vector<std::string> configErrors = validateConfig(config);
    if (!configErrors.empty()) {
        result.status = GenerationStatus::InvalidConfiguration;
        result.error = joinErrors(configErrors);
        return result;
    }

    const int targetCount = targetSheepCount(config);
    std::optional<CandidateBoard> bestComplete;
    std::optional<CandidateBoard> bestPartial;

    for (int attempt = 0; attempt < config.maxAttempts; ++attempt) {
        CandidateBoard candidate = buildCandidate(config, targetCount, attempt);
        if (!bestPartial || candidate.board.sheepCount() > bestPartial->board.sheepCount() ||
            (candidate.board.sheepCount() == bestPartial->board.sheepCount() &&
             candidate.quality > bestPartial->quality)) {
            bestPartial = candidate;
        }

        if (static_cast<int>(candidate.board.sheepCount()) != targetCount ||
            !candidate.analysis.solvable || !candidate.board.validateStructure().valid()) {
            continue;
        }

        if (!bestComplete || candidate.quality > bestComplete->quality) {
            bestComplete = std::move(candidate);
        }
    }

    const CandidateBoard *selected = bestComplete ? &*bestComplete : &*bestPartial;
    result.board = selected->board;
    result.constructionOrder = selected->constructionOrder;
    result.guaranteedSolution = selected->guaranteedSolution;
    result.metrics = makeMetrics(config, targetCount, *selected, config.maxAttempts);

    if (!bestComplete) {
        result.status = GenerationStatus::TargetDensityUnreachable;
        result.error = "Generator could not reach the requested sheep count after all attempts";
        return result;
    }

    result.status = GenerationStatus::Success;
    const GenerationValidationReport validation = validateResult(config, result);
    if (!validation.valid()) {
        result.status = GenerationStatus::ValidationFailed;
        result.error = joinErrors(validation.errors);
    }
    return result;
}

int BoardGenerator::targetSheepCount(const GenerationConfig &config) noexcept {
    if (config.width <= 0 || config.height <= 0 || !std::isfinite(config.targetDensity) ||
        config.targetDensity <= 0.0 || config.targetDensity > 1.0) {
        return 0;
    }

    const std::int64_t cells =
        static_cast<std::int64_t>(config.width) * static_cast<std::int64_t>(config.height);
    if (cells > std::numeric_limits<int>::max()) {
        return 0;
    }

    const int maximum = static_cast<int>(cells / 2);
    const int rounded =
        static_cast<int>(std::lround(static_cast<double>(cells) * config.targetDensity / 2.0));
    return std::clamp(rounded, 1, maximum);
}

std::vector<std::string> BoardGenerator::validateConfig(const GenerationConfig &config) {
    std::vector<std::string> errors;
    if (config.width <= 0 || config.height <= 0) {
        errors.emplace_back("width and height must be positive");
    } else if (config.width > std::numeric_limits<int>::max() / config.height) {
        errors.emplace_back("board cell count exceeds the supported integer range");
    }
    if (!std::isfinite(config.targetDensity) || config.targetDensity <= 0.0 ||
        config.targetDensity > 1.0) {
        errors.emplace_back("targetDensity must be in the range (0, 1]");
    }
    if ((config.allowedDirections & kAllDirections) == 0 ||
        (config.allowedDirections & static_cast<DirectionMask>(~kAllDirections)) != 0) {
        errors.emplace_back(
            "allowedDirections must contain only known directions and cannot be empty"
        );
    }
    if (config.maxAttempts <= 0) {
        errors.emplace_back("maxAttempts must be positive");
    }
    if (config.candidateChoicePool <= 0) {
        errors.emplace_back("candidateChoicePool must be positive");
    }
    if (config.minInitialEscapable < 0 || config.maxInitialEscapable < config.minInitialEscapable) {
        errors.emplace_back("initial escapable range is invalid");
    }
    if (config.minSolutionWaves < 0 || config.maxSolutionWaves < config.minSolutionWaves) {
        errors.emplace_back("solution wave range is invalid");
    }
    return errors;
}

GenerationValidationReport
BoardGenerator::validateResult(const GenerationConfig &config, const GenerationResult &result) {
    GenerationValidationReport report;
    const std::vector<std::string> configErrors = validateConfig(config);
    report.errors.insert(report.errors.end(), configErrors.begin(), configErrors.end());
    if (!result.board) {
        report.errors.emplace_back("generation result has no board");
        return report;
    }

    const BoardModel &board = *result.board;
    if (!board.validateStructure().valid()) {
        report.errors.emplace_back("generated board failed structural validation");
    }
    if (board.width() != config.width || board.height() != config.height) {
        report.errors.emplace_back("generated board dimensions differ from the configuration");
    }

    const int expectedCount = targetSheepCount(config);
    if (static_cast<int>(board.sheepCount()) != expectedCount) {
        report.errors.emplace_back("generated board did not reach the target sheep count");
    }
    if (result.constructionOrder.size() != board.sheepCount()) {
        report.errors.emplace_back("construction order size differs from the board sheep count");
    }
    if (result.guaranteedSolution.size() != board.sheepCount()) {
        report.errors.emplace_back("guaranteed solution size differs from the board sheep count");
    }

    std::unordered_set<SheepId> solutionIds;
    BoardModel solved = board;
    for (const SheepId id : result.guaranteedSolution) {
        if (!solutionIds.insert(id).second) {
            report.errors.emplace_back("guaranteed solution contains a duplicate sheep id");
            break;
        }
        if (solved.tryEscape(id) != EscapeResult::Escaped) {
            report.errors.emplace_back("guaranteed solution contains a blocked or unknown sheep");
            break;
        }
    }
    if (!solved.empty()) {
        report.errors.emplace_back("guaranteed solution does not clear the board");
    }

    const SolvabilityReport analysis = board.analyzeSolvability();
    if (!analysis.solvable) {
        report.errors.emplace_back("general solvability analysis reports a deadlock");
    }
    if (result.metrics.targetSheepCount != expectedCount ||
        result.metrics.generatedSheepCount != static_cast<int>(board.sheepCount())) {
        report.errors.emplace_back("generation count metrics are inconsistent");
    }
    if (result.metrics.initialEscapableCount != analysis.initialEscapableCount ||
        result.metrics.solutionWaveCount != analysis.solutionWaveCount ||
        result.metrics.widestSolutionWave != analysis.widestSolutionWave) {
        report.errors.emplace_back("difficulty metrics are inconsistent with board analysis");
    }
    if (result.metrics.layoutFingerprint != board.layoutFingerprint()) {
        report.errors.emplace_back("layout fingerprint metric is inconsistent");
    }

    return report;
}

} // namespace LiberateTheSheep::Game
