#include "UI/RescueNoticeView.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

constexpr float kFadeInDuration = .24f;
constexpr float kFadeOutDuration = .50f;
constexpr float kDuration = 3.8f;

float smoothFade(float value) {
    const float t = std::clamp(value, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

PandaUI::Color faded(PandaUI::Color color, float opacity) {
    color.a *= opacity;
    return color;
}

} // namespace

RescueNoticeView::RescueNoticeView(std::string title, std::string message) {
    setBackgroundColor(Colors::transparent());
    setUserInteractionEnabled(false);
    layoutSetAbsolute();
    layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));
    layout().setAlignItems(PandaUI::Align::Center);
    layout().setFlexDirection(PandaUI::FlexDirection::Column);
    layout().setPadding(PandaUI::Edge::Horizontal, 16.f);

    card_ = makeCard(390.f);
    card_->setUserInteractionEnabled(false);
    card_->layout().setMargin(PandaUI::Edge::Top, 154.f);
    card_->layout().setPadding(PandaUI::Edge::Horizontal, 18.f);
    card_->layout().setPadding(PandaUI::Edge::Vertical, 13.f);
    card_->layout().setGap(6.f);
    card_->layout().setFlexShrink(0.f);
    card_->surface().setCornerRadius(18.f);
    card_->surface().setShadowRadius(14.f);
    card_->surface().setShadowOffset(PandaUI::Point(0.f, 4.f));

    title_ = makeLabel(std::move(title), 15.f, PandaUI::FontWeight::Bold,
                      Colors::sunlight(), 2, PandaUI::TextAlignment::Center);
    title_->setLetterSpacing(.4f);
    title_->setUserInteractionEnabled(false);
    title_->layout().setWidth(PandaUI::Length::percent(100.f));
    card_->addSubview(title_);

    message_ = makeLabel(std::move(message), 14.f, PandaUI::FontWeight::Regular,
                        Colors::mutedCream(), 0, PandaUI::TextAlignment::Center);
    message_->setUserInteractionEnabled(false);
    message_->setLineSpacing(2.f);
    message_->layout().setWidth(PandaUI::Length::percent(100.f));
    card_->addSubview(message_);
    addSubview(card_);
    dismiss();
}

void RescueNoticeView::show() {
    // A second rescue refreshes the notice without a visible blink.
    initialOpacity_ = active_ ? opacity_ : 0.f;
    elapsed_ = 0.f;
    active_ = true;
    setHidden(false);
    applyOpacity(initialOpacity_);
}

void RescueNoticeView::dismiss() {
    active_ = false;
    elapsed_ = 0.f;
    initialOpacity_ = 0.f;
    applyOpacity(0.f);
    setHidden(true);
}

void RescueNoticeView::update(double deltaTime) {
    if (!active_ || !std::isfinite(deltaTime)) { return; }
    elapsed_ += static_cast<float>(std::clamp(deltaTime, 0.0, .1));
    if (elapsed_ >= kDuration) {
        dismiss();
        return;
    }
    const float fadeIn = smoothFade(elapsed_ / kFadeInDuration);
    const float fadeOut = smoothFade((kDuration - elapsed_) / kFadeOutDuration);
    applyOpacity((initialOpacity_ + (1.f - initialOpacity_) * fadeIn) * fadeOut);
}

void RescueNoticeView::applyOpacity(float opacity) {
    opacity_ = std::clamp(opacity, 0.f, 1.f);
    // Explicit colors also fade SDK-native text, which View::opacity does not
    // currently cover. Keep the original RGB values throughout the transition.
    card_->setBackgroundColor(faded(Colors::card(), opacity_));
    card_->surface().setBackgroundGradient(PandaUI::SurfaceGradient(
        faded(PandaUI::Color(0x353535D6), opacity_), faded(Colors::card(), opacity_)));
    card_->surface().setBorderColor(faded(Colors::border(), opacity_));
    card_->surface().setShadowColor(PandaUI::Color(0.f, 0.f, 0.f, .24f * opacity_));
    title_->setTextColor(faded(Colors::sunlight(), opacity_));
    message_->setTextColor(faded(Colors::mutedCream(), opacity_));
    setNeedsDisplay();
}

} // namespace LiberateTheSheep::UI
