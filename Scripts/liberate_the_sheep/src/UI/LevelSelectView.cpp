#include "UI/LevelSelectView.hpp"

#include "Game/LevelCatalog.hpp"
#include "UI/UIStyle.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace LiberateTheSheep::UI {
namespace {

    constexpr std::size_t kLevelsPerRow = 3;

    enum class LevelStatus { Current, Completed, Locked };

    class MeadowButton : public PandaUI::Control {
    public:
        MeadowButton(
            std::string title, float fontSize, float height, LevelStatus status
        )
            : status_(status) {
            layout().setHeight(PandaUI::Length::points(height));
            layout().setMinHeight(PandaUI::Length::points(height));
            layout().setFlexDirection(PandaUI::FlexDirection::Column);
            layout().setJustifyContent(PandaUI::Justify::Center);
            surface().setCornerRadius(19.f);
            surface().setBorderWidth(1.f);
            surface().setShadowColor(PandaUI::Color(0x00000028));
            surface().setShadowOffset(PandaUI::Point(0.f, 3.f));
            surface().setShadowRadius(8.f);

            title_ = makeLabel(
                std::move(title), fontSize, PandaUI::FontWeight::Bold,
                PandaUI::Color(0xF5F5F5FF),
                1, PandaUI::TextAlignment::Center
            );
            title_->layout().setWidth(PandaUI::Length::percent(100.f));
            title_->setUserInteractionEnabled(false);
            addSubview(title_);
            setEnabled(status_ != LevelStatus::Locked);
            setFocusable(status_ != LevelStatus::Locked);
            setCursor(status_ == LevelStatus::Locked ? PandaUI::Cursor::Arrow
                                                    : PandaUI::Cursor::PointingHand);
            updateAppearance();
            fill_ = targetFill_;
            border_ = targetBorder_;
            appearanceAnimating_ = false;
            applyAppearance();
        }

    protected:
        void update(double deltaTime) override {
            if (!appearanceAnimating_ || deltaTime <= 0.0) {
                return;
            }

            // Follow the latest state from the current color, including quick
            // press/release changes: never restart at a previous state's color.
            const float blend = 1.f - std::exp(-static_cast<float>(
                std::clamp(deltaTime, 0.0, 0.1)
            ) / .07f);
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
            appearanceAnimating_ = fillMoving || borderMoving;
            applyAppearance();
        }

        void controlStateChanged(PandaUI::ControlState, PandaUI::ControlState) override {
            updateAppearance();
        }

        void draw(float, float) override {
            if (status_ == LevelStatus::Current || !getContext()) {
                return;
            }

            // Draw native geometry instead of font glyphs: the badges remain
            // readable in every language and do not depend on emoji fallback.
            const auto frame = getWorldFrame();
            const float x = frame.origin.x + frame.size.width - 21.f;
            const float y = frame.origin.y + 20.f;
            auto &renderer = getContext()->getRenderer();
            PandaUI::UIRenderer::RectData badge;
            badge.center = glm::vec3(x, y, 0.f);
            badge.size = PandaUI::Size(25.f, 25.f);
            badge.color = status_ == LevelStatus::Completed
                              ? PandaUI::Color(0x518554FF)
                              : PandaUI::Color(0x343434D9);
            renderer.drawRoundedRect(badge, 12.5f);

            if (status_ == LevelStatus::Completed) {
                const PandaUI::Color cream(0xFFF9E9FF);
                renderer.drawLine({x - 6.f, y}, {x - 1.5f, y + 4.f}, 2.6f, cream);
                renderer.drawLine({x - 1.5f, y + 4.f}, {x + 6.f, y - 4.f}, 2.6f, cream);
                return;
            }

            const PandaUI::Color ink(0xD7D7D7FF);
            PandaUI::UIRenderer::RectData shackle;
            shackle.center = glm::vec3(x, y - 2.5f, 0.f);
            shackle.size = PandaUI::Size(10.f, 12.f);
            shackle.color = ink;
            renderer.drawRoundedRectBorder(shackle, 5.f, 2.f);

            PandaUI::UIRenderer::RectData lockBody;
            lockBody.center = glm::vec3(x, y + 3.5f, 0.f);
            lockBody.size = PandaUI::Size(14.f, 10.f);
            lockBody.color = ink;
            renderer.drawRoundedRect(lockBody, 2.5f);
            renderer.drawLine({x, y + 2.f}, {x, y + 5.f}, 1.8f, badge.color);
        }

    private:
        void updateAppearance() {
            const bool locked = status_ == LevelStatus::Locked;
            // Shared with Settings: keep levels and Back on the same neutral base.
            PandaUI::Color fill = Colors::menuCard();
            if (!locked && isHighlighted()) {
                fill = PandaUI::Color(.10f, .10f, .10f, .76f);
            } else if (!locked && isHovered()) {
                fill = PandaUI::Color(.16f, .16f, .16f, .72f);
            }
            const PandaUI::Color border =
                isFocused() ? PandaUI::Color(0xFFFFFFA0)
                            : locked ? PandaUI::Color(0xFFFFFF1A)
                                     : PandaUI::Color(0xFFFFFF24);
            targetFill_ = fill;
            targetBorder_ = border;
            appearanceAnimating_ = true;
            if (title_) {
                title_->setTextColor(
                    PandaUI::Color(locked ? 0xC7C7C7FF : 0xF5F5F5FF)
                );
            }
        }

