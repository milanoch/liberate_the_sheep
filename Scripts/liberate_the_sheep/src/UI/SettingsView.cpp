#include "UI/SettingsView.hpp"

#include "UI/UIStyle.hpp"

#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    std::shared_ptr<PandaUI::Panel> makeSettingSection() {
        auto section = std::make_shared<PandaUI::Panel>();
        section->setBackgroundColor(Colors::cardSoft());
        section->layout().setWidth(PandaUI::Length::percent(100.f));
        section->layout().setFlexDirection(PandaUI::FlexDirection::Column);
        section->layout().setPadding(PandaUI::Edge::All, 12.f);
        section->layout().setGap(8.f);
        section->surface().setCornerRadius(14.f);
        section->surface().setBorderWidth(1.f);
        section->surface().setBorderColor(PandaUI::Color(0xFFFFFF1A));
        return section;
    }

    std::shared_ptr<PandaUI::Panel> makeHeader(
        const std::string &title, const std::string &description
    ) {
        auto header = std::make_shared<PandaUI::Panel>();
        header->setBackgroundColor(Colors::transparent());
        header->layout().setFlexDirection(PandaUI::FlexDirection::Column);
        header->layout().setGap(3.f);

        auto titleLabel = makeLabel(
            title, 14.f, PandaUI::FontWeight::Bold, Colors::cream(), 1
        );
        titleLabel->setLetterSpacing(0.5f);
        titleLabel->layout().setWidth(PandaUI::Length::percent(100.f));
        header->addSubview(titleLabel);

        auto descriptionLabel = makeLabel(
            description,
            12.f,
            PandaUI::FontWeight::Regular,
            Colors::mutedCream(),
            2
        );
        descriptionLabel->layout().setWidth(PandaUI::Length::percent(100.f));
        header->addSubview(descriptionLabel);
        return header;
    }

} // namespace

