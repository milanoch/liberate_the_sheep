#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace LiberateTheSheep::UI {

struct MainMenuText {
    std::string demoBadge = "PANDA ENGINE DEMO";
    std::string title = "LIBERATE\nTHE SHEEP";
    std::string subtitle =
        "Find a clear path and help every two-cell sheep escape the flock.";
    std::string playButton = "PLAY";
    std::string continueButton = "CONTINUE";
    std::string levelSelectButton = "LEVEL SELECT";
    // ASCII fallback for fonts that do not contain the Unicode gear glyph.
    std::string settingsButton = "SET";
    std::string settingsTooltip = "Settings";
    std::string footer = "Tap a sheep only when the way ahead is clear.";
};

class MainMenuView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    MainMenuView(
        MainMenuText text,
        Action playOrContinue,
        Action levelSelect,
        Action settings,
        bool continueAvailable = false,
        PandaUI::TextureHandle settingsIcon = {}
    );

    // Transitional overload for worlds that have not wired Settings yet.
    MainMenuView(Action playOrContinue, Action levelSelect, bool continueAvailable = false);

    void setContinueAvailable(bool continueAvailable);

private:
    MainMenuText text_;
    std::shared_ptr<PandaUI::Button> primaryButton_;
};

} // namespace LiberateTheSheep::UI
