#include "UI/GameplayRootView.hpp"

#include <algorithm>
#include <string>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    std::shared_ptr<PandaUI::Panel>
    makeStat(std::string caption, std::shared_ptr<PandaUI::Label> valueLabel) {
        auto stat = makePill();
        stat->layout().setFlexGrow(1.f);
        stat->layout().setFlexBasis(0.f);
        stat->layout().setFlexDirection(PandaUI::FlexDirection::Column);
        stat->layout().setAlignItems(PandaUI::Align::Center);
        stat->layout().setGap(2.f);

        auto captionLabel = makeLabel(
            std::move(caption),
            10.f,
            PandaUI::FontWeight::Bold,
            PandaUI::Color(0xAEB9ACFF),
            1,
            PandaUI::TextAlignment::Center
        );
        captionLabel->setLetterSpacing(0.9f);
        stat->addSubview(captionLabel);

        valueLabel->layout().setWidth(PandaUI::Length::percent(100.f));
        stat->addSubview(std::move(valueLabel));
        return stat;
    }

    void connectButton(const std::shared_ptr<PandaUI::Button> &button, std::function<void()> action) {
        button->setOnClick([callback = std::move(action)](PandaUI::Button &) {
            if (callback) {
                callback();
            }
        });
    }

} // namespace

GameplayRootView::GameplayRootView(Callbacks callbacks, GameplayText text)
    : text_(std::move(text)) {
    setBackgroundColor(Colors::transparent());
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));

    boardInput_ = std::make_shared<BoardInputView>(std::move(callbacks.selectCell));
    boardInput_->layoutSetAbsolute();
    boardInput_->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    boardInput_->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    boardInput_->layout().setWidth(PandaUI::Length::points(0.f));
    boardInput_->layout().setHeight(PandaUI::Length::points(0.f));
    addSubview(boardInput_);

    addSubview(makeHud(callbacks));

    resultOverlay_ = makeResultOverlay(callbacks);
    resultOverlay_->setHidden(true);
    addSubview(resultOverlay_);
}

void GameplayRootView::setBoardFrame(PandaUI::Rect frame) {
    if (!boardInput_) {
        return;
    }

    const float width = std::max(0.f, frame.size.width);
    const float height = std::max(0.f, frame.size.height);
    boardInput_->layout().setPosition(
        PandaUI::Edge::Left, PandaUI::Length::points(frame.origin.x)
    );
    boardInput_->layout().setPosition(
        PandaUI::Edge::Top, PandaUI::Length::points(frame.origin.y)
    );
    boardInput_->layout().setWidth(PandaUI::Length::points(width));
    boardInput_->layout().setHeight(PandaUI::Length::points(height));
}

void GameplayRootView::setBoardSize(int columns, int rows) {
    if (boardInput_) {
        boardInput_->setBoardSize(columns, rows);
    }
}

void GameplayRootView::updateHUD(int levelNumber, int hearts, int sheepLeft) {
    if (levelLabel_) {
        levelLabel_->setText(std::to_string(std::max(1, levelNumber)));
    }
    if (heartsLabel_) {
        heartsLabel_->setText(std::to_string(std::clamp(hearts, 0, 3)) + " / 3");
        heartsLabel_->setTextColor(hearts > 1 ? Colors::cream() : Colors::danger());
    }
    if (sheepLabel_) {
        sheepLabel_->setText(std::to_string(std::max(0, sheepLeft)));
    }
}

