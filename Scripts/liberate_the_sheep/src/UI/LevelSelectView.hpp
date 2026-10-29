#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <span>
#include <string>

namespace LiberateTheSheep::UI {

struct LevelSelectText {
    std::string title = "LEVELS";
    // Retained for existing localized callers; tiles now show numbers and
    // native check/lock badges, without subtitles or heart counters.
    std::string subtitle = "39 pasture puzzles. Timed and impact bomb sheep await.";
    std::string levelPrefix = "LEVEL";
    std::string locked = "LOCKED";
    std::string ready = "READY";
    std::string hearts = "HEARTS";
    std::string backButton = "BACK";
};

class LevelSelectView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;
    using LevelAction = std::function<void(int levelNumber)>;

    LevelSelectView(
        LevelSelectText text,
        int highestUnlocked,
        std::span<const int> bestHearts,
        LevelAction playLevel,
        Action back,
        bool allLevelsUnlocked = false
    );

    // Transitional overload for callers that have not selected a language yet.
    LevelSelectView(
        int highestUnlocked,
        std::span<const int> bestHearts,
        LevelAction playLevel,
        Action back
    );
};

} // namespace LiberateTheSheep::UI
