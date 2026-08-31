#include "Game/LevelController.hpp"

#include "Game/BoardGenerator.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/Localization.hpp"
#include "Game/SessionState.hpp"
#include "Game/SheepAppearance.hpp"
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
    constexpr float kEscapeDuration = 0.42f;
    constexpr float kBlockedDuration = 0.30f;
    constexpr float kHintDuration = 2.4f;
    constexpr float kSheepWidthInCells = 0.82f;
    constexpr float kSheepLengthInCells = 1.78f;
    constexpr float kGridCellFill = 0.90f;

    [[nodiscard]] float clamp01(float value) noexcept {
        return std::clamp(value, 0.f, 1.f);
    }

    [[nodiscard]] float easeOutCubic(float value) noexcept {
        const float inverse = 1.f - clamp01(value);
        return 1.f - inverse * inverse * inverse;
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
    backgroundEntity_ = Bamboo::WorldAPI::findByTag("GameBackground");
    boardSurfaceEntity_ = Bamboo::WorldAPI::findByTag("BoardSurface");
    boardShadowEntity_ = Bamboo::WorldAPI::findByTag("BoardShadow");

    if (!handleValid(cameraEntity_)) {
        LOG_ERROR("LevelController: Camera entity was not found");
    }
    if (!handleValid(backgroundEntity_)) {
        LOG_WARN("LevelController: GameBackground entity was not found");
    }
    if (!handleValid(boardSurfaceEntity_)) {
        LOG_WARN("LevelController: BoardSurface entity was not found");
    }
    if (!handleValid(boardShadowEntity_)) {
        LOG_WARN("LevelController: BoardShadow entity was not found");
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
    activeSheep_ = kInvalidSheepId;
    animationElapsed_ = 0.f;
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

    gridEntities_.reserve(static_cast<std::size_t>(board_->cellCount()));
    for (int y = 0; y < board_->height(); ++y) {
        for (int x = 0; x < board_->width(); ++x) {
            Bamboo::EntityHandle entity = Bamboo::WorldAPI::createEntity("Generated Grid Cell");
            Bamboo::EntityAPI::addComponent(
                entity, Bamboo::ComponentType::SPRITE_RENDERER_COMPONENT
            );
            const bool alternate = ((x + y) & 1) != 0;
            Bamboo::SpriteRendererComponentAPI::setColor(
                entity,
                alternate ? Bamboo::Color{0.24f, 0.50f, 0.20f, 0.14f}
                          : Bamboo::Color{0.92f, 1.f, 0.72f, 0.12f}
            );
            gridEntities_.push_back(entity);
        }
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
        if (sheepTexture.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setTexture(entity, sheepTexture);
            Bamboo::SpriteRendererComponentAPI::setCell(
                entity,
                static_cast<int>(kSheepCoatCount),
                1,
                static_cast<int>(coatOrder[visualIndex])
            );
        }

        SheepVisual visual;
        visual.id = sheep.id;
        visual.entity = entity;
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
    for (Bamboo::EntityHandle &entity : gridEntities_) {
        if (entity.isValid()) {
            Bamboo::WorldAPI::destroyEntity(entity);
        }
    }
    gridEntities_.clear();

    for (SheepVisual &visual : sheepVisuals_) {
        if (visual.entity.isValid()) {
            Bamboo::WorldAPI::destroyEntity(visual.entity);
        }
    }
    sheepVisuals_.clear();
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
    layout_ = calculateBoardLayout(
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

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const float visibleWidth = cameraOrthoSize * aspect;
    const float visibleHeight = cameraOrthoSize;
    constexpr float backgroundAspect = 2.f / 3.f;
    float backgroundWidth = visibleWidth;
    float backgroundHeight = visibleHeight;
    if (visibleWidth / visibleHeight > backgroundAspect) {
        backgroundHeight = visibleWidth / backgroundAspect;
    } else {
        backgroundWidth = visibleHeight * backgroundAspect;
    }
    if (handleValid(backgroundEntity_)) {
        Bamboo::TransformComponentAPI::setPosition(
            backgroundEntity_, {cameraPosition.x, cameraPosition.y, -2.f}
        );
        Bamboo::TransformComponentAPI::setScale(
            backgroundEntity_, {backgroundWidth, backgroundHeight, 1.f}
        );
    }

    const float boardCenterX = layout_.worldRect.x + layout_.worldRect.width * 0.5f;
    const float boardCenterY = layout_.worldRect.y + layout_.worldRect.height * 0.5f;
    if (handleValid(boardSurfaceEntity_)) {
        Bamboo::TransformComponentAPI::setPosition(
            boardSurfaceEntity_, {boardCenterX, boardCenterY, -0.7f}
        );
        Bamboo::TransformComponentAPI::setScale(
            boardSurfaceEntity_,
            {layout_.worldRect.width + 0.36f, layout_.worldRect.height + 0.36f, 1.f}
        );
    }
    if (handleValid(boardShadowEntity_)) {
        Bamboo::TransformComponentAPI::setPosition(
            boardShadowEntity_, {boardCenterX, boardCenterY - 0.14f, -0.8f}
        );
        Bamboo::TransformComponentAPI::setScale(
            boardShadowEntity_,
            {layout_.worldRect.width + 0.66f, layout_.worldRect.height + 0.66f, 1.f}
        );
    }

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

    std::size_t gridIndex = 0;
    for (int y = 0; y < board_->height(); ++y) {
        for (int x = 0; x < board_->width(); ++x) {
            if (gridIndex >= gridEntities_.size()) {
                break;
            }
            Bamboo::EntityHandle &entity = gridEntities_[gridIndex++];
            const LayoutPoint center = layout_.cellCenter({x, y});
            Bamboo::TransformComponentAPI::setPosition(entity, {center.x, center.y, -0.5f});
            Bamboo::TransformComponentAPI::setScale(
                entity,
                {
                    layout_.cellWorldSize * kGridCellFill,
                    layout_.cellWorldSize * kGridCellFill,
                    1.f,
                }
            );
        }
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

    if (activeSheep_ != kInvalidSheepId) {
        if (SheepVisual *visual = findVisual(activeSheep_)) {
            animationStart_ = visual->basePosition;
            if (const Sheep *sheep = activeSheepModel()) {
                animationTarget_ = escapeTarget(*sheep);
            }
        }
    }
}

void LevelController::updateAnimation(float deltaTime) {
    if (phase_ != Phase::Escaping && phase_ != Phase::Blocked) {
        return;
    }

    SheepVisual *visual = findVisual(activeSheep_);
    if (visual == nullptr || !visual->entity.isValid()) {
        phase_ = Phase::Error;
        return;
    }

    animationElapsed_ += deltaTime;
    if (phase_ == Phase::Escaping) {
        const float progress = clamp01(animationElapsed_ / kEscapeDuration);
        const float eased = easeOutCubic(progress);
        const float x = animationStart_.x + (animationTarget_.x - animationStart_.x) * eased;
        const float y = animationStart_.y + (animationTarget_.y - animationStart_.y) * eased;
        Bamboo::TransformComponentAPI::setPosition(visual->entity, {x, y, 0.1f});
        Bamboo::TransformComponentAPI::setRotationEuler(
            visual->entity,
            {0.f, 0.f, visual->rotationDegrees + std::sin(progress * kPi * 4.f) * 2.5f}
        );
        if (progress >= 1.f) {
            finishEscape();
        }
        return;
    }

    const float progress = clamp01(animationElapsed_ / kBlockedDuration);
    const float bump = std::sin(progress * kPi) * layout_.cellWorldSize * 0.18f;
    const Sheep *sheep = activeSheepModel();
    const Cell delta = sheep != nullptr ? directionDelta(sheep->direction) : Cell{};
    Bamboo::TransformComponentAPI::setPosition(
        visual->entity,
        {
            animationStart_.x + static_cast<float>(delta.x) * bump,
            animationStart_.y + static_cast<float>(delta.y) * bump,
            0.1f,
        }
    );
    Bamboo::SpriteRendererComponentAPI::setColor(
        visual->entity,
        blend(visual->baseColor, Bamboo::Color{1.f, 0.28f, 0.22f, 1.f}, std::sin(progress * kPi))
    );
    if (progress >= 1.f) {
        finishBlocked();
    }
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
    if (phase_ != Phase::Playing || !board_) {
        return;
    }

    const std::optional<SheepId> sheepId = board_->sheepAt(cell);
    if (!sheepId) {
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
    const Sheep *sheep = board_ ? board_->findSheep(sheepId) : nullptr;
    if (visual == nullptr || sheep == nullptr) {
        return;
    }

    activeSheep_ = sheepId;
    animationElapsed_ = 0.f;
    animationStart_ = visual->basePosition;
    animationTarget_ = escapeTarget(*sheep);
    phase_ = Phase::Escaping;
    if (rootView_) {
        rootView_->setBoardInputEnabled(false);
    }
}

void LevelController::beginBlocked(SheepId sheepId) {
    SheepVisual *visual = findVisual(sheepId);
    if (visual == nullptr) {
        return;
    }

    activeSheep_ = sheepId;
    animationElapsed_ = 0.f;
    animationStart_ = visual->basePosition;
    hearts_ = std::max(0, hearts_ - 1);
    phase_ = Phase::Blocked;
    updateHUD();
    if (rootView_) {
        rootView_->setBoardInputEnabled(false);
    }
}

void LevelController::finishEscape() {
    if (!board_ || activeSheep_ == kInvalidSheepId) {
        phase_ = Phase::Error;
        return;
    }

    const SheepId escaped = activeSheep_;
    SheepVisual *visual = findVisual(escaped);
    if (visual != nullptr && visual->entity.isValid()) {
        Bamboo::WorldAPI::destroyEntity(visual->entity);
    }
    sheepVisuals_.erase(
        std::remove_if(
            sheepVisuals_.begin(),
            sheepVisuals_.end(),
            [escaped](const SheepVisual &candidate) { return candidate.id == escaped; }
        ),
        sheepVisuals_.end()
    );

    if (!board_->eraseSheep(escaped)) {
        LOG_ERROR("LevelController: escaped sheep %u was absent from the model", escaped);
        phase_ = Phase::Error;
        return;
    }

    activeSheep_ = kInvalidSheepId;
    animationElapsed_ = 0.f;
    updateHUD();
    if (board_->empty()) {
        phase_ = Phase::Victory;
        if (progress_.recordCompletion(currentLevel_, hearts_)) {
            (void)progress_.save();
        }
        if (rootView_) {
            rootView_->showVictory(hearts_, currentLevel_ < ProgressStore::kLastLevel);
        }
    } else {
        phase_ = Phase::Playing;
        if (rootView_) {
            rootView_->setBoardInputEnabled(true);
        }
    }
}

void LevelController::finishBlocked() {
    if (SheepVisual *visual = findVisual(activeSheep_)) {
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

    activeSheep_ = kInvalidSheepId;
    animationElapsed_ = 0.f;
    if (hearts_ <= 0) {
        phase_ = Phase::Defeat;
        if (rootView_) {
            rootView_->showDefeat();
        }
    } else {
        phase_ = Phase::Playing;
        if (rootView_) {
            rootView_->setBoardInputEnabled(true);
        }
    }
}

void LevelController::showHint() {
    if (phase_ != Phase::Playing || !board_) {
        return;
    }

    clearHint();
    const std::vector<SheepId> available = board_->escapableSheep();
    if (available.empty()) {
        LOG_WARN("LevelController: no escapable sheep found on a validated level");
        return;
    }
    hintedSheep_ = available.front();
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
            currentLevel_, hearts_, board_ ? static_cast<int>(board_->sheepCount()) : 0
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

const Sheep *LevelController::activeSheepModel() const noexcept {
    return board_ ? board_->findSheep(activeSheep_) : nullptr;
}

LayoutPoint LevelController::escapeTarget(const Sheep &sheep) const noexcept {
    const Cell delta = directionDelta(sheep.direction);
    const float distance =
        std::max(layout_.worldRect.width, layout_.worldRect.height) +
        layout_.cellWorldSize * 3.f;
    return {
        animationStart_.x + static_cast<float>(delta.x) * distance,
        animationStart_.y + static_cast<float>(delta.y) * distance,
    };
}

} // namespace LiberateTheSheep::Game
