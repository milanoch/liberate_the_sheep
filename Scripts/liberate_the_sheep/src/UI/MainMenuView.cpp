#include "UI/MainMenuView.hpp"

#include "UI/UIStyle.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

PandaUI::Rect uprightAtlasRegion(float x, float y, float width, float height) {
    // drawRect maps the first V edge to the bottom of the quad. Source PNG
    // coordinates start at the top: reverse V INSIDE this crop, not the atlas.
    return {x / 1536.f, (y + height) / 1024.f, width / 1536.f, -height / 1024.f};
}

// Separate regions of an authored texture: no generated sheep geometry and
// no rapidly switched whole-body frames. Only the foreleg moves.
class SheepPlayButton final : public PandaUI::Control {
public:
    SheepPlayButton(std::string title, PandaUI::TextureHandle artwork)
        : artwork_(artwork) {
        setBackgroundColor(Colors::transparent());
        setClipsToBounds(false);
        setFocusable(true);
        setCursor(PandaUI::Cursor::PointingHand);
        layout().setWidth(PandaUI::Length::percent(100.f));
        layout().setMaxWidth(PandaUI::Length::points(500.f));
        layout().setHeight(PandaUI::Length::points(216.f));
        layout().setFlexShrink(0.f);

        // Authored short-fleece lettering, selected just like the localized
        // title. Keep ordinary text as a fallback for other languages/assets.
        if (title == "ИГРАТЬ") {
            titleRegion_ = {30.f, 696.f, 682.f, 238.f};
        } else if (title == "PLAY") {
            titleRegion_ = {984.f, 696.f, 528.f, 238.f};
        }
        label_ = makeLabel(
            std::move(title), 30.f, PandaUI::FontWeight::Bold,
            PandaUI::Color(0x4E6240FF), 1, PandaUI::TextAlignment::Center
        );
        label_->setHidden(artwork_ && titleRegion_.size.width > 0.f);
        label_->setUserInteractionEnabled(false);
        label_->layout().setPositionType(PandaUI::PositionType::Absolute);
        label_->layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::percent(23.f));
        label_->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::percent(32.f));
        label_->layout().setWidth(PandaUI::Length::percent(54.f));
        label_->layout().setHeight(PandaUI::Length::percent(36.f));
        addSubview(label_);
    }

    void viewDidCalculateLayout() override {
        // First layout supplies the responsive width; label layout changes
        // only on resize, not on animation ticks.
        const auto size = getFrame().size;
        if (size.width != previousSize_.width || size.height != previousSize_.height) {
            previousSize_ = size;
            label_->setFont(PandaUI::Font(
                std::clamp(size.width * .073f, 18.f, 34.f), PandaUI::FontWeight::Bold
            ));
        }
    }

    void update(double deltaTime) override {
        const double step = std::clamp(deltaTime, 0.0, .1);
        elapsed_ = std::fmod(elapsed_ + step, 4.2);
        const float targetTint = isHighlighted() ? .91f : 1.f;
        pressTint_ += (targetTint - pressTint_) * static_cast<float>(1.0 - std::exp(-step / .06));
        setNeedsDisplay();
    }

