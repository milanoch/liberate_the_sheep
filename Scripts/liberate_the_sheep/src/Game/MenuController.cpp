#include "Game/MenuController.hpp"

#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "UI/LevelSelectView.hpp"
#include "UI/MainMenuView.hpp"
#include "UI/SettingsView.hpp"

#include <Bamboo/Assets/TextureAPI.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/UI/TextureAPI.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>

namespace LiberateTheSheep::Game {
namespace {

    [[nodiscard]] std::string text(TextKey key, Language language) {
        return std::string(localizedText(key, language));
    }

    [[nodiscard]] UI::MainMenuText mainMenuText(Language language) {
        UI::MainMenuText result;
        result.playButton = text(TextKey::Play, language);
        return result;
    }

    PandaUI::TextureHandle menuTexture(Bamboo::TextureHandle source, const char *name) {
        if (!source.isValid()) {
            LOG_WARN("MenuController: %s texture is not configured in menu.world", name);
            return {};
        }
        const auto pixels = Bamboo::TextureAPI::readPixelsRGBA8(source);
        const auto maxDimension = static_cast<uint32_t>(std::numeric_limits<uint16_t>::max());
        const uint64_t expectedBytes = static_cast<uint64_t>(pixels.width) * pixels.height * 4u;
        if (!pixels || pixels.width > maxDimension || pixels.height > maxDimension ||
            expectedBytes != pixels.pixels.size()) {
            LOG_WARN("MenuController: cannot read %s texture asset %u", name, source.id);
            return {};
        }
        auto result = Bamboo::UI::createTextureRGBA8(
            pixels.pixels.data(), static_cast<uint16_t>(pixels.width),
            static_cast<uint16_t>(pixels.height)
        );
        if (!result) {
            LOG_WARN("MenuController: cannot create %s UI texture", name);
        }
        return result;
    }

    [[nodiscard]] UI::SettingsText settingsText(Language language) {
        const bool russian = language == Language::Russian;

        UI::SettingsText result;
        result.title = text(TextKey::SettingsTitle, language);
        result.subtitle = russian ? "Язык, музыка и режим игры."
                                  : "Language, music and game mode.";
        result.languageTitle = text(TextKey::Language, language);
        result.languageDescription = russian ? "Язык меню и игры"
                                             : "Menu and game language";
        result.russianButton =
            std::string(localizedLanguageName(Language::Russian, language));
        result.englishButton =
            std::string(localizedLanguageName(Language::English, language));
        result.musicTitle = text(TextKey::Music, language);
        result.musicDescription =
            russian ? "Настройка сохранится для будущей поддержки звука."
                    : "This preference is saved for future audio support.";
        result.immortalModeTitle = text(TextKey::ImmortalMode, language);
        result.immortalModeDescription = text(TextKey::ImmortalModeDescription, language);
        result.cheatModeTitle = text(TextKey::CheatMode, language);
        result.cheatModeDescription = text(TextKey::CheatModeDescription, language);
        result.resetTitle = text(TextKey::ResetProgress, language);
        result.resetDescription = text(TextKey::ResetProgressDescription, language);
        result.resetButton = text(TextKey::ResetProgress, language);
        result.resetConfirmButton = text(TextKey::ConfirmReset, language);
        result.resetConfirmHint = text(TextKey::ResetProgressConfirmation, language);
        result.backButton = text(TextKey::Back, language);
        return result;
    }

    [[nodiscard]] UI::LevelSelectText levelSelectText(Language language) {
        UI::LevelSelectText result;
        result.title = text(TextKey::ChooseField, language);
        result.subtitle = text(TextKey::LevelSelectDescription, language);
        result.levelPrefix = text(TextKey::Level, language);
        result.locked = text(TextKey::Locked, language);
        result.ready = text(TextKey::Ready, language);
        result.hearts = text(TextKey::Hearts, language);
        result.backButton = text(TextKey::Back, language);
        return result;
    }

    [[nodiscard]] UI::MenuLanguage menuLanguage(Language language) noexcept {
        return language == Language::English ? UI::MenuLanguage::English
                                             : UI::MenuLanguage::Russian;
    }

