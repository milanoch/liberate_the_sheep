#include "UI/BombSheepView.hpp"

#include "UI/UIStyle.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace LiberateTheSheep::UI {
namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kBurstDuration = .65f;

// Each square cell of the red/black atlas has its screen at (.503, .560).
// The tutorial and field share these coordinates and display geometry.
void drawDigits(PandaUI::UIRenderer &renderer, PandaUI::Point bombCenter,
                float bombSize, float worldRotationDegrees, int seconds, bool impact = false) {
    if (bombSize <= 0.f) { return; }
    seconds = std::clamp(seconds, 0, 5999);
    const std::array<int, 4> digits{
        seconds / 600, (seconds / 60) % 10, (seconds % 60) / 10, seconds % 10
    };
    constexpr std::array<unsigned char, 10> masks{
        0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f
    };
    struct Segment { float x; float y; float width; float height; };
    // Bits run clockwise from the top, then the middle bar.
    constexpr std::array<Segment, 7> segments{{
        {4.f, .8f, 5.4f, 1.4f}, {7.2f, 4.2f, 1.4f, 5.1f},
        {7.2f, 11.8f, 1.4f, 5.1f}, {4.f, 15.2f, 5.4f, 1.4f},
        {.8f, 11.8f, 1.4f, 5.1f}, {.8f, 4.2f, 1.4f, 5.1f},
        {4.f, 8.f, 5.4f, 1.4f}
    }};
    constexpr std::array<float, 4> digitX{0.f, 10.5f, 25.5f, 36.f};
    const float scale = impact ? bombSize * .145f / 16.f : bombSize * .36f / 44.f;
    const float originX = .003f * bombSize - (impact ? 4.f : 22.f) * scale;
    const float originY = .060f * bombSize - 8.f * scale;
    const auto transform = glm::rotate(
        glm::translate(glm::mat4(1.f), glm::vec3(bombCenter.x, bombCenter.y, 0.f)),
        -worldRotationDegrees * kPi / 180.f, glm::vec3(0.f, 0.f, 1.f));
    const auto color = impact ? PandaUI::Color(0xFFF2D7FF)
        : seconds <= 10 ? PandaUI::Color(0xFF6B53FF) : PandaUI::Color(0xF97865FF);
    const auto drawSegment = [&](float x, float y, float width, float height) {
        PandaUI::UIRenderer::RectData bar;
        bar.center = {originX + x * scale, originY + y * scale, 0.f};
        bar.size = {width * scale, height * scale};
        bar.transform = transform;
        bar.color = color;
        renderer.drawRoundedRect(bar, .32f * scale);
    };
    const std::size_t digitCount = impact ? 1u : digits.size();
    for (std::size_t digit = 0; digit < digitCount; ++digit) {
        const int value = impact ? std::clamp(seconds, 0, 9) : digits[digit];
        const auto mask = masks[static_cast<std::size_t>(value)];
        for (std::size_t segment = 0; segment < segments.size(); ++segment) {
            if ((mask & (1u << segment)) == 0u) { continue; }
            const auto &bar = segments[segment];
            drawSegment((impact ? 0.f : digitX[digit]) + bar.x, bar.y, bar.width, bar.height);
        }
    }
    if (!impact) {
        drawSegment(22.f, 5.f, 1.6f, 1.6f);
        drawSegment(22.f, 11.f, 1.6f, 1.6f);
    }
}

void drawDisc(PandaUI::UIRenderer &renderer, PandaUI::Point center,
              float radius, PandaUI::Color color) {
    PandaUI::UIRenderer::RectData disc;
    disc.center = {center.x, center.y, 0.f};
    disc.size = {radius * 2.f, radius * 2.f};
    disc.color = color;
    renderer.drawRoundedRect(disc, radius);
}
} // namespace

BombSheepPicture::BombSheepPicture() {
    setBackgroundColor(Colors::transparent());
    setUserInteractionEnabled(false);
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::points(136.f));
    layout().setFlexShrink(0.f);
}

void BombSheepPicture::setArtwork(PandaUI::TextureHandle sheep, PandaUI::TextureHandle bomb) {
    sheep_ = sheep;
    bomb_ = bomb;
    setNeedsDisplay();
}

void BombSheepPicture::setCoat(int coat) {
    coat_ = std::clamp(coat, 0, 2);
    setNeedsDisplay();
}

void BombSheepPicture::setSeconds(int seconds) {
    seconds_ = std::clamp(seconds, 0, 5999);
    setNeedsDisplay();
}

void BombSheepPicture::setImpactMode(bool impact) {
    impact_ = impact;
    setNeedsDisplay();
}

