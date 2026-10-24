#include "UI/MenuShellView.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

constexpr float kFadeOutDuration = .10f;
constexpr float kFadeInDuration = .24f;
constexpr float kTransitionDuration = kFadeOutDuration + kFadeInDuration;

float easedOpacity(float progress) {
    const float t = std::clamp(progress, 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

void fillParent(PandaUI::View &view) {
    view.layout().setPositionType(PandaUI::PositionType::Absolute);
    view.layout().setPosition(PandaUI::Edge::Left, PandaUI::Length::points(0.f));
    view.layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(0.f));
    view.layout().setWidth(PandaUI::Length::percent(100.f));
    view.layout().setHeight(PandaUI::Length::percent(100.f));
}

} // namespace

// SDK compatibility layer: View::opacity affects Surface, but not native text,
// images or custom draw() geometry. Modulate this page's freshly emitted vertex
// alpha instead, so every element follows the same fade without changing colors.
class MenuShellView::PageLayer final : public PassThroughPanel {
    struct FadeRange {
        std::size_t firstVertex{};
        float opacity{1.f};
        bool open{};
    };

    class EndMarker final : public PandaUI::View {
    public:
        explicit EndMarker(std::shared_ptr<FadeRange> range) : range_(std::move(range)) {
            setBackgroundColor(Colors::transparent());
            setUserInteractionEnabled(false);
            // Full size keeps the marker in the render traversal even though
            // it emits no visible geometry of its own.
            fillParent(*this);
        }

    protected:
        void draw(float, float) override {
            if (!range_->open || !getContext()) {
                return;
            }
            range_->open = false;
            if (range_->opacity >= 1.f) {
                return;
            }

            auto &renderer = getContext()->getRenderer();
            // The SDK exposes a const accessor to its non-const, in-progress
            // CPU draw list. This isolated adapter edits only alpha in this
            // subtree's range before submission. Never resize the list or
            // alter commands, indices, UVs, clip state or another page's vertices.
            auto &vertices = const_cast<PandaUI::DrawList &>(renderer.getDrawList()).vertices;
            const auto first = std::min(range_->firstVertex, vertices.size());
            for (std::size_t index = first; index < vertices.size(); ++index) {
                vertices[index].color.a *= range_->opacity;
            }
        }

    private:
        std::shared_ptr<FadeRange> range_;
    };

public:
    explicit PageLayer(std::shared_ptr<PandaUI::View> page)
        : range_(std::make_shared<FadeRange>()) {
        setBackgroundColor(Colors::transparent());
        fillParent(*this);
        fillParent(*page);
        addSubview(std::move(page));
        // Must stay last: View renders itself, then its children in order.
        addSubview(std::make_shared<EndMarker>(range_));
    }

    void setFadeOpacity(float opacity) {
        const float value = std::clamp(opacity, 0.f, 1.f);
        if (range_->opacity != value) {
            range_->opacity = value;
            setNeedsDisplay();
        }
    }

protected:
    void draw(float, float) override {
        if (getContext()) {
            range_->firstVertex = getContext()->getRenderer().getDrawList().vertices.size();
            range_->open = true;
        }
    }

private:
    std::shared_ptr<FadeRange> range_;
};

