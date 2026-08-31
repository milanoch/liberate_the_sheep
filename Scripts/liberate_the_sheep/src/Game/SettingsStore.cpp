#include "Game/SettingsStore.hpp"

#include <Bamboo/ApplicationAPI.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace LiberateTheSheep::Game {
namespace {

    constexpr std::string_view kFileMagic = "LIBERATE_THE_SHEEP_SETTINGS";
    constexpr std::string_view kSaveFileName = "liberate_the_sheep_settings.sav";
    constexpr std::string_view kTemporarySuffix = ".tmp";
    constexpr std::string_view kBackupSuffix = ".bak";
    constexpr std::uintmax_t kMaximumSaveFileSize = 256;

    [[nodiscard]] bool isValid(const SettingsData &settings) noexcept {
        return isValidLanguage(settings.language);
    }

    [[nodiscard]] bool persistentDirectory(std::filesystem::path &directory) noexcept {
        try {
            const std::string path = Bamboo::ApplicationAPI::getPersistentDataPath();
            if (path.empty()) {
                return false;
            }
            directory = std::filesystem::path(path);
            return !directory.empty();
        } catch (...) {
            return false;
        }
    }

    [[nodiscard]] bool readSettings(
        const std::filesystem::path &path,
        SettingsData &result
    ) noexcept {
        try {
            std::error_code error;
            const std::uintmax_t fileSize = std::filesystem::file_size(path, error);
            if (error || fileSize == 0 || fileSize > kMaximumSaveFileSize) {
                return false;
            }

            std::ifstream input(path, std::ios::binary);
            if (!input) {
                return false;
            }

            std::string magic;
            std::uint32_t fileVersion{};
            unsigned int languageValue{};
            unsigned int musicValue{};
            if (!(input >> magic >> fileVersion >> languageValue >> musicValue)) {
                return false;
            }

            input >> std::ws;
            if (input.peek() != std::char_traits<char>::eof()) {
                return false;
            }
            if (magic != kFileMagic || fileVersion != SettingsStore::kFileVersion ||
                languageValue > static_cast<unsigned int>(Language::English) || musicValue > 1U) {
                return false;
            }

            const SettingsData loaded{
                static_cast<Language>(languageValue),
                musicValue != 0U,
            };
            if (!isValid(loaded)) {
                return false;
            }

            result = loaded;
            return true;
        } catch (...) {
            return false;
        }
    }

    void removeFile(const std::filesystem::path &path) noexcept {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    [[nodiscard]] bool replaceWithTemporaryFile(
        const std::filesystem::path &temporaryPath,
        const std::filesystem::path &destinationPath
    ) noexcept {
        std::error_code error;
        std::filesystem::rename(temporaryPath, destinationPath, error);
        if (!error) {
            return true;
        }

        // Preserve the previous file until the replacement has completed. This
        // mirrors ProgressStore and also works on platforms where rename() does
        // not replace an existing destination.
        error.clear();
        if (!std::filesystem::exists(destinationPath, error) || error) {
            removeFile(temporaryPath);
            return false;
        }

        std::filesystem::path backupPath = destinationPath;
        backupPath += kBackupSuffix;
        removeFile(backupPath);

        error.clear();
        std::filesystem::rename(destinationPath, backupPath, error);
        if (error) {
            removeFile(temporaryPath);
            return false;
        }

        error.clear();
        std::filesystem::rename(temporaryPath, destinationPath, error);
        if (!error) {
            removeFile(backupPath);
            return true;
        }

        std::error_code restoreError;
        std::filesystem::rename(backupPath, destinationPath, restoreError);
        removeFile(temporaryPath);
        return false;
    }

    [[nodiscard]] bool writeSettings(
        const std::filesystem::path &directory,
        const SettingsData &settings
    ) noexcept {
        try {
            std::error_code error;
            std::filesystem::create_directories(directory, error);
            if (error) {
                return false;
            }

            const std::filesystem::path destinationPath = directory / kSaveFileName;
            std::filesystem::path temporaryPath = destinationPath;
            temporaryPath += kTemporarySuffix;
            removeFile(temporaryPath);

            {
                std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
                if (!output) {
                    return false;
                }

                output << kFileMagic << ' ' << SettingsStore::kFileVersion << ' '
                       << static_cast<unsigned int>(settings.language) << ' '
                       << (settings.musicEnabled ? 1 : 0) << '\n';
                output.flush();
                if (!output) {
                    output.close();
                    removeFile(temporaryPath);
                    return false;
                }
            }

            return replaceWithTemporaryFile(temporaryPath, destinationPath);
        } catch (...) {
            return false;
        }
    }

} // namespace

bool SettingsStore::load() noexcept {
    reset();

    std::filesystem::path directory;
    if (!persistentDirectory(directory)) {
        return false;
    }

    try {
        SettingsData loaded;
        if (!readSettings(directory / kSaveFileName, loaded)) {
            return false;
        }
        m_settings = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool SettingsStore::save() const noexcept {
    if (!isValid(m_settings)) {
        return false;
    }

    std::filesystem::path directory;
    return persistentDirectory(directory) && writeSettings(directory, m_settings);
}

void SettingsStore::reset() noexcept {
    m_settings = {};
}

const SettingsData &SettingsStore::settings() const noexcept {
    return m_settings;
}

Language SettingsStore::language() const noexcept {
    return m_settings.language;
}

bool SettingsStore::musicEnabled() const noexcept {
    return m_settings.musicEnabled;
}

bool SettingsStore::setLanguage(Language language) noexcept {
    if (!isValidLanguage(language) || language == m_settings.language) {
        return false;
    }
    m_settings.language = language;
    return true;
}

Language SettingsStore::cycleLanguage() noexcept {
    m_settings.language = nextLanguage(m_settings.language);
    return m_settings.language;
}

bool SettingsStore::setMusicEnabled(bool enabled) noexcept {
    if (enabled == m_settings.musicEnabled) {
        return false;
    }
    m_settings.musicEnabled = enabled;
    return true;
}

bool SettingsStore::toggleMusic() noexcept {
    m_settings.musicEnabled = !m_settings.musicEnabled;
    return m_settings.musicEnabled;
}

} // namespace LiberateTheSheep::Game
