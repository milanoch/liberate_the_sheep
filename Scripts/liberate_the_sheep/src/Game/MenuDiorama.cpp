#include "Game/MenuDiorama.hpp"

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
    constexpr float kRunnerOffscreenMargin = 1.4f;
    constexpr float kCloudOffscreenMargin = 0.35f;
    constexpr float kMinimumJumpHalfWidth = 1.35f;
    constexpr float kMinimumJumpHeight = 0.80f;
    constexpr float kObstacleClearance = 0.10f;
    // Each square atlas cell contains transparent vertical padding; the visible
    // sheep occupies roughly 72% of the cell height.
    constexpr float kRunnerVisibleHalfHeight = 0.36f;
    constexpr float kRunnerSpeed = 2.05f;
    constexpr float kRunnerCenterSpacing = 2.2f;
    constexpr float kRunnerOffscreenHold = 1.f;
    constexpr float kRunFramesPerSecond = 20.f;
    constexpr int kRunAtlasColumns = 16;
    constexpr int kRunAtlasRows = 9;
    constexpr int kRunFrameCount = 16;
    constexpr int kJumpFirstFrame = 16;
    constexpr int kJumpFrameCount = 24;
    constexpr int kCoatStride = 48;

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
        obstacleX_ = position.x;
        obstacleTopY_ = position.y + scale.y * 0.5f;
        obstacleHalfWidth_ = scale.x * 0.5f;
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

    ground_ = Bamboo::WorldAPI::findByTag("MenuGround");
    if (valid(ground_)) {
        groundBaseScale_ = absoluteScale(
            Bamboo::TransformComponentAPI::getScale(ground_)
        );
    } else {
        LOG_WARN("MenuDiorama: MenuGround entity was not found");
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
        // The sheep art faces right. Mirroring scale.x in menu.world is the
        // editor-authored direction control and remains stable across updates.
        runner.direction = scale.x < 0.f ? -1.f : 1.f;
        runner.baseScale = absoluteScale(scale);
        runner.speed = kRunnerSpeed;
        runners_.push_back(runner);
    }

    std::stable_sort(
        runners_.begin(),
        runners_.end(),
        [](const Runner &lhs, const Runner &rhs) {
            if (lhs.direction != rhs.direction) {
                return lhs.direction > rhs.direction;
            }
            return lhs.direction > 0.f ? lhs.x > rhs.x : lhs.x < rhs.x;
        }
    );

    std::uint64_t coatSeed = 1469598103934665603ULL;
    for (const Runner &runner : runners_) {
        coatSeed = (coatSeed ^ runner.entity.id) * 1099511628211ULL;
    }
    const std::vector<SheepCoat> coatOrder = makeBalancedCoatOrder(
        runners_.size(), coatSeed
    );
    for (std::size_t index = 0; index < runners_.size(); ++index) {
        runners_[index].coat = static_cast<int>(coatOrder[index]);
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
    for (Runner &runner : runners_) {
        updateScheduledRunnerPosition(runner);
        applyRunnerTransform(runner);
    }
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
    for (Runner &runner : runners_) {
        if (!valid(runner.entity)) {
            continue;
        }

        updateScheduledRunnerPosition(runner);
        applyRunnerTransform(runner);
    }

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

    const float resetDistance = visibleHalfWidth_ + kRunnerOffscreenMargin;
    const float directionSwitchGap =
        2.f * resetDistance / kRunnerSpeed + kRunnerOffscreenHold;

    const std::size_t rightCount = static_cast<std::size_t>(std::count_if(
        runners_.begin(),
        runners_.end(),
        [](const Runner &runner) { return runner.direction > 0.f; }
    ));
    const std::size_t leftCount = runners_.size() - rightCount;

    if (rightCount > 0u && leftCount > 0u) {
        // Run small same-direction flocks, then leave enough time for the last
        // sheep to exit before a flock enters from the opposite side. This
        // prevents two mirrored sprites from merging into a two-headed shape.
        const float rightSpan = kRunnerCenterSpacing *
            static_cast<float>(rightCount - 1u);
        const float leftSpan = kRunnerCenterSpacing *
            static_cast<float>(leftCount - 1u);
        const float leftStart = rightSpan + directionSwitchGap;
        runnerCycleDuration_ =
            rightSpan + directionSwitchGap + leftSpan + directionSwitchGap;

        std::size_t rightOrdinal = 0u;
        std::size_t leftOrdinal = 0u;
        for (Runner &runner : runners_) {
            if (runner.direction > 0.f) {
                runner.scheduleTime =
                    static_cast<float>(rightOrdinal++) * kRunnerCenterSpacing;
            } else {
                runner.scheduleTime = leftStart +
                    static_cast<float>(leftOrdinal++) * kRunnerCenterSpacing;
            }
        }
    } else {
        const float minimumCycle = std::max(
            kRunnerCenterSpacing *
                static_cast<float>(std::max<std::size_t>(1u, runners_.size())),
            directionSwitchGap
        );
        runnerCycleDuration_ = minimumCycle;
        for (std::size_t index = 0; index < runners_.size(); ++index) {
            runners_[index].scheduleTime =
                runnerCycleDuration_ * static_cast<float>(index) /
                static_cast<float>(runners_.size());
        }
    }

    if (valid(background_)) {
        const Bamboo::Vec3 position =
            Bamboo::TransformComponentAPI::getPosition(background_);
        Bamboo::TransformComponentAPI::setPosition(
            background_,
            {
                cameraCenterX_ + backgroundOrigin_.x,
                cameraCenterY_ + backgroundOrigin_.y,
                position.z,
            }
        );
        // The authored 2:3 portrait illustration keeps its aspect ratio. Narrow
        // phones crop its sides; wide windows expose the blue MenuBackdrop and
        // the responsive MenuGround instead of stretching the trees and hills.
        Bamboo::TransformComponentAPI::setScale(
            background_,
            backgroundBaseScale_
        );
    }

    if (valid(ground_)) {
        const Bamboo::Vec3 position =
            Bamboo::TransformComponentAPI::getPosition(ground_);
        const float groundHeight = groundBaseScale_.y;
        Bamboo::TransformComponentAPI::setPosition(
            ground_,
            {
                cameraCenterX_,
                cameraCenterY_ - kCameraOrthoSize * 0.5f + groundHeight * 0.5f,
                position.z,
            }
        );
        Bamboo::TransformComponentAPI::setScale(
            ground_,
            {
                visibleHalfWidth_ * 2.f + 0.2f,
                groundHeight,
                groundBaseScale_.z,
            }
        );
    }
}