        void applyAppearance() {
            setBackgroundColor(fill_);
            surface().setBorderColor(border_);
            setNeedsDisplay();
        }

        LevelStatus status_;
        std::shared_ptr<PandaUI::Label> title_;
        PandaUI::Color fill_;
        PandaUI::Color border_;
        PandaUI::Color targetFill_;
        PandaUI::Color targetBorder_;
        bool appearanceAnimating_ = false;
    };

} // namespace

LevelSelectView::LevelSelectView(
    LevelSelectText text,
    int highestUnlocked,
    std::span<const int> bestHearts,
    LevelAction playLevel,
    Action back,
    bool allLevelsUnlocked
) {
    const std::size_t levelCount = std::max(
        static_cast<std::size_t>(Game::kCampaignLevelCount), bestHearts.size()
    );
    const int unlockedThrough =
        std::clamp(highestUnlocked, 1, static_cast<int>(levelCount));

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
    scrollContent->layout().setPadding(PandaUI::Edge::Horizontal, 18.f);
    scrollContent->layout().setPadding(PandaUI::Edge::Top, 84.f);
    scrollContent->layout().setPadding(PandaUI::Edge::Bottom, 18.f);

    const auto makeFlexibleSpacer = [] {
        auto spacer = std::make_shared<PandaUI::Panel>();
        spacer->setBackgroundColor(Colors::transparent());
        spacer->layout().setFlexGrow(1.f);
        spacer->layout().setFlexShrink(1.f);
        spacer->layout().setFlexBasis(0.f);
        return spacer;
    };
    scrollView->addContentSubview(makeFlexibleSpacer());

    auto card = makeCard(460.f);
    // Keep the layout container, but let the animated menu scene show through.
    card->setBackgroundColor(Colors::transparent());
    card->surface().setBorderWidth(0.f);
    card->surface().setBorderColor(Colors::transparent());
    card->surface().setShadowColor(Colors::transparent());
    card->surface().setShadowRadius(0.f);
    card->layout().setPadding(PandaUI::Edge::All, 20.f);
    card->layout().setGap(18.f);
    card->layout().setFlexShrink(0.f);

    auto title = makeLabel(
        text.title,
        30.f,
        PandaUI::FontWeight::Bold,
        PandaUI::Color(0x343941FF),
        1,
        PandaUI::TextAlignment::Center
    );
    title->layout().setWidth(PandaUI::Length::percent(100.f));
    card->addSubview(title);

    auto grid = std::make_shared<PandaUI::Panel>();
    grid->setBackgroundColor(Colors::transparent());
    grid->layout().setWidth(PandaUI::Length::percent(100.f));
    grid->layout().setFlexDirection(PandaUI::FlexDirection::Column);
    grid->layout().setGap(12.f);

    for (std::size_t rowStart = 0; rowStart < levelCount; rowStart += kLevelsPerRow) {
        auto row = std::make_shared<PandaUI::Panel>();
        row->setBackgroundColor(Colors::transparent());
        row->layout().setWidth(PandaUI::Length::percent(100.f));
        row->layout().setFlexDirection(PandaUI::FlexDirection::Row);
        row->layout().setGap(12.f);
        row->layout().setFlexShrink(0.f);

        for (std::size_t column = 0; column < kLevelsPerRow; ++column) {
            const std::size_t index = rowStart + column;
            if (index >= levelCount) {
                auto spacer = std::make_shared<PandaUI::Panel>();
                spacer->setBackgroundColor(Colors::transparent());
                spacer->layout().setFlexGrow(1.f);
                spacer->layout().setFlexBasis(0.f);
                row->addSubview(spacer);
                continue;
            }

            const int levelNumber = static_cast<int>(index + 1);
            const bool unlocked = allLevelsUnlocked || levelNumber <= unlockedThrough;
            const int hearts = index < bestHearts.size() ? bestHearts[index] : 0;
            const bool completed = unlocked && (hearts > 0 || levelNumber < unlockedThrough);
            const LevelStatus status = !unlocked ? LevelStatus::Locked
                                                : completed ? LevelStatus::Completed
                                                            : LevelStatus::Current;

            auto levelButton = std::make_shared<MeadowButton>(
                std::to_string(levelNumber), 32.f, 88.f, status
            );
            levelButton->layout().setFlexGrow(1.f);
            levelButton->layout().setFlexBasis(0.f);
            levelButton->setPrimaryAction([action = playLevel, levelNumber](PandaUI::Control &) {
                if (action) {
                    action(levelNumber);
                }
            });
            row->addSubview(levelButton);
        }
        grid->addSubview(row);
    }
    card->addSubview(grid);

    auto backButton = std::make_shared<MeadowButton>(
        text.backButton, 16.f, 48.f, LevelStatus::Current
    );
    backButton->layout().setWidth(PandaUI::Length::percent(100.f));
    backButton->setPrimaryAction([action = std::move(back)](PandaUI::Control &) {
        if (action) {
            action();
        }
    });
    card->addSubview(backButton);

    scrollView->addContentSubview(card);
    scrollView->addContentSubview(makeFlexibleSpacer());
    safeArea->addSubview(scrollView);
    addSubview(safeArea);
}

LevelSelectView::LevelSelectView(
    int highestUnlocked,
    std::span<const int> bestHearts,
    LevelAction playLevel,
    Action back
)
    : LevelSelectView(
          LevelSelectText{},
          highestUnlocked,
          bestHearts,
          std::move(playLevel),
          std::move(back)
      ) {}

} // namespace LiberateTheSheep::UI
