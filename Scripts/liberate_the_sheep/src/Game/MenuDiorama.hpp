#pragma once

#include <Bamboo/Bamboo.hpp>

#include <cstdint>
#include <vector>

namespace LiberateTheSheep::Game {

// Drives the entities authored in menu.world. It deliberately owns no resources
// and creates no entities: the editor remains the source of truth for the scene.
class MenuDiorama final {
public:
    void start();
    void update(float deltaTime);
    void setRunning(bool running) noexcept;

private:
    struct Runner {
        Bamboo::EntityHandle entity;
        float x{};
        float groundY{};
        float depth{};
        float speed{};
        float scheduleTime{};
        float direction{1.f};
        int coat{};
        int frame{-1};
        Bamboo::Vec3 baseScale{1.f, 1.f, 1.f};
    };

    struct Cloud {
        Bamboo::EntityHandle entity;
        float x{};
        float baseY{};
        float depth{};
        float speed{};
        float phase{};
        float halfWidth{0.5f};
    };

    std::vector<Runner> runners_;
    std::vector<Cloud> clouds_;
    Bamboo::EntityHandle obstacle_;
    Bamboo::EntityHandle sun_;
    Bamboo::EntityHandle background_;
    Bamboo::EntityHandle ground_;

    Bamboo::Vec3 sunOrigin_{};
    Bamboo::Vec3 backgroundOrigin_{};
    Bamboo::Vec3 sunBaseScale_{1.f, 1.f, 1.f};
    Bamboo::Vec3 backgroundBaseScale_{1.f, 1.f, 1.f};
    Bamboo::Vec3 groundBaseScale_{1.f, 1.f, 1.f};

    bool running_{true};
    std::uint32_t viewportWidth_{};
    std::uint32_t viewportHeight_{};
    float cameraCenterX_{};
    float cameraCenterY_{};
    float visibleHalfWidth_{4.5f};
    float obstacleX_{};
    float obstacleTopY_{};
    float obstacleHalfWidth_{0.75f};
    float runnerCycleDuration_{13.2f};
    float elapsed_{};

    void refreshLayout();
    void updateScheduledRunnerPosition(Runner &runner) const;
    void applyRunnerTransform(Runner &runner) const;
    void applyCloudTransform(Cloud &cloud) const;
    void applySunTransform();
};

} // namespace LiberateTheSheep::Game
