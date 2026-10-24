#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <string>

namespace LiberateTheSheep::UI {

struct MainMenuText {
    std::string playButton = "PLAY";
};

class MainMenuView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;

    MainMenuView(
        MainMenuText text,
        Action levelSelect,
        PandaUI::TextureHandle sheepArtwork
    );
};

} // namespace LiberateTheSheep::UI
