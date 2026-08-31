#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace LiberateTheSheep::UI {

enum class MenuLanguage {
    Russian,
    English,
};

struct SettingsText {
    std::string title = "SETTINGS";
    std::string subtitle = "Choose how the game looks and feels.";
    std::string languageTitle = "LANGUAGE";
    std::string languageDescription = "Menu and game text";
    std::string russianButton = "RU";
    std::string englishButton = "EN";
    std::string musicTitle = "MUSIC";
    std::string musicDescription = "The setting is ready for audio support.";
    std::string resetTitle = "RESET PROGRESS";
    std::string resetDescription = "Erase unlocked levels and collected hearts.";
    std::string resetButton = "RESET PROGRESS";
    std::string resetConfirmButton = "CONFIRM RESET";
    std::string resetConfirmHint = "Tap again to permanently reset your progress.";
    std::string backButton = "BACK";
};

class SettingsView final : public PandaUI::Panel {
public:
    using Action = std::function<void()>;
    using LanguageChangedAction = std::function<void(MenuLanguage language)>;
    using MusicChangedAction = std::function<void(bool enabled)>;

    SettingsView(
        SettingsText text,
        MenuLanguage language,
        bool musicEnabled,
        LanguageChangedAction languageChanged,
        MusicChangedAction musicChanged,
        Action resetProgress,
        Action back
    );

    void setLanguage(MenuLanguage language);
    void setMusicEnabled(bool enabled);

private:
    void handleReset();

    SettingsText text_;
    MenuLanguage language_ = MenuLanguage::English;
    bool resetArmed_ = false;
    Action resetProgress_;
    std::shared_ptr<PandaUI::Button> russianButton_;
    std::shared_ptr<PandaUI::Button> englishButton_;
    std::shared_ptr<PandaUI::Toggle> musicToggle_;
    std::shared_ptr<PandaUI::Button> resetButton_;
    std::shared_ptr<PandaUI::Label> resetHint_;
};

} // namespace LiberateTheSheep::UI
