#include "UI/SettingsView.hpp"

#include "UI/SheepToggle.hpp"
#include "UI/UIStyle.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    const PandaUI::Color kText(0xF5F5F5FF);
    const PandaUI::Color kSecondaryText(0xC7C7C7FF);
    const PandaUI::Color kDangerText(0xFFAAA5FF);

    // Native ButtonStyle::AccentOutline installs a blue surface and blue text.
    // Keep the palette local instead of changing the editor's global UI theme.
    class SettingsButton final : public PandaUI::Button {
    public:
        explicit SettingsButton(std::string title, bool destructive = false)
            : PandaUI::Button(std::move(title)), destructive_(destructive) {
            layout().setHeight(PandaUI::Length::points(48.f));
            layout().setMinHeight(PandaUI::Length::points(48.f));
            layout().setFlexShrink(0.f);
            setFont(PandaUI::Font(15.f, PandaUI::FontWeight::Bold));
            surface().setCornerRadius(12.f);
            surface().setBorderWidth(1.f);
            surface().setShadowColor(Colors::transparent());
            refreshAppearance();
            fill_ = targetFill_;
            border_ = targetBorder_;
            text_ = targetText_;
            animating_ = false;
            applyAppearance();
        }

    protected:
        void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
            refreshAppearance();
        }

        void themeChanged() override {
            refreshAppearance();
        }

        void update(double deltaTime) override {
            if (!animating_ || deltaTime <= 0.0) {
                return;
            }
            const float blend = 1.f - std::exp(
                -static_cast<float>(std::clamp(deltaTime, 0.0, .1)) / .08f
            );
            const auto approach = [blend](PandaUI::Color &value, const PandaUI::Color &target) {
                const auto channel = [blend](float from, float to) {
                    const float next = from + (to - from) * blend;
                    return std::abs(next - to) < .001f ? to : next;
                };
                value = PandaUI::Color(
                    channel(value.r, target.r), channel(value.g, target.g),
                    channel(value.b, target.b), channel(value.a, target.a)
                );
                return value.r != target.r || value.g != target.g ||
                       value.b != target.b || value.a != target.a;
            };
            const bool fillMoving = approach(fill_, targetFill_);
            const bool borderMoving = approach(border_, targetBorder_);
            const bool textMoving = approach(text_, targetText_);
            animating_ = fillMoving || borderMoving || textMoving;
            applyAppearance();
        }

    private:
        void refreshAppearance() {
            const auto &theme = PandaUI::theme();
            targetFill_ = PandaUI::Color(0xFFFFFF0F);
            targetBorder_ = PandaUI::Color(0xFFFFFF24);
            targetText_ = kText;

            if (isSelected() && !destructive_) {
                // The same semantic accent as the sheep toggle's on state.
                targetFill_ = isHighlighted() ? theme.accentPressed
                              : isHovered() ? theme.accentHover : theme.accent;
                targetBorder_ = Colors::transparent();
                targetText_ = theme.onAccent;
            } else {
                if (isHighlighted()) {
                    targetFill_ = PandaUI::Color(0xFFFFFF09);
                } else if (isHovered()) {
                    targetFill_ = PandaUI::Color(0xFFFFFF1F);
                }
                if (destructive_ && isSelected()) {
                    // A clearly red confirmation, with white text for contrast.
                    targetFill_ = PandaUI::Color(isHighlighted() ? 0x9E3037FF
                                                 : isHovered() ? 0xD44953FF
                                                               : 0xBF3943FF);
                    targetBorder_ = PandaUI::Color(0xFFAAA566);
                    targetText_ = PandaUI::Color(0xFFFFFFFF);
                }
            }
            if (isFocused()) {
                targetBorder_ = PandaUI::Color(0xFFFFFFA0);
            }
            if (!isEnabled()) {
                targetFill_ = PandaUI::Color(0xFFFFFF08);
                targetBorder_ = PandaUI::Color(0xFFFFFF14);
                targetText_ = PandaUI::Color(0x999999FF);
            }
            animating_ = true;
        }

        void applyAppearance() {
            setBackgroundColor(fill_);
            surface().setBorderColor(border_);
            getTitleLabel()->setTextColor(text_);
            setNeedsDisplay();
        }

        bool destructive_;
        bool animating_ = false;
        PandaUI::Color fill_;
        PandaUI::Color border_;
        PandaUI::Color text_;
        PandaUI::Color targetFill_;
        PandaUI::Color targetBorder_;
        PandaUI::Color targetText_;
    };

    std::shared_ptr<PandaUI::Panel> makeDivider() {
        auto divider = std::make_shared<PandaUI::Panel>();
        divider->setBackgroundColor(PandaUI::Color(0xFFFFFF1A));
        divider->layout().setWidth(PandaUI::Length::percent(100.f));
        divider->layout().setHeight(PandaUI::Length::points(1.f));
        divider->layout().setFlexShrink(0.f);
        return divider;
    }

    std::shared_ptr<PandaUI::Panel> makeSettingSection() {
        auto section = std::make_shared<PandaUI::Panel>();
        // The outer card provides the shared menu tint. A second translucent
        // fill here would make the sections darker and less transparent.
        section->setBackgroundColor(Colors::transparent());
        section->layout().setWidth(PandaUI::Length::percent(100.f));
        section->layout().setFlexDirection(PandaUI::FlexDirection::Column);
        section->layout().setGap(10.f);
        section->layout().setFlexShrink(0.f);
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
            title, 15.f, PandaUI::FontWeight::Bold, kText, 2
        );
        titleLabel->layout().setWidth(PandaUI::Length::percent(100.f));
        header->addSubview(titleLabel);

        auto descriptionLabel = makeLabel(
            description,
            13.f,
            PandaUI::FontWeight::Regular,
            kSecondaryText,
            3
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
    bool immortalModeEnabled,
    LanguageChangedAction languageChanged,
    MusicChangedAction musicChanged,
    ImmortalModeChangedAction immortalModeChanged,
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
    // Keep the persistent settings gear clear even on compact portrait screens.
    scrollContent->layout().setPadding(PandaUI::Edge::Top, 84.f);
    scrollContent->layout().setPadding(PandaUI::Edge::Bottom, 14.f);

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
    card->setBackgroundColor(Colors::menuCard());
    card->surface().setShadowColor(PandaUI::Color(0x00000028));
    card->surface().setShadowOffset(PandaUI::Point(0.f, 3.f));
    card->surface().setShadowRadius(8.f);
    card->layout().setFlexShrink(0.f);
    card->layout().setPadding(PandaUI::Edge::All, 22.f);
    card->layout().setGap(16.f);

    auto heading = std::make_shared<PandaUI::Panel>();
    heading->setBackgroundColor(Colors::transparent());
    heading->layout().setWidth(PandaUI::Length::percent(100.f));
    heading->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    heading->layout().setGap(4.f);
    heading->layout().setFlexShrink(0.f);

    auto title = makeLabel(
        text_.title,
        28.f,
        PandaUI::FontWeight::Bold,
        kText,
        1,
        PandaUI::TextAlignment::Center
    );
    title->layout().setWidth(PandaUI::Length::percent(100.f));
    heading->addSubview(title);

    auto subtitle = makeLabel(
        text_.subtitle,
        13.f,
        PandaUI::FontWeight::Regular,
        kSecondaryText,
        2,
        PandaUI::TextAlignment::Center
    );
    subtitle->layout().setWidth(PandaUI::Length::percent(100.f));
    heading->addSubview(subtitle);
    card->addSubview(heading);

    auto languageSection = makeSettingSection();
    languageSection->addSubview(
        makeHeader(text_.languageTitle, text_.languageDescription)
    );

    auto languageButtons = std::make_shared<PandaUI::Panel>();
    languageButtons->setBackgroundColor(Colors::transparent());
    languageButtons->layout().setWidth(PandaUI::Length::percent(100.f));
    languageButtons->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    languageButtons->layout().setGap(10.f);

    russianButton_ = std::make_shared<SettingsButton>(text_.russianButton);
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

    englishButton_ = std::make_shared<SettingsButton>(text_.englishButton);
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
    card->addSubview(makeDivider());

    auto musicSection = makeSettingSection();
    musicSection->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    musicSection->layout().setAlignItems(PandaUI::Align::Center);
    musicSection->layout().setMinHeight(PandaUI::Length::points(56.f));
    auto musicHeader = makeHeader(text_.musicTitle, text_.musicDescription);
    musicHeader->layout().setFlexGrow(1.f);
    musicHeader->layout().setFlexBasis(0.f);
    musicHeader->layout().setMinWidth(PandaUI::Length::points(0.f));
    musicSection->addSubview(musicHeader);

    musicToggle_ = std::make_shared<SheepToggle>(musicEnabled);
    musicToggle_->setTooltip(text_.musicTitle);
    musicToggle_->setOnValueChanged(
        [action = std::move(musicChanged)](SheepToggle &toggle) {
            if (action) {
                action(toggle.isOn());
            }
        }
    );
    musicSection->addSubview(musicToggle_);
    card->addSubview(musicSection);

    auto immortalModeSection = makeSettingSection();
    immortalModeSection->layout().setFlexDirection(PandaUI::FlexDirection::Row);
    immortalModeSection->layout().setAlignItems(PandaUI::Align::Center);
    immortalModeSection->layout().setMinHeight(PandaUI::Length::points(56.f));
    auto immortalModeHeader =
        makeHeader(text_.immortalModeTitle, text_.immortalModeDescription);
    immortalModeHeader->layout().setFlexGrow(1.f);
    immortalModeHeader->layout().setFlexBasis(0.f);
    immortalModeHeader->layout().setMinWidth(PandaUI::Length::points(0.f));
    immortalModeSection->addSubview(immortalModeHeader);

    immortalModeToggle_ = std::make_shared<SheepToggle>(immortalModeEnabled);
    immortalModeToggle_->setTooltip(text_.immortalModeTitle);
    immortalModeToggle_->setOnValueChanged(
        [action = std::move(immortalModeChanged)](SheepToggle &toggle) {
            if (action) {
                action(toggle.isOn());
            }
        }
    );
    immortalModeSection->addSubview(immortalModeToggle_);
    card->addSubview(immortalModeSection);
    card->addSubview(makeDivider());

    auto resetSection = makeSettingSection();
    auto resetHeader = std::make_shared<PandaUI::Panel>();
    resetHeader->setBackgroundColor(Colors::transparent());
    resetHeader->layout().setWidth(PandaUI::Length::percent(100.f));
    resetHeader->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    resetHeader->layout().setGap(3.f);
    auto resetTitle = makeLabel(text_.resetTitle, 15.f, PandaUI::FontWeight::Bold, kText);
    resetTitle->layout().setWidth(PandaUI::Length::percent(100.f));
    resetHeader->addSubview(resetTitle);

    // Reuse the description's space for confirmation instead of leaving an
    // empty warning slot. Its fixed height keeps the button still when armed.
    resetHint_ = makeLabel(
        text_.resetDescription, 13.f, PandaUI::FontWeight::Regular,
        kSecondaryText, 2
    );
    resetHint_->layout().setWidth(PandaUI::Length::percent(100.f));
    resetHint_->layout().setHeight(PandaUI::Length::points(40.f));
    resetHint_->setVerticalTextAlignment(PandaUI::TextVerticalAlignment::Top);
    resetHeader->addSubview(resetHint_);
    resetSection->addSubview(resetHeader);

    resetButton_ = std::make_shared<SettingsButton>(text_.resetButton, true);
    resetButton_->layout().setWidth(PandaUI::Length::percent(100.f));
    resetButton_->setOnClick([this](PandaUI::Button &) { handleReset(); });
    resetSection->addSubview(resetButton_);
    card->addSubview(resetSection);

    auto backButton = std::make_shared<SettingsButton>(text_.backButton);
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
        russianButton_->setSelected(language == MenuLanguage::Russian);
    }
    if (englishButton_) {
        englishButton_->setSelected(language == MenuLanguage::English);
    }
}

void SettingsView::setMusicEnabled(bool enabled) {
    if (musicToggle_) {
        musicToggle_->setOn(enabled);
    }
}

void SettingsView::setImmortalModeEnabled(bool enabled) {
    if (immortalModeToggle_) {
        immortalModeToggle_->setOn(enabled);
    }
}

void SettingsView::handleReset() {
    if (!resetArmed_) {
        resetArmed_ = true;
        resetHint_->setText(text_.resetConfirmHint);
        resetHint_->setTextColor(kDangerText);
        resetButton_->setText(text_.resetConfirmButton);
        resetButton_->setSelected(true);
        return;
    }

    resetArmed_ = false;
    resetHint_->setText(text_.resetDescription);
    resetHint_->setTextColor(kSecondaryText);
    resetButton_->setText(text_.resetButton);
    resetButton_->setSelected(false);
    if (resetProgress_) {
        resetProgress_();
    }
}

} // namespace LiberateTheSheep::UI
