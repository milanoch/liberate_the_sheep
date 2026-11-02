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
    return PandaUI::Color(0x30291FFF);
}

PandaUI::Color cream() {
    return PandaUI::Color(0xFFF4DEFF);
}

PandaUI::Color mutedCream() {
    return PandaUI::Color(0xD8CDBAFF);
}

PandaUI::Color meadow() {
    return PandaUI::Color(0xA5BA8DFF);
}

PandaUI::Color sunlight() {
    return PandaUI::Color(0xEDC97BFF);
}

PandaUI::Color danger() {
    return PandaUI::Color(0xF09A8EFF);
}

PandaUI::Color card() {
    return menuCard();
}

PandaUI::Color cardSoft() {
    return PandaUI::Color(0xFFFFFF0C);
}

PandaUI::Color menuCard() {
    return PandaUI::Color(0x292929D6);
}

PandaUI::Color scrim() {
    return PandaUI::Color(0x10101099);
}

PandaUI::Color border() { return PandaUI::Color(0xFFF1D52E); }
PandaUI::Color focus() { return PandaUI::Color(0xF5D599E6); }
PandaUI::Color control() { return menuCard(); }
PandaUI::Color controlHover() { return PandaUI::Color(0x494744E8); }
PandaUI::Color controlPressed() { return PandaUI::Color(0x242424EB); }

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
    applyGameButtonStyle(*button, style, height);
    return button;
}

PandaUI::ButtonAppearance gameButtonAppearance(PandaUI::ButtonStyle style, float height) {
    using namespace PandaUI;
    ButtonAppearance look;
    const auto plate = [](Color top, Color bottom, Color text, Color borderColor) {
        ButtonAppearance::State state;
        state.surface = SurfaceStyle(top, borderColor).withGradient(SurfaceGradient(top, bottom));
        state.text = text;
        return state;
    };
    look.normal = plate(Color(0x3B3A38D6), Colors::control(), Colors::cream(), Colors::border());
    look.hovered = plate(Color(0x55514BE8), Colors::controlHover(), Colors::cream(), Color(0xFFF1D55C));
    look.pressed = plate(Colors::controlPressed(), Color(0x30302DEB), Colors::cream(), Color(0xFFF1D540));
    switch (style) {
    case ButtonStyle::Accent:
        look.normal = plate(Color(0xF5D89CFF), Color(0xD8AF66FF), Colors::ink(), Color(0xFFF1C999));
        look.hovered = plate(Color(0xFFE8B9FF), Color(0xEDC781FF), Colors::ink(), Color(0xFFF4DACC));
        look.pressed = plate(Color(0xCAA15EFF), Color(0xDBB571FF), Colors::ink(), Color(0xB88C49FF));
        break;
    case ButtonStyle::AccentOutline:
        look.normal.text = Colors::sunlight();
        look.normal.surface.border = Color(0xEDC97B80);
        look.hovered.text = Color(0xFFE5ACFF);
        look.hovered.surface.border = Colors::sunlight();
        look.pressed.text = Colors::sunlight();
        break;
    case ButtonStyle::Destructive:
        look.normal = plate(Color(0xBF6259FF), Color(0x9F4844FF), Colors::cream(), Color(0xF5A99A88));
        look.hovered = plate(Color(0xD97C70FF), Color(0xB5564DFF), Colors::cream(), Color(0xFFD0BD99));
        look.pressed = plate(Color(0x8D3C39FF), Color(0xA74C46FF), Colors::cream(), Color(0xD9897D80));
        break;
    case ButtonStyle::Link:
        look.normal = {SurfaceStyle(Colors::transparent()), Colors::sunlight()};
        look.hovered = {SurfaceStyle(Color(0xFFF1D510)), Color(0xFFE5ACFF)};
        look.pressed = {SurfaceStyle(Color(0x00000020)), Colors::sunlight()};
        break;
    case ButtonStyle::Neutral:
        break;
    }
    look.focused = look.normal;
    look.focused.surface.border = Colors::focus();
    look.disabled = plate(Color(0x333333A8), Color(0x292929A8),
                          Color(0xC0B8A6A6), Color(0xFFF1D514));
    look.font = Font(16.f, FontWeight::Bold);
    look.padding = EdgeInsets(10.f, Spacing::xl, Spacing::xl, 10.f);
    look.minHeight = height;
    look.cornerRadius = CornerRadii(12.f);
    look.borderWidth = 1.f;
    look.titleShadow = TextShadow(Point(0.f, 1.f), Color(0x00000026));
    return look;
}

void applyGameButtonStyle(PandaUI::Button &button, PandaUI::ButtonStyle style, float height) {
    button.setStyle(style);
    button.setAppearance(gameButtonAppearance(style, height));
    button.layout().setHeight(PandaUI::Length::points(height));
    button.layout().setMinHeight(PandaUI::Length::points(height));
    button.layout().setFlexShrink(0.f);
    button.surface().setShadow(PandaUI::Shadow::none);
}

std::shared_ptr<PandaUI::Panel> makeCard(float maxWidth) {
    auto card = std::make_shared<PandaUI::Panel>();
    card->setBackgroundColor(Colors::card());
    card->surface().setBackgroundGradient(PandaUI::SurfaceGradient(
        PandaUI::Color(0x353535D6), Colors::card()
    ));
    card->layout().setWidth(PandaUI::Length::percent(100.f));
    card->layout().setMaxWidth(PandaUI::Length::points(maxWidth));
    card->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    card->layout().setPadding(PandaUI::Edge::Horizontal, PandaUI::Spacing::xxl);
    card->layout().setPadding(PandaUI::Edge::Vertical, PandaUI::Spacing::xxl);
    card->layout().setGap(PandaUI::Spacing::xl);
    card->surface().setCornerRadius(20.f);
    card->surface().setBorderWidth(1.f);
    card->surface().setBorderColor(Colors::border());
    card->surface().setShadow(PandaUI::Shadow::modal);
    return card;
}

std::shared_ptr<PandaUI::Panel> makePill() {
    auto pill = std::make_shared<PandaUI::Panel>();
    pill->setBackgroundColor(Colors::cardSoft());
    pill->surface().setBackgroundGradient(PandaUI::SurfaceGradient(
        PandaUI::Color(0xFFFFFF10), Colors::cardSoft()
    ));
    pill->layout().setPadding(PandaUI::Edge::Horizontal, PandaUI::Spacing::lg);
    pill->layout().setPadding(PandaUI::Edge::Vertical, PandaUI::Spacing::md);
    pill->surface().setCornerRadius(10.f);
    pill->surface().setBorderWidth(1.f);
    pill->surface().setBorderColor(PandaUI::Color(0xFFF1D51F));
    return pill;
}

} // namespace LiberateTheSheep::UI
