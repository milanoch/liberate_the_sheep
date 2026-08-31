#pragma once

#include <PandaUI/PandaUI.hpp>

#include <memory>
#include <string>

namespace LiberateTheSheep::UI {

namespace Colors {

[[nodiscard]] PandaUI::Color transparent();
[[nodiscard]] PandaUI::Color ink();
[[nodiscard]] PandaUI::Color cream();
[[nodiscard]] PandaUI::Color mutedCream();
[[nodiscard]] PandaUI::Color meadow();
[[nodiscard]] PandaUI::Color sunlight();
[[nodiscard]] PandaUI::Color danger();
[[nodiscard]] PandaUI::Color card();
[[nodiscard]] PandaUI::Color cardSoft();
[[nodiscard]] PandaUI::Color scrim();

} // namespace Colors

// Transparent HUD containers must not consume pointer events in their empty area.
// Their interactive descendants are still hit-tested from front to back.
class PassThroughPanel : public PandaUI::Panel {
public:
    using PandaUI::Panel::Panel;

    PandaUI::View *hitTest(PandaUI::Point windowPosition) override;
};

class PassThroughSafeAreaView : public PandaUI::SafeAreaView {
public:
    PandaUI::View *hitTest(PandaUI::Point windowPosition) override;
};

[[nodiscard]] std::shared_ptr<PandaUI::Label> makeLabel(
    std::string text,
    float fontSize,
    PandaUI::FontWeight weight = PandaUI::FontWeight::Regular,
    PandaUI::Color color = Colors::cream(),
    int numberOfLines = 1,
    PandaUI::TextAlignment alignment = PandaUI::TextAlignment::Left
);

[[nodiscard]] std::shared_ptr<PandaUI::Button> makeButton(
    std::string title,
    PandaUI::ButtonStyle style = PandaUI::ButtonStyle::Neutral,
    float height = 52.f
);

[[nodiscard]] std::shared_ptr<PandaUI::Panel> makeCard(float maxWidth = 520.f);
[[nodiscard]] std::shared_ptr<PandaUI::Panel> makePill();

} // namespace LiberateTheSheep::UI
