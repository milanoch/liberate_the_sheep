#pragma once

#include <PandaUI/PandaUI.hpp>

namespace LiberateTheSheep::UI {

// Uses the same registered sheep and bomb art as the field. The controller
// owns the UI texture copies and keeps them alive until the root view is gone.
class BombSheepPicture final : public PandaUI::View {
public:
    BombSheepPicture();
    void setArtwork(PandaUI::TextureHandle sheep, PandaUI::TextureHandle bomb);
    void setCoat(int coat);
    void setSeconds(int seconds);
    void setImpactMode(bool impact);

protected:
    void draw(float, float) override;

private:
    PandaUI::TextureHandle sheep_;
    PandaUI::TextureHandle bomb_;
    int coat_{};
    int seconds_{};
    bool impact_{};
};

// The bomb itself is a world Sprite; this view draws its digital display in
// the same pose, plus the short burst effect, without intercepting input.
class BombFieldOverlay final : public PandaUI::View {
public:
    BombFieldOverlay();
    void setMarker(PandaUI::Point center, float bombSize, float rotationDegrees,
                   int seconds, bool visible, bool impact = false);
    void playBurst(PandaUI::Point center, bool harmless);
    void reset();
    void update(double deltaTime) override;

protected:
    void draw(float, float) override;

private:
    PandaUI::Point displayCenter_;
    float displaySize_{};
    float displayRotation_{};
    int displayedSeconds_{};
    bool displayVisible_{};
    bool displayImpact_{};
    PandaUI::Point burstCenter_;
    float burstElapsed_{1.f};
    bool harmless_{};
};

} // namespace LiberateTheSheep::UI
