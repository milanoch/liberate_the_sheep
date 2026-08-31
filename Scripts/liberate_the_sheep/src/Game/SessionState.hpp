#pragma once

namespace LiberateTheSheep::Game {

// Process-local handoff between controllers on either side of WorldAPI::load().
// The script library remains loaded while worlds change, so this state survives
// destruction of the menu and game script instances.
class SessionState final {
public:
    static constexpr int kNoLevelRequested = 0;
    static constexpr int kFirstLevel = 1;
    static constexpr int kLastLevel = 9;

    static void requestLevel(int levelNumber) noexcept;
    [[nodiscard]] static int peekRequestedLevel() noexcept;
    [[nodiscard]] static int consumeRequestedLevel() noexcept;
    static void clear() noexcept;
};

} // namespace LiberateTheSheep::Game
