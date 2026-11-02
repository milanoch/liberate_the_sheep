#include "Game/LevelController.hpp"

#include "Game/BoardGenerator.hpp"
#include "Game/BombPlacement.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "Game/SheepAppearance.hpp"
#include "Game/SheepRunAnimation.hpp"
#include "Game/ShepherdDogAnimation.hpp"
#include "UI/GameplayRootView.hpp"

#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/Assets/TextureAPI.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/UI/TextureAPI.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <limits>
#include <string>
#include <utility>

namespace LiberateTheSheep::Game {
namespace {

    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kBlockedDuration = 0.30f;
    constexpr float kHintDuration = 2.4f;
    // The atlas includes transparent padding. These quad sizes give a visible
    // sheep of 0.82 x 1.78 cells, within the path planner's tested envelope.
    constexpr float kSheepWidthInCells = 1.23f;
    constexpr float kSheepLengthInCells = 1.97f;
    constexpr float kBombSizeInCells = 1.16f;
    constexpr float kBombBackOffset = -0.12f;
    // Include the full tail and fence thickness, not just the sheep's center.
    constexpr float kBombGateClearance = 1.10f;
    constexpr float kBombExplosionDuration = 0.70f;

    PandaUI::TextureHandle uiTexture(Bamboo::TextureHandle source) {
        if (!source.isValid()) { return {}; }
        const auto pixels = Bamboo::TextureAPI::readPixelsRGBA8(source);
        constexpr auto maxSize = std::numeric_limits<std::uint16_t>::max();
        if (!pixels || pixels.width > maxSize || pixels.height > maxSize ||
            static_cast<std::uint64_t>(pixels.width) * pixels.height * 4u != pixels.pixels.size()) {
            return {};
        }
        return Bamboo::UI::createTextureRGBA8(pixels.pixels.data(),
            static_cast<std::uint16_t>(pixels.width), static_cast<std::uint16_t>(pixels.height));
    }

    [[nodiscard]] float clamp01(float value) noexcept {
        return std::clamp(value, 0.f, 1.f);
    }

    [[nodiscard]] float directionAngle(Direction direction) noexcept {
        switch (direction) {
            case Direction::Up:
                return 0.f;
            case Direction::Right:
                return -90.f;
            case Direction::Down:
                return 180.f;
            case Direction::Left:
                return 90.f;
        }
        return 0.f;
    }

    [[nodiscard]] Bamboo::Color blend(
        const Bamboo::Color &from, const Bamboo::Color &to, float amount
    ) noexcept {
        const float t = clamp01(amount);
        return {
            from.r + (to.r - from.r) * t,
            from.g + (to.g - from.g) * t,
            from.b + (to.b - from.b) * t,
            from.a + (to.a - from.a) * t,
        };
    }

    [[nodiscard]] bool handleValid(Bamboo::EntityHandle &handle) noexcept {
        return handle.isValid();
    }

