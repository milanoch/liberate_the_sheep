#pragma once

#include "Game/Localization.hpp"

#include <cstdint>

namespace LiberateTheSheep::Game {

struct SettingsData final {
    Language language{Language::Russian};
    bool musicEnabled{true};
};

class SettingsStore final {
public:
    static constexpr std::uint32_t kFileVersion = 1;

    SettingsStore() noexcept = default;

    // Returns true only when a current, fully valid file was loaded. Any error
    // restores safe defaults so callers can use settings() unconditionally.
    [[nodiscard]] bool load() noexcept;
    [[nodiscard]] bool save() const noexcept;

    void reset() noexcept;

    [[nodiscard]] const SettingsData &settings() const noexcept;
    [[nodiscard]] Language language() const noexcept;
    [[nodiscard]] bool musicEnabled() const noexcept;

    // Mutators do not write implicitly: menu code can update several controls
    // and call save() once. The setters report whether the value changed;
    // cycle/toggle return their new value for immediate UI refresh.
    [[nodiscard]] bool setLanguage(Language language) noexcept;
    [[nodiscard]] Language cycleLanguage() noexcept;
    [[nodiscard]] bool setMusicEnabled(bool enabled) noexcept;
    [[nodiscard]] bool toggleMusic() noexcept;

private:
    SettingsData m_settings;
};

} // namespace LiberateTheSheep::Game