    [[nodiscard]] Language gameLanguage(UI::MenuLanguage language) noexcept {
        return language == UI::MenuLanguage::English ? Language::English
                                                     : Language::Russian;
    }

} // namespace

void MenuController::start() {
    (void)progress_.load();
    (void)settings_.load();
    window_ = PandaUI::Window::main();
    if (!window_.isValid()) {
        LOG_ERROR("MenuController: main PandaUI window is unavailable");
        return;
    }

    settingsGearUiTexture_ = menuTexture(settingsGearTexture, "settings gear");
    // The source Sprite lives in the world, so its texture can be changed in
    // PandaEditor without hard-coding an asset ID or file path in the script.
    auto playArtwork = Bamboo::WorldAPI::findByTag("MenuPlayButtonArt");
    if (playArtwork.isValid()) {
        sheepPlayUiTexture_ = menuTexture(
            Bamboo::SpriteRendererComponentAPI::getTexture(playArtwork), "sheep play button"
        );
    } else {
        LOG_WARN("MenuController: MenuPlayButtonArt is missing from menu.world");
    }

    leavingMenu_ = false;
    currentPage_ = Page::Main;
    settingsReturnPage_ = Page::Main;
    rootView_ = std::make_shared<UI::MenuShellView>(settingsGearUiTexture_, [this]() {
        defer([this]() {
            if (currentPage_ == Page::Settings) {
                closeSettings();
            } else {
                showSettings();
            }
        });
    });
    window_.setRootView(rootView_);
    diorama_.start();
    showMainMenu();
}

void MenuController::update(float deltaTime) {
    diorama_.update(deltaTime);
}

void MenuController::shutdown() {
    leavingMenu_ = true;
    diorama_.setRunning(false);
    if (window_.isValid() && rootView_ &&
        window_.context().getRootView() == rootView_) {
        window_.setRootView(nullptr);
    }
    rootView_.reset();
    if (settingsGearUiTexture_) {
        Bamboo::UI::destroyTexture(settingsGearUiTexture_);
        settingsGearUiTexture_ = {};
    }
    if (sheepPlayUiTexture_) {
        Bamboo::UI::destroyTexture(sheepPlayUiTexture_);
        sheepPlayUiTexture_ = {};
    }
}

bool MenuController::canNavigate() const {
    return window_.isValid() && rootView_ && !leavingMenu_ && !rootView_->isTransitioning();
}

void MenuController::showMainMenu() {
    if (!canNavigate()) {
        return;
    }

    diorama_.setRunning(true);
    auto page = std::make_shared<UI::MainMenuView>(
        mainMenuText(settings_.language()),
        [this]() {
            defer([this]() { showLevelSelect(); });
        },
        sheepPlayUiTexture_
    );
    if (rootView_->showPage(std::move(page))) {
        currentPage_ = Page::Main;
        rootView_->setSettingsTooltip(text(TextKey::Settings, settings_.language()));
    }
}

void MenuController::showLevelSelect() {
    if (!canNavigate()) {
        return;
    }

    // Menu pages share the live scene; navigating between them must not
    // freeze the sheep, clouds, sun or the individual spawn timers.
    diorama_.setRunning(true);
    auto page = std::make_shared<UI::LevelSelectView>(
        levelSelectText(settings_.language()),
        progress_.progress().highestUnlocked,
        progress_.progress().bestHearts,
        [this](int levelNumber) { loadLevel(levelNumber); },
        [this]() { defer([this]() { showMainMenu(); }); },
        settings_.cheatModeEnabled()
    );
    if (rootView_->showPage(std::move(page))) {
        currentPage_ = Page::Levels;
        rootView_->setSettingsTooltip(text(TextKey::Settings, settings_.language()));
    }
}

void MenuController::showSettings() {
    if (!canNavigate()) {
        return;
    }

    diorama_.setRunning(true);
    const bool updatingLanguage = currentPage_ == Page::Settings;
    if (!updatingLanguage) {
        settingsReturnPage_ = currentPage_;
    }
    const Language language = settings_.language();
    auto page = std::make_shared<UI::SettingsView>(
        settingsText(language),
        menuLanguage(language),
        settings_.musicEnabled(),
        settings_.immortalModeEnabled(),
        settings_.cheatModeEnabled(),
        [this](UI::MenuLanguage selectedLanguage) {
            const bool changed = settings_.setLanguage(gameLanguage(selectedLanguage));
            if (changed && !settings_.save()) {
                LOG_WARN("MenuController: failed to save language setting");
            }
            if (changed) {
                defer([this]() { showSettings(); });
            }
        },
        [this](bool enabled) {
            if (settings_.setMusicEnabled(enabled) && !settings_.save()) {
                LOG_WARN("MenuController: failed to save music setting");
            }
        },
        [this](bool enabled) {
            if (settings_.setImmortalModeEnabled(enabled) && !settings_.save()) {
                LOG_WARN("MenuController: failed to save immortal mode setting");
            }
        },
        [this](bool enabled) {
            if (settings_.setCheatModeEnabled(enabled) && !settings_.save()) {
                LOG_WARN("MenuController: failed to save cheat mode setting");
            }
        },
        [this]() {
            progress_.reset();
            if (!progress_.save()) {
                LOG_WARN("MenuController: failed to save reset progress");
            }
        },
        [this]() { defer([this]() { closeSettings(); }); }
    );
    // Fade between localized pages too, so old and new text never swap abruptly.
    // The shell keeps the gear and diorama visible throughout the transition.
    if (rootView_->showPage(std::move(page))) {
        currentPage_ = Page::Settings;
        rootView_->setSettingsTooltip(text(TextKey::Settings, language));
    }
}

void MenuController::closeSettings() {
    if (settingsReturnPage_ == Page::Levels) {
        showLevelSelect();
    } else {
        showMainMenu();
    }
}

void MenuController::loadLevel(int levelNumber) {
    if (!canNavigate()) {
        return;
    }
    if (levelNumber < ProgressStore::kFirstLevel || levelNumber > ProgressStore::kLastLevel ||
        (!settings_.cheatModeEnabled() && !progress_.isUnlocked(levelNumber))) {
        LOG_WARN("MenuController: level %d is locked", levelNumber);
        return;
    }
    if (!gameWorld.isValid()) {
        LOG_ERROR("MenuController: gameWorld is not configured in menu.world");
        return;
    }

    leavingMenu_ = true;
    rootView_->setUserInteractionEnabled(false);
    diorama_.setRunning(false);
    SessionState::requestLevel(levelNumber);
    const Bamboo::WorldHandle targetWorld = gameWorld;
    defer([targetWorld]() { Bamboo::WorldAPI::load(targetWorld); });
}

void MenuController::defer(std::function<void()> action) {
    if (!action) {
        return;
    }
    if (window_.isValid()) {
        window_.context().postDeferred(std::move(action));
    } else {
        action();
    }
}

} // namespace LiberateTheSheep::Game
