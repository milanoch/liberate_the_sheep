#pragma once

#include "UI/UIStyle.hpp"

#include <memory>
#include <string>

namespace LiberateTheSheep::UI {

// A short, non-modal explanation of immortal-mode rescues. It never captures
// input, darkens the scene or advances gameplay while the dog is performing.
class RescueNoticeView final : public PassThroughSafeAreaView {
public:
    RescueNoticeView(std::string title, std::string message);

    void show();
    void dismiss();
    void update(double deltaTime) override;

private:
    void applyOpacity(float opacity);

    std::shared_ptr<PandaUI::Panel> card_;
    std::shared_ptr<PandaUI::Label> title_;
    std::shared_ptr<PandaUI::Label> message_;
    float elapsed_{};
    float opacity_{};
    float initialOpacity_{};
    bool active_{};
};

} // namespace LiberateTheSheep::UI
