#pragma once

#include "UI/UIStyle.hpp"

#include <functional>
#include <memory>
#include <string>

namespace LiberateTheSheep::UI {

// The menu scene and gear stay visible while the page content fades in place.
class MenuShellView final : public PandaUI::Panel,
                            public std::enable_shared_from_this<MenuShellView> {
public:
    MenuShellView(PandaUI::TextureHandle settingsIcon, std::function<void()> settings);

    bool showPage(std::shared_ptr<PandaUI::View> page, bool animated = true);
    void setSettingsTooltip(std::string tooltip);
    bool isTransitioning() const { return transitioning_; }
    void update(double deltaTime) override;

private:
    class PageLayer;

    std::shared_ptr<PassThroughPanel> pageHost_;
    std::shared_ptr<PandaUI::Button> settingsButton_;
    std::shared_ptr<PageLayer> currentPage_;
    std::shared_ptr<PageLayer> outgoingPage_;
    float elapsed_{};
    bool transitioning_{};
    bool finishQueued_{};

    void applyFade();
    void finishTransition();
    void setNavigationEnabled(bool enabled);
};

} // namespace LiberateTheSheep::UI
