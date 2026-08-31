#include "Game/ProgressStore.hpp"

#include <Bamboo/ApplicationAPI.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

namespace LiberateTheSheep::Game {
namespace {

    constexpr std::string_view kFileMagic = "LIBERATE_THE_SHEEP_PROGRESS";
    constexpr std::string_view kSaveFileName = "liberate_the_sheep_progress.sav";
    constexpr std::string_view kTemporarySuffix = ".tmp";
    constexpr std::string_view kBackupSuffix = ".bak";
    constexpr std::uintmax_t kMaximumSaveFileSize = 1024;

    [[nodiscard]] bool isValid(const ProgressData &progress) noexcept {
        if (progress.highestUnlocked < ProgressStore::kFirstLevel ||
            progress.highestUnlocked > ProgressStore::kLastLevel) {
            return false;
        }

        return std::ranges::all_of(progress.bestHearts, [](int hearts) {
            return hearts >= 0 && hearts <= ProgressStore::kMaximumHearts;
        });
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

    [[nodiscard]] bool readProgress(
        const std::filesystem::path &path,
        ProgressData &result
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
            std::uint32_t campaignVersion{};
            ProgressData loaded;
            if (!(input >> magic >> fileVersion >> campaignVersion >> loaded.highestUnlocked)) {
                return false;
            }
            for (int &hearts : loaded.bestHearts) {
                if (!(input >> hearts)) {
                    return false;
                }
            }

            input >> std::ws;
            if (input.peek() != std::char_traits<char>::eof()) {
                return false;
            }
            if (magic != kFileMagic || fileVersion != ProgressStore::kFileVersion ||
                campaignVersion != ProgressStore::kCampaignVersion || !isValid(loaded)) {
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

        // Some platforms do not replace an existing destination with rename().
        // Preserve it as a backup until the new file is in place.
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

    [[nodiscard]] bool writeProgress(
        const std::filesystem::path &directory,
        const ProgressData &progress
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

                output << kFileMagic << ' ' << ProgressStore::kFileVersion << ' '
                       << ProgressStore::kCampaignVersion << ' ' << progress.highestUnlocked;
                for (const int hearts : progress.bestHearts) {
                    output << ' ' << hearts;
                }
                output << '\n';
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

bool ProgressStore::load() noexcept {
    reset();

    std::filesystem::path directory;
    if (!persistentDirectory(directory)) {
        return false;
    }

    try {
        ProgressData loaded;
        if (!readProgress(directory / kSaveFileName, loaded)) {
            return false;
        }
        m_progress = loaded;
        return true;
    } catch (...) {
        return false;
    }
}

bool ProgressStore::save() const noexcept {
    if (!isValid(m_progress)) {
        return false;
    }

    std::filesystem::path directory;
    return persistentDirectory(directory) && writeProgress(directory, m_progress);
}

void ProgressStore::reset() noexcept {
    m_progress = {};
}

const ProgressData &ProgressStore::progress() const noexcept {
    return m_progress;
}

bool ProgressStore::isUnlocked(int levelNumber) const noexcept {
    return levelNumber >= kFirstLevel && levelNumber <= m_progress.highestUnlocked;
}

int ProgressStore::bestHearts(int levelNumber) const noexcept {
    if (levelNumber < kFirstLevel || levelNumber > kLastLevel) {
        return 0;
    }
    return m_progress.bestHearts[static_cast<std::size_t>(levelNumber - kFirstLevel)];
}

bool ProgressStore::recordCompletion(int levelNumber, int remainingHearts) noexcept {
    if (!isUnlocked(levelNumber) || levelNumber > kLastLevel || remainingHearts < 0 ||
        remainingHearts > kMaximumHearts) {
        return false;
    }

    bool changed = false;
    int &best = m_progress.bestHearts[static_cast<std::size_t>(levelNumber - kFirstLevel)];
    if (remainingHearts > best) {
        best = remainingHearts;
        changed = true;
    }

    const int unlockedAfterCompletion = std::min(levelNumber + 1, kLastLevel);
    if (unlockedAfterCompletion > m_progress.highestUnlocked) {
        m_progress.highestUnlocked = unlockedAfterCompletion;
        changed = true;
    }
    return changed;
}

} // namespace LiberateTheSheep::Game
