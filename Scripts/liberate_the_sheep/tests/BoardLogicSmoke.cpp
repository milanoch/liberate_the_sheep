#include "Game/BoardLayout.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "Game/SheepAppearance.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>

namespace {

bool layoutChecksPass() {
    using namespace LiberateTheSheep::Game;

    const BoardLayout large =
        calculateBoardLayout(1024, 1536, 8, 12, 9.f, {}, {180.f, 24.f, 24.f, 24.f});
    const float expectedCellWorldSize =
        (large.screenRect.width / 8.f) * 9.f / 1536.f;
    if (!large.valid() ||
        std::abs(large.cellWorldSize - expectedCellWorldSize) > 0.001f ||
        large.screenRect.x < 0.f || large.screenRect.y < 0.f ||
        large.screenRect.x + large.screenRect.width > 1024.1f ||
        large.screenRect.y + large.screenRect.height > 1536.1f) {
        return false;
    }

    const Sheep upward{1, {2, 3}, Direction::Up};
    const LayoutPoint center = large.sheepCenter(upward);
    const LayoutPoint tail = large.cellCenter(upward.tail);
    const LayoutPoint head = large.cellCenter(upward.head());
    if (std::abs(center.x - tail.x) > 0.001f || center.y <= tail.y || center.y >= head.y) {
        return false;
    }

    const BoardLayout compact =
        calculateBoardLayout(375, 667, 8, 12, 9.f, {}, {160.f, 18.f, 20.f, 18.f});
    if (!compact.valid() || compact.screenRect.y < 159.9f ||
        compact.screenRect.y + compact.screenRect.height > 647.1f) {
        return false;
    }

    return calculateBoardLayout(0, 1536, 8, 12, 9.f).valid() == false;
}

bool sessionChecksPass() {
    using LiberateTheSheep::Game::SessionState;

    SessionState::clear();
    if (SessionState::peekRequestedLevel() != SessionState::kNoLevelRequested ||
        SessionState::consumeRequestedLevel() != SessionState::kNoLevelRequested) {
        return false;
    }

    SessionState::requestLevel(7);
    if (SessionState::peekRequestedLevel() != 7 ||
        SessionState::consumeRequestedLevel() != 7 ||
        SessionState::peekRequestedLevel() != SessionState::kNoLevelRequested) {
        return false;
    }

    SessionState::requestLevel(99);
    return SessionState::consumeRequestedLevel() == SessionState::kLastLevel;
}

bool localizationChecksPass() {
    using namespace LiberateTheSheep::Game;

    for (const Language language : {Language::Russian, Language::English}) {
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(TextKey::Count);
             ++index) {
            if (localizedText(static_cast<TextKey>(index), language).empty()) {
                return false;
            }
        }
    }

    return languageCode(Language::Russian) == "ru" &&
           languageCode(Language::English) == "en" &&
           localizedText(TextKey::Continue, Language::Russian) !=
               localizedText(TextKey::Continue, Language::English) &&
           localizedLanguageName(Language::Russian, Language::English) == "RUSSIAN" &&
           localizedLanguageName(Language::English, Language::Russian) == "АНГЛИЙСКИЙ";
}

bool sheepAppearanceChecksPass() {
    using namespace LiberateTheSheep::Game;

    const std::vector<SheepCoat> first = makeBalancedCoatOrder(41u, 0x12345678ULL);
    const std::vector<SheepCoat> repeated = makeBalancedCoatOrder(41u, 0x12345678ULL);
    const std::vector<SheepCoat> different = makeBalancedCoatOrder(41u, 0x87654321ULL);
    if (first != repeated || first == different || first.size() != 41u) {
        return false;
    }

    std::array<int, kSheepCoatCount> counts{};
    for (const SheepCoat coat : first) {
        const std::size_t index = static_cast<std::size_t>(coat);
        if (index >= counts.size()) {
            return false;
        }
        ++counts[index];
    }
    const auto [minimum, maximum] = std::minmax_element(counts.begin(), counts.end());
    if (*maximum - *minimum > 1) {
        return false;
    }

    const std::vector<SheepCoat> tutorial = makeBalancedCoatOrder(3u, 1u);
    std::array<bool, kSheepCoatCount> seen{};
    for (const SheepCoat coat : tutorial) {
        seen[static_cast<std::size_t>(coat)] = true;
    }
    return std::all_of(seen.begin(), seen.end(), [](bool value) { return value; });
}

} // namespace

int main() {
    const LiberateTheSheep::Game::LogicSelfValidationReport report =
        LiberateTheSheep::Game::runBoardLogicSelfValidation();

    if (!report.valid()) {
        std::cerr << "Board logic validation failed after checking " << report.levelsChecked
                  << " levels:\n";
        for (const std::string &error : report.errors) {
            std::cerr << "- " << error << '\n';
        }
        return 1;
    }

    if (!layoutChecksPass()) {
        std::cerr << "Board layout validation failed.\n";
        return 1;
    }

    if (!sessionChecksPass()) {
        std::cerr << "Menu-to-level session validation failed.\n";
        return 1;
    }

    if (!localizationChecksPass()) {
        std::cerr << "Localization validation failed.\n";
        return 1;
    }

    if (!sheepAppearanceChecksPass()) {
        std::cerr << "Sheep coat distribution validation failed.\n";
        return 1;
    }

    std::cout << "Board logic and layout validation passed for " << report.levelsChecked
              << " levels.\n";
    return 0;
}
