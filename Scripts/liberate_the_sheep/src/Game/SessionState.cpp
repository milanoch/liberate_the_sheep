#include "Game/SessionState.hpp"

#include <algorithm>
#include <atomic>

namespace LiberateTheSheep::Game {
namespace {

    std::atomic<int> g_requestedLevel{SessionState::kNoLevelRequested};

    [[nodiscard]] int clampLevel(int levelNumber) noexcept {
        return std::clamp(levelNumber, SessionState::kFirstLevel, SessionState::kLastLevel);
    }

} // namespace

void SessionState::requestLevel(int levelNumber) noexcept {
    g_requestedLevel.store(clampLevel(levelNumber), std::memory_order_relaxed);
}

int SessionState::peekRequestedLevel() noexcept {
    return g_requestedLevel.load(std::memory_order_relaxed);
}

int SessionState::consumeRequestedLevel() noexcept {
    return g_requestedLevel.exchange(kNoLevelRequested, std::memory_order_relaxed);
}

void SessionState::clear() noexcept {
    g_requestedLevel.store(kNoLevelRequested, std::memory_order_relaxed);
}

} // namespace LiberateTheSheep::Game
