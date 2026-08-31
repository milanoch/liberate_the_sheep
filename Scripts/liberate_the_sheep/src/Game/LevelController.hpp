#pragma once

#include "Game/BoardLayout.hpp"
#include "Game/BoardModel.hpp"
#include "Game/ProgressStore.hpp"
#include "Game/SettingsStore.hpp"

#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Script.hpp>
#include <PandaUI/PandaUI.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace LiberateTheSheep::UI {
class GameplayRootView;
}

namespace LiberateTheSheep::Game {

class LevelController final : public Bamboo::Script {
public:
    Bamboo::WorldHandle menuWorld;
    Bamboo::TextureHandle sheepTexture;
    float cameraOrthoSize = 9.f;

    PANDA_FIELDS_BEGIN(LevelController)
    PANDA_FIELD(menuWorld)
    PANDA_FIELD(sheepTexture)
    PANDA_FIELD(cameraOrthoSize)
    PANDA_FIELDS_END

    void start() override;
    void update(float deltaTime) override;
    void shutdown() override;

private:
    enum class Phase {
        Loading,
        Playing,
        Escaping,
        Blocked,
        Victory,
        Defeat,
        Error,
    };

    struct SheepVisual {
        SheepId id{kInvalidSheepId};
        Bamboo::EntityHandle entity;
        Bamboo::Color baseColor{1.f, 1.f, 1.f, 1.f};
        LayoutPoint basePosition{};
        float rotationDegrees{};
    };

    PandaUI::Window window_;
    std::shared_ptr<UI::GameplayRootView> rootView_;
    ProgressStore progress_;
    SettingsStore settings_;
    std::optional<BoardModel> board_;
    BoardLayout layout_;

    Bamboo::EntityHandle cameraEntity_;
    Bamboo::EntityHandle backgroundEntity_;
    Bamboo::EntityHandle boardSurfaceEntity_;
    Bamboo::EntityHandle boardShadowEntity_;
    std::vector<Bamboo::EntityHandle> gridEntities_;
    std::vector<SheepVisual> sheepVisuals_;

    Phase phase_{Phase::Loading};
    int currentLevel_{ProgressStore::kFirstLevel};
    int hearts_{ProgressStore::kMaximumHearts};
    int viewportWidth_{};
    int viewportHeight_{};
    bool startupMenuPending_{};

    SheepId activeSheep_{kInvalidSheepId};
    SheepId hintedSheep_{kInvalidSheepId};
    float animationElapsed_{};
    float hintTimeRemaining_{};
    LayoutPoint animationStart_{};
    LayoutPoint animationTarget_{};

    void createUI();
    void resolveSceneEntities();
    void loadLevel(int levelNumber);
    void createGeneratedEntities(std::uint64_t appearanceSeed);
    void destroyGeneratedEntities();
    void refreshLayout(bool force = false);
    void applyLayoutToGeneratedEntities();
    void updateAnimation(float deltaTime);
    void updateHint(float deltaTime);

    void selectCell(Cell cell);
    void beginEscape(SheepId sheepId);
    void beginBlocked(SheepId sheepId);
    void finishEscape();
    void finishBlocked();
    void showHint();
    void clearHint();
    void restartLevel();
    void nextLevel();
    void returnToMenu();
    void updateHUD();
    void defer(std::function<void()> action);

    [[nodiscard]] SheepVisual *findVisual(SheepId sheepId) noexcept;
    [[nodiscard]] const Sheep *activeSheepModel() const noexcept;
    [[nodiscard]] LayoutPoint escapeTarget(const Sheep &sheep) const noexcept;
};

REGISTER_SCRIPT(LevelController)

} // namespace LiberateTheSheep::Game
