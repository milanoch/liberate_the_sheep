#include "Game/MenuDiorama.hpp"

#include "Game/MenuSheepMotion.hpp"
#include "Game/SheepAppearance.hpp"

#include <Bamboo/ApplicationAPI.hpp>
#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace LiberateTheSheep::Game {
namespace {

    constexpr float kCameraOrthoSize = 9.f;
    constexpr float kCameraHalfHeight = kCameraOrthoSize * 0.5f;
    // The opaque menu panorama has its playable grass line at 9% of the
    // texture height measured from the bottom. Keep 0.9 world units of its
    // painted soil visible below that line on every viewport shape.
    constexpr float kBackgroundGroundFromBottom = 0.09f;
    constexpr float kVisibleSoilDepth = 0.90f;
    constexpr float kRunnerOffscreenMargin = 1.4f;
    constexpr float kCloudOffscreenMargin = 0.35f;
    constexpr std::size_t kMaximumActiveRunners = 6u;
    // Visible alpha bounds of hay_bale.png, expressed from the bottom of its
    // Sprite quad. A tiny overlap settles the bale into the grass blades.
    constexpr float kHayVisibleBottom = 166.f / 1024.f;
    constexpr float kHayVisibleTop = (1024.f - 117.f) / 1024.f;
    // Alpha spans x=133..1420 in the 1536px hay texture. Enclose the
    // painted bale about the Sprite centre, not its transparent side padding.
    constexpr float kHayVisibleHalfWidth = 0.425f;
    constexpr float kHayGrassOverlap = 0.025f;
    constexpr int kRunAtlasColumns = 16;
    constexpr int kRunAtlasRows = 15;
    constexpr int kRunFrameCount = MenuSheepMotion::kRunFrames;
    constexpr int kJumpFirstFrame = kRunFrameCount;
    constexpr int kJumpFrameCount = 24;
    // Whole-sprite inbetweens preserve the original artwork. The atlas stores
    // chronological poses with eight padding cells per coat; no runtime rig.
    constexpr int kCoatStride = 80;

    static_assert(kJumpFirstFrame + kJumpFrameCount <= kCoatStride);
    static_assert(kCoatStride * kSheepCoatCount == kRunAtlasColumns * kRunAtlasRows);

    // PandaSDK currently resolves an atlas cell as
    //   column = index % cols, row = index % rows
    // instead of using index / cols for the row. Convert our ordinary row-major
    // index to the value expected by that ABI. 16 and 15 are coprime, so every
    // physical cell has exactly one encoded value in the 240-cell range.
    [[nodiscard]] constexpr int pandaAtlasIndex(int logicalIndex) noexcept {
        const int column = logicalIndex % kRunAtlasColumns;
        const int row = logicalIndex / kRunAtlasColumns;
        const int cellCount = kRunAtlasColumns * kRunAtlasRows;
        for (int encoded = column; encoded < cellCount; encoded += kRunAtlasColumns) {
            if (encoded % kRunAtlasRows == row) {
                return encoded;
            }
        }
        return 0;
    }

    [[nodiscard]] constexpr bool pandaAtlasMappingIsComplete() noexcept {
        const int cellCount = kRunAtlasColumns * kRunAtlasRows;
        for (int logical = 0; logical < cellCount; ++logical) {
            const int encoded = pandaAtlasIndex(logical);
            if (
                encoded % kRunAtlasColumns != logical % kRunAtlasColumns ||
                encoded % kRunAtlasRows != logical / kRunAtlasColumns
            ) {
                return false;
            }
        }
        return true;
    }

    static_assert(pandaAtlasMappingIsComplete());

    [[nodiscard]] bool valid(Bamboo::EntityHandle &entity) noexcept {
        return entity.isValid();
    }

    [[nodiscard]] Bamboo::Vec3 absoluteScale(const Bamboo::Vec3 &scale) noexcept {
        return {
            std::max(0.001f, std::abs(scale.x)),
            std::max(0.001f, std::abs(scale.y)),
            std::max(0.001f, std::abs(scale.z)),
        };
    }

    [[nodiscard]] float wrapped(float value, float length) noexcept {
        if (length <= 0.f) {
            return 0.f;
        }
        const float result = std::fmod(value, length);
        return result < 0.f ? result + length : result;
    }

} // namespace

