#include "UI/SheepToggle.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

constexpr float kDuration = .22f;
constexpr float kPi = 3.14159265358979323846f;

float ease(float value) {
    return value * value * (3.f - 2.f * value);
}

PandaUI::Color mix(PandaUI::Color from, PandaUI::Color to, float amount) {
    return {
        from.r + (to.r - from.r) * amount, from.g + (to.g - from.g) * amount,
        from.b + (to.b - from.b) * amount, from.a + (to.a - from.a) * amount
    };
}

bool sameColor(PandaUI::Color left, PandaUI::Color right) {
    return left.r == right.r && left.g == right.g && left.b == right.b && left.a == right.a;
}

float advance(float progress, float delta) {
    const float next = std::min(1.f, progress + delta / kDuration);
    return next > .999f ? 1.f : next;
}

} // namespace

SheepToggle::SheepToggle(bool initialOn) {
    setBackgroundColor(PandaUI::Color(0x00000000));
    surface().setBorderWidth(0.f);
    surface().setShadowColor(PandaUI::Color(0x00000000));
    setFocusable(true);
    setCursor(PandaUI::Cursor::PointingHand);
    layout().setWidth(PandaUI::Length::points(104.f));
    layout().setHeight(PandaUI::Length::points(52.f));
    layout().setMinWidth(PandaUI::Length::points(104.f));
    layout().setMinHeight(PandaUI::Length::points(52.f));
    layout().setFlexShrink(0.f);

    setSelected(initialOn);
    position_ = startPosition_ = targetPosition_ = initialOn ? 1.f : 0.f;
    movementProgress_ = 1.f;
    refreshAppearance();
    track_ = startTrack_ = targetTrack_;
    colorProgress_ = 1.f;
}

void SheepToggle::setOn(bool on) {
    setSelected(on);
}

void SheepToggle::setOnValueChanged(ValueChangedCallback callback) {
    onValueChanged_ = std::move(callback);
}

void SheepToggle::performPrimaryAction() {
    if (!isEnabled()) {
        return;
    }
    // An inherited action or the settings callback may remove this page.
    // Keep the control alive through the complete event dispatch.
    const auto keepAlive = weak_from_this().lock();
    setOn(!isOn());
    auto callback = onValueChanged_;
    PandaUI::Control::performPrimaryAction();
    sendActions(PandaUI::ControlEvent::ValueChanged);
    // Settings callbacks may replace the page. Finish all member work first,
    // and keep the callable itself alive independently of this control.
    if (callback) {
        callback(*this);
    }
}

void SheepToggle::controlStateChanged(
    PandaUI::ControlState previousState, PandaUI::ControlState currentState
) {
    if (PandaUI::contains(previousState, PandaUI::ControlState::Selected) !=
        PandaUI::contains(currentState, PandaUI::ControlState::Selected)) {
        startPosition_ = position_;
        targetPosition_ = isOn() ? 1.f : 0.f;
        startHop_ = hop_;
        startTilt_ = tilt_;
        movementProgress_ = 0.f;
    }
    refreshAppearance();
    setNeedsDisplay();
}

void SheepToggle::themeChanged() {
    refreshAppearance();
    setNeedsDisplay();
}

void SheepToggle::refreshAppearance() {
    const auto &theme = PandaUI::theme();
    const auto next = !isEnabled() ? PandaUI::Color(0x454545FF)
        : isOn() ? (isHighlighted() ? theme.accentPressed
                    : isHovered() ? theme.accentHover : theme.accent)
        : isHighlighted() ? PandaUI::Color(0x454545FF)
        : isHovered() ? PandaUI::Color(0x626262FF) : PandaUI::Color(0x555555FF);
    if (!sameColor(next, targetTrack_)) {
        startTrack_ = track_;
        targetTrack_ = next;
        colorProgress_ = 0.f;
    }
}

void SheepToggle::update(double deltaTime) {
    if ((movementProgress_ == 1.f && colorProgress_ == 1.f) || deltaTime <= 0.0) {
        return;
    }
    const float delta = static_cast<float>(std::min(deltaTime, .1));
    if (movementProgress_ < 1.f) {
        movementProgress_ = advance(movementProgress_, delta);
        const float blend = ease(movementProgress_);
        const float distance = targetPosition_ - startPosition_;
        position_ = startPosition_ + distance * blend;
        const float bounce = std::sin(kPi * movementProgress_);
        hop_ = std::min(3.5f,
            startHop_ * (1.f - blend) + 3.f * std::abs(distance) * bounce
        );
        tilt_ = startTilt_ * (1.f - blend) + .10f * distance * bounce;
        if (movementProgress_ == 1.f) {
            position_ = targetPosition_;
            hop_ = tilt_ = 0.f;
        }
    }
    if (colorProgress_ < 1.f) {
        colorProgress_ = advance(colorProgress_, delta);
        track_ = colorProgress_ == 1.f ? targetTrack_
            : mix(startTrack_, targetTrack_, ease(colorProgress_));
    }
    setNeedsDisplay();
}

