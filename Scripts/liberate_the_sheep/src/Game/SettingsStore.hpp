#pragma once

#include "Game/Localization.hpp"

#include <cstdint>

namespace LiberateTheSheep::Game {

struct SettingsData final {
    Language language{Language::Russian};
    bool musicEnabled{true};
    bool immortalModeEnabled{false};
};

class SettingsStore final {
public:
    static constexpr std::uint32_t kFileVersion = 2;

    SettingsStore() noexcept = default;

    // Reads fully valid v1 or v2 files. Legacy v1 settings default immortal
    // mode to false. Errors restore defaults without modifying the saved file.
    [[nodiscard]] bool load() noexcept;
    [[nodiscard]] bool save() const noexcept;

    void reset() noexcept;

    [[nodiscard]] const SettingsData &settings() const noexcept;
    [[nodiscard]] Language language() const noexcept;
    [[nodiscard]] bool musicEnabled() const noexcept;
    [[nodiscard]] bool immortalModeEnabled() const noexcept;

    // Mutators do not write implicitly: menu code can update several controls
    // and call save() once. The setters report whether the value changed;
    // cycle/toggle return their new value for immediate UI refresh.
    [[nodiscard]] bool setLanguage(Language language) noexcept;
    [[nodiscard]] Language cycleLanguage() noexcept;
    [[nodiscard]] bool setMusicEnabled(bool enabled) noexcept;
    [[nodiscard]] bool toggleMusic() noexcept;
    [[nodiscard]] bool setImmortalModeEnabled(bool enabled) noexcept;

private:
    SettingsData m_settings;
};

} // namespace LiberateTheSheep::Game