void MenuDiorama::updateScheduledRunnerPosition(Runner &runner) const {
    const float relativeTime = wrapped(
        elapsed_ - runner.scheduleTime + runnerCycleDuration_ * 0.5f,
        runnerCycleDuration_
    ) - runnerCycleDuration_ * 0.5f;
    runner.x = obstacleX_ + runner.direction * runner.speed * relativeTime;
}

void MenuDiorama::applyRunnerTransform(Runner &runner) const {
    const float jumpHalfWidth = std::max(
        kMinimumJumpHalfWidth,
        obstacleHalfWidth_ + runner.baseScale.x * 0.75f
    );
    const float normalizedDistance =
        (runner.x - obstacleX_) / jumpHalfWidth;
    const float jumpProgress = normalizedDistance * runner.direction;
    const float jumpTimeline = std::clamp((jumpProgress + 1.f) * 0.5f, 0.f, 1.f);
    constexpr float kLiftOffTimeline = 0.17f;
    constexpr float kTouchDownTimeline = 0.83f;
    const float airborneTimeline = std::clamp(
        (jumpTimeline - kLiftOffTimeline) /
            (kTouchDownTimeline - kLiftOffTimeline),
        0.f,
        1.f
    );
    float jumpAmount = 0.f;
    float jump = 0.f;
    if (
        std::abs(normalizedDistance) < 1.f &&
        jumpTimeline > kLiftOffTimeline &&
        jumpTimeline < kTouchDownTimeline
    ) {
        // The first four atlas poses stay on the ground for approach and
        // compression. The body then follows one smooth parabola while the
        // 24-frame sequence shows push, tuck, descent, contact, and recovery.
        jumpAmount = 4.f * airborneTimeline * (1.f - airborneTimeline);
        const float clearanceHeight =
            obstacleTopY_ + runner.baseScale.y * kRunnerVisibleHalfHeight +
            kObstacleClearance - runner.groundY;
        jump = jumpAmount * std::max(kMinimumJumpHeight, clearanceHeight);
    } else if (std::abs(normalizedDistance) < 1.f) {
        // Anticipation and landing compression are continuous and return to
        // zero at take-off/touch-down. They support the authored poses without
        // reintroducing per-frame transform pops.
        if (jumpTimeline < kLiftOffTimeline) {
            const float anticipation = jumpTimeline / kLiftOffTimeline;
            jump = -std::sin(anticipation * 3.14159265359f) * 0.045f;
        } else if (jumpTimeline > kTouchDownTimeline) {
            const float landing = (jumpTimeline - kTouchDownTimeline) /
                (1.f - kTouchDownTimeline);
            jump = -std::sin(landing * 3.14159265359f) * 0.035f;
        }
    }

    const float framesPerWorldUnit = kRunFramesPerSecond / runner.speed;
    int runFrame = 0;
    if (jumpProgress < -1.f) {
        const float distanceToTakeOff = (-1.f - jumpProgress) * jumpHalfWidth;
        const int framesToTakeOff = static_cast<int>(
            std::floor(distanceToTakeOff * framesPerWorldUnit)
        );
        runFrame = (kRunFrameCount - framesToTakeOff % kRunFrameCount) %
            kRunFrameCount;
    } else if (jumpProgress > 1.f) {
        const float distanceFromLanding = (jumpProgress - 1.f) * jumpHalfWidth;
        runFrame = static_cast<int>(
            std::floor(distanceFromLanding * framesPerWorldUnit)
        ) % kRunFrameCount;
    }

    const int coatBase = std::clamp(
        runner.coat,
        0,
        static_cast<int>(kSheepCoatCount) - 1
    ) * kCoatStride;
    int frame = coatBase + runFrame;
    if (std::abs(normalizedDistance) < 1.f) {
        const int jumpFrame = std::min(
            kJumpFrameCount - 1,
            static_cast<int>(std::floor(jumpTimeline * kJumpFrameCount))
        );
        frame = coatBase + kJumpFirstFrame + jumpFrame;
    }

    Bamboo::TransformComponentAPI::setPosition(
        runner.entity,
        {runner.x, runner.groundY + jump, runner.depth}
    );
    Bamboo::TransformComponentAPI::setScale(
        runner.entity,
        {
            runner.direction * runner.baseScale.x,
            runner.baseScale.y,
            runner.baseScale.z,
        }
    );
    Bamboo::TransformComponentAPI::setRotationEuler(
        runner.entity,
        {0.f, 0.f, 0.f}
    );

    if (frame != runner.frame) {
        Bamboo::SpriteRendererComponentAPI::setCell(
            runner.entity,
            kRunAtlasColumns,
            kRunAtlasRows,
            frame
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
        cameraCenterX_ - visibleHalfWidth_ + 1.0f;
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