void MenuDiorama::start() {
    runners_.clear();
    clouds_.clear();
    elapsed_ = 0.f;
    running_ = true;
    spawnDelay_ = 0.15f;
    random_.seed(std::random_device{}());

    Bamboo::EntityHandle camera = Bamboo::WorldAPI::findByTag("Camera");
    if (valid(camera)) {
        const Bamboo::Vec3 cameraPosition =
            Bamboo::TransformComponentAPI::getPosition(camera);
        cameraCenterX_ = cameraPosition.x;
        cameraCenterY_ = cameraPosition.y;
    } else {
        cameraCenterX_ = 0.f;
        cameraCenterY_ = 0.f;
        LOG_WARN("MenuDiorama: Camera entity was not found; using origin fallback");
    }

    obstacle_ = Bamboo::WorldAPI::findByTag("MenuObstacle");
    if (valid(obstacle_)) {
        const Bamboo::Vec3 position =
            Bamboo::TransformComponentAPI::getPosition(obstacle_);
        const Bamboo::Vec3 scale = absoluteScale(
            Bamboo::TransformComponentAPI::getScale(obstacle_)
        );
        obstacleCameraOffsetX_ = position.x - cameraCenterX_;
        obstacleX_ = position.x;
        obstacleDepth_ = position.z;
        obstacleBaseScale_ = scale;
        obstacleTopY_ = position.y + scale.y * 0.5f;
        obstacleHalfWidth_ = scale.x * kHayVisibleHalfWidth;
    } else {
        obstacleX_ = cameraCenterX_;
        obstacleTopY_ = cameraCenterY_ - 2.5f;
        obstacleHalfWidth_ = 0.75f;
        LOG_WARN(
            "MenuDiorama: MenuObstacle entity was not found; using centre jump fallback"
        );
    }

    background_ = Bamboo::WorldAPI::findByTag("MenuBackground");
    if (valid(background_)) {
        backgroundOrigin_ =
            Bamboo::TransformComponentAPI::getPosition(background_);
        backgroundBaseScale_ = absoluteScale(
            Bamboo::TransformComponentAPI::getScale(background_)
        );
    } else {
        LOG_WARN("MenuDiorama: MenuBackground entity was not found");
    }

    sun_ = Bamboo::WorldAPI::findByTag("MenuSun");
    if (valid(sun_)) {
        sunOrigin_ = Bamboo::TransformComponentAPI::getPosition(sun_);
        sunBaseScale_ = absoluteScale(
            Bamboo::TransformComponentAPI::getScale(sun_)
        );
    } else {
        LOG_WARN("MenuDiorama: MenuSun entity was not found");
    }

    std::vector<Bamboo::EntityHandle> entities =
        Bamboo::WorldAPI::findAllByTag("MenuRunner");
    runners_.reserve(entities.size());
    for (std::size_t index = 0; index < entities.size(); ++index) {
        Bamboo::EntityHandle &entity = entities[index];
        if (!valid(entity)) {
            continue;
        }

        const Bamboo::Vec3 position =
            Bamboo::TransformComponentAPI::getPosition(entity);
        const Bamboo::Vec3 scale =
            Bamboo::TransformComponentAPI::getScale(entity);

        Runner runner;
        runner.entity = entity;
        runner.x = position.x;
        runner.groundY = position.y;
        runner.depth = position.z;
        // Keep authored scale/depth, but choose direction anew for each sheep.
        // Until then, this direction only determines the offscreen parking side.
        runner.direction = scale.x < 0.f ? -1.f : 1.f;
        runner.baseScale = absoluteScale(scale);
        runners_.push_back(runner);
    }

    if (runners_.empty()) {
        LOG_WARN("MenuDiorama: no MenuRunner entities were found");
    }

    std::vector<Bamboo::EntityHandle> cloudEntities =
        Bamboo::WorldAPI::findAllByTag("MenuCloud");
    clouds_.reserve(cloudEntities.size());
    for (std::size_t index = 0; index < cloudEntities.size(); ++index) {
        Bamboo::EntityHandle &entity = cloudEntities[index];
        if (!valid(entity)) {
            continue;
        }

        const Bamboo::Vec3 position =
            Bamboo::TransformComponentAPI::getPosition(entity);
        const Bamboo::Vec3 scale = absoluteScale(
            Bamboo::TransformComponentAPI::getScale(entity)
        );

        Cloud cloud;
        cloud.entity = entity;
        cloud.x = position.x;
        cloud.baseY = position.y;
        cloud.depth = position.z;
        cloud.speed = 0.11f + static_cast<float>(index % 4u) * 0.055f;
        cloud.phase = static_cast<float>(index) * 1.91f;
        cloud.halfWidth = std::max(0.35f, scale.x * 0.5f);
        clouds_.push_back(cloud);
    }

    if (clouds_.empty()) {
        LOG_WARN("MenuDiorama: no MenuCloud entities were found");
    }

    refreshLayout();
    for (Cloud &cloud : clouds_) {
        applyCloudTransform(cloud);
    }
    applySunTransform();

    LOG_INFO(
        "MenuDiorama: started with %zu runners and %zu clouds",
        runners_.size(),
        clouds_.size()
    );
}

