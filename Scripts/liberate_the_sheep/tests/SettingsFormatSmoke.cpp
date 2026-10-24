#include "Game/SettingsFormat.hpp"

#include <array>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {
using namespace LiberateTheSheep::Game;

bool same(const SettingsData &a, const SettingsData &b) {
    return a.language == b.language && a.musicEnabled == b.musicEnabled &&
           a.immortalModeEnabled == b.immortalModeEnabled;
}

bool parse(std::string_view text, SettingsData &result) {
    std::istringstream input{std::string(text)};
    return SettingsFormat::read(input, result);
}
} // namespace

int main() {
    using namespace LiberateTheSheep::Game;
    const SettingsData defaults;
    if (defaults.language != Language::Russian || !defaults.musicEnabled ||
        defaults.immortalModeEnabled || SettingsStore::kFileVersion != 2) {
        return 1;
    }

    for (Language language : {Language::Russian, Language::English}) {
        for (bool music : {false, true}) {
            // Version 1 contains no immortal-mode token and must turn it off,
            // even when the destination previously held an enabled setting.
            const SettingsData expected{language, music, false};
            const std::string legacy = std::string(SettingsFormat::kMagic) + " 1 " +
                (language == Language::Russian ? "0" : "1") + " " + (music ? "1" : "0");
            SettingsData migrated{Language::English, false, true};
            if (!parse(legacy, migrated) || !same(migrated, expected)) {
                std::cerr << "Legacy settings migration failed.\n";
                return 1;
            }

            for (bool immortal : {false, true}) {
                const SettingsData initial{language, music, immortal};
                std::ostringstream output;
                if (!SettingsFormat::write(output, initial) ||
                    !output.str().starts_with("LIBERATE_THE_SHEEP_SETTINGS 2 ")) {
                    return 1;
                }
                SettingsData restored;
                if (!parse(output.str(), restored) || !same(initial, restored)) {
                    std::cerr << "Version 2 settings round-trip failed.\n";
                    return 1;
                }
            }
        }
    }

    constexpr std::array invalid{
        "", "WRONG_MAGIC 2 0 1 1", "LIBERATE_THE_SHEEP_SETTINGS",
        "LIBERATE_THE_SHEEP_SETTINGS 0 0 1", "LIBERATE_THE_SHEEP_SETTINGS 3 0 1 1",
        "LIBERATE_THE_SHEEP_SETTINGS -1 0 1", "LIBERATE_THE_SHEEP_SETTINGS 2 0",
        "LIBERATE_THE_SHEEP_SETTINGS 1 0", "LIBERATE_THE_SHEEP_SETTINGS 2 0 1",
        "LIBERATE_THE_SHEEP_SETTINGS 2 0 1 1 extra", "LIBERATE_THE_SHEEP_SETTINGS 1 0 1 0",
        "LIBERATE_THE_SHEEP_SETTINGS 2 2 1 1", "LIBERATE_THE_SHEEP_SETTINGS 2 -1 1 1",
        "LIBERATE_THE_SHEEP_SETTINGS 2 0 2 1", "LIBERATE_THE_SHEEP_SETTINGS 2 0 -1 1",
        "LIBERATE_THE_SHEEP_SETTINGS 2 0 1 2", "LIBERATE_THE_SHEEP_SETTINGS 2 0 1 -1",
        "LIBERATE_THE_SHEEP_SETTINGS 2 0 1 true", "LIBERATE_THE_SHEEP_SETTINGS 2 0 1 1x",
        "LIBERATE_THE_SHEEP_SETTINGS 2 0 1 0.0", "LIBERATE_THE_SHEEP_SETTINGS 1 0 2",
        "LIBERATE_THE_SHEEP_SETTINGS 999999999999999999999 0 1 1",
    };
    const SettingsData sentinel{Language::English, false, true};
    for (std::string_view text : invalid) {
        SettingsData result = sentinel;
        if (parse(text, result) || !same(result, sentinel)) {
            std::cerr << "Invalid settings accepted or modified data: " << text << '\n';
            return 1;
        }
    }
    SettingsData spaced;
    if (!parse(" \nLIBERATE_THE_SHEEP_SETTINGS\t2 1 0 1 \n\t", spaced) ||
        !same(spaced, sentinel)) {
        return 1;
    }
    std::ostringstream invalidOutput;
    if (SettingsFormat::write(invalidOutput, SettingsData{static_cast<Language>(99), true, true}) ||
        !invalidOutput.str().empty()) {
        return 1;
    }
    std::istringstream brokenInput("LIBERATE_THE_SHEEP_SETTINGS 2 0 1 1");
    brokenInput.setstate(std::ios::badbit);
    SettingsData unchanged = sentinel;
    if (SettingsFormat::read(brokenInput, unchanged) || !same(unchanged, sentinel)) {
        return 1;
    }
    std::ostringstream brokenOutput;
    brokenOutput.setstate(std::ios::badbit);
    if (SettingsFormat::write(brokenOutput, defaults)) {
        return 1;
    }
    std::cout << "Settings v1 migration, v2 round-trip, defaults and invalid data checks passed.\n";
}
