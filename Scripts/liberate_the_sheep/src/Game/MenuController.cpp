#include "Game/MenuController.hpp"

#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "UI/LevelSelectView.hpp"
#include "UI/MainMenuView.hpp"
#include "UI/SettingsView.hpp"

#include <Bamboo/Assets/TextureAPI.hpp>
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
        result.demoBadge = text(TextKey::DemoEyebrow, language);
        result.title = text(TextKey::GameTitle, language);
        result.subtitle = text(TextKey::MainMenuDescription, language);
        result.playButton = text(TextKey::Play, language);
        result.continueButton = text(TextKey::Continue, language);
        result.levelSelectButton = text(TextKey::SelectLevel, language);
        result.settingsTooltip = text(TextKey::Settings, language);
        result.footer = text(TextKey::MainMenuHint, language);
        return result;
    }

    [[nodiscard]] UI::SettingsText settingsText(Language language) {
        const bool russian = language == Language::Russian;

        UI::SettingsText result;
        result.title = text(TextKey::SettingsTitle, language);
        result.subtitle = russian ? "Язык, музыка и прогресс игры."
                                  : "Language, music and game progress.";
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

    if (settingsGearTexture.isValid()) {
        const Bamboo::TextureAPI::TexturePixelsRGBA8 pixels =
            Bamboo::TextureAPI::readPixelsRGBA8(settingsGearTexture);
        const auto maxDimension =
            static_cast<uint32_t>(std::numeric_limits<uint16_t>::max());
        const uint64_t expectedBytes = static_cast<uint64_t>(pixels.width) *
                                       static_cast<uint64_t>(pixels.height) * 4u;
        if (!pixels) {
            LOG_WARN(
                "MenuController: failed to read settings gear texture asset %u",
                settingsGearTexture.id
            );
        } else if (pixels.width > maxDimension || pixels.height > maxDimension ||
                   expectedBytes != pixels.pixels.size()) {
            LOG_WARN(
                "MenuController: settings gear texture asset %u has unsupported pixel data",
                settingsGearTexture.id
            );
        } else {
            settingsGearUiTexture_ = Bamboo::UI::createTextureRGBA8(
                pixels.pixels.data(),
                static_cast<uint16_t>(pixels.width),
                static_cast<uint16_t>(pixels.height)
            );
            if (!settingsGearUiTexture_) {
                LOG_WARN("MenuController: failed to create PandaUI settings gear texture");
            }
        }
    }

    diorama_.start();
    showMainMenu();
}

void MenuController::update(float deltaTime) {
    diorama_.update(deltaTime);
}

void MenuController::shutdown() {
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
}

void MenuController::showMainMenu() {
    if (!window_.isValid()) {
        return;
    }

    diorama_.setRunning(true);
    rootView_ = std::make_shared<UI::MainMenuView>(
        mainMenuText(settings_.language()),
        [this]() {
            loadLevel(progress_.progress().highestUnlocked);
        },
        [this]() {
            diorama_.setRunning(false);
            defer([this]() { showLevelSelect(); });
        },
        [this]() {
            diorama_.setRunning(false);
            defer([this]() { showSettings(); });
        },
        true,
        settingsGearUiTexture_
    );
    window_.setRootView(rootView_);
}

void MenuController::showLevelSelect() {
    if (!window_.isValid()) {
        return;
    }

    diorama_.setRunning(false);
    rootView_ = std::make_shared<UI::LevelSelectView>(
        levelSelectText(settings_.language()),
        progress_.progress().highestUnlocked,
        progress_.progress().bestHearts,
        [this](int levelNumber) { loadLevel(levelNumber); },
        [this]() { defer([this]() { showMainMenu(); }); }
    );
    window_.setRootView(rootView_);
}

void MenuController::showSettings() {
    if (!window_.isValid()) {
        return;
    }

    diorama_.setRunning(false);
    const Language language = settings_.language();
    rootView_ = std::make_shared<UI::SettingsView>(
        settingsText(language),
        menuLanguage(language),
        settings_.musicEnabled(),
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
        [this]() {
            progress_.reset();
            if (!progress_.save()) {
                LOG_WARN("MenuController: failed to save reset progress");
            }
        },
        [this]() { defer([this]() { showMainMenu(); }); }
    );
    window_.setRootView(rootView_);
}

void MenuController::loadLevel(int levelNumber) {
    if (!progress_.isUnlocked(levelNumber)) {
        LOG_WARN("MenuController: level %d is locked", levelNumber);
        return;
    }
    if (!gameWorld.isValid()) {
        LOG_ERROR("MenuController: gameWorld is not configured in menu.world");
        return;
    }

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