SettingsView::SettingsView(
    SettingsText text,
    MenuLanguage language,
    bool musicEnabled,
    LanguageChangedAction languageChanged,
    MusicChangedAction musicChanged,
    Action resetProgress,
    Action back
)
    : text_(std::move(text)),
      language_(language),
      resetProgress_(std::move(resetProgress)) {
    setBackgroundColor(Colors::transparent());
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));
    layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto safeArea = std::make_shared<PandaUI::SafeAreaView>();
    safeArea->setBackgroundColor(Colors::transparent());
    safeArea->layout().setWidth(PandaUI::Length::percent(100.f));
    safeArea->layout().setFlexGrow(1.f);
    safeArea->layout().setFlexDirection(PandaUI::FlexDirection::Column);

    auto scrollView = std::make_shared<PandaUI::ScrollView>();
    scrollView->setBackgroundColor(Colors::transparent());
    scrollView->setScrollDirection(PandaUI::ScrollDirection::Vertical);
    scrollView->setAutomaticallyAdjustsContentSize(true);
    scrollView->setScrollEnabled(true);
    scrollView->setBounces(true);
    scrollView->layout().setWidth(PandaUI::Length::percent(100.f));
    scrollView->layout().setFlexGrow(1.f);

    auto scrollContent = scrollView->getContentView();
    scrollContent->setBackgroundColor(Colors::transparent());
    scrollContent->layout().setWidth(PandaUI::Length::percent(100.f));
    scrollContent->layout().setMinHeight(PandaUI::Length::percent(100.f));
    scrollContent->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    scrollContent->layout().setAlignItems(PandaUI::Align::Center);
    scrollContent->layout().setPadding(PandaUI::Edge::Horizontal, 14.f);
    scrollContent->layout().setPadding(PandaUI::Edge::Vertical, 14.f);

    // Equal flexible spacers center the card on a tall screen. They collapse to
    // zero on compact portrait screens so every row remains reachable by scroll.
    const auto makeFlexibleSpacer = [] {
        auto spacer = std::make_shared<PandaUI::Panel>();
        spacer->setBackgroundColor(Colors::transparent());
        spacer->layout().setFlexGrow(1.f);
        spacer->layout().setFlexShrink(1.f);
        spacer->layout().setFlexBasis(0.f);
        return spacer;
    };

    scrollView->addContentSubview(makeFlexibleSpacer());

    auto card = makeCard(500.f);
    card->layout().setFlexShrink(0.f);
    card->layout().setPadding(PandaUI::Edge::Horizontal, 16.f);
    card->layout().setPadding(PandaUI::Edge::Vertical, 18.f);
    card->layout().setGap(10.f);

    auto title = makeLabel(
        text_.title,
        28.f,
        PandaUI::FontWeight::Bold,
        Colors::cream(),
        1,
        PandaUI::TextAlignment::Center
    );
    title->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(title);

    auto subtitle = makeLabel(
        text_.subtitle,
        13.f,
        PandaUI::FontWeight::Regular,
        Colors::mutedCream(),
        2,
        PandaUI::TextAlignment::Center
    );
    subtitle->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(subtitle);

    auto languageSection = makeSettingSection();
    languageSection->addSubview(
        makeHeader(text_.languageTitle, text_.languageDescription)
    );

    auto languageButtons = std::make_shared<PandaUI::Panel>();
    languageButtons->setBackgroundColor(Colors::transparent());
    languageButtons->layout().setWidth(PandaUI::Length::percent(100.f));
    languageButtons->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    languageButtons->layout().setGap(10.f);

    russianButton_ = makeButton(text_.russianButton, PandaUI::ButtonStyle::AccentOutline, 46.f);
    russianButton_->layout().setFlexGrow(1.f);
    russianButton_->layout().setFlexBasis(0.f);
    russianButton_->setOnClick(
        [this, action = languageChanged](PandaUI::Button &) {
            setLanguage(MenuLanguage::Russian);
            if (action) {
                action(MenuLanguage::Russian);
            }
        }
    );
    languageButtons->addSubview(russianButton_);

    englishButton_ = makeButton(text_.englishButton, PandaUI::ButtonStyle::AccentOutline, 46.f);
    englishButton_->layout().setFlexGrow(1.f);
    englishButton_->layout().setFlexBasis(0.f);
    englishButton_->setOnClick(
        [this, action = std::move(languageChanged)](PandaUI::Button &) {
            setLanguage(MenuLanguage::English);
            if (action) {
                action(MenuLanguage::English);
            }
        }
    );
    languageButtons->addSubview(englishButton_);
    languageSection->addSubview(languageButtons);
    card->addSubview(languageSection);

    auto musicSection = makeSettingSection();
    musicSection->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    musicSection->layout().setAlignItems(PandaUI::Align::Center);
    auto musicHeader = makeHeader(text_.musicTitle, text_.musicDescription);
    musicHeader->layout().setFlexGrow(1.f);
    musicHeader->layout().setFlexBasis(0.f);
    musicSection->addSubview(musicHeader);

    musicToggle_ = std::make_shared<PandaUI::Toggle>();
    musicToggle_->setOn(musicEnabled);
    musicToggle_->setOnValueChanged(
        [action = std::move(musicChanged)](PandaUI::Toggle &toggle) {
            if (action) {
                action(toggle.isOn());
            }
        }
    );
    musicSection->addSubview(musicToggle_);
    card->addSubview(musicSection);

    auto resetSection = makeSettingSection();
    resetSection->addSubview(makeHeader(text_.resetTitle, text_.resetDescription));

    // Keep a stable slot for the warning. Revealing it does not push the
    // confirmation button below the viewport after the first tap.
    auto resetConfirmationSlot = std::make_shared<PandaUI::Panel>();
    resetConfirmationSlot->setBackgroundColor(Colors::transparent());
    resetConfirmationSlot->layout().setWidth(PandaUI::Length::percent(100.f));
    resetConfirmationSlot->layout().setHeight(PandaUI::Length::points(44.f));
    resetConfirmationSlot->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    resetConfirmationSlot->layout().setJustifyContent(PandaUI::Justify::Center);
    resetConfirmationSlot->layout().setAlignItems(PandaUI::Align::Center);

    resetHint_ = makeLabel(
        text_.resetConfirmHint,
        12.f,
        PandaUI::FontWeight::Medium,
        Colors::danger(),
        3,
        PandaUI::TextAlignment::Center
    );
    resetHint_->layout().setWidth(PandaUI::Length::percent(100.f));
    resetHint_->setHidden(true);
    resetConfirmationSlot->addSubview(resetHint_);
    resetSection->addSubview(resetConfirmationSlot);

    resetButton_ = makeButton(text_.resetButton, PandaUI::ButtonStyle::Neutral, 48.f);
    resetButton_->layout().setWidth(PandaUI::Length::percent(100.f));
    resetButton_->getTitleLabel()->setTextColor(Colors::danger());
    resetButton_->setOnClick([this](PandaUI::Button &) { handleReset(); });
    resetSection->addSubview(resetButton_);
    card->addSubview(resetSection);

    auto backButton = makeButton(text_.backButton, PandaUI::ButtonStyle::AccentOutline, 50.f);
    backButton->layout().setWidth(PandaUI::Length::percent(100.f));
    backButton->setOnClick([action = std::move(back)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    card->addSubview(backButton);

    scrollView->addContentSubview(card);
    scrollView->addContentSubview(makeFlexibleSpacer());
    safeArea->addSubview(scrollView);
    addSubview(safeArea);

    setLanguage(language_);
}

void SettingsView::setLanguage(MenuLanguage language) {
    language_ = language;
    if (russianButton_) {
        russianButton_->setStyle(
            language == MenuLanguage::Russian ? PandaUI::ButtonStyle::Accent
                                              : PandaUI::ButtonStyle::AccentOutline
        );
    }
    if (englishButton_) {
        englishButton_->setStyle(
            language == MenuLanguage::English ? PandaUI::ButtonStyle::Accent
                                              : PandaUI::ButtonStyle::AccentOutline
        );
    }
}

void SettingsView::setMusicEnabled(bool enabled) {
    if (musicToggle_) {
        musicToggle_->setOn(enabled);
    }
}

void SettingsView::handleReset() {
    if (!resetArmed_) {
        resetArmed_ = true;
        resetHint_->setHidden(false);
        resetButton_->setText(text_.resetConfirmButton);
        resetButton_->setStyle(PandaUI::ButtonStyle::AccentOutline);
        resetButton_->getTitleLabel()->setTextColor(Colors::danger());
        return;
    }

    resetArmed_ = false;
    resetHint_->setHidden(true);
    resetButton_->setText(text_.resetButton);
    resetButton_->setStyle(PandaUI::ButtonStyle::Neutral);
    resetButton_->getTitleLabel()->setTextColor(Colors::danger());
    if (resetProgress_) {
        resetProgress_();
    }
}

} // namespace LiberateTheSheep::UI