void SheepToggle::draw(float, float) {
    if (!getContext()) {
        return;
    }
    const auto frame = getWorldFrame();
    const float scale = std::min({1.f, frame.size.width / 104.f, frame.size.height / 52.f});
    if (scale <= 0.f) {
        return;
    }
    const glm::vec3 origin(
        frame.origin.x + (frame.size.width - 104.f * scale) * .5f,
        frame.origin.y + (frame.size.height - 52.f * scale) * .5f, 0.f
    );
    auto transform = glm::scale(
        glm::translate(glm::mat4(1.f), origin), glm::vec3(scale, scale, 1.f)
    );
    auto &renderer = getContext()->getRenderer();
    const auto part = [&](float x, float y, float width, float height, float radius,
                          PandaUI::Color color, float rotation = 0.f) {
        PandaUI::UIRenderer::RectData shape;
        shape.size = {width, height};
        shape.color = color;
        shape.transform = glm::rotate(
            glm::translate(transform, glm::vec3(x, y, 0.f)), rotation,
            glm::vec3(0.f, 0.f, 1.f)
        );
        renderer.drawRoundedRect(shape, radius);
    };

    part(52.f, 27.f, 96.f, 38.f, 19.f, PandaUI::Color(0x00000024));
    part(52.f, 26.f, 96.f, 38.f, 19.f, track_);

    PandaUI::UIRenderer::RectData outline;
    outline.center = {52.f, 26.f, 0.f};
    outline.size = {96.f, 38.f};
    outline.transform = transform;
    outline.color = PandaUI::Color(0xFFFFFF24);
    renderer.drawRoundedRectBorder(outline, 19.f, 1.f);
    if (isFocused()) {
        outline.size = {102.f, 44.f};
        outline.color = isEnabled() ? PandaUI::theme().focusRing : PandaUI::Color(0xB0B0B0FF);
        renderer.drawRoundedRectBorder(outline, 22.f, 1.5f);
    }

    const float sheepX = 26.f + 52.f * position_;
    part(sheepX - 1.f, 40.f, 28.f - hop_, 3.f, 1.5f, PandaUI::Color(0x00000025));
    transform = glm::rotate(
        glm::translate(transform, glm::vec3(sheepX, 25.f - hop_, 0.f)), tilt_,
        glm::vec3(0.f, 0.f, 1.f)
    );
    const auto wool = isEnabled() ? PandaUI::Color(0xFFFFFFFF) : PandaUI::Color(0xC9CBCDFF);
    const auto fleeceShade = isEnabled() ? PandaUI::Color(0xE4E8EBFF) : PandaUI::Color(0xAFB2B5FF);
    const auto face = isEnabled() ? PandaUI::Color(0x444B55FF) : PandaUI::Color(0x686C72FF);

    part(-10.f, 9.f, 3.5f, 10.f, 1.7f, face, -.08f);
    part(4.f, 9.f, 3.5f, 10.f, 1.7f, face, .08f);
    part(-17.f, -2.f, 5.f, 7.f, 2.5f, wool, -.45f);
    part(9.f, -9.f, 5.f, 9.f, 2.5f, face, -.65f);
    part(17.f, -6.f, 5.f, 8.f, 2.5f, face, .65f);
    part(-3.f, 0.f, 28.f, 22.f, 10.f, fleeceShade);
    part(-3.f, -1.f, 27.f, 21.f, 10.f, wool);
    for (const PandaUI::Point tuft : {
        PandaUI::Point(-13.f, -5.f), PandaUI::Point(-10.f, -10.f),
        PandaUI::Point(-3.f, -11.f), PandaUI::Point(4.f, -9.f),
        PandaUI::Point(8.f, -3.f), PandaUI::Point(5.f, 4.f),
        PandaUI::Point(-2.f, 6.f), PandaUI::Point(-9.f, 5.f),
        PandaUI::Point(-14.f, 1.f)
    }) {
        part(tuft.x, tuft.y, 10.f, 10.f, 5.f, wool);
    }
    part(12.f, -.5f, 10.f, 15.f, 5.f, face, -.12f);
    part(9.f, -8.f, 8.f, 6.f, 3.f, wool, -.15f);
    part(14.f, -3.f, 2.5f, 2.5f, 1.25f, PandaUI::Color(0xFFFFFFFF));
    part(14.4f, -2.9f, 1.2f, 1.2f, .6f, PandaUI::Color(0x232930FF));
    part(15.f, 3.f, 2.5f, 1.f, .5f, PandaUI::Color(0xAEB8C2FF));
}

} // namespace LiberateTheSheep::UI