void GameplayRootView::showVictory(int heartsRemaining, bool hasNextLevel) {
    setBoardInputEnabled(false);
    if (resultEyebrowLabel_) {
        resultEyebrowLabel_->setText(text_.fieldCleared);
        resultEyebrowLabel_->setTextColor(Colors::meadow());
    }
    if (resultTitleLabel_) {
        resultTitleLabel_->setText(text_.victoryTitle);
    }
    if (resultMessageLabel_) {
        resultMessageLabel_->setText(
            text_.victoryMessage + "\n" +
            std::to_string(std::clamp(heartsRemaining, 0, 3)) + "/3 " + text_.heartsStat
        );
    }
    if (nextButton_) {
        nextButton_->setHidden(!hasNextLevel);
        nextButton_->setEnabled(hasNextLevel);
    }
    if (replayButton_) {
        replayButton_->setStyle(
            hasNextLevel ? PandaUI::ButtonStyle::AccentOutline : PandaUI::ButtonStyle::Accent
        );
    }
    if (resultOverlay_) {
        resultOverlay_->setHidden(false);
    }
}

void GameplayRootView::showDefeat() {
    setBoardInputEnabled(false);
    if (resultEyebrowLabel_) {
        resultEyebrowLabel_->setText(text_.noHeartsLeft);
        resultEyebrowLabel_->setTextColor(Colors::danger());
    }
    if (resultTitleLabel_) {
        resultTitleLabel_->setText(text_.defeatTitle);
    }
    if (resultMessageLabel_) {
        resultMessageLabel_->setText(text_.defeatMessage);
    }
    if (nextButton_) {
        nextButton_->setHidden(true);
        nextButton_->setEnabled(false);
    }
    if (replayButton_) {
        replayButton_->setStyle(PandaUI::ButtonStyle::Accent);
    }
    if (resultOverlay_) {
        resultOverlay_->setHidden(false);
    }
}

void GameplayRootView::hideResult() {
    if (resultOverlay_) {
        resultOverlay_->setHidden(true);
    }
}

void GameplayRootView::setBoardInputEnabled(bool enabled) {
    if (boardInput_) {
        boardInput_->setInputEnabled(enabled);
    }
}

std::shared_ptr<BoardInputView> GameplayRootView::boardInputView() const {
    return boardInput_;
}

std::shared_ptr<PandaUI::Panel> GameplayRootView::makeHud(Callbacks &callbacks) {
    auto safeArea = std::make_shared<PassThroughSafeAreaView>();
    safeArea->setBackgroundColor(Colors::transparent());
    safeArea->layoutSetAbsolute();
    safeArea->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    safeArea->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    safeArea->layout().setWidth(PandaUI::Length::percent(100.f));
    safeArea->layout().setHeight(PandaUI::Length::percent(100.f));
    safeArea->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    safeArea->layout().setAlignItems(PandaUI::Align::Center);

    auto hudCard = makeCard(560.f);
    hudCard->layout().setMargin(PandaUI::Edge::Top, 10.f);
    hudCard->layout().setPadding(PandaUI::Edge::Horizontal, 12.f);
    hudCard->layout().setPadding(PandaUI::Edge::Vertical, 12.f);
    hudCard->layout().setGap(10.f);
    hudCard->surface().setCornerRadius(20.f);

    auto stats = std::make_shared<PandaUI::Panel>();
    stats->setBackgroundColor(Colors::transparent());
    stats->layout().setWidth(PandaUI::Length::percent(100.f));
    stats->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    stats->layout().setGap(8.f);

    levelLabel_ = makeLabel(
        "1", 20.f, PandaUI::FontWeight::Bold, Colors::cream(), 1, PandaUI::TextAlignment::Center
    );
    heartsLabel_ = makeLabel(
        "3 / 3",
        20.f,
        PandaUI::FontWeight::Bold,
        Colors::cream(),
        1,
        PandaUI::TextAlignment::Center
    );
    sheepLabel_ = makeLabel(
        "0", 20.f, PandaUI::FontWeight::Bold, Colors::cream(), 1, PandaUI::TextAlignment::Center
    );
    stats->addSubview(makeStat(text_.levelStat, levelLabel_));
    stats->addSubview(makeStat(text_.heartsStat, heartsLabel_));
    stats->addSubview(makeStat(text_.sheepLeftStat, sheepLabel_));
    hudCard->addSubview(stats);

    auto actions = std::make_shared<PandaUI::Panel>();
    actions->setBackgroundColor(Colors::transparent());
    actions->layout().setWidth(PandaUI::Length::percent(100.f));
    actions->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    actions->layout().setGap(8.f);

    auto hint = makeButton(text_.hintButton, PandaUI::ButtonStyle::AccentOutline, 46.f);
    auto restart = makeButton(text_.restartButton, PandaUI::ButtonStyle::Neutral, 46.f);
    auto menu = makeButton(text_.menuButton, PandaUI::ButtonStyle::Neutral, 46.f);
    for (const auto &button : {hint, restart, menu}) {
        button->layout().setFlexGrow(1.f);
        button->layout().setFlexBasis(0.f);
    }
    connectButton(hint, std::move(callbacks.hint));
    connectButton(restart, std::move(callbacks.restart));
    // Menu is intentionally copied: the same action is also used by the result modal.
    connectButton(menu, callbacks.menu);
    actions->addSubview(hint);
    actions->addSubview(restart);
    actions->addSubview(menu);
    hudCard->addSubview(actions);

    safeArea->addSubview(hudCard);
    return safeArea;
}