MenuShellView::MenuShellView(
    PandaUI::TextureHandle settingsIcon, std::function<void()> settings
) {
    setBackgroundColor(Colors::transparent());
    layout().setWidth(PandaUI::Length::percent(100.f));
    layout().setHeight(PandaUI::Length::percent(100.f));

    pageHost_ = std::make_shared<PassThroughPanel>();
    pageHost_->setBackgroundColor(Colors::transparent());
    pageHost_->setClipsToBounds(true);
    fillParent(*pageHost_);
    addSubview(pageHost_);

    // This overlay passes empty-space input through to the page beneath it.
    auto chrome = std::make_shared<PassThroughSafeAreaView>();
    chrome->setBackgroundColor(Colors::transparent());
    fillParent(*chrome);
    settingsButton_ = makeButton("SET", PandaUI::ButtonStyle::Neutral, 54.f);
    settingsButton_->setFont(PandaUI::Font(13.f, PandaUI::FontWeight::Bold));
    settingsButton_->layout().setPositionType(PandaUI::PositionType::Absolute);
    settingsButton_->layout().setPosition(PandaUI::Edge::Top, PandaUI::Length::points(18.f));
    settingsButton_->layout().setPosition(PandaUI::Edge::Right, PandaUI::Length::points(18.f));
    settingsButton_->layout().setWidth(PandaUI::Length::points(54.f));
    if (settingsIcon) {
        settingsButton_->getTitleLabel()->setHidden(true);
        settingsButton_->layout().setAlignItems(PandaUI::Align::Center);
        settingsButton_->layout().setJustifyContent(PandaUI::Justify::Center);
        auto icon = std::make_shared<PandaUI::ImageView>(settingsIcon);
        icon->setContentMode(PandaUI::ImageContentMode::Fit);
        icon->setUserInteractionEnabled(false);
        icon->layout().setWidth(PandaUI::Length::points(34.f));
        icon->layout().setHeight(PandaUI::Length::points(34.f));
        settingsButton_->addSubview(icon);
    }
    settingsButton_->setOnClick([action = std::move(settings)](PandaUI::Button &) {
        if (action) {
            action();
        }
    });
    chrome->addSubview(settingsButton_);
    addSubview(chrome);
}

void MenuShellView::setSettingsTooltip(std::string tooltip) {
    settingsButton_->setTooltip(std::move(tooltip));
}

bool MenuShellView::showPage(std::shared_ptr<PandaUI::View> page, bool animated) {
    if (!page || transitioning_) {
        return false;
    }
    if (getContext()) {
        getContext()->setFocusedView(nullptr);
    }
    auto layer = std::make_shared<PageLayer>(std::move(page));

    if (!currentPage_ || !animated) {
        pageHost_->removeAllSubviews();
        currentPage_ = std::move(layer);
        outgoingPage_.reset();
        pageHost_->addSubview(currentPage_);
        return true;
    }

    outgoingPage_ = std::move(currentPage_);
    currentPage_ = std::move(layer);
    pageHost_->addSubview(currentPage_);
    elapsed_ = 0.f;
    transitioning_ = true;
    finishQueued_ = false;
    setNavigationEnabled(false);
    applyFade();
    return true;
}

void MenuShellView::setNavigationEnabled(bool enabled) {
    pageHost_->setEnabled(enabled);
    pageHost_->setUserInteractionEnabled(enabled);
    // Do not dim or remove the gear while switching pages.
    settingsButton_->setUserInteractionEnabled(enabled);
    settingsButton_->setFocusable(enabled);
}

void MenuShellView::applyFade() {
    // Fade the old page out first, then reveal the new one. Text from different
    // pages never overlaps, and neither page changes position or size.
    outgoingPage_->setFadeOpacity(1.f - easedOpacity(elapsed_ / kFadeOutDuration));
    currentPage_->setFadeOpacity(easedOpacity((elapsed_ - kFadeOutDuration) / kFadeInDuration));
}

void MenuShellView::update(double deltaTime) {
    if (!transitioning_ || finishQueued_) {
        return;
    }
    elapsed_ += static_cast<float>(std::clamp(deltaTime, 0.0, .05));
    applyFade();
    if (elapsed_ >= kTransitionDuration && getContext()) {
        finishQueued_ = true;
        // Never destroy a page while the UI hierarchy is being traversed.
        const auto weak = weak_from_this();
        getContext()->postDeferred([weak]() {
            if (const auto shell = weak.lock()) {
                shell->finishTransition();
            }
        });
    }
}

void MenuShellView::finishTransition() {
    if (!transitioning_) {
        return;
    }
    if (outgoingPage_) {
        pageHost_->removeSubview(outgoingPage_);
        outgoingPage_.reset();
    }
    currentPage_->setFadeOpacity(1.f);
    transitioning_ = false;
    finishQueued_ = false;
    setNavigationEnabled(true);
}

} // namespace LiberateTheSheep::UI
