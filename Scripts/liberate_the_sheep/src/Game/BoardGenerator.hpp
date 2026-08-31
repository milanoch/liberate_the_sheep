#pragma once

#include "Game/BoardModel.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace LiberateTheSheep::Game {

struct GenerationConfig {
    int width{4};
    int height{4};
    double targetDensity{0.5};
    std::uint64_t seed{1};
    DirectionMask allowedDirections{kAllDirections};

    // Each attempt uses a deterministically derived stream. More attempts improve
    // packing and difficulty matching without changing a level between runs.
    int maxAttempts{96};
    int candidateChoicePool{5};

    // These ranges are preferences used to rank complete candidates. Density and
    // solvability remain hard requirements.
    int minInitialEscapable{1};
    int maxInitialEscapable{8};
    int minSolutionWaves{1};
    int maxSolutionWaves{64};
};

enum class GenerationStatus {
    Success,
    InvalidConfiguration,
    TargetDensityUnreachable,
    ValidationFailed,
};

struct GenerationMetrics {
    int targetSheepCount{};
    int generatedSheepCount{};
    double targetDensity{};
    double achievedDensity{};
    std::size_t initialEscapableCount{};
    std::size_t solutionWaveCount{};
    std::size_t widestSolutionWave{};
    bool meetsDifficultyTargets{false};
    int qualityScore{};
    int attemptsEvaluated{};
    int selectedAttempt{-1};
    std::uint64_t selectedAttemptSeed{};
    std::uint64_t layoutFingerprint{};
};

struct GenerationResult {
    GenerationStatus status{GenerationStatus::InvalidConfiguration};
    std::optional<BoardModel> board;
    GenerationMetrics metrics;
    // Insertion order is useful for diagnostics. Reversing it is the solution
    // guaranteed by the construction algorithm and stored explicitly below.
    std::vector<SheepId> constructionOrder;
    std::vector<SheepId> guaranteedSolution;
    std::string error;

    [[nodiscard]] bool succeeded() const noexcept {
        return status == GenerationStatus::Success && board.has_value();
    }
};

struct GenerationValidationReport {
    std::vector<std::string> errors;

    [[nodiscard]] bool valid() const noexcept {
        return errors.empty();
    }
};

class BoardGenerator {
public:
    [[nodiscard]] static GenerationResult generate(const GenerationConfig &config);

    [[nodiscard]] static int targetSheepCount(const GenerationConfig &config) noexcept;
    [[nodiscard]] static std::vector<std::string> validateConfig(const GenerationConfig &config);
    [[nodiscard]] static GenerationValidationReport
    validateResult(const GenerationConfig &config, const GenerationResult &result);
};

} // namespace LiberateTheSheep::Game