void MenuDiorama::update(float deltaTime) {
    const std::uint32_t width =
        std::max<std::uint32_t>(1u, Bamboo::ApplicationAPI::getWidth());
    const std::uint32_t height =
        std::max<std::uint32_t>(1u, Bamboo::ApplicationAPI::getHeight());
    if (width != viewportWidth_ || height != viewportHeight_) {
        refreshLayout();
    }

    if (!running_) {
        return;
    }

    const float step = std::clamp(deltaTime, 0.f, 0.1f);
    elapsed_ += step;
    updateRunners(step);

    for (Cloud &cloud : clouds_) {
        if (!valid(cloud.entity)) {
            continue;
        }

        const float cloudLeft =
            cameraCenterX_ - visibleHalfWidth_ - cloud.halfWidth -
            kCloudOffscreenMargin;
        const float cloudRight =
            cameraCenterX_ + visibleHalfWidth_ + cloud.halfWidth +
            kCloudOffscreenMargin;
        const float cloudRouteLength = cloudRight - cloudLeft;

        cloud.x -= cloud.speed * step;
        if (cloud.x < cloudLeft) {
            cloud.x = cloudRight -
                wrapped(cloudLeft - cloud.x, cloudRouteLength);
        }
        applyCloudTransform(cloud);
    }

    applySunTransform();
}

void MenuDiorama::setRunning(bool running) noexcept {
    running_ = running;
}

void MenuDiorama::refreshLayout() {
    viewportWidth_ =
        std::max<std::uint32_t>(1u, Bamboo::ApplicationAPI::getWidth());
    viewportHeight_ =
        std::max<std::uint32_t>(1u, Bamboo::ApplicationAPI::getHeight());

    const float aspect =
        static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_);
    visibleHalfWidth_ = kCameraOrthoSize * aspect * 0.5f;
    const float cameraBottom = cameraCenterY_ - kCameraHalfHeight;
    const float cameraTop = cameraCenterY_ + kCameraHalfHeight;
    groundSurfaceY_ = cameraBottom + kVisibleSoilDepth;

    if (valid(background_)) {
        const float baseWidth = std::max(0.001f, backgroundBaseScale_.x);
        const float baseHeight = std::max(0.001f, backgroundBaseScale_.y);
        const float visibleWidth = visibleHalfWidth_ * 2.f;
        const float heightForSoil =
            (groundSurfaceY_ - cameraBottom) / kBackgroundGroundFromBottom;
        const float heightForSky =
            (cameraTop - groundSurfaceY_) /
            (1.f - kBackgroundGroundFromBottom);
        const float cover = std::max({
            visibleWidth / baseWidth,
            heightForSoil / baseHeight,
            heightForSky / baseHeight,
        });
        const float scaledWidth = baseWidth * cover;
        const float scaledHeight = baseHeight * cover;
        const float backgroundCenterY = groundSurfaceY_ +
            (0.5f - kBackgroundGroundFromBottom) * scaledHeight;
        Bamboo::TransformComponentAPI::setPosition(
            background_,
            {
                cameraCenterX_,
                backgroundCenterY,
                backgroundOrigin_.z,
            }
        );
        Bamboo::TransformComponentAPI::setScale(
            background_,
            {scaledWidth, scaledHeight, backgroundBaseScale_.z}
        );
    }

    if (valid(obstacle_)) {
        obstacleX_ = cameraCenterX_ + obstacleCameraOffsetX_;
        const float obstacleY = groundSurfaceY_ +
            (0.5f - kHayVisibleBottom) * obstacleBaseScale_.y -
            kHayGrassOverlap;
        Bamboo::TransformComponentAPI::setPosition(
            obstacle_,
            {obstacleX_, obstacleY, obstacleDepth_}
        );
        obstacleTopY_ = groundSurfaceY_ - kHayGrassOverlap +
            (kHayVisibleTop - kHayVisibleBottom) * obstacleBaseScale_.y;
    }

    for (Runner &runner : runners_) {
        if (!valid(runner.entity)) { continue; }
        runner.groundY = groundSurfaceY_ +
            MenuSheepMotion::kGroundOffset * runner.baseScale.y;
        if (runner.active) {
            // Preserve position/animation phase on resize, even while paused.
            const auto arc = MenuSheepMotion::makeJumpArc(
                obstacleHalfWidth_, obstacleTopY_ - runner.groundY,
                runner.baseScale.x, runner.baseScale.y
            );
            runner.routeTime = MenuSheepMotion::routeTime(
                (runner.x - obstacleX_) * runner.direction, arc
            );
            applyRunnerTransform(runner);
        } else {
            // Hidden actors must stay outside the *new* viewport as it widens.
            parkRunner(runner);
        }
    }
}

