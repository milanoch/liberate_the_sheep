#include "UI/UIStyle.hpp"

#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    PandaUI::View *hitTestChildrenOnly(PandaUI::View &view, PandaUI::Point windowPosition) {
        if (view.isHidden() || !view.isEnabled() || !view.isUserInteractionEnabled() ||
            !view.pointInside(windowPosition)) {
            return nullptr;
        }

        const auto &subviews = view.getSubviews();
        for (auto it = subviews.rbegin(); it != subviews.rend(); ++it) {
            if (PandaUI::View *hit = (*it)->hitTest(windowPosition)) {
                return hit;
            }
        }
        return nullptr;
    }

} // namespace

namespace Colors {

PandaUI::Color transparent() {
    return PandaUI::Color(0x00000000);
}

PandaUI::Color ink() {
    return PandaUI::Color(0x19251FFF);
}

PandaUI::Color cream() {
    return PandaUI::Color(0xFFF9E9FF);
}

PandaUI::Color mutedCream() {
    return PandaUI::Color(0xD7DDCFFF);
}

PandaUI::Color meadow() {
    return PandaUI::Color(0x72C66BFF);
}

PandaUI::Color sunlight() {
    return PandaUI::Color(0xF4C95DFF);
}

PandaUI::Color danger() {
    return PandaUI::Color(0xEF7770FF);
}

PandaUI::Color card() {
    return PandaUI::Color(0x19251FF0);
}

PandaUI::Color cardSoft() {
    return PandaUI::Color(0x27372FD9);
}

PandaUI::Color scrim() {
    return PandaUI::Color(0x08100BCC);
}

} // namespace Colors

PandaUI::View *PassThroughPanel::hitTest(PandaUI::Point windowPosition) {
    return hitTestChildrenOnly(*this, windowPosition);
}

PandaUI::View *PassThroughSafeAreaView::hitTest(PandaUI::Point windowPosition) {
    return hitTestChildrenOnly(*this, windowPosition);
}

std::shared_ptr<PandaUI::Label> makeLabel(
    std::string text,
    float fontSize,
    PandaUI::FontWeight weight,
    PandaUI::Color color,
    int numberOfLines,
    PandaUI::TextAlignment alignment
) {
    auto label = std::make_shared<PandaUI::Label>(std::move(text));
    label->setFont(PandaUI::Font(fontSize, weight));
    label->setTextColor(color);
    label->setNumberOfLines(numberOfLines);
    label->setTextAlignment(alignment);
    label->setVerticalTextAlignment(PandaUI::TextVerticalAlignment::Center);
    return label;
}

std::shared_ptr<PandaUI::Button>
makeButton(std::string title, PandaUI::ButtonStyle style, float height) {
    auto button = std::make_shared<PandaUI::Button>(std::move(title));
    button->setStyle(style);
    button->setFont(PandaUI::Font(17.f, PandaUI::FontWeight::Bold));
    button->layout().setHeight(PandaUI::Length::points(height));
    button->layout().setMinHeight(PandaUI::Length::points(44.f));
    button->surface().setCornerRadius(15.f);
    button->surface().setShadowColor(PandaUI::Color(0x00000038));
    button->surface().setShadowOffset(PandaUI::Point(0.f, 3.f));
    button->surface().setShadowRadius(8.f);
    return button;
}

std::shared_ptr<PandaUI::Panel> makeCard(float maxWidth) {
    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(Colors::card());
    card->layout().setWidth(PandaUI::Length::percent(100.f));
    card->layout().setMaxWidth(PandaUI::Length::points(maxWidth));
    card->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    card->layout().setPadding(PandaUI::Edge::Horizontal, 22.f);
    card->layout().setPadding(PandaUI::Edge::Vertical, 22.f);
    card->layout().setGap(14.f);
    card->surface().setCornerRadius(24.f);
    card->surface().setBorderWidth(1.f);
    card->surface().setBorderColor(PandaUI::Color(0xFFFFFF24));
    card->surface().setShadowColor(PandaUI::Color(0x00000066));
    card->surface().setShadowOffset(PandaUI::Point(0.f, 8.f));
    card->surface().setShadowRadius(24.f);
    return card;
}

std::shared_ptr<PandaUI::Panel> makePill() {
    auto pill = std::make_shared<PandaUI::Panel>();
    pill->setBackgroundColor(Colors::cardSoft());
    pill->layout().setPadding(PandaUI::Edge::Horizontal, 10.f);
    pill->layout().setPadding(PandaUI::Edge::Vertical, 8.f);
    pill->surface().setCornerRadius(12.f);
    pill->surface().setBorderWidth(1.f);
    pill->surface().setBorderColor(PandaUI::Color(0xFFFFFF1F));
    return pill;
}

} // namespace LiberateTheSheep::UI
