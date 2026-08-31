#include "UI/LevelSelectView.hpp"

#include "UI/UIStyle.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    constexpr std::size_t kCampaignLevelCount = 9;
    constexpr std::size_t kLevelsPerRow = 3;

    std::string levelButtonTitle(
        const LevelSelectText &text,
        int level,
        bool unlocked,
        int bestHearts
    ) {
        if (!unlocked) {
            return text.levelPrefix + " " + std::to_string(level) + "\n" + text.locked;
        }
        if (bestHearts <= 0) {
            return text.levelPrefix + " " + std::to_string(level) + "\n" + text.ready;
        }
        return text.levelPrefix + " " + std::to_string(level) + "\n" +
               std::to_string(std::clamp(bestHearts, 0, 3)) + "/3 " + text.hearts;
    }

} // namespace

LevelSelectView::LevelSelectView(
    LevelSelectText text,
    int highestUnlocked,
    std::span<const int> bestHearts,
    LevelAction playLevel,
    Action back
) {
    const std::size_t levelCount = std::max(kCampaignLevelCount, bestHearts.size());
    const int unlockedThrough =
        std::clamp(highestUnlocked, 1, static_cast<int>(levelCount));

    setBackgroundColor(Colors::transparent());
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(Colors::transparent());
    safeArea->layout().setWidth(PandaUI::Length::percent(100.f));
    safeArea->layout().setFlexGrow(1.f);
    safeArea->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    safeArea->layout().setAlignItems(PandaUI::Align::Center);
    safeArea->layout().setJustifyContent(PandaUI::Justify::Center);
    safeArea->layout().setPadding(PandaUI::Edge::All, 20.f);

    auto card = makeCard(520.f);

    auto title = makeLabel(
        text.title,
        30.f,
        PandaUI::FontWeight::Bold,
        Colors::cream(),
        1,
        PandaUI::TextAlignment::Center
    );
    title->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(title);

    auto subtitle = makeLabel(
        text.subtitle,
        14.f,
        PandaUI::FontWeight::Regular,
        Colors::mutedCream(),
        2,
        PandaUI::TextAlignment::Center
    );
    subtitle->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(subtitle);

    auto grid = std::make_shared<PandaUI::Panel>();
    grid->setBackgroundColor(Colors::transparent());
    grid->layout().setWidth(PandaUI::Length::percent(100.f));
    grid->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    grid->layout().setGap(10.f);

    for (std::size_t rowStart = 0; rowStart < levelCount; rowStart += kLevelsPerRow) {
        auto row = std::make_shared<PandaUI::Panel>();
        row->setBackgroundColor(Colors::transparent());
        row->layout().setWidth(PandaUI::Length::percent(100.f));
        row->layout().setFlexDirection(PandaUI::FlexDirection::Row);
        row->layout().setGap(10.f);

        for (std::size_t column = 0; column < kLevelsPerRow; ++column) {
            const std::size_t index = rowStart + column;
            if (index >= levelCount) {
                auto spacer = std::make_shared<PandaUI::Panel>();
                spacer->setBackgroundColor(Colors::transparent());
                spacer->layout().setFlexGrow(1.f);
                spacer->layout().setFlexBasis(0.f);
                row->addSubview(spacer);
                continue;
            }

            const int levelNumber = static_cast<int>(index + 1);
            const bool unlocked = levelNumber <= unlockedThrough;
            const int hearts = index < bestHearts.size() ? bestHearts[index] : 0;
            const bool isCurrent = levelNumber == unlockedThrough && hearts <= 0;

            auto levelButton = makeButton(
                levelButtonTitle(text, levelNumber, unlocked, hearts),
                isCurrent ? PandaUI::ButtonStyle::Accent : PandaUI::ButtonStyle::AccentOutline,
                72.f
            );
            levelButton->setNumberOfLines(2);
            levelButton->layout().setFlexGrow(1.f);
            levelButton->layout().setFlexBasis(0.f);
            levelButton->setEnabled(unlocked);
            levelButton->setOnClick([action = playLevel, levelNumber](PandaUI::Button &) {
                if (action) {
                    action(levelNumber);
                }
            });
            row->addSubview(levelButton);
        }
        grid->addSubview(row);
    }
    card->addSubview(grid);

    auto backButton = makeButton(text.backButton, PandaUI::ButtonStyle::Neutral, 50.f);
    backButton->layout().setWidth(PandaUI::Length::percent(100.f));
    backButton->setOnClick([action = std::move(back)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(backButton);

    safeArea->addSubview(card);
    addSubview(safeArea);
}

LevelSelectView::LevelSelectView(
    int highestUnlocked,
    std::span<const int> bestHearts,
    LevelAction playLevel,
    Action back
)
    : LevelSelectView(
          LevelSelectText{},
          highestUnlocked,
          bestHearts,
          std::move(playLevel),
          std::move(back)
      ) {}

} // namespace LiberateTheSheep::UI
