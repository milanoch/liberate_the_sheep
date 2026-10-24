#pragma once

#include "Game/SettingsStore.hpp"

#include <istream>
#include <ostream>
#include <string>
#include <string_view>

namespace LiberateTheSheep::Game::SettingsFormat {

inline constexpr std::string_view kMagic = "LIBERATE_THE_SHEEP_SETTINGS";

// Keep the format independent of the engine and filesystem. Parse into a
// temporary value so invalid input cannot partially modify the caller's data.
[[nodiscard]] inline bool read(std::istream &input, SettingsData &result) noexcept {
    try {
        std::string magic;
        std::string version;
        std::string language;
        std::string music;
        std::string immortal{"0"};
        if (!(input >> magic >> version >> language >> music)) {
            return false;
        }
        if (magic != kMagic || (version != "1" && version != "2")) {
            return false;
        }
        if (version == "2" && !(input >> immortal)) {
            return false;
        }
        const auto isBoolean = [](const std::string &value) {
            return value == "0" || value == "1";
        };
        if ((language != "0" && language != "1") || !isBoolean(music) ||
            !isBoolean(immortal)) {
            return false;
        }
        input >> std::ws;
        if (input.bad() || input.peek() != std::char_traits<char>::eof()) {
            return false;
        }

        result = SettingsData{
            language == "0" ? Language::Russian : Language::English,
            music == "1",
            immortal == "1",
        };
        return true;
    } catch (...) {
        return false;
    }
}

[[nodiscard]] inline bool write(std::ostream &output, const SettingsData &settings) noexcept {
    if (!isValidLanguage(settings.language)) {
        return false;
    }
    try {
        output << kMagic << ' ' << SettingsStore::kFileVersion << ' '
               << static_cast<unsigned int>(settings.language) << ' '
               << (settings.musicEnabled ? 1 : 0) << ' '
               << (settings.immortalModeEnabled ? 1 : 0) << '\n';
        return static_cast<bool>(output);
    } catch (...) {
        return false;
    }
}

} // namespace LiberateTheSheep::Game::SettingsFormat