float MenuDiorama::runnerOffscreenDistance(const Runner &runner) const {
    const auto arc = MenuSheepMotion::makeJumpArc(
        obstacleHalfWidth_, obstacleTopY_ - runner.groundY,
        runner.baseScale.x, runner.baseScale.y
    );
    const float margin = std::max(kRunnerOffscreenMargin, runner.baseScale.x * 0.5f + 0.1f);
    return std::max(visibleHalfWidth_ + std::abs(obstacleX_ - cameraCenterX_) + margin,
                    arc.windowHalfWidth() + margin);
}

void MenuDiorama::parkRunner(Runner &runner) const {
    runner.x = obstacleX_ - runner.direction * runnerOffscreenDistance(runner);
    applyRunnerTransform(runner);
}

void MenuDiorama::updateRunners(float step) {
    std::size_t activeCount = 0u;
    for (Runner &runner : runners_) {
        if (!valid(runner.entity)) {
            runner.active = false;
            continue;
        }
        if (!runner.active) { continue; }
        runner.routeTime += step;
        updateRunnerPosition(runner);
        if ((runner.x - obstacleX_) * runner.direction > runnerOffscreenDistance(runner)) {
            // Recycle only after the complete sheep has left the current view.
            runner.active = false;
            parkRunner(runner);
        } else {
            ++activeCount;
            applyRunnerTransform(runner);
        }
    }

    // One continuous arrival timer, not a preselected flock or a direction cycle.
    // At capacity wait for just one free actor, without building a spawn backlog.
    spawnDelay_ = std::max(0.f, spawnDelay_ - step);
    if (spawnDelay_ > 0.f || activeCount >= kMaximumActiveRunners) { return; }

    auto next = std::find_if(runners_.begin(), runners_.end(), [](Runner &runner) {
        return !runner.active && valid(runner.entity);
    });
    if (next == runners_.end()) { return; }
    Runner &runner = *next;
    // Sample only this arrival. Other sheep may still be running either way.
    runner.direction = std::uniform_int_distribution<int>(0, 1)(random_) == 0 ? -1.f : 1.f;
    runner.coat = std::uniform_int_distribution<int>(0, static_cast<int>(kSheepCoatCount) - 1)(random_);
    runner.frame = -1;
    const auto arc = MenuSheepMotion::makeJumpArc(
        obstacleHalfWidth_, obstacleTopY_ - runner.groundY,
        runner.baseScale.x, runner.baseScale.y
    );
    float spawnDistance = -runnerOffscreenDistance(runner);
    for (const Runner &other : runners_) {
        if (!other.active || other.direction != runner.direction) { continue; }
        // A narrower viewport must not move a new entry ahead of the old tail.
        const float spacing = std::max(MenuSheepMotion::kRunSpeed * 1.1f,
            (runner.baseScale.x + other.baseScale.x) * 0.5f + 0.15f);
        spawnDistance = std::min(spawnDistance,
            (other.x - obstacleX_) * runner.direction - spacing);
    }
    runner.routeTime = MenuSheepMotion::routeTime(spawnDistance, arc);
    runner.active = true;
    updateRunnerPosition(runner);
    applyRunnerTransform(runner);
    spawnDelay_ = std::uniform_real_distribution<float>(0.7f, 2.1f)(random_);
}