std::shared_ptr<PandaUI::Panel> GameplayRootView::makeResultOverlay(Callbacks &callbacks) {
    auto overlay = std::make_shared<PandaUI::Panel>();
    overlay->setBackgroundColor(Colors::scrim());
    overlay->layoutSetAbsolute();
    overlay->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    overlay->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    overlay->layout().setWidth(PandaUI::Length::percent(100.f));
    overlay->layout().setHeight(PandaUI::Length::percent(100.f));
    overlay->layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(Colors::transparent());
    safeArea->layout().setWidth(PandaUI::Length::percent(100.f));
    safeArea->layout().setFlexGrow(1.f);
    safeArea->layout().setAlignItems(PandaUI::Align::Center);
    safeArea->layout().setJustifyContent(PandaUI::Justify::Center);
    safeArea->layout().setPadding(PandaUI::Edge::All, 24.f);

    auto card = makeCard(460.f);

    resultEyebrowLabel_ = makeLabel(
        text_.fieldCleared,
        12.f,
        PandaUI::FontWeight::Bold,
        Colors::meadow(),
        1,
        PandaUI::TextAlignment::Center
    );
    resultEyebrowLabel_->setLetterSpacing(1.3f);
    resultEyebrowLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(resultEyebrowLabel_);

    resultTitleLabel_ = makeLabel(
        text_.victoryTitle,
        30.f,
        PandaUI::FontWeight::Bold,
        Colors::cream(),
        2,
        PandaUI::TextAlignment::Center
    );
    resultTitleLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(resultTitleLabel_);

    resultMessageLabel_ = makeLabel(
        text_.victoryMessage,
        16.f,
        PandaUI::FontWeight::Regular,
        Colors::mutedCream(),
        3,
        PandaUI::TextAlignment::Center
    );
    resultMessageLabel_->setLineSpacing(3.f);
    resultMessageLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(resultMessageLabel_);

    nextButton_ = makeButton(text_.nextLevelButton, PandaUI::ButtonStyle::Accent, 56.f);
    nextButton_->layout().setWidth(PandaUI::Length::percent(100.f));
    connectButton(nextButton_, std::move(callbacks.next));
    card->addSubview(nextButton_);

    replayButton_ = makeButton(text_.replayButton, PandaUI::ButtonStyle::AccentOutline, 52.f);
    replayButton_->layout().setWidth(PandaUI::Length::percent(100.f));
    connectButton(replayButton_, std::move(callbacks.replay));
    card->addSubview(replayButton_);

    auto menuButton = makeButton(text_.menuButton, PandaUI::ButtonStyle::Neutral, 50.f);
    menuButton->layout().setWidth(PandaUI::Length::percent(100.f));
    connectButton(menuButton, std::move(callbacks.menu));
    card->addSubview(menuButton);

    safeArea->addSubview(card);
    overlay->addSubview(safeArea);
    return overlay;
}

} // namespace LiberateTheSheep::UI
