#include "Game/LevelController.hpp"

#include "Game/BoardGenerator.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "Game/SheepAppearance.hpp"
#include "Game/SheepRunAnimation.hpp"
#include "UI/GameplayRootView.hpp"

#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/EntityAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
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
    if (!progress_.isUnlocked(levelToLoad)) {
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
    updateAnimation(std::max(0.f, deltaTime));
    updateHint(std::max(0.f, deltaTime));
}

void LevelController::shutdown() {
    if (window_.isValid() && rootView_ &&
        window_.context().getRootView() == rootView_) {
        window_.setRootView(nullptr);
    }
    rootView_.reset();
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

    rootView_ =
        std::make_shared<UI::GameplayRootView>(std::move(callbacks), std::move(text));
    window_.setRootView(rootView_);
}

void LevelController::resolveSceneEntities() {
    cameraEntity_ = Bamboo::WorldAPI::findByTag("Camera");
    pasture_.resolve();
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
    hearts_ = ProgressStore::kMaximumHearts;
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
    createGeneratedEntities(
        definition->generation.seed ^ result.metrics.layoutFingerprint
    );
    viewportWidth_ = 0;
    viewportHeight_ = 0;
    refreshLayout(true);

    if (rootView_) {
        rootView_->setBoardSize(board_->width(), board_->height());
        rootView_->hideResult();
        rootView_->setBoardInputEnabled(true);
    }
    phase_ = Phase::Playing;
    updateHUD();

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
    if (!board_ || !layout_.valid() || phase_ == Phase::Loading || phase_ == Phase::Error) {
        return;
    }
    updateEscapes(deltaTime);
    updateBlockedReactions(deltaTime);
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

void LevelController::selectCell(Cell cell) {
    if (phase_ != Phase::Playing || hearts_ <= 0 || !board_) {
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
    if (findVisual(sheepId) == nullptr || isReacting(sheepId) || hearts_ <= 0) {
        return;
    }
    blockedReactions_.push_back({sheepId, 0.f});
    hearts_ = std::max(0, hearts_ - 1);
    updateHUD();
    // Only defeat locks the whole field; a short reaction locks its own sheep.
    if (hearts_ == 0 && rootView_) {
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
    if (phase_ == Phase::Playing && hearts_ > 0 && board_ &&
        board_->empty() && escapeHerd_.empty()) {
        phase_ = Phase::Victory;
        if (progress_.recordCompletion(currentLevel_, hearts_)) {
            (void)progress_.save();
        }
        if (rootView_) {
            rootView_->setBoardInputEnabled(false);
            rootView_->showVictory(hearts_, currentLevel_ < ProgressStore::kLastLevel);
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
    if (hearts_ <= 0 && phase_ == Phase::Playing) {
        clearHint();
        phase_ = Phase::Defeat;
        if (rootView_) {
            rootView_->setBoardInputEnabled(false);
            rootView_->showDefeat();
        }
    }
}

void LevelController::showHint() {
    if (phase_ != Phase::Playing || hearts_ <= 0 || !board_) {
        return;
    }

    clearHint();
    const std::vector<SheepId> available = board_->escapableSheep();
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
            currentLevel_, hearts_, board_ ?
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
