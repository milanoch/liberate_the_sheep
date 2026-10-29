#pragma once

#include "Game/LevelCatalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace LiberateTheSheep::Game {

struct ProgressData final {
    int highestUnlocked{1};
    std::array<int, kCampaignLevelCount> bestHearts{};
    bool bombTutorialSeen{};
};

class ProgressStore final {
public:
    static constexpr int kFirstLevel = 1;
    static constexpr int kLastLevel = kCampaignLevelCount;
    static constexpr std::size_t kLevelCount = kCampaignLevelCount;
    static constexpr int kMaximumHearts = 3;

    // Appending levels preserves the campaign revision and old completions.
    // Format 2 stores its own level count, so shorter campaigns can migrate
    // without discarding their completions. The tutorial flag is legacy data.
    static constexpr std::uint32_t kFileVersion = 2;
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
    [[nodiscard]] bool markBombTutorialSeen() noexcept;

private:
    ProgressData m_progress;
};

} // namespace LiberateTheSheep::Game
