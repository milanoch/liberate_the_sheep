#include "UI/GameplayRootView.hpp"
#include "UI/BombSheepView.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    std::string countdown(float seconds) {
        const int total = std::max(0, static_cast<int>(std::ceil(seconds)));
        const int remainder = total % 60;
        return std::to_string(total / 60) + ":" +
            (remainder < 10 ? "0" : "") + std::to_string(remainder);
    }

    std::shared_ptr<PandaUI::Panel>
    makeStat(std::string caption, std::shared_ptr<PandaUI::Label> valueLabel) {
        auto stat = makePill();
        stat->layout().setFlexGrow(1.f);
        stat->layout().setFlexBasis(0.f);
        stat->layout().setFlexDirection(PandaUI::FlexDirection::Column);
        stat->layout().setAlignItems(PandaUI::Align::Center);
        stat->layout().setGap(2.f);
        stat->layout().setHeight(PandaUI::Length::points(44.f));
        stat->layout().setPadding(PandaUI::Edge::Vertical, 4.f);

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

GameplayRootView::GameplayRootView(Callbacks callbacks, GameplayText text, bool showHearts)
    : text_(std::move(text)), showHearts_(showHearts) {
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

    bombFieldOverlay_ = std::make_shared<BombFieldOverlay>();
    addSubview(bombFieldOverlay_);

    addSubview(makeHud(callbacks));

    resultOverlay_ = makeResultOverlay(callbacks);
    resultOverlay_->setHidden(true);
    addSubview(resultOverlay_);

    bombTutorial_ = makeBombTutorial(callbacks);
    bombTutorial_->setHidden(true);
    addSubview(bombTutorial_);
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
        std::string message = text_.victoryMessage;
        if (showHearts_) {
            message += "\n" + std::to_string(std::clamp(heartsRemaining, 0, 3)) + "/3 " + text_.heartsStat;
        }
        resultMessageLabel_->setText(message);
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
    bombFieldOverlay_->reset();
}

void GameplayRootView::setBombArtwork(PandaUI::TextureHandle sheepAtlas,
                                      PandaUI::TextureHandle bomb) {
    bombPicture_->setArtwork(sheepAtlas, bomb);
}

void GameplayRootView::showBombTutorial(bool immortal, float seconds, int coat,
                                        bool impact, int hits) {
    setBoardInputEnabled(false);
    bombTitleLabel_->setText(impact ? text_.impactBombTitle : text_.bombTitle);
    bombInstructionsLabel_->setText(impact ? text_.impactBombInstructions : text_.bombInstructions);
    bombImmortalLabel_->setText(impact ? text_.impactBombImmortalInstructions
                                     : text_.bombImmortalInstructions);
    bombPicture_->setCoat(coat);
    bombPicture_->setImpactMode(impact);
    bombPicture_->setSeconds(impact ? hits : static_cast<int>(std::ceil(seconds)));
    bombImmortalLabel_->setHidden(!immortal);
    bombImmortalLabel_->layout().setDisplay(immortal ? PandaUI::Display::Flex
                                                  : PandaUI::Display::None);
    bombStartButton_->setText(impact ? text_.bombStart
                                    : text_.bombStart + "  ·  " + countdown(seconds));
    bombStartButton_->setEnabled(true);
    bombTutorial_->setHidden(false);
}

void GameplayRootView::hideBombTutorial() {
    bombTutorial_->setHidden(true);
}

void GameplayRootView::updateBombTimer(float seconds, bool visible, bool rescued,
                                       bool defused, bool immortal, bool impact, int hits) {
    bombTimerLabel_->setHidden(!visible);
    bombTimerLabel_->layout().setDisplay(visible ? PandaUI::Display::Flex
                                               : PandaUI::Display::None);
    if (!visible) { return; }
    const bool exhausted = impact ? hits <= 0 : seconds <= 0.f;
    const std::string value = rescued ? text_.bombRescued
        : defused ? text_.bombDefused
        : exhausted ? (impact ? text_.impactBombExpired : text_.bombExpired)
        : (impact ? text_.impactBombHits + "  " + std::to_string(std::max(0, hits))
                  : text_.bombTimer + "  " + countdown(seconds)) + (immortal ? "  ∞" : "");
    if (bombTimerLabel_->getText() != value) { bombTimerLabel_->setText(value); }
    bombTimerLabel_->setTextColor(rescued || defused ? Colors::meadow()
        : (impact ? hits <= 1 : seconds <= 10.f) ? Colors::danger() : Colors::cream());
}

void GameplayRootView::setBombMarker(PandaUI::Point center, float bombSize,
                                     float rotationDegrees, int seconds, bool visible, bool impact) {
    bombFieldOverlay_->setMarker(center, bombSize, rotationDegrees, seconds, visible, impact);
}

void GameplayRootView::playBombBurst(PandaUI::Point center, bool harmless) {
    bombFieldOverlay_->playBurst(center, harmless);
}

void GameplayRootView::showBombDefeat(bool impact) {
    showDefeat();
    resultEyebrowLabel_->setText(impact ? text_.impactBombExpired : text_.bombExpired);
    resultMessageLabel_->setText(impact ? text_.impactBombDefeatMessage : text_.bombDefeatMessage);
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
    // Even with the bomb timer, keep the HUD within the field's reserved
    // 154-point top inset. Buttons retain their 44-point touch target.
    hudCard->layout().setPadding(PandaUI::Edge::Vertical, 8.f);
    hudCard->layout().setGap(6.f);
    hudCard->surface().setCornerRadius(20.f);

    auto stats = std::make_shared<PandaUI::Panel>();
    stats->setBackgroundColor(Colors::transparent());
    stats->layout().setWidth(PandaUI::Length::percent(100.f));
    stats->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    stats->layout().setGap(8.f);

    levelLabel_ = makeLabel(
        "1", 20.f, PandaUI::FontWeight::Bold, Colors::cream(), 1, PandaUI::TextAlignment::Center
    );
    sheepLabel_ = makeLabel(
        "0", 20.f, PandaUI::FontWeight::Bold, Colors::cream(), 1, PandaUI::TextAlignment::Center
    );
    stats->addSubview(makeStat(text_.levelStat, levelLabel_));
    if (showHearts_) {
        heartsLabel_ = makeLabel(
            "3 / 3", 20.f, PandaUI::FontWeight::Bold,
            Colors::cream(), 1, PandaUI::TextAlignment::Center);
        stats->addSubview(makeStat(text_.heartsStat, heartsLabel_));
    }
    stats->addSubview(makeStat(text_.sheepLeftStat, sheepLabel_));
    hudCard->addSubview(stats);

    auto actions = std::make_shared<PandaUI::Panel>();
    actions->setBackgroundColor(Colors::transparent());
    actions->layout().setWidth(PandaUI::Length::percent(100.f));
    actions->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    actions->layout().setGap(8.f);

    auto hint = makeButton(text_.hintButton, PandaUI::ButtonStyle::AccentOutline, 44.f);
    auto restart = makeButton(text_.restartButton, PandaUI::ButtonStyle::Neutral, 44.f);
    auto menu = makeButton(text_.menuButton, PandaUI::ButtonStyle::Neutral, 44.f);
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

    bombTimerLabel_ = makeLabel("", 12.f, PandaUI::FontWeight::Bold,
                               Colors::cream(), 1, PandaUI::TextAlignment::Center);
    bombTimerLabel_->setUserInteractionEnabled(false);
    bombTimerLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    bombTimerLabel_->layout().setHeight(PandaUI::Length::points(18.f));
    bombTimerLabel_->layout().setDisplay(PandaUI::Display::None);
    bombTimerLabel_->setHidden(true);
    hudCard->addSubview(bombTimerLabel_);

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
        0,
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
    connectButton(menuButton, callbacks.menu);
    card->addSubview(menuButton);

    safeArea->addSubview(card);
    overlay->addSubview(safeArea);
    return overlay;
}

std::shared_ptr<PandaUI::Panel> GameplayRootView::makeBombTutorial(Callbacks &callbacks) {
    auto overlay = std::make_shared<PandaUI::Panel>();
    overlay->setBackgroundColor(PandaUI::Color(0x101010B8));
    overlay->layoutSetAbsolute();
    overlay->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    overlay->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    overlay->layout().setWidth(PandaUI::Length::percent(100.f));
    overlay->layout().setHeight(PandaUI::Length::percent(100.f));

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(Colors::transparent());
    safeArea->layout().setWidth(PandaUI::Length::percent(100.f));
    safeArea->layout().setHeight(PandaUI::Length::percent(100.f));

    auto scroll = std::make_shared<PandaUI::ScrollView>();
    scroll->setBackgroundColor(Colors::transparent());
    scroll->setScrollDirection(PandaUI::ScrollDirection::Vertical);
    scroll->setAutomaticallyAdjustsContentSize(true);
    scroll->layout().setWidth(PandaUI::Length::percent(100.f));
    scroll->layout().setFlexGrow(1.f);
    auto content = scroll->getContentView();
    content->layout().setWidth(PandaUI::Length::percent(100.f));
    content->layout().setMinHeight(PandaUI::Length::percent(100.f));
    content->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    content->layout().setAlignItems(PandaUI::Align::Center);
    content->layout().setPadding(PandaUI::Edge::All, 18.f);

    const auto spacer = [] {
        auto view = std::make_shared<PandaUI::Panel>();
        view->setBackgroundColor(Colors::transparent());
        view->layout().setFlexGrow(1.f);
        view->layout().setFlexShrink(1.f);
        view->layout().setFlexBasis(0.f);
        return view;
    };
    scroll->addContentSubview(spacer());
    auto card = makeCard(440.f);
    card->setBackgroundColor(PandaUI::Color(0x242424F5));
    card->layout().setFlexShrink(0.f);
    card->layout().setPadding(PandaUI::Edge::All, 18.f);
    card->layout().setGap(12.f);

    bombTitleLabel_ = makeLabel(text_.bombTitle, 23.f, PandaUI::FontWeight::Bold,
                           Colors::cream(), 2, PandaUI::TextAlignment::Center);
    bombTitleLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(bombTitleLabel_);
    bombPicture_ = std::make_shared<BombSheepPicture>();
    card->addSubview(bombPicture_);

    bombInstructionsLabel_ = makeLabel(text_.bombInstructions, 15.f,
        PandaUI::FontWeight::Regular, PandaUI::Color(0xE7E7E7FF), 0,
        PandaUI::TextAlignment::Center);
    bombInstructionsLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    bombInstructionsLabel_->setLineSpacing(3.f);
    card->addSubview(bombInstructionsLabel_);

    bombImmortalLabel_ = makeLabel(text_.bombImmortalInstructions, 14.f,
        PandaUI::FontWeight::Regular, PandaUI::Color(0xB8DDAEFF), 0,
        PandaUI::TextAlignment::Center);
    bombImmortalLabel_->layout().setWidth(PandaUI::Length::percent(100.f));
    bombImmortalLabel_->setLineSpacing(2.f);
    card->addSubview(bombImmortalLabel_);

    bombStartButton_ = makeButton(text_.bombStart, PandaUI::ButtonStyle::Accent, 48.f);
    bombStartButton_->layout().setWidth(PandaUI::Length::percent(100.f));
    bombStartButton_->setOnClick([action = std::move(callbacks.startBombLevel)](PandaUI::Button &button) {
        if (action) {
            button.setEnabled(false);
            action();
        }
    });
    card->addSubview(bombStartButton_);
    auto menu = makeButton(text_.menuButton, PandaUI::ButtonStyle::Neutral, 44.f);
    menu->layout().setWidth(PandaUI::Length::percent(100.f));
    connectButton(menu, callbacks.menu);
    card->addSubview(menu);

    scroll->addContentSubview(card);
    scroll->addContentSubview(spacer());
    safeArea->addSubview(scroll);
    overlay->addSubview(safeArea);
    return overlay;
}

} // namespace LiberateTheSheep::UI