protected:
    void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
        if (label_) {
            label_->setTextColor(isHighlighted() ? PandaUI::Color(0x30482AFF)
                                                 : PandaUI::Color(0x4E6240FF));
        }
        setNeedsDisplay();
    }

    void draw(float, float) override {
        if (!getContext()) {
            return;
        }
        const auto frame = getWorldFrame();
        auto art = artworkRect(frame.size);
        art.origin.x += frame.origin.x;
        art.origin.y += frame.origin.y;
        auto &renderer = getContext()->getRenderer();

        PandaUI::UIRenderer::RectData body;
        // Longer resting legs extend below the original layout region. Keep
        // its pixel scale/origin for the label and shoulder, only reveal more
        // of the atlas (the separate waving hoof starts below this crop).
        body.size = PandaUI::Size(art.size.width, art.size.height * 596.f / 544.f);
        body.center = glm::vec3(
            art.origin.x + body.size.width * .5f, art.origin.y + body.size.height * .5f, 0.f
        );
        body.texture = artwork_;
        body.textureCoords = uprightAtlasRegion(0.f, 0.f, 1536.f, 596.f);
        const float tint = pressTint_;
        body.color = PandaUI::Color(tint, tint, tint, 1.f);

        if (!artwork_) {
            // Navigation still works if the scene's source asset is missing.
            body.color = PandaUI::Color(0xFFF5DBFF);
            renderer.drawRoundedRect(body, art.size.height * .45f);
            return;
        }
        constexpr float pi = 3.14159265358979323846f;
        const float greeting = std::clamp(static_cast<float>(elapsed_) / 1.9f, 0.f, 1.f);
        const float envelope = std::sin(pi * greeting);
        // Lift outward from the chest, rather than hiding an upright leg
        // under the ear. The envelope eases into and out of the greeting.
        const float angle = 1.65f - .38f * envelope * envelope
            * (.5f - .5f * std::cos(4.f * pi * greeting));
        const float scale = art.size.width / 1536.f;
        const float limbScale = scale * .80f;
        const glm::vec3 shoulder(
            art.origin.x + 1240.f * scale, art.origin.y + 425.f * scale, 0.f
        );

        PandaUI::UIRenderer::RectData foreleg;
        foreleg.texture = artwork_;
        // Leave the lettering to the left outside this crop.
        foreleg.textureCoords = uprightAtlasRegion(712.f, 570.f, 212.f, 440.f);
        foreleg.size = PandaUI::Size(212.f * limbScale, 440.f * limbScale);
        // Pivot at source shoulder (810, 920), not at the hoof's center.
        foreleg.center = glm::vec3(8.f * limbScale, -130.f * limbScale, 0.f);
        foreleg.transform = glm::rotate(
            glm::translate(glm::mat4(1.f), shoulder), angle, glm::vec3(0.f, 0.f, 1.f)
        );
        foreleg.color = body.color;
        // The shoulder tucks into the front fleece. Draw the body in front
        // so the waving limb cannot cover the face or look pasted onto it.
        renderer.drawRect(foreleg);
        renderer.drawRect(body);

        if (titleRegion_.size.width > 0.f) {
            PandaUI::UIRenderer::RectData title;
            title.texture = artwork_;
            title.textureCoords = uprightAtlasRegion(
                titleRegion_.origin.x, titleRegion_.origin.y,
                titleRegion_.size.width, titleRegion_.size.height
            );
            constexpr float letteringScale = .84f;
            title.size = PandaUI::Size(
                titleRegion_.size.width * scale * letteringScale,
                titleRegion_.size.height * scale * letteringScale
            );
            title.center = glm::vec3(
                art.origin.x + 768.f * scale, art.origin.y + 255.f * scale, 0.f
            );
            title.color = body.color;
            renderer.drawRect(title);
        }

    }

private:
    static PandaUI::Rect artworkRect(PandaUI::Size available) {
        const float width = std::min(available.width, available.height * 1536.f / 544.f);
        const float height = width * 544.f / 1536.f;
        return {(available.width - width) * .5f, (available.height - height) * .5f, width, height};
    }

    PandaUI::TextureHandle artwork_;
    PandaUI::Rect titleRegion_{};
    std::shared_ptr<PandaUI::Label> label_;
    PandaUI::Size previousSize_;
    double elapsed_{};
    float pressTint_{1.f};
};

} // namespace

MainMenuView::MainMenuView(
    MainMenuText text,
    Action levelSelect,
    PandaUI::TextureHandle sheepArtwork
) {
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
    safeArea->layout().setPadding(PandaUI::Edge::Top, 76.f);
    safeArea->layout().setPadding(PandaUI::Edge::Bottom, 18.f);

    auto playButton = std::make_shared<SheepPlayButton>(text.playButton, sheepArtwork);
    playButton->setPrimaryAction([action = std::move(levelSelect)](PandaUI::Control &sender) {
        if (action) {
            sender.setEnabled(false);
            action();
        }
    });
    safeArea->addSubview(playButton);

    addSubview(safeArea);
}

} // namespace LiberateTheSheep::UI
