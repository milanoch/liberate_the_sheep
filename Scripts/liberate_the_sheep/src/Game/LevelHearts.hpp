#pragma once

#include "Game/ProgressStore.hpp"

namespace LiberateTheSheep::Game {

// Per-level life rules, independent of the renderer and persistence. A blocked
// sheep still reacts in immortal mode; only its heart penalty is suppressed.
class LevelHearts final {
public:
    void reset(bool immortalModeEnabled) noexcept {
        immortal_ = immortalModeEnabled;
        remaining_ = ProgressStore::kMaximumHearts;
    }

    void blockedMove() noexcept {
        if (!immortal_ && remaining_ > 0) {
            --remaining_;
        }
    }

    [[nodiscard]] int remaining() const noexcept { return remaining_; }
    [[nodiscard]] bool isDepleted() const noexcept { return !immortal_ && remaining_ == 0; }
    [[nodiscard]] bool counterVisible() const noexcept { return !immortal_; }

private:
    int remaining_{ProgressStore::kMaximumHearts};
    bool immortal_{};
};

} // namespace LiberateTheSheep::Game
