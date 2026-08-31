#pragma once

#include "Game/MenuDiorama.hpp"
#include "Game/ProgressStore.hpp"
#include "Game/SettingsStore.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>

namespace LiberateTheSheep::Game {

class MenuController final : public Bamboo::Script {
public:
    Bamboo::WorldHandle gameWorld;
    Bamboo::TextureHandle settingsGearTexture;

    PANDA_FIELDS_BEGIN(MenuController)
    PANDA_FIELD(gameWorld)
    PANDA_FIELD(settingsGearTexture)
    PANDA_FIELDS_END

    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    PandaUI::Window window_;
    std::shared_ptr<PandaUI::View> rootView_;
    PandaUI::TextureHandle settingsGearUiTexture_;
    ProgressStore progress_;
    SettingsStore settings_;
    MenuDiorama diorama_;

    void showMainMenu();
    void showLevelSelect();
    void showSettings();
    void loadLevel(int levelNumber);
    void defer(std::function<void()> action);
};

REGISTER_SCRIPT(MenuController)

} // namespace LiberateTheSheep::Game
