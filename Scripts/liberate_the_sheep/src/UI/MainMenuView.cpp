#include "UI/MainMenuView.hpp"

#include "UI/UIStyle.hpp"

#include <utility>

namespace LiberateTheSheep::UI {

MainMenuView::MainMenuView(
    MainMenuText text,
    Action playOrContinue,
    Action levelSelect,
    Action settings,
    bool continueAvailable,
    PandaUI::TextureHandle settingsIcon
)
    : text_(std::move(text)) {
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
    safeArea->layout().setJustifyContent(PandaUI::Justify::Start);
    safeArea->layout().setPadding(PandaUI::Edge::Horizontal, 18.f);
    safeArea->layout().setPadding(PandaUI::Edge::Top, 72.f);
    safeArea->layout().setPadding(PandaUI::Edge::Bottom, 18.f);

    auto card = makeCard(440.f);
    card->layout().setPadding(PandaUI::Edge::Horizontal, 18.f);
    card->layout().setPadding(PandaUI::Edge::Vertical, 14.f);
    card->layout().setGap(8.f);

    // Keep the requested two actions in one compact window. The animated sky
    // and the sheep jump remain readable below it even on a short phone.
    auto title = makeLabel(
        text_.title,
        30.f,
        PandaUI::FontWeight::Bold,
        Colors::cream(),
        2,
        PandaUI::TextAlignment::Center
    );
    title->setLineSpacing(-2.f);
    title->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(title);

    primaryButton_ = makeButton(
        continueAvailable ? text_.continueButton : text_.playButton,
        PandaUI::ButtonStyle::Accent,
        54.f
    );
    primaryButton_->layout().setWidth(PandaUI::Length::percent(100.f));
    primaryButton_->setOnClick([action = std::move(playOrContinue)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(primaryButton_);

    auto levelsButton =
        makeButton(text_.levelSelectButton, PandaUI::ButtonStyle::AccentOutline, 50.f);
    levelsButton->layout().setWidth(PandaUI::Length::percent(100.f));
    levelsButton->setOnClick([action = std::move(levelSelect)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(levelsButton);

    safeArea->addSubview(card);

    // It deliberately lives outside the main card so the two primary navigation
    // actions remain visually grouped in one window.
    auto settingsButton = makeButton(text_.settingsButton, PandaUI::ButtonStyle::Neutral, 54.f);
    settingsButton->setFont(PandaUI::Font(13.f, PandaUI::FontWeight::Bold));
    settingsButton->setTooltip(text_.settingsTooltip);
    settingsButton->setHidden(!settings);
    settingsButton->layout().setPositionType(PandaUI::PositionType::Absolute);
    settingsButton->layout().setPosition(
        PandaUI::Edge::Top, PandaUI::Length::points(18.f)
    );
    settingsButton->layout().setPosition(
        PandaUI::Edge::Right, PandaUI::Length::points(18.f)
    );
    settingsButton->layout().setWidth(PandaUI::Length::points(54.f));
    if (settingsIcon) {
        settingsButton->getTitleLabel()->setHidden(true);
        settingsButton->layout().setAlignItems(PandaUI::Align::Center);
        settingsButton->layout().setJustifyContent(PandaUI::Justify::Center);

        auto icon = std::make_shared<PandaUI::ImageView>(settingsIcon);
        icon->setContentMode(PandaUI::ImageContentMode::Fit);
        icon->setUserInteractionEnabled(false);
        icon->layout().setWidth(PandaUI::Length::points(34.f));
        icon->layout().setHeight(PandaUI::Length::points(34.f));
        settingsButton->addSubview(icon);
    }
    settingsButton->setOnClick([action = std::move(settings)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    safeArea->addSubview(settingsButton);
    addSubview(safeArea);
}

MainMenuView::MainMenuView(
    Action playOrContinue, Action levelSelect, bool continueAvailable
)
    : MainMenuView(
          MainMenuText{},
          std::move(playOrContinue),
          std::move(levelSelect),
          Action{},
          continueAvailable,
          PandaUI::TextureHandle{}
      ) {}

void MainMenuView::setContinueAvailable(bool continueAvailable) {
    if (primaryButton_) {
        primaryButton_->setText(
            continueAvailable ? text_.continueButton : text_.playButton
        );
    }
}

} // namespace LiberateTheSheep::UI
