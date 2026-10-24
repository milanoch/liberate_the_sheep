#pragma once

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>

namespace LiberateTheSheep::UI {

class SheepToggle final : public PandaUI::Control,
                          public std::enable_shared_from_this<SheepToggle> {
public:
    using ValueChangedCallback = std::function<void(SheepToggle &)>;

    explicit SheepToggle(bool initialOn = false);

    void setOn(bool on);
    [[nodiscard]] bool isOn() const { return isSelected(); }
    void setOnValueChanged(ValueChangedCallback callback);

protected:
    void update(double deltaTime) override;
    void draw(float offsetX, float offsetY) override;
    void performPrimaryAction() override;
    void controlStateChanged(
        PandaUI::ControlState previousState, PandaUI::ControlState currentState
    ) override;
    void themeChanged() override;

private:
    void refreshAppearance();

    ValueChangedCallback onValueChanged_;
    float position_ = 0.f;
    float startPosition_ = 0.f;
    float targetPosition_ = 0.f;
    float hop_ = 0.f;
    float startHop_ = 0.f;
    float tilt_ = 0.f;
    float startTilt_ = 0.f;
    float movementProgress_ = 1.f;
    PandaUI::Color track_;
    PandaUI::Color startTrack_;
    PandaUI::Color targetTrack_;
    float colorProgress_ = 1.f;
};

} // namespace LiberateTheSheep::UI