    [[nodiscard]] std::string localizedString(TextKey key, Language language) {
        return std::string(localizedText(key, language));
    }

} // namespace

void LevelController::start() {
    (void)progress_.load();
    (void)settings_.load();
    hearts_.reset(settings_.immortalModeEnabled());

    const int requestedLevel = SessionState::consumeRequestedLevel();
    if (requestedLevel == SessionState::kNoLevelRequested && menuWorld.isValid()) {
        // A direct launch of game.world is an application startup, not a level
        // selection. Load the authored menu on the first script update. World loads
        // are supported from gameplay callbacks/updates; unlike PandaUI's deferred
        // queue, this path also runs when the current world has no UI root yet.
        startupMenuPending_ = true;
        return;
    }

    resolveSceneEntities();
    createUI();

    int levelToLoad = requestedLevel;
    if (levelToLoad < ProgressStore::kFirstLevel || levelToLoad > ProgressStore::kLastLevel ||
        (!settings_.cheatModeEnabled() && !progress_.isUnlocked(levelToLoad))) {
        levelToLoad = progress_.progress().highestUnlocked;
    }
    loadLevel(levelToLoad);
}

void LevelController::update(float deltaTime) {
    if (startupMenuPending_) {
        startupMenuPending_ = false;
        const Bamboo::WorldHandle targetWorld = menuWorld;
        LOG_INFO("LevelController: direct launch redirected to main menu");
        Bamboo::WorldAPI::load(targetWorld);
        return;
    }

    refreshLayout();
    // Use the same simulation clock as EscapeHerd. A suspended application or
    // stalled frame must not burn a deadline while the sheep cannot move.
    const float step = std::isfinite(deltaTime) ? std::clamp(deltaTime, 0.f, 0.1f) : 0.f;
    if (phase_ == Phase::DogRescue) {
        updateDogRescue(step);
    } else if (phase_ == Phase::BombExplosion) {
        bombExplosionElapsed_ += step;
        if (bombExplosionElapsed_ >= kBombExplosionDuration) {
            phase_ = Phase::Defeat;
            if (rootView_) { rootView_->showBombDefeat(fatalBombImpact_); }
        }
    } else {
        updateAnimation(step);
        updateHint(step);
    }
    updateBombVisual();
}

void LevelController::shutdown() {
    pasture_.shutdown();
    if (window_.isValid() && rootView_ &&
        window_.context().getRootView() == rootView_) {
        window_.setRootView(nullptr);
    }
    rootView_.reset();
    if (bombUiTexture_) { Bamboo::UI::destroyTexture(bombUiTexture_); bombUiTexture_ = {}; }
    if (standingSheepUiTexture_) {
        Bamboo::UI::destroyTexture(standingSheepUiTexture_);
        standingSheepUiTexture_ = {};
    }
    destroyGeneratedEntities();
    (void)progress_.save();
    board_.reset();
    phase_ = Phase::Loading;
}

void LevelController::createUI() {
    window_ = PandaUI::Window::main();
    if (!window_.isValid()) {
        LOG_ERROR("LevelController: main PandaUI window is unavailable");
        return;
    }

    UI::GameplayRootView::Callbacks callbacks;
    callbacks.selectCell = [this](Cell cell) { selectCell(cell); };
    callbacks.hint = [this]() { showHint(); };
    callbacks.restart = [this]() { defer([this]() { restartLevel(); }); };
    callbacks.menu = [this]() { returnToMenu(); };
    callbacks.next = [this]() { defer([this]() { nextLevel(); }); };
    callbacks.replay = [this]() { defer([this]() { restartLevel(); }); };
    callbacks.startBombLevel = [this]() { defer([this]() { startBombLevel(); }); };

    const Language language = settings_.language();
    UI::GameplayText text;
    text.levelStat = localizedString(TextKey::LevelStat, language);
    text.heartsStat = localizedString(TextKey::HeartsStat, language);
    text.sheepLeftStat = localizedString(TextKey::SheepLeftStat, language);
    text.hintButton = localizedString(TextKey::Hint, language);
    text.restartButton = localizedString(TextKey::Restart, language);
    text.menuButton = localizedString(TextKey::Menu, language);
    text.fieldCleared = localizedString(TextKey::FieldCleared, language);
    text.victoryTitle = localizedString(TextKey::VictoryTitle, language);
    text.victoryMessage = localizedString(TextKey::VictoryMessage, language);
    text.noHeartsLeft = localizedString(TextKey::NoHeartsLeft, language);
    text.defeatTitle = localizedString(TextKey::DefeatTitle, language);
    text.defeatMessage = localizedString(TextKey::DefeatMessage, language);
    text.loadErrorTitle = localizedString(TextKey::LoadErrorTitle, language);
    text.loadErrorMessage = localizedString(TextKey::LoadErrorMessage, language);
    text.nextLevelButton = localizedString(TextKey::NextLevel, language);
    text.replayButton = localizedString(TextKey::Replay, language);
    text.bombTitle = localizedString(TextKey::BombTitle, language);
    text.bombInstructions = localizedString(TextKey::BombInstructions, language);
    text.bombImmortalInstructions = localizedString(TextKey::BombImmortalInstructions, language);
    text.bombStart = localizedString(TextKey::BombStart, language);
    text.bombTimer = localizedString(TextKey::BombTimer, language);
    text.bombRescued = localizedString(TextKey::BombRescued, language);
    text.bombDefused = localizedString(TextKey::BombDefused, language);
    text.bombExpired = localizedString(TextKey::BombExpired, language);
    text.bombDefeatMessage = localizedString(TextKey::BombDefeatMessage, language);
    text.impactBombTitle = localizedString(TextKey::ImpactBombTitle, language);
    text.impactBombInstructions = localizedString(TextKey::ImpactBombInstructions, language);
    text.impactBombImmortalInstructions = localizedString(TextKey::ImpactBombImmortalInstructions, language);
    text.impactBombHits = localizedString(TextKey::ImpactBombHits, language);
    text.impactBombExpired = localizedString(TextKey::ImpactBombExpired, language);
    text.impactBombDefeatMessage = localizedString(TextKey::ImpactBombDefeatMessage, language);
    text.dogRescueTitle = localizedString(TextKey::DogRescueTitle, language);
    text.dogRescueMessage = localizedString(TextKey::DogRescueMessage, language);

    rootView_ =
        std::make_shared<UI::GameplayRootView>(
            std::move(callbacks), std::move(text), hearts_.counterVisible());
    standingSheepUiTexture_ = uiTexture(sheepTexture);
    if (bombEntity_.isValid()) {
        bombUiTexture_ = uiTexture(Bamboo::SpriteRendererComponentAPI::getTexture(bombEntity_));
    }
    rootView_->setBombArtwork(standingSheepUiTexture_, bombUiTexture_);
    window_.setRootView(rootView_);
}

void LevelController::resolveSceneEntities() {
    cameraEntity_ = Bamboo::WorldAPI::findByTag("Camera");
    rescueDogEntity_ = Bamboo::WorldAPI::findByTag("ShepherdDogRescuer");
    rescueDogShadow_ = Bamboo::WorldAPI::findByTag("ShepherdDogShadow");
    if (rescueDogShadow_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(rescueDogShadow_, {0.f, 0.f, 0.f, 0.f});
    }
    if (rescueDogEntity_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(rescueDogEntity_, {1.f, 1.f, 1.f, 0.f});
    }
    pasture_.resolve();
    bombEntity_ = Bamboo::WorldAPI::findByTag("BombSheepAccessory");
    bombAccessoryEntities_.clear();
    for (const char *tag : {"BombSheepAccessory", "BombSheepAccessory2", "BombSheepAccessory3"}) {
        auto entity = Bamboo::WorldAPI::findByTag(tag);
        if (entity.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setColor(entity, {1.f, 1.f, 1.f, 0.f});
            bombAccessoryEntities_.push_back(entity);
        }
    }
    if (bombEntity_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(bombEntity_, {1.f, 1.f, 1.f, 0.f});
        if (!Bamboo::SpriteRendererComponentAPI::getTexture(bombEntity_).isValid()) {
            LOG_ERROR("LevelController: assign the registered sheep_bomb.png texture to BombSheepAccessory in game.world");
        }
    }
    Bamboo::EntityHandle templateEntity = Bamboo::WorldAPI::findByTag("GameSheepRunTemplate");
    if (templateEntity.isValid()) {
        runningSheepTexture_ = Bamboo::SpriteRendererComponentAPI::getTexture(templateEntity);
    }
    if (!handleValid(cameraEntity_)) {
        LOG_ERROR("LevelController: Camera entity was not found");
    }
    if (!runningSheepTexture_.isValid()) {
        LOG_WARN("LevelController: running sheep template missing, using standing art");
    }
}

void LevelController::loadLevel(int levelNumber) {
    const int clampedLevel =
        std::clamp(levelNumber, ProgressStore::kFirstLevel, ProgressStore::kLastLevel);
    const LevelDefinition *definition = findLevel(clampedLevel);
    if (definition == nullptr) {
        LOG_ERROR("LevelController: level %d is absent from the catalog", clampedLevel);
        phase_ = Phase::Error;
        return;
    }

    destroyGeneratedEntities();
    board_.reset();
    clearHint();
    currentLevel_ = clampedLevel;
    hearts_.reset(settings_.immortalModeEnabled());
    phase_ = Phase::Loading;
    updateHUD();
    if (rootView_) {
        rootView_->hideResult();
        rootView_->setBoardInputEnabled(false);
    }

    GenerationResult result = BoardGenerator::generate(definition->generation);
    if (!result.succeeded()) {
        LOG_ERROR(
            "LevelController: failed to generate level %d: %s",
            currentLevel_,
            result.error.c_str()
        );
        phase_ = Phase::Error;
        if (rootView_) {
            rootView_->showLoadError();
        }
        return;
    }

    const GenerationValidationReport validation =
        BoardGenerator::validateResult(definition->generation, result);
    if (!validation.valid()) {
        LOG_ERROR("LevelController: generated level %d failed validation", currentLevel_);
        for (const std::string &error : validation.errors) {
            LOG_ERROR("LevelController: %s", error.c_str());
        }
        phase_ = Phase::Error;
        if (rootView_) {
            rootView_->showLoadError();
        }
        return;
    }

    if (result.metrics.generatedSheepCount < result.metrics.targetSheepCount) {
        LOG_WARN("Level %d: using fullest solvable packing (%d of %d requested sheep)",
            currentLevel_, result.metrics.generatedSheepCount, result.metrics.targetSheepCount);
    }
    board_.emplace(std::move(*result.board));
    // Capture once: worn pasture does not instantly regrow as the flock leaves.
    pasture_.captureFlock(*board_, definition->generation.seed ^ result.metrics.layoutFingerprint);
    createGeneratedEntities(
        definition->generation.seed ^ result.metrics.layoutFingerprint
    );
    viewportWidth_ = 0;
    viewportHeight_ = 0;
    refreshLayout(true);

    if (rootView_) {
        rootView_->hideBombTutorial();
        rootView_->setBoardSize(board_->width(), board_->height());
        rootView_->hideResult();
        rootView_->setBoardInputEnabled(true);
    }
    phase_ = Phase::Playing;
    prepareBombs(*definition);
    updateHUD();
    updateBombVisual();

    LOG_INFO(
        "Level %d ready: %dx%d, %zu sheep, %.0f%% density, %zu solution waves",
        currentLevel_,
        board_->width(),
        board_->height(),
        board_->sheepCount(),
        board_->density() * 100.0,
        result.metrics.solutionWaveCount
    );
}

void LevelController::createGeneratedEntities(std::uint64_t appearanceSeed) {
    if (!board_) {
        return;
    }

    const std::vector<SheepCoat> coatOrder = makeBalancedCoatOrder(
        board_->sheepCount(), appearanceSeed
    );
    sheepVisuals_.reserve(board_->sheepCount());
    std::size_t visualIndex = 0u;
    for (const Sheep &sheep : board_->sheep()) {
        Bamboo::EntityHandle entity = Bamboo::WorldAPI::createEntity("Generated Sheep");
        Bamboo::EntityAPI::addComponent(
            entity, Bamboo::ComponentType::SPRITE_RENDERER_COMPONENT
        );
        Bamboo::TextureHandle texture =
            runningSheepTexture_.isValid() ? runningSheepTexture_ : sheepTexture;
        if (texture.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setTexture(entity, texture);
        }

        SheepVisual visual;
        visual.id = sheep.id;
        visual.entity = entity;
        visual.coat = static_cast<int>(coatOrder[visualIndex]);
        setSheepFrame(visual, 0);
        visual.baseColor = {1.f, 1.f, 1.f, 1.f};
        visual.rotationDegrees = directionAngle(sheep.direction);
        Bamboo::SpriteRendererComponentAPI::setColor(entity, visual.baseColor);
        sheepVisuals_.push_back(visual);
        ++visualIndex;
    }

    if (!sheepTexture.isValid()) {
        LOG_ERROR("LevelController: sheepTexture is not configured in game.world");
    }
}

void LevelController::destroyGeneratedEntities() {
    resetDogRescue();
    bombs_.clear();
    bombTutorialOrder_.clear();
    bombTutorialCursor_ = 0;
    bombExplosionElapsed_ = 0.f;
    fatalBombImpact_ = false;
    for (auto entity : bombAccessoryEntities_) {
        if (entity.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setColor(entity, {1.f, 1.f, 1.f, 0.f});
        }
    }
    if (rootView_) {
        rootView_->hideBombTutorial();
        rootView_->updateBombStatus({}, settings_.immortalModeEnabled());
        rootView_->setBombMarkers({});
    }
    for (SheepVisual &visual : sheepVisuals_) {
        if (visual.entity.isValid()) {
            Bamboo::WorldAPI::destroyEntity(visual.entity);
        }
    }
    sheepVisuals_.clear();
    escapeHerd_.clear();
    blockedReactions_.clear();
}

void LevelController::refreshLayout(bool force) {
    if (!board_) {
        return;
    }

    int width = static_cast<int>(
        std::max<std::uint32_t>(1u, Bamboo::ApplicationAPI::getWidth())
    );
    int height = static_cast<int>(
        std::max<std::uint32_t>(1u, Bamboo::ApplicationAPI::getHeight())
    );
    PandaUI::EdgeInsets safeArea;
    if (window_.isValid()) {
        const PandaUI::Size viewport = window_.context().getViewportSize();
        width = std::max(1, static_cast<int>(std::lround(viewport.width)));
        height = std::max(1, static_cast<int>(std::lround(viewport.height)));
        safeArea = window_.getSafeAreaInsets();
    }
    if (!force && width == viewportWidth_ && height == viewportHeight_) {
        return;
    }
    viewportWidth_ = width;
    viewportHeight_ = height;

    LayoutPoint cameraPosition;
    if (handleValid(cameraEntity_)) {
        const Bamboo::Vec3 position = Bamboo::TransformComponentAPI::getPosition(cameraEntity_);
        cameraPosition = {position.x, position.y};
    }
    layout_ = calculatePastureLayout(
        width,
        height,
        board_->width(),
        board_->height(),
        std::max(0.1f, cameraOrthoSize),
        cameraPosition,
        {
            safeArea.top + 154.f,
            safeArea.right + 18.f,
            safeArea.bottom + 20.f,
            safeArea.left + 18.f,
        }
    );
    if (!layout_.valid()) {
        LOG_ERROR("LevelController: could not calculate a valid board layout");
        phase_ = Phase::Error;
        return;
    }

    pasture_.apply(layout_);

    applyLayoutToGeneratedEntities();
    if (rootView_) {
        rootView_->setBoardFrame(PandaUI::Rect(
            layout_.screenRect.x,
            layout_.screenRect.y,
            layout_.screenRect.width,
            layout_.screenRect.height
        ));
    }
}

void LevelController::applyLayoutToGeneratedEntities() {
    if (!board_ || !layout_.valid()) {
        return;
    }

    for (SheepVisual &visual : sheepVisuals_) {
        const Sheep *sheep = board_->findSheep(visual.id);
        if (sheep == nullptr || !visual.entity.isValid()) {
            continue;
        }
        visual.basePosition = layout_.sheepCenter(*sheep);
        Bamboo::TransformComponentAPI::setPosition(
            visual.entity, {visual.basePosition.x, visual.basePosition.y, 0.1f}
        );
        Bamboo::TransformComponentAPI::setRotationEuler(
            visual.entity, {0.f, 0.f, visual.rotationDegrees}
        );
        Bamboo::TransformComponentAPI::setScale(
            visual.entity,
            {
                layout_.cellWorldSize * kSheepWidthInCells,
                layout_.cellWorldSize * kSheepLengthInCells,
                1.f,
            }
        );
    }

    // Running sheep are no longer in BoardModel. Their original placements and
    // individual distances survive resize; only the final runout is extended.
    if (!escapeHerd_.rebuildPaths(board_->width(), board_->height(), escapeRunout())) {
        LOG_ERROR("LevelController: could not resize active sheep routes");
        phase_ = Phase::Error;
        return;
    }
    updateAnimation(0.f);
}

void LevelController::updateAnimation(float deltaTime) {
    if (!board_ || !layout_.valid() || phase_ != Phase::Playing) {
        return;
    }
    // Stop motion precisely at the deadline before deciding whether the tail
    // crossed the gate. A long frame cannot award a late rescue.
    float step = deltaTime;
    for (const auto &bomb : bombs_) {
        if (bomb.state == BombSheep::State::Armed && !bomb.impact) {
            step = std::min(step, bomb.seconds);
        }
    }
    updateEscapes(step);
    updateBlockedReactions(step);
    for (auto &bomb : bombs_) {
        if (phase_ != Phase::Playing) { break; }
        if (bomb.state == BombSheep::State::Armed && !bomb.impact) {
            bomb.seconds = std::max(0.f, bomb.seconds - step);
            if (bomb.seconds <= 0.f) { expireBomb(bomb); }
        }
    }
    updateBombHUD();
}

void LevelController::updateEscapes(float deltaTime) {
    escapeHerd_.advance(deltaTime);
    for (const EscapingSheep &runner : escapeHerd_.runners()) {
        SheepVisual *visual = findVisual(runner.sheep.id);
        if (visual == nullptr || !visual->entity.isValid()) {
            phase_ = Phase::Error;
            return;
        }
        const EscapeGait gait = sampleEscapeGait(runner);
        // Motion within the run frames supplies the gait. Keep the Sprite on
        // its route without rotating or stretching the entire sheep per step.
        const PenEscape::Sample sample = PenEscape::sample(runner.path, runner.distance);
        if (
            sample.position.y <= -PenEscape::kFenceOffset - kBombGateClearance &&
            std::abs(sample.position.x - static_cast<float>(board_->width()) * 0.5f) < 0.1f) {
            for (auto &bomb : bombs_) {
                if (runner.sheep.id == bomb.id && bomb.state == BombSheep::State::Armed) {
                    bomb.state = BombSheep::State::Rescued;
                    updateBombHUD();
                }
            }
        }
        const float x = layout_.worldRect.x + sample.position.x * layout_.cellWorldSize;
        const float y = layout_.worldRect.y + sample.position.y * layout_.cellWorldSize;
        Bamboo::TransformComponentAPI::setPosition(visual->entity, {x, y, 0.1f});
        Bamboo::TransformComponentAPI::setRotationEuler(
            visual->entity, {0.f, 0.f, sample.rotationDegrees}
        );
        Bamboo::TransformComponentAPI::setScale(visual->entity, {
            layout_.cellWorldSize * kSheepWidthInCells,
            layout_.cellWorldSize * kSheepLengthInCells, 1.f
        });
        setSheepFrame(*visual, gait.pose);
    }

    // Erase state and visuals only after iteration, never invalidate another
    // runner while updating the flock.
    for (SheepId id : escapeHerd_.takeFinished()) {
        finishEscape(id);
    }
}

void LevelController::updateBlockedReactions(float deltaTime) {
    std::vector<SheepId> completed;
    for (auto &reaction : blockedReactions_) {
        reaction.elapsed += deltaTime;
        SheepVisual *visual = findVisual(reaction.id);
        const Sheep *sheep = board_->findSheep(reaction.id);
        if (visual == nullptr || !visual->entity.isValid() || sheep == nullptr) {
            completed.push_back(reaction.id);
            continue;
        }
        const float progress = clamp01(reaction.elapsed / kBlockedDuration);
        const float wave = std::sin(progress * kPi);
        const float bump = wave * layout_.cellWorldSize * 0.18f;
        const Cell delta = directionDelta(sheep->direction);
        Bamboo::TransformComponentAPI::setPosition(visual->entity, {
            visual->basePosition.x + static_cast<float>(delta.x) * bump,
            visual->basePosition.y + static_cast<float>(delta.y) * bump, 0.1f
        });
        Bamboo::SpriteRendererComponentAPI::setColor(
            visual->entity, blend(visual->baseColor, Bamboo::Color{1.f, 0.28f, 0.22f, 1.f}, wave)
        );
        if (progress >= 1.f) { completed.push_back(reaction.id); }
    }
    std::erase_if(blockedReactions_, [](const BlockedReaction &reaction) {
        return reaction.elapsed >= kBlockedDuration;
    });
    for (SheepId id : completed) { finishBlocked(id); }
}

void LevelController::updateHint(float deltaTime) {
    if (hintedSheep_ == kInvalidSheepId || phase_ != Phase::Playing) {
        return;
    }

    SheepVisual *visual = findVisual(hintedSheep_);
    if (visual == nullptr || !visual->entity.isValid()) {
        hintedSheep_ = kInvalidSheepId;
        hintTimeRemaining_ = 0.f;
        return;
    }

    hintTimeRemaining_ = std::max(0.f, hintTimeRemaining_ - deltaTime);
    const float pulse = 0.35f + 0.25f * (std::sin(hintTimeRemaining_ * 8.f) + 1.f);
    Bamboo::SpriteRendererComponentAPI::setColor(
        visual->entity,
        blend(visual->baseColor, Bamboo::Color{1.f, 0.78f, 0.20f, 1.f}, pulse)
    );
    if (hintTimeRemaining_ <= 0.f) {
        clearHint();
    }
}

void LevelController::prepareBombs(const LevelDefinition &definition) {
    if (!board_) { return; }
    const auto placements = planBombPlacement(*board_, definition);
    bool timedTutorial = false;
    bool impactTutorial = false;
    for (const auto &placement : placements) {
        if (bombs_.size() >= bombAccessoryEntities_.size()) {
            LOG_ERROR("LevelController: not enough authored bomb accessory Sprites");
            phase_ = Phase::Error;
            return;
        }
        BombSheep bomb;
        bomb.id = placement.id;
        bomb.impact = placement.impact;
        bomb.seconds = placement.seconds;
        bomb.hitsRemaining = placement.hits;
        bomb.state = BombSheep::State::Armed;
        bomb.entity = bombAccessoryEntities_[bombs_.size()];
        Bamboo::SpriteRendererComponentAPI::setCell(bomb.entity, 2, 1, bomb.impact ? 1 : 0);
        const std::size_t index = bombs_.size();
        bombs_.push_back(bomb);
        if ((!bomb.impact && !timedTutorial) || (bomb.impact && !impactTutorial)) {
            bombTutorialOrder_.push_back(index);
            if (bomb.impact) { impactTutorial = true; } else { timedTutorial = true; }
        }
        LOG_INFO("Level %d: %s bomb sheep %u, %d required removals, %.1f seconds, %d hits",
            currentLevel_, bomb.impact ? "impact" : "timed", bomb.id,
            placement.requiredMoves, bomb.seconds, bomb.hitsRemaining);
    }
    updateBombHUD();
    // Both clocks stay paused while all applicable explanations are read.
    // Replaying a level shows one card for each mechanic again.
    if (rootView_ && !bombTutorialOrder_.empty()) {
        phase_ = Phase::Tutorial;
        rootView_->setBoardInputEnabled(false);
        showBombTutorial();
    }
}

void LevelController::showBombTutorial() {
    if (!rootView_ || bombTutorialCursor_ >= bombTutorialOrder_.size()) { return; }
    const auto &bomb = bombs_[bombTutorialOrder_[bombTutorialCursor_]];
    const SheepVisual *visual = findVisual(bomb.id);
    const auto count = std::count_if(bombs_.begin(), bombs_.end(), [&bomb](const auto &other) {
        return other.impact == bomb.impact;
    });
    rootView_->showBombTutorial(settings_.immortalModeEnabled(), bomb.seconds,
        visual ? visual->coat : 0, bomb.impact, bomb.hitsRemaining, static_cast<int>(count));
}

bool LevelController::hitImpactBomb(SheepId attacker) {
    if (!board_) { return false; }
    // Only the nearest obstacle actually receives this blocked move. A sheep
    // behind another sheep cannot be hit, and the bomb cannot hit itself.
    const auto move = board_->inspectMove(attacker);
    if (move.state != MoveState::Blocked || !move.blocker) { return false; }
    const auto found = std::find_if(bombs_.begin(), bombs_.end(), [&move, attacker](const auto &bomb) {
        return bomb.impact && bomb.state == BombSheep::State::Armed &&
            bomb.id != attacker && bomb.id == *move.blocker;
    });
    if (found == bombs_.end()) { return false; }
    found->hitsRemaining = std::max(0, found->hitsRemaining - 1);
    if (found->hitsRemaining == 0) { expireBomb(*found); }
    updateBombHUD();
    return true;
}

void LevelController::startBombLevel() {
    if (phase_ != Phase::Tutorial) { return; }
    ++bombTutorialCursor_;
    if (bombTutorialCursor_ < bombTutorialOrder_.size()) {
        showBombTutorial();
        return;
    }
    if (rootView_) {
        rootView_->hideBombTutorial();
        rootView_->setBoardInputEnabled(true);
    }
    phase_ = Phase::Playing;
}

PandaUI::Point LevelController::sheepScreenPosition(SheepId id) {
    SheepVisual *visual = findVisual(id);
    if (!visual || !visual->entity.isValid() || !layout_.valid()) { return {}; }
    const auto position = Bamboo::TransformComponentAPI::getPosition(visual->entity);
    const float pixelsPerWorldUnit = static_cast<float>(layout_.viewportHeight) / layout_.cameraOrthoSize;
    return {
        static_cast<float>(layout_.viewportWidth) * 0.5f +
            (position.x - layout_.cameraPosition.x) * pixelsPerWorldUnit,
        static_cast<float>(layout_.viewportHeight) * 0.5f -
            (position.y - layout_.cameraPosition.y) * pixelsPerWorldUnit,
    };
}

void LevelController::expireBomb(BombSheep &bomb) {
    clearHint();
    const bool harmless = settings_.immortalModeEnabled();
    if (harmless && beginDogRescue(bomb)) { return; }
    // Missing authored art or an invalid route must never turn immortality
    // into a defeat. Normal scenes take the animated rescue path above.
    bomb.state = harmless ? BombSheep::State::Defused : BombSheep::State::Exploded;
    if (rootView_) { rootView_->playBombBurst(sheepScreenPosition(bomb.id), harmless); }
    if (!harmless) {
        // A brief, non-graphic puff replaces the sheep before the defeat card.
        // No escape or victory can be completed while the explosion is playing.
        phase_ = Phase::BombExplosion;
        bombExplosionElapsed_ = 0.f;
        fatalBombImpact_ = bomb.impact;
        if (SheepVisual *visual = findVisual(bomb.id); visual && visual->entity.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setColor(visual->entity, {1.f, 1.f, 1.f, 0.f});
        }
        if (rootView_) { rootView_->setBoardInputEnabled(false); }
    }
    updateBombHUD();
}

float LevelController::dogOffscreenY() const {
    const float cameraBottom = layout_.cameraPosition.y - layout_.cameraOrthoSize * .5f;
    return (cameraBottom - layout_.worldRect.y) / layout_.cellWorldSize - 2.8f;
}

bool LevelController::beginDogRescue(BombSheep &bomb) {
    auto *visual = findVisual(bomb.id);
    if (!board_ || !layout_.valid() || !visual || !visual->entity.isValid() ||
        !rescueDogEntity_.isValid() ||
        !Bamboo::SpriteRendererComponentAPI::getTexture(rescueDogEntity_).isValid()) {
        LOG_WARN("LevelController: dog rescue needs the authored ShepherdDogRescuer Sprite and texture");
        return false;
    }
    const auto position = Bamboo::TransformComponentAPI::getPosition(visual->entity);
    const float rotation = Bamboo::TransformComponentAPI::getRotationEuler(visual->entity).z;
    const float angle = rotation * kPi / 180.f;
    const DogRescue::Point bombPosition{
        (position.x - layout_.worldRect.x) / layout_.cellWorldSize - std::sin(angle) * kBombBackOffset,
        (position.y - layout_.worldRect.y) / layout_.cellWorldSize + std::cos(angle) * kBombBackOffset
    };
    std::vector<DogRescue::OtherSheepPose> runners;
    for (const auto &runner : escapeHerd_.runners()) {
        auto *runnerVisual = findVisual(runner.sheep.id);
        if (!runnerVisual || !runnerVisual->entity.isValid()) { continue; }
        const auto p = Bamboo::TransformComponentAPI::getPosition(runnerVisual->entity);
        const float heading = Bamboo::TransformComponentAPI::getRotationEuler(runnerVisual->entity).z;
        runners.push_back({runner.sheep.id,
            {(p.x - layout_.worldRect.x) / layout_.cellWorldSize,
             (p.y - layout_.worldRect.y) / layout_.cellWorldSize}, heading});
    }
    if (!dogRescue_.begin(*board_, bomb.id, bombPosition, rotation, dogOffscreenY(), runners)) {
        LOG_WARN("LevelController: no safe dog route to sheep %u; keeping immortal protection", bomb.id);
        return false;
    }
    dogRescueBomb_ = bomb.id;
    rescueFlockPoses_.clear();
    for (auto &sheep : sheepVisuals_) {
        if (sheep.id == bomb.id || !sheep.entity.isValid()) { continue; }
        const auto p = Bamboo::TransformComponentAPI::getPosition(sheep.entity);
        rescueFlockPoses_.push_back({sheep.id,
            {(p.x - layout_.worldRect.x) / layout_.cellWorldSize,
             (p.y - layout_.worldRect.y) / layout_.cellWorldSize}, {}});
    }
    bomb.state = BombSheep::State::DogRescue;
    phase_ = Phase::DogRescue;
    if (rootView_) {
        rootView_->setBoardInputEnabled(false);
        rootView_->showDogRescueNotice();
    }
    applyDogRescuePose();
    updateBombHUD();
    return true;
}

void LevelController::updateDogRescue(float deltaTime) {
    dogRescue_.setOffscreenY(dogOffscreenY());
    dogRescue_.advance(deltaTime);
    applyDogRescuePose(deltaTime);
    if (dogRescue_.stage() != DogRescue::Stage::Finished) { return; }
    for (auto &bomb : bombs_) {
        if (bomb.id == dogRescueBomb_ && bomb.state == BombSheep::State::DogRescue) {
            bomb.state = BombSheep::State::Defused;
            break;
        }
    }
    resetDogRescue();
    phase_ = Phase::Playing;
    if (rootView_) { rootView_->setBoardInputEnabled(true); }
    updateBombHUD();
}

void LevelController::applyDogRescuePose(float deltaTime) {
    if (!rescueDogEntity_.isValid() || !layout_.valid() || !dogRescue_.active()) { return; }
    const auto &pose = dogRescue_.pose();
    const auto pivot = ShepherdDogAnimation::pivot(pose.frame);
    const float angle = pose.rotationDegrees * kPi / 180.f;
    if (pose.clearingFlock) {
        // In a sealed crowd the flock makes a little room for its shepherd.
        // Only temporary visual offsets change; puzzle cells and sheep IDs do
        // not move. The original poses are restored on exit or cancellation.
        const float follow = 1.f - std::exp(-16.f * std::max(0.f, deltaTime));
        for (auto &saved : rescueFlockPoses_) {
            auto *sheep = findVisual(saved.id);
            if (!sheep || !sheep->entity.isValid()) { continue; }
            const float dx = saved.position.x - pose.position.x;
            const float dy = saved.position.y - pose.position.y;
            const float along = -dx * std::sin(angle) + dy * std::cos(angle);
            const float across = dx * std::cos(angle) + dy * std::sin(angle);
            const float proximity = clamp01((2.3f - std::abs(along - .25f)) / 1.25f);
            const float sign = across == 0.f ? (saved.id % 2 ? 1.f : -1.f)
                                             : (across > 0.f ? 1.f : -1.f);
            const float push = std::clamp(1.05f - std::abs(across), 0.f, .58f) * proximity * sign;
            saved.offset.x += (std::cos(angle) * push - saved.offset.x) * follow;
            saved.offset.y += (std::sin(angle) * push - saved.offset.y) * follow;
            Bamboo::TransformComponentAPI::setPosition(sheep->entity, {
                layout_.worldRect.x + (saved.position.x + saved.offset.x) * layout_.cellWorldSize,
                layout_.worldRect.y + (saved.position.y + saved.offset.y) * layout_.cellWorldSize, .1f
            });
        }
    }
    const float size = layout_.cellWorldSize * ShepherdDogAnimation::kQuadSize;
    // Register the painted torso, not the wagging tail's bounding box. This
    // removes atlas placement jitter without distorting the dog's anatomy.
    const float localX = (.5f - pivot.x) * size;
    const float localY = (pivot.y - .5f) * size;
    Bamboo::TransformComponentAPI::setPosition(rescueDogEntity_, {
        layout_.worldRect.x + pose.position.x * layout_.cellWorldSize +
            localX * std::cos(angle) - localY * std::sin(angle),
        layout_.worldRect.y + (pose.position.y + pose.lift) * layout_.cellWorldSize +
            localX * std::sin(angle) + localY * std::cos(angle), .20f
    });
    Bamboo::TransformComponentAPI::setRotationEuler(rescueDogEntity_, {0.f, 0.f, pose.rotationDegrees});
    Bamboo::TransformComponentAPI::setScale(rescueDogEntity_, {size, size, 1.f});
    Bamboo::SpriteRendererComponentAPI::setCell(rescueDogEntity_, ShepherdDogAnimation::kColumns,
        ShepherdDogAnimation::kRows, ShepherdDogAnimation::encodedCell(pose.frame));
    Bamboo::SpriteRendererComponentAPI::setColor(rescueDogEntity_, {1.f, 1.f, 1.f, 1.f});
    if (rescueDogShadow_.isValid()) {
        // Sheep have no ground-contact shadow, so the dog only gains a faint
        // one while airborne. Fade from zero at takeoff/landing, and lighten
        // it with height instead of darkening the grounded silhouette.
        const float airborne = clamp01((pose.lift - .035f) / .12f);
        const float shadowAlpha = .11f * airborne * airborne * (3.f - 2.f * airborne) *
            (1.f - .25f * clamp01(pose.lift / .65f));
        const float shadowSize = size * (1.f - pose.lift * .08f);
        const float sx = (.5f - pivot.x) * shadowSize;
        const float sy = (pivot.y - .5f) * shadowSize;
        Bamboo::TransformComponentAPI::setPosition(rescueDogShadow_, {
            layout_.worldRect.x + (pose.position.x + .045f) * layout_.cellWorldSize +
                sx * std::cos(angle) - sy * std::sin(angle),
            layout_.worldRect.y + (pose.position.y - .07f) * layout_.cellWorldSize +
                sx * std::sin(angle) + sy * std::cos(angle), .09f
        });
        Bamboo::TransformComponentAPI::setScale(rescueDogShadow_, {shadowSize, shadowSize, 1.f});
        Bamboo::TransformComponentAPI::setRotationEuler(rescueDogShadow_, {0.f, 0.f, pose.rotationDegrees});
        Bamboo::SpriteRendererComponentAPI::setCell(rescueDogShadow_, ShepherdDogAnimation::kColumns,
            ShepherdDogAnimation::kRows, ShepherdDogAnimation::encodedCell(pose.frame));
        Bamboo::SpriteRendererComponentAPI::setColor(rescueDogShadow_, {0.f, 0.f, 0.f, shadowAlpha});
    }
}

void LevelController::resetDogRescue() {
    if (layout_.valid()) {
        for (const auto &saved : rescueFlockPoses_) {
            auto *sheep = findVisual(saved.id);
            if (sheep && sheep->entity.isValid()) {
                Bamboo::TransformComponentAPI::setPosition(sheep->entity, {
                    layout_.worldRect.x + saved.position.x * layout_.cellWorldSize,
                    layout_.worldRect.y + saved.position.y * layout_.cellWorldSize, .1f
                });
            }
        }
    }
    rescueFlockPoses_.clear();
    dogRescue_.clear();
    dogRescueBomb_ = kInvalidSheepId;
    if (rescueDogEntity_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(rescueDogEntity_, {1.f, 1.f, 1.f, 0.f});
    }
    if (rootView_) { rootView_->hideDogRescueNotice(); }
    if (rescueDogShadow_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(rescueDogShadow_, {0.f, 0.f, 0.f, 0.f});
    }
}

void LevelController::updateBombHUD() {
    if (!rootView_) { return; }
    std::vector<UI::BombHUDState> states;
    for (const auto &bomb : bombs_) {
        UI::BombHUDState state;
        state.seconds = bomb.seconds;
        state.hits = bomb.hitsRemaining;
        state.impact = bomb.impact;
        state.rescued = bomb.state == BombSheep::State::Rescued;
        state.defused = bomb.state == BombSheep::State::Defused;
        states.push_back(state);
    }
    rootView_->updateBombStatus(states, settings_.immortalModeEnabled());
}

void LevelController::updateBombVisual() {
    std::vector<UI::BombDisplay> displays;
    for (auto &bomb : bombs_) {
        SheepVisual *visual = findVisual(bomb.id);
        const bool dogRescue = bomb.state == BombSheep::State::DogRescue && bomb.id == dogRescueBomb_;
        const bool visible = (bomb.state == BombSheep::State::Armed || dogRescue) && visual &&
            visual->entity.isValid() && layout_.valid() && bomb.entity.isValid() &&
            Bamboo::SpriteRendererComponentAPI::getTexture(bomb.entity).isValid();
        if (bomb.entity.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setColor(bomb.entity,
                {1.f, 1.f, 1.f, visible ? 1.f : 0.f});
        }
        if (!visible) { continue; }
        if (dogRescue && dogRescue_.pose().carrying) {
            const auto &pose = dogRescue_.pose();
            const float x = layout_.worldRect.x + pose.bombPosition.x * layout_.cellWorldSize;
            const float y = layout_.worldRect.y + (pose.bombPosition.y + pose.lift) * layout_.cellWorldSize;
            Bamboo::TransformComponentAPI::setPosition(bomb.entity, {x, y, .22f});
            Bamboo::TransformComponentAPI::setRotationEuler(bomb.entity, {0.f, 0.f, pose.bombRotationDegrees});
            const float size = layout_.cellWorldSize * kBombSizeInCells;
            Bamboo::TransformComponentAPI::setScale(bomb.entity, {size, size, 1.f});
            // The disarmed display goes dark after the jaws take its strap.
            continue;
        }
        const auto position = Bamboo::TransformComponentAPI::getPosition(visual->entity);
        const auto rotation = Bamboo::TransformComponentAPI::getRotationEuler(visual->entity);
        const float angle = rotation.z * kPi / 180.f;
        const float offset = layout_.cellWorldSize * kBombBackOffset;
        Bamboo::TransformComponentAPI::setPosition(bomb.entity, {
            position.x - std::sin(angle) * offset,
            position.y + std::cos(angle) * offset, 0.16f
        });
        Bamboo::TransformComponentAPI::setRotationEuler(bomb.entity, rotation);
        const float size = layout_.cellWorldSize * kBombSizeInCells;
        Bamboo::TransformComponentAPI::setScale(bomb.entity, {size, size, 1.f});
        const float cellPixels = layout_.screenRect.width / static_cast<float>(layout_.columns);
        auto displayCenter = sheepScreenPosition(bomb.id);
        // Same back offset, size and rotation as the world Sprite. Digits are
        // painted in its display, including throughout turns and blocked bumps.
        displayCenter.x -= std::sin(angle) * cellPixels * kBombBackOffset;
        displayCenter.y -= std::cos(angle) * cellPixels * kBombBackOffset;
        UI::BombDisplay display;
        display.center = displayCenter;
        display.size = cellPixels * kBombSizeInCells;
        display.rotationDegrees = rotation.z;
        display.value = bomb.impact ? bomb.hitsRemaining : static_cast<int>(std::ceil(bomb.seconds));
        display.impact = bomb.impact;
        displays.push_back(display);
    }
    if (rootView_) { rootView_->setBombMarkers(displays); }
}

void LevelController::selectCell(Cell cell) {
    if (phase_ != Phase::Playing || hearts_.isDepleted() || !board_) {
        return;
    }

    const std::optional<SheepId> sheepId = board_->sheepAt(cell);
    if (!sheepId || isReacting(*sheepId)) {
        return;
    }

    clearHint();
    const MoveCheck move = board_->inspectMove(*sheepId);
    if (move.canEscape()) {
        beginEscape(*sheepId);
    } else if (move.state == MoveState::Blocked) {
        beginBlocked(*sheepId);
    }
}

void LevelController::beginEscape(SheepId sheepId) {
    SheepVisual *visual = findVisual(sheepId);
    if (visual == nullptr || !visual->entity.isValid() || !board_ || isReacting(sheepId)) {
        return;
    }
    if (!escapeHerd_.launch(*board_, sheepId, escapeRunout())) {
        LOG_ERROR("LevelController: could not start sheep %u", sheepId);
        return;
    }
    setSheepFrame(*visual, 0);
    // Keep Playing and leave board input enabled. Cells are already free, and
    // this sheep's route/animation no longer depends on subsequent selections.
    updateHUD();
}

void LevelController::beginBlocked(SheepId sheepId) {
    if (findVisual(sheepId) == nullptr || isReacting(sheepId) || hearts_.isDepleted()) {
        return;
    }
    blockedReactions_.push_back({sheepId, 0.f});
    // The impact bomb is its own mistake budget: do not spend a heart as well.
    // Other blocked attempts keep the existing life penalty.
    if (!hitImpactBomb(sheepId)) { hearts_.blockedMove(); }
    updateHUD();
    // Only defeat locks the whole field; a short reaction locks its own sheep.
    if (hearts_.isDepleted() && rootView_) {
        rootView_->setBoardInputEnabled(false);
    }
}

void LevelController::finishEscape(SheepId escaped) {
    SheepVisual *visual = findVisual(escaped);
    if (visual != nullptr && visual->entity.isValid()) {
        Bamboo::WorldAPI::destroyEntity(visual->entity);
    }
    std::erase_if(sheepVisuals_, [escaped](const SheepVisual &candidate) {
        return candidate.id == escaped;
    });

    updateHUD();
    // Empty logical cells are not victory yet: wait for every running sheep.
    // Escapes may also finish behind the defeat dialog without changing it.
    if (phase_ == Phase::Playing && !hearts_.isDepleted() && board_ &&
        board_->empty() && escapeHerd_.empty()) {
        phase_ = Phase::Victory;
        // Testing unlocked levels must not advance the normal campaign or
        // mark those levels complete after the cheat is switched off.
        if (!settings_.cheatModeEnabled() &&
            progress_.recordCompletion(currentLevel_, hearts_.remaining())) {
            (void)progress_.save();
        }
        if (rootView_) {
            rootView_->setBoardInputEnabled(false);
            rootView_->showVictory(hearts_.remaining(), currentLevel_ < ProgressStore::kLastLevel);
        }
    }
}

void LevelController::finishBlocked(SheepId id) {
    // Also remove a reaction whose entity disappeared during teardown/error.
    std::erase_if(blockedReactions_, [id](const BlockedReaction &reaction) {
        return reaction.id == id;
    });
    if (SheepVisual *visual = findVisual(id)) {
        if (visual->entity.isValid()) {
            Bamboo::TransformComponentAPI::setPosition(
                visual->entity, {visual->basePosition.x, visual->basePosition.y, 0.1f}
            );
            Bamboo::TransformComponentAPI::setRotationEuler(
                visual->entity, {0.f, 0.f, visual->rotationDegrees}
            );
            Bamboo::SpriteRendererComponentAPI::setColor(visual->entity, visual->baseColor);
        }
    }
    if (hearts_.isDepleted() && phase_ == Phase::Playing) {
        clearHint();
        phase_ = Phase::Defeat;
        if (rootView_) {
            rootView_->setBoardInputEnabled(false);
            rootView_->showDefeat();
        }
    }
}

void LevelController::showHint() {
    if (phase_ != Phase::Playing || hearts_.isDepleted() || !board_) {
        return;
    }

    clearHint();
    const std::vector<SheepId> available = board_->escapableSheep();
    // While the clock runs, help clear the bomb sheep's actual blocker chain.
    const BombSheep *priority = nullptr;
    for (const auto &bomb : bombs_) {
        if (bomb.state != BombSheep::State::Armed || !board_->findSheep(bomb.id)) { continue; }
        if (!priority || (!bomb.impact && (priority->impact || bomb.seconds < priority->seconds))) {
            priority = &bomb;
        }
    }
    if (priority) {
        SheepId target = priority->id;
        for (std::size_t depth = 0; depth < board_->sheepCount(); ++depth) {
            const MoveCheck move = board_->inspectMove(target);
            if (move.canEscape()) {
                if (!isReacting(target)) {
                    hintedSheep_ = target;
                    hintTimeRemaining_ = kHintDuration;
                }
                return;
            }
            if (move.state != MoveState::Blocked || !move.blocker) { break; }
            target = *move.blocker;
        }
    }
    const auto candidate = std::find_if(available.begin(), available.end(),
        [this](SheepId id) { return !isReacting(id); });
    if (candidate == available.end()) {
        // All remaining sheep may already be running, or finishing a reaction.
        return;
    }
    hintedSheep_ = *candidate;
    hintTimeRemaining_ = kHintDuration;
}

void LevelController::clearHint() {
    if (hintedSheep_ != kInvalidSheepId) {
        if (SheepVisual *visual = findVisual(hintedSheep_)) {
            if (visual->entity.isValid()) {
                Bamboo::SpriteRendererComponentAPI::setColor(visual->entity, visual->baseColor);
            }
        }
    }
    hintedSheep_ = kInvalidSheepId;
    hintTimeRemaining_ = 0.f;
}

void LevelController::restartLevel() {
    loadLevel(currentLevel_);
}

void LevelController::nextLevel() {
    if (currentLevel_ >= ProgressStore::kLastLevel) {
        returnToMenu();
        return;
    }
    loadLevel(currentLevel_ + 1);
}

void LevelController::returnToMenu() {
    (void)progress_.save();
    if (!menuWorld.isValid()) {
        LOG_ERROR("LevelController: menuWorld is not configured in game.world");
        return;
    }
    const Bamboo::WorldHandle targetWorld = menuWorld;
    defer([targetWorld]() { Bamboo::WorldAPI::load(targetWorld); });
}

void LevelController::updateHUD() {
    if (rootView_) {
        rootView_->updateHUD(
            currentLevel_, hearts_.remaining(), board_ ?
                static_cast<int>(board_->sheepCount() + escapeHerd_.size()) : 0
        );
    }
}

void LevelController::defer(std::function<void()> action) {
    if (!action) {
        return;
    }
    if (window_.isValid()) {
        window_.context().postDeferred(std::move(action));
    } else {
        action();
    }
}

LevelController::SheepVisual *LevelController::findVisual(SheepId sheepId) noexcept {
    const auto found = std::find_if(
        sheepVisuals_.begin(), sheepVisuals_.end(), [sheepId](const SheepVisual &visual) {
            return visual.id == sheepId;
        }
    );
    return found == sheepVisuals_.end() ? nullptr : &*found;
}

bool LevelController::isReacting(SheepId id) const noexcept {
    return std::any_of(blockedReactions_.begin(), blockedReactions_.end(),
        [id](const BlockedReaction &reaction) { return reaction.id == id; });
}

float LevelController::escapeRunout() const {
    if (!layout_.valid()) { return PenEscape::kOutsideRunout; }
    const float cameraBottom = layout_.cameraPosition.y - layout_.cameraOrthoSize * 0.5f;
    // Keep every sheep visible until its tail has left even a tall viewport.
    return (layout_.worldRect.y - cameraBottom) / layout_.cellWorldSize
        - PenEscape::kFenceOffset + 1.2f;
}

void LevelController::setSheepFrame(SheepVisual &visual, int pose) {
    if (!runningSheepTexture_.isValid()) {
        if (visual.frame != 0) {
            Bamboo::SpriteRendererComponentAPI::setCell(visual.entity, 3, 1, visual.coat);
            visual.frame = 0;
        }
        return;
    }
    const int logical = SheepRunAnimation::logicalCell(visual.coat, pose);
    if (visual.frame == logical) { return; }
    Bamboo::SpriteRendererComponentAPI::setCell(visual.entity,
        SheepRunAnimation::kColumns, SheepRunAnimation::kRows,
        SheepRunAnimation::encodedCell(visual.coat, pose));
    visual.frame = logical;
}

} // namespace LiberateTheSheep::Game