void MenuDiorama::updateRunnerPosition(Runner &runner) const {
    const MenuSheepMotion::JumpArc arc = MenuSheepMotion::makeJumpArc(
        obstacleHalfWidth_, obstacleTopY_ - runner.groundY,
        runner.baseScale.x, runner.baseScale.y
    );
    runner.x = obstacleX_ + runner.direction *
        MenuSheepMotion::routeDistance(runner.routeTime, arc);
}

void MenuDiorama::applyRunnerTransform(Runner &runner) const {
    const MenuSheepMotion::JumpArc arc = MenuSheepMotion::makeJumpArc(
        obstacleHalfWidth_,
        obstacleTopY_ - runner.groundY,
        runner.baseScale.x,
        runner.baseScale.y
    );
    const MenuSheepMotion::Pose pose = MenuSheepMotion::sample(
        (runner.x - obstacleX_) * runner.direction, arc, runner.baseScale.y
    );
    // Articulation is baked into each complete Sprite frame. Distance selects
    // the run phase; the jump sequence follows the faster airborne traversal.
    const int runFrame = std::clamp(
        static_cast<int>(std::floor(pose.runFramePosition)), 0, kRunFrameCount - 1
    );
    const int coatBase = std::clamp(
        runner.coat, 0, static_cast<int>(kSheepCoatCount) - 1
    ) * kCoatStride;
    int frame = coatBase + runFrame;
    if (pose.jumpFramePosition >= 0.f) {
        const int jumpPose = std::clamp(
            static_cast<int>(std::lround(pose.jumpFramePosition)), 0, kJumpFrameCount - 1
        );
        frame = coatBase + kJumpFirstFrame + jumpPose;
    }

    Bamboo::TransformComponentAPI::setPosition(
        runner.entity,
        {
            runner.x + runner.direction * pose.forwardOffset,
            runner.groundY + pose.rise,
            runner.depth,
        }
    );
    Bamboo::TransformComponentAPI::setScale(
        runner.entity,
        {
            runner.direction * runner.baseScale.x * pose.scaleX,
            runner.baseScale.y * pose.scaleY,
            runner.baseScale.z,
        }
    );
    Bamboo::TransformComponentAPI::setRotationEuler(
        runner.entity,
        {0.f, 0.f, runner.direction * pose.pitchDegrees}
    );

    if (frame != runner.frame) {
        Bamboo::SpriteRendererComponentAPI::setCell(
            runner.entity, kRunAtlasColumns, kRunAtlasRows, pandaAtlasIndex(frame)
        );
        runner.frame = frame;
    }
}

void MenuDiorama::applyCloudTransform(Cloud &cloud) const {
    const float gentleBob =
        std::sin(elapsed_ * 0.31f + cloud.phase) * 0.045f;
    Bamboo::TransformComponentAPI::setPosition(
        cloud.entity,
        {cloud.x, cloud.baseY + gentleBob, cloud.depth}
    );
}

void MenuDiorama::applySunTransform() {
    if (!valid(sun_)) {
        return;
    }

    const float responsiveAnchorX =
        cameraCenterX_ - visibleHalfWidth_ + 1.15f;
    const float driftX = std::sin(elapsed_ * 0.13f + 0.45f) * 0.13f;
    const float driftY = std::sin(elapsed_ * 0.19f) * 0.075f;
    const float breathe = 1.f + std::sin(elapsed_ * 0.23f) * 0.008f;
    Bamboo::TransformComponentAPI::setPosition(
        sun_,
        {responsiveAnchorX + driftX, sunOrigin_.y + driftY, sunOrigin_.z}
    );
    Bamboo::TransformComponentAPI::setScale(
        sun_,
        {
            sunBaseScale_.x * breathe,
            sunBaseScale_.y * breathe,
            sunBaseScale_.z,
        }
    );
}

} // namespace LiberateTheSheep::Game
