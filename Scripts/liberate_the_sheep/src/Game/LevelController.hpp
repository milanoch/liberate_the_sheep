#pragma once

#include "Game/BoardLayout.hpp"
#include "Game/BoardModel.hpp"
#include "Game/EscapeHerd.hpp"
#include "Game/LevelHearts.hpp"
#include "Game/PastureScene.hpp"
#include "Game/PenEscapePath.hpp"
#include "Game/ProgressStore.hpp"
#include "Game/SettingsStore.hpp"
#include "Game/ShepherdDogRescue.hpp"

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
        Tutorial,
        Playing,
        DogRescue,
        BombExplosion,
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
        int coat{};
        int frame{-1};
    };

    struct BlockedReaction {
        SheepId id{kInvalidSheepId};
        float elapsed{};
    };

    struct BombSheep {
        enum class State { None, Armed, DogRescue, Rescued, Defused, Exploded };
        SheepId id{kInvalidSheepId};
        Bamboo::EntityHandle entity;
        State state{State::None};
        float seconds{};
        bool impact{};
        int hitsRemaining{};
    };

    struct RescueFlockPose {
        SheepId id{kInvalidSheepId};
        DogRescue::Point position{};
        DogRescue::Point offset{};
    };

    PandaUI::Window window_;
    std::shared_ptr<UI::GameplayRootView> rootView_;
    ProgressStore progress_;
    SettingsStore settings_;
    std::optional<BoardModel> board_;
    BoardLayout layout_;

    Bamboo::EntityHandle cameraEntity_;
    PastureScene pasture_;
    Bamboo::TextureHandle runningSheepTexture_;
    Bamboo::EntityHandle bombEntity_;
    std::vector<Bamboo::EntityHandle> bombAccessoryEntities_;
    PandaUI::TextureHandle bombUiTexture_;
    PandaUI::TextureHandle standingSheepUiTexture_;
    std::vector<BombSheep> bombs_;
    Bamboo::EntityHandle rescueDogEntity_;
    Bamboo::EntityHandle rescueDogShadow_;
    DogRescue::Motion dogRescue_;
    SheepId dogRescueBomb_{kInvalidSheepId};
    std::vector<RescueFlockPose> rescueFlockPoses_;
    std::vector<std::size_t> bombTutorialOrder_;
    std::size_t bombTutorialCursor_{};
    float bombExplosionElapsed_{};
    bool fatalBombImpact_{};
    std::vector<SheepVisual> sheepVisuals_;
    EscapeHerd escapeHerd_;
    std::vector<BlockedReaction> blockedReactions_;

    Phase phase_{Phase::Loading};
    int currentLevel_{ProgressStore::kFirstLevel};
    LevelHearts hearts_;
    int viewportWidth_{};
    int viewportHeight_{};
    bool startupMenuPending_{};

    SheepId hintedSheep_{kInvalidSheepId};
    float hintTimeRemaining_{};

    void createUI();
    void resolveSceneEntities();
    void loadLevel(int levelNumber);
    void createGeneratedEntities(std::uint64_t appearanceSeed);
    void destroyGeneratedEntities();
    void refreshLayout(bool force = false);
    void applyLayoutToGeneratedEntities();
    void updateAnimation(float deltaTime);
    void updateEscapes(float deltaTime);
    void updateBlockedReactions(float deltaTime);
    void updateHint(float deltaTime);
    void prepareBombs(const LevelDefinition &definition);
    void showBombTutorial();
    [[nodiscard]] bool hitImpactBomb(SheepId attacker);
    void startBombLevel();
    void expireBomb(BombSheep &bomb);
    [[nodiscard]] bool beginDogRescue(BombSheep &bomb);
    void updateDogRescue(float deltaTime);
    void applyDogRescuePose(float deltaTime = 0.f);
    void resetDogRescue();
    [[nodiscard]] float dogOffscreenY() const;
    void updateBombVisual();
    void updateBombHUD();
    [[nodiscard]] PandaUI::Point sheepScreenPosition(SheepId id);
    void setSheepFrame(SheepVisual &visual, int pose);
    [[nodiscard]] float escapeRunout() const;
    [[nodiscard]] bool isReacting(SheepId id) const noexcept;

    void selectCell(Cell cell);
    void beginEscape(SheepId sheepId);
    void beginBlocked(SheepId sheepId);
    void finishEscape(SheepId id);
    void finishBlocked(SheepId id);
    void showHint();
    void clearHint();
    void restartLevel();
    void nextLevel();
    void returnToMenu();
    void updateHUD();
    void defer(std::function<void()> action);

    [[nodiscard]] SheepVisual *findVisual(SheepId sheepId) noexcept;
};

REGISTER_SCRIPT(LevelController)

} // namespace LiberateTheSheep::Game
