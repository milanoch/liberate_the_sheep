#include "Game/EscapeHerd.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace LiberateTheSheep::Game {

EscapeGait sampleEscapeGait(const EscapingSheep &runner) noexcept {
    constexpr float kTau = 6.28318530717958647692f;
    const float activity = runner.speed <= 0.02f ? 0.f :
        std::clamp(runner.speed / kEscapeRunSpeed, 0.f, 1.f);
    const float cycle = runner.distance / kEscapeRunStride;
    return {
        runner.speed <= 0.02f ? 0 : 1 + static_cast<int>(
            std::floor(cycle * static_cast<float>(SheepRunAnimation::kRunPoseCount))) %
            SheepRunAnimation::kRunPoseCount,
        std::sin(cycle * kTau) * activity,
    };
}

bool EscapeHerd::launch(BoardModel &board, SheepId id, float outsideRunout) {
    if (!runners_.empty() && (columns_ != board.width() || rows_ != board.height())) {
        return false;
    }
    const Sheep *sheep = board.findSheep(id);
    if (sheep == nullptr || !board.inspectMove(id).canEscape() ||
        std::any_of(runners_.begin(), runners_.end(), [id](const auto &runner) {
            return runner.sheep.id == id;
        })) {
        return false;
    }
    EscapingSheep runner{*sheep, PenEscape::makePath(board.width(), board.height(), *sheep, outsideRunout), 0.f};
    if (!runner.path.valid()) { return false; }
    TrafficState state;
    state.footprint = EscapeTraffic::buildRoute(runner.path, board.width(), board.height());
    state.earlier.reserve(traffic_.size());
    for (const auto &leader : traffic_) {
        state.earlier.push_back({EscapeTraffic::reserveAgainst(state.footprint, leader.footprint), 0});
    }
    // Copy the source placement before erase invalidates BoardModel pointers.
    if (!board.eraseSheep(id)) { return false; }
    columns_ = board.width();
    rows_ = board.height();
    runners_.push_back(std::move(runner));
    traffic_.push_back(std::move(state));
    return true;
}

void EscapeHerd::advance(float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime <= 0.f) { return; }
    float remaining = std::min(deltaTime, 0.1f);
    while (remaining > 0.f) {
        const float timeStep = std::min(remaining, kEscapeTrafficStep);
        step(timeStep);
        remaining -= timeStep;
    }
}

void EscapeHerd::step(float deltaTime) {
    // Youngest first: all queried leaders are still at their pre-step positions.
    // A follower never enters space that its leader clears only later this step.
    for (std::size_t index = runners_.size(); index > 0; --index) {
        const std::size_t i = index - 1;
        auto &runner = runners_[i];
        if (runner.finished()) { continue; }
        float stop = std::numeric_limits<float>::infinity();
        for (std::size_t leader = 0; leader < i; ++leader) {
            auto &reservation = traffic_[i].earlier[leader];
            // Leader distance is monotone: already released intervals can be
            // skipped forever. Large flocks never rescan a whole route per tick.
            while (reservation.firstPending < reservation.conflicts.size() &&
                runners_[leader].distance >=
                    reservation.conflicts[reservation.firstPending].leaderClearDistance) {
                ++reservation.firstPending;
            }
            if (reservation.firstPending < reservation.conflicts.size()) {
                stop = std::min(stop,
                    reservation.conflicts[reservation.firstPending].followerDistance);
            }
        }

        float safeSpeed = kEscapeRunSpeed;
        if (std::isfinite(stop)) {
            // Keep a small additional buffer before the already padded body
            // reservation. Include this step's trapezoidal travel in the braking
            // budget, so deceleration stays bounded without emergency snapping.
            const float room = std::max(0.f, stop - runner.distance - 0.01f);
            const float halfBrakeStep = 0.5f * kEscapeBraking * deltaTime;
            const float discriminant = halfBrakeStep * halfBrakeStep +
                2.f * kEscapeBraking * room - kEscapeBraking * deltaTime * runner.speed;
            safeSpeed = std::clamp(
                std::sqrt(std::max(0.f, discriminant)) - halfBrakeStep, 0.f, kEscapeRunSpeed
            );
        }
        runner.yielding = safeSpeed < kEscapeRunSpeed - 0.01f;
        float nextSpeed = std::clamp(safeSpeed,
            std::max(0.f, runner.speed - kEscapeBraking * deltaTime),
            std::min(kEscapeRunSpeed, runner.speed + kEscapeAcceleration * deltaTime));
        float travel = (runner.speed + nextSpeed) * 0.5f * deltaTime;
        if (std::isfinite(stop) && runner.distance + travel > stop) {
            // Numerical safety net. Normal motion satisfies the braking budget
            // above; never move backwards or cross a reserved body footprint.
            travel = std::max(0.f, stop - runner.distance);
            nextSpeed = 0.f;
        }
        runner.distance = std::min(runner.path.length, runner.distance + travel);
        runner.speed = nextSpeed;
    }
}

std::vector<SheepId> EscapeHerd::takeFinished() {
    std::vector<SheepId> completed;
    for (std::size_t index = runners_.size(); index > 0; --index) {
        const std::size_t i = index - 1;
        if (!runners_[i].finished()) { continue; }
        completed.push_back(runners_[i].sheep.id);
        runners_.erase(runners_.begin() + static_cast<std::ptrdiff_t>(i));
        traffic_.erase(traffic_.begin() + static_cast<std::ptrdiff_t>(i));
        for (std::size_t follower = i; follower < traffic_.size(); ++follower) {
            auto &reservations = traffic_[follower].earlier;
            reservations.erase(reservations.begin() + static_cast<std::ptrdiff_t>(i));
        }
    }
    return completed;
}

bool EscapeHerd::rebuildPaths(int columns, int rows, float outsideRunout) {
    if (columns == columns_ && rows == rows_ && std::isfinite(outsideRunout)) {
        const float endY = -PenEscape::kFenceOffset -
            std::max(PenEscape::kOutsideRunout, outsideRunout);
        if (std::all_of(runners_.begin(), runners_.end(), [endY](const auto &runner) {
                return runner.path.end.y == endY;
            })) {
            // A world-space rescale does not change cell-space traffic. Most
            // viewport changes retain the same minimum four-cell runout.
            return true;
        }
    }
    std::vector<PenEscape::Path> updated;
    std::vector<TrafficState> updatedTraffic;
    updated.reserve(runners_.size());
    updatedTraffic.reserve(runners_.size());
    for (const auto &runner : runners_) {
        auto path = PenEscape::makePath(columns, rows, runner.sheep, outsideRunout);
        if (!path.valid()) { return false; }
        TrafficState state;
        state.footprint = EscapeTraffic::buildRoute(path, columns, rows);
        state.earlier.reserve(updatedTraffic.size());
        for (const auto &leader : updatedTraffic) {
            state.earlier.push_back({EscapeTraffic::reserveAgainst(state.footprint, leader.footprint), 0});
        }
        updatedTraffic.push_back(std::move(state));
        updated.push_back(std::move(path));
    }
    for (std::size_t i = 0; i < runners_.size(); ++i) {
        runners_[i].path = std::move(updated[i]);
    }
    traffic_ = std::move(updatedTraffic);
    columns_ = columns;
    rows_ = rows;
    return true;
}

void EscapeHerd::clear() noexcept {
    runners_.clear();
    traffic_.clear();
    columns_ = 0;
    rows_ = 0;
}

} // namespace LiberateTheSheep::Game
