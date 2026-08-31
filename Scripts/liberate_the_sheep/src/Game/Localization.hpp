#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace LiberateTheSheep::Game {

// Values are persisted by SettingsStore. Keep the explicit numeric values
// stable when extending the language list.
enum class Language : std::uint8_t {
    Russian = 0,
    English = 1,
};

enum class TextKey : std::size_t {
    DemoEyebrow,
    GameTitle,
    MainMenuDescription,
    Continue,
    Play,
    SelectLevel,
    MainMenuHint,

    ChooseField,
    LevelSelectDescription,
    Level,
    Locked,
    Ready,
    Hearts,
    Back,

    Settings,
    SettingsTitle,
    Close,
    Language,
    Russian,
    English,
    Music,
    MusicEnabled,
    MusicDisabled,
    ResetProgress,
    ResetProgressDescription,
    ResetProgressConfirmation,
    Cancel,
    ConfirmReset,
    ProgressReset,

    LevelStat,
    HeartsStat,
    SheepLeftStat,
    Hint,
    Restart,
    Menu,

    FieldCleared,
    VictoryTitle,
    VictoryMessage,
    NoHeartsLeft,
    DefeatTitle,
    DefeatMessage,
    NextLevel,
    Replay,

    Count,
};

[[nodiscard]] constexpr bool isValidLanguage(Language language) noexcept {
    return language == Language::Russian || language == Language::English;
}

[[nodiscard]] constexpr Language nextLanguage(Language language) noexcept {
    return language == Language::Russian ? Language::English : Language::Russian;
}

// ISO-style stable code suitable for diagnostics and future platform-language
// detection. Invalid values fall back to Russian, as does localizedText().
[[nodiscard]] std::string_view languageCode(Language language) noexcept;

// Returned views have static lifetime. Unknown keys or languages safely fall
// back to a visible Russian value instead of returning a dangling/empty view.
[[nodiscard]] std::string_view localizedText(TextKey key, Language language) noexcept;

// Returns the language name translated into displayLanguage.
[[nodiscard]] std::string_view localizedLanguageName(
    Language language,
    Language displayLanguage
) noexcept;

} // namespace LiberateTheSheep::Game
