#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace LiberateTheSheep::Game {

struct ProgressData final {
    int highestUnlocked{1};
    std::array<int, 9> bestHearts{};
};

class ProgressStore final {
public:
    static constexpr int kFirstLevel = 1;
    static constexpr int kLastLevel = 9;
    static constexpr std::size_t kLevelCount = 9;
    static constexpr int kMaximumHearts = 3;

    // File format and campaign revisions are deliberately independent. Changing
    // the generated campaign invalidates progress without requiring a new parser.
    static constexpr std::uint32_t kFileVersion = 1;
    static constexpr std::uint32_t kCampaignVersion = 1;

    ProgressStore() noexcept = default;

    // Returns true only when a current, fully valid save was loaded. On every
    // failure the store contains a safe new-campaign default instead.
    [[nodiscard]] bool load() noexcept;
    [[nodiscard]] bool save() const noexcept;

    void reset() noexcept;

    [[nodiscard]] const ProgressData &progress() const noexcept;
    [[nodiscard]] bool isUnlocked(int levelNumber) const noexcept;
    [[nodiscard]] int bestHearts(int levelNumber) const noexcept;

    // Records a completed, already-unlocked level and unlocks its successor.
    // Returns true only if the stored progress changed.
    [[nodiscard]] bool recordCompletion(int levelNumber, int remainingHearts) noexcept;

private:
    ProgressData m_progress;
};

} // namespace LiberateTheSheep::Game