void BombSheepPicture::draw(float, float) {
    if (!getContext()) { return; }
    const auto frame = getWorldFrame();
    const float x = frame.origin.x + frame.size.width * .5f;
    const float y = frame.origin.y + frame.size.height * .5f;
    const float length = std::min(frame.size.height, 136.f);
    auto &renderer = getContext()->getRenderer();

    // V is reversed within the selected source cell, as in the menu atlas.
    if (sheep_) {
        PandaUI::UIRenderer::RectData sheep;
        sheep.center = {x, y, 0.f};
        sheep.size = {length * .625f, length};
        sheep.texture = sheep_;
        sheep.textureCoords = {static_cast<float>(coat_) / 3.f, 1.f, 1.f / 3.f, -1.f};
        sheep.color = PandaUI::Color(0xFFFFFFFF);
        renderer.drawRect(sheep);
    }
    if (bomb_) {
        const float size = length * (1.16f / 1.97f);
        const PandaUI::Point center(x, y + length * (.12f / 1.97f));
        PandaUI::UIRenderer::RectData bomb;
        bomb.center = {center.x, center.y, 0.f};
        bomb.size = {size, size};
        bomb.texture = bomb_;
        bomb.textureCoords = {impact_ ? .5f : 0.f, 1.f, .5f, -1.f};
        bomb.color = PandaUI::Color(0xFFFFFFFF);
        renderer.drawRect(bomb);
        drawDigits(renderer, center, size, 0.f, seconds_, impact_);
    }
}

BombFieldOverlay::BombFieldOverlay() {
    setBackgroundColor(Colors::transparent());
    setUserInteractionEnabled(false);
    setClipsToBounds(false);
    layoutSetAbsolute();
    layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));

}

void BombFieldOverlay::setMarker(PandaUI::Point center, float bombSize,
                                float rotationDegrees, int seconds, bool visible, bool impact) {
    displayCenter_ = center;
    displaySize_ = bombSize;
    displayRotation_ = rotationDegrees;
    displayedSeconds_ = std::clamp(seconds, 0, 5999);
    displayVisible_ = visible && bombSize > 0.f;
    displayImpact_ = impact;
    setNeedsDisplay();
}

void BombFieldOverlay::playBurst(PandaUI::Point center, bool harmless) {
    burstCenter_ = center;
    burstElapsed_ = 0.f;
    harmless_ = harmless;
    displayVisible_ = false;
    setNeedsDisplay();
}

void BombFieldOverlay::reset() {
    displayVisible_ = false;
    burstElapsed_ = kBurstDuration;
    displayedSeconds_ = 0;
    setNeedsDisplay();
}

void BombFieldOverlay::update(double deltaTime) {
    if (burstElapsed_ >= kBurstDuration) { return; }
    burstElapsed_ = std::min(kBurstDuration,
        burstElapsed_ + static_cast<float>(std::max(0.0, deltaTime)));
    setNeedsDisplay();
}

void BombFieldOverlay::draw(float, float) {
    if (!getContext()) { return; }
    auto &renderer = getContext()->getRenderer();
    if (displayVisible_) {
        drawDigits(renderer, displayCenter_, displaySize_, displayRotation_, displayedSeconds_,
                   displayImpact_);
    }
    if (burstElapsed_ >= kBurstDuration) { return; }
    const float t = burstElapsed_ / kBurstDuration;
    const float ease = 1.f - (1.f - t) * (1.f - t);
    const float alpha = (1.f - t) * (1.f - t);
    const float reach = harmless_ ? 25.f : 60.f;
    // One short local puff: no fullscreen strobe, gore or displaced sheep.
    if (!harmless_ && t < .25f) {
        drawDisc(renderer, burstCenter_, 12.f + 35.f * ease,
                 PandaUI::Color(1.f, .77f, .24f, (1.f - t * 4.f) * .9f));
    }
    for (int i = 0; i < 9; ++i) {
        const float angle = static_cast<float>(i) * (2.f * kPi / 9.f) + .13f;
        const float spread = reach * ease * (i % 2 == 0 ? 1.f : .76f);
        const PandaUI::Point center(
            burstCenter_.x + std::cos(angle) * spread,
            burstCenter_.y + std::sin(angle) * spread - 16.f * ease);
        const float shade = harmless_ ? .87f : .70f;
        drawDisc(renderer, center, (8.f + 15.f * ease) * (i % 2 == 0 ? 1.f : .8f),
                 PandaUI::Color(shade, shade - .03f, shade - .07f, alpha * .88f));
    }
}

} // namespace LiberateTheSheep::UI
