#include "Game/LevelController.hpp"

#include "Game/BoardGenerator.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "Game/SheepAppearance.hpp"
#include "Game/SheepRunAnimation.hpp"
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
    if (phase_ == Phase::BombExplosion) {
        bomb_.explosionElapsed += step;
        if (bomb_.explosionElapsed >= kBombExplosionDuration) {
            phase_ = Phase::Defeat;
            if (rootView_) { rootView_->showBombDefeat(bomb_.impact); }
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
    pasture_.resolve();
    bombEntity_ = Bamboo::WorldAPI::findByTag("BombSheepAccessory");
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

    GenerationResult result = BoardGenerator::generate(definition->generation);
    if (!result.succeeded()) {
        LOG_ERROR(
            "LevelController: failed to generate level %d: %s",
            currentLevel_,
            result.error.c_str()
        );
        phase_ = Phase::Error;
        if (rootView_) {
            rootView_->showDefeat();
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
            rootView_->showDefeat();
        }
        return;
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
    prepareBomb(definition->bombSeconds, definition->bombPreferredWave,
        definition->generation.seed ^ result.metrics.layoutFingerprint,
        definition->impactBombHits, definition->impactBombPreferredWave);
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
    bomb_ = {};
    if (bombEntity_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(bombEntity_, {1.f, 1.f, 1.f, 0.f});
    }
    if (rootView_) {
        rootView_->hideBombTutorial();
        rootView_->updateBombTimer(0.f, false, false, false, settings_.immortalModeEnabled());
        rootView_->setBombMarker({}, 0.f, 0.f, 0, false);
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
    const float step = bomb_.state == BombSheep::State::Armed && !bomb_.impact
        ? std::min(deltaTime, bomb_.seconds) : deltaTime;
    updateEscapes(step);
    updateBlockedReactions(step);
    if (phase_ == Phase::Playing && bomb_.state == BombSheep::State::Armed && !bomb_.impact) {
        bomb_.seconds = std::max(0.f, bomb_.seconds - step);
        if (bomb_.seconds <= 0.f) { expireBomb(); }
        updateBombHUD();
    }
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
        if (runner.sheep.id == bomb_.id && bomb_.state == BombSheep::State::Armed &&
            sample.position.y <= -PenEscape::kFenceOffset - kBombGateClearance &&
            std::abs(sample.position.x - static_cast<float>(board_->width()) * 0.5f) < 0.1f) {
            bomb_.state = BombSheep::State::Rescued;
            updateBombHUD();
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

void LevelController::prepareBomb(float seconds, int preferredWave, std::uint64_t seed,
                                 int impactHits, int impactPreferredWave) {
    if (!board_ || (seconds <= 0.f && impactHits <= 0)) { return; }
    bomb_.impact = impactHits > 0;
    if (bomb_.impact) { preferredWave = impactPreferredWave; }
    // Choose from the generator's solvable dependency waves, rather than a
    // random deeply buried sheep. The first encounter needs only an early chain.
    const auto solution = board_->analyzeSolvability();
    if (solution.solutionWaves.empty()) { return; }
    const auto waveIndex = std::min(static_cast<std::size_t>(std::max(1, preferredWave) - 1),
        solution.solutionWaves.size() - 1);
    const auto &candidates = solution.solutionWaves[waveIndex];
    if (candidates.empty()) { return; }
    bomb_.id = candidates[seed % candidates.size()];
    if (bomb_.impact) {
        // Introduce an actual collision hazard: prefer a sheep which is the
        // nearest blocker of another sheep, while retaining a solvable route.
        // Try the preferred wave first, then the other dependency waves.
        for (std::size_t offset = 0; offset < solution.solutionWaves.size(); ++offset) {
            const auto &wave = solution.solutionWaves[(waveIndex + offset) % solution.solutionWaves.size()];
            std::vector<SheepId> exposed;
            for (const SheepId candidate : wave) {
                if (std::any_of(board_->sheep().begin(), board_->sheep().end(),
                    [this, candidate](const Sheep &attacker) {
                        return board_->inspectMove(attacker.id).blocker == candidate;
                    })) {
                    exposed.push_back(candidate);
                }
            }
            if (!exposed.empty()) {
                bomb_.id = exposed[seed % exposed.size()];
                break;
            }
        }
    }
    bomb_.seconds = seconds;
    bomb_.hitsRemaining = std::max(0, impactHits);
    bomb_.state = BombSheep::State::Armed;
    if (bombEntity_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setCell(bombEntity_, 2, 1, bomb_.impact ? 1 : 0);
    }
    updateBombHUD();

    // Explain the mechanic on every entry, including replaying a completed
    // level and restarting it. The countdown begins only after confirmation.
    if (rootView_) {
        const SheepVisual *visual = findVisual(bomb_.id);
        phase_ = Phase::Tutorial;
        rootView_->setBoardInputEnabled(false);
        rootView_->showBombTutorial(settings_.immortalModeEnabled(), seconds,
            visual ? visual->coat : 0, bomb_.impact, bomb_.hitsRemaining);
    }
}

bool LevelController::hitImpactBomb(SheepId attacker) {
    if (!board_ || !bomb_.impact || bomb_.state != BombSheep::State::Armed ||
        attacker == bomb_.id) {
        return false;
    }
    // Only the nearest obstacle actually receives this blocked move. A sheep
    // behind another sheep cannot be hit, and the bomb cannot hit itself.
    const auto move = board_->inspectMove(attacker);
    if (move.state != MoveState::Blocked || move.blocker != bomb_.id) { return false; }
    bomb_.hitsRemaining = std::max(0, bomb_.hitsRemaining - 1);
    if (bomb_.hitsRemaining == 0) { expireBomb(); }
    updateBombHUD();
    return true;
}

void LevelController::startBombLevel() {
    if (phase_ != Phase::Tutorial) { return; }
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

void LevelController::expireBomb() {
    clearHint();
    const bool harmless = settings_.immortalModeEnabled();
    bomb_.state = harmless ? BombSheep::State::Defused : BombSheep::State::Exploded;
    if (rootView_) { rootView_->playBombBurst(sheepScreenPosition(bomb_.id), harmless); }
    if (!harmless) {
        // A brief, non-graphic puff replaces the sheep before the defeat card.
        // No escape or victory can be completed while the explosion is playing.
        phase_ = Phase::BombExplosion;
        bomb_.explosionElapsed = 0.f;
        if (SheepVisual *visual = findVisual(bomb_.id); visual && visual->entity.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setColor(visual->entity, {1.f, 1.f, 1.f, 0.f});
        }
        if (rootView_) { rootView_->setBoardInputEnabled(false); }
    }
    updateBombHUD();
}

void LevelController::updateBombHUD() {
    if (!rootView_) { return; }
    rootView_->updateBombTimer(bomb_.seconds, bomb_.state != BombSheep::State::None,
        bomb_.state == BombSheep::State::Rescued, bomb_.state == BombSheep::State::Defused,
        settings_.immortalModeEnabled(), bomb_.impact, bomb_.hitsRemaining);
}

void LevelController::updateBombVisual() {
    SheepVisual *visual = findVisual(bomb_.id);
    const bool visible = bomb_.state == BombSheep::State::Armed && visual &&
        visual->entity.isValid() && layout_.valid() && bombEntity_.isValid() &&
        Bamboo::SpriteRendererComponentAPI::getTexture(bombEntity_).isValid();
    if (bombEntity_.isValid()) {
        Bamboo::SpriteRendererComponentAPI::setColor(bombEntity_, {1.f, 1.f, 1.f, visible ? 1.f : 0.f});
    }
    if (!visible) {
        if (rootView_) { rootView_->setBombMarker({}, 0.f, 0.f, 0, false); }
        return;
    }
    const auto position = Bamboo::TransformComponentAPI::getPosition(visual->entity);
    const auto rotation = Bamboo::TransformComponentAPI::getRotationEuler(visual->entity);
    const float angle = rotation.z * kPi / 180.f;
    const float offset = layout_.cellWorldSize * kBombBackOffset;
    if (bombEntity_.isValid()) {
        Bamboo::TransformComponentAPI::setPosition(bombEntity_, {
            position.x - std::sin(angle) * offset,
            position.y + std::cos(angle) * offset, 0.16f
        });
        Bamboo::TransformComponentAPI::setRotationEuler(bombEntity_, rotation);
        const float size = layout_.cellWorldSize * kBombSizeInCells;
        Bamboo::TransformComponentAPI::setScale(bombEntity_, {size, size, 1.f});
    }
    if (rootView_) {
        const float cellPixels = layout_.screenRect.width / static_cast<float>(layout_.columns);
        auto displayCenter = sheepScreenPosition(bomb_.id);
        // Same back offset, size and rotation as the world Sprite. Digits are
        // painted in its display, including throughout turns and blocked bumps.
        displayCenter.x -= std::sin(angle) * cellPixels * kBombBackOffset;
        displayCenter.y -= std::cos(angle) * cellPixels * kBombBackOffset;
        rootView_->setBombMarker(displayCenter, cellPixels * kBombSizeInCells,
            rotation.z, bomb_.impact ? bomb_.hitsRemaining : static_cast<int>(std::ceil(bomb_.seconds)),
            true, bomb_.impact);
    }
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
    if (bomb_.state == BombSheep::State::Armed && board_->findSheep(bomb_.id)) {
        SheepId target = bomb_.id;
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
