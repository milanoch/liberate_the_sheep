#include "Game/EscapeHerd.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using namespace LiberateTheSheep::Game;

void require(bool condition, const std::string &message) {
    if (!condition) { throw std::runtime_error(message); }
}

bool near(float actual, float expected) {
    return std::isfinite(actual) && std::isfinite(expected) &&
        std::abs(actual - expected) <= 0.0001f;
}

bool near(PenEscape::Point actual, PenEscape::Point expected) {
    return near(actual.x, expected.x) && near(actual.y, expected.y);
}

BoardModel makeBoard() {
    BoardModel board(4, 6);
    require(board.addSheep({1, {1, 3}, Direction::Up}), "Cannot place leading sheep");
    require(board.addSheep({2, {1, 0}, Direction::Up}), "Cannot place dependent sheep");
    require(board.addSheep({3, {3, 2}, Direction::Up}), "Cannot place independent sheep");
    require(board.validateStructure().valid(), "Invalid test fixture");
    return board;
}

const EscapingSheep &runner(const EscapeHerd &herd, SheepId id) {
    const auto found = std::find_if(herd.runners().begin(), herd.runners().end(),
        [id](const EscapingSheep &value) { return value.sheep.id == id; });
    require(found != herd.runners().end(), "Expected runner is missing: " + std::to_string(id));
    return *found;
}

void requireSamePath(const PenEscape::Path &actual, const PenEscape::Path &expected) {
    require(actual.segments.size() == expected.segments.size() &&
            near(actual.length, expected.length) && near(actual.start, expected.start) &&
            near(actual.end, expected.end), "Rejected operation changed a route");
    for (std::size_t i = 0; i < actual.segments.size(); ++i) {
        const auto &a = actual.segments[i];
        const auto &b = expected.segments[i];
        require(a.kind == b.kind && near(a.start, b.start) && near(a.end, b.end) &&
                near(a.center, b.center) && near(a.radius, b.radius) &&
                near(a.startAngleRadians, b.startAngleRadians) &&
                near(a.sweepRadians, b.sweepRadians) && near(a.length, b.length),
            "Rejected operation changed a route segment");
    }
}

void requireSameRunners(const EscapeHerd &actual, const std::vector<EscapingSheep> &expected) {
    require(actual.size() == expected.size(), "Rejected operation changed the runner count");
    for (std::size_t i = 0; i < expected.size(); ++i) {
        const auto &a = actual.runners()[i];
        const auto &b = expected[i];
        require(a.sheep == b.sheep && near(a.distance, b.distance) &&
                near(a.speed, b.speed) && a.yielding == b.yielding,
            "Rejected operation changed a runner's identity, distance or traffic state");
        requireSamePath(a.path, b.path);
    }
}

void requireRejectedLaunch(EscapeHerd &herd, BoardModel &board, SheepId id, float runout) {
    const auto fingerprint = board.layoutFingerprint();
    const auto previous = herd.runners();
    require(!herd.launch(board, id, runout), "Invalid launch succeeded");
    require(board.layoutFingerprint() == fingerprint && board.validateStructure().valid(),
        "Rejected launch changed logical occupancy");
    requireSameRunners(herd, previous);
}

void testConcurrentLaunchesAndCompletion() {
    BoardModel board = makeBoard();
    EscapeHerd herd;
    require(herd.empty() && herd.takeFinished().empty(), "New herd is not empty");
    require(board.inspectMove(2).state == MoveState::Blocked, "Dependent sheep is not blocked");
    requireRejectedLaunch(herd, board, 2, PenEscape::kOutsideRunout);
    requireRejectedLaunch(herd, board, 0, PenEscape::kOutsideRunout);
    requireRejectedLaunch(herd, board, 999, PenEscape::kOutsideRunout);

    require(herd.launch(board, 1, PenEscape::kOutsideRunout), "First launch failed");
    require(!board.isOccupied({1, 3}) && !board.isOccupied({1, 4}) &&
            board.findSheep(1) == nullptr && board.sheepCount() == 2,
        "Launch did not release both cells immediately");
    require(board.inspectMove(2).canEscape(), "Launch did not unlock the dependent sheep");
    require(runner(herd, 1).sheep == Sheep{1, {1, 3}, Direction::Up},
        "Runner lost its placement when the board entry was erased");
    herd.advance(0.05f);
    const float initialLead = runner(herd, 1).distance;
    require(initialLead > 0.f && initialLead < 0.05f * kEscapeRunSpeed &&
            runner(herd, 1).speed > 0.f && runner(herd, 1).speed <= 0.05f * kEscapeAcceleration + 0.0001f,
        "First sheep did not accelerate smoothly from rest");
    requireRejectedLaunch(herd, board, 1, PenEscape::kOutsideRunout);

    require(herd.launch(board, 2, PenEscape::kOutsideRunout),
        "Dependent sheep cannot launch while the leading sheep runs");
    require(herd.launch(board, 3, PenEscape::kOutsideRunout),
        "Third sheep cannot launch alongside the first two");
    require(herd.size() == 3 && board.empty() && board.occupiedCellCount() == 0 &&
            board.validateStructure().valid(), "Concurrent launches left incorrect occupancy");
    require(!herd.empty() && herd.takeFinished().empty(),
        "An empty board incorrectly completed still-running sheep");
    require(near(runner(herd, 1).distance, initialLead) &&
            near(runner(herd, 2).distance, 0.f) && near(runner(herd, 3).distance, 0.f),
        "A later launch reset an existing runner or inherited its distance");

    herd.advance(0.075f);
    require(runner(herd, 1).distance > initialLead &&
            runner(herd, 2).distance > 0.f && runner(herd, 3).distance > 0.f,
        "Independent concurrent runners did not start moving together");
    require(runner(herd, 1).distance <= initialLead + 0.075f * kEscapeRunSpeed &&
            runner(herd, 2).distance <= 0.075f * kEscapeRunSpeed &&
            runner(herd, 3).distance <= 0.075f * kEscapeRunSpeed,
        "Concurrent runners exceeded the running speed");

    // Also exercise the explicit duplicate-runner guard, not merely the
    // ordinary missing-board-entry guard after the first successful launch.
    require(board.addSheep({1, {0, 0}, Direction::Up}), "Cannot prepare duplicate-ID fixture");
    requireRejectedLaunch(herd, board, 1, PenEscape::kOutsideRunout);
    require(board.eraseSheep(1), "Cannot restore the empty fixture");

    std::unordered_set<SheepId> completed;
    bool sawPartialCompletion = false;
    for (int frame = 0; frame < 1000 && !herd.empty(); ++frame) {
        herd.advance(0.1f);
        const auto previous = herd.runners();
        const auto finished = herd.takeFinished();
        for (const SheepId id : finished) {
            const auto found = std::find_if(previous.begin(), previous.end(),
                [id](const EscapingSheep &value) { return value.sheep.id == id; });
            require(found != previous.end() && found->finished() &&
                    PenEscape::sample(found->path, found->distance).finished,
                "Runner completed before reaching the route endpoint");
            require(completed.insert(id).second, "A runner completed more than once");
        }
        sawPartialCompletion = sawPartialCompletion || (!finished.empty() && !herd.empty());
        require(herd.takeFinished().empty(), "Polling returned the same completion twice");
        require(herd.size() + completed.size() == 3, "A running sheep disappeared or duplicated");
    }
    require(herd.empty() && completed == std::unordered_set<SheepId>{1, 2, 3},
        "Not all concurrent sheep reached the gate endpoint");
    require(sawPartialCompletion, "Different routes completed as one global animation");
    herd.advance(0.1f);
    require(herd.takeFinished().empty(), "Finished runners returned on a later frame");
}

void testResizeAndReset() {
    BoardModel board = makeBoard();
    const BoardModel original = board;
    EscapeHerd herd;
    for (const SheepId id : {1u, 2u, 3u}) {
        require(herd.launch(board, id, PenEscape::kOutsideRunout), "Resize fixture launch failed");
        herd.advance(0.05f);
    }
    const auto before = herd.runners();
    require(herd.rebuildPaths(board.width(), board.height(), 12.f), "Viewport route extension failed");
    require(herd.size() == before.size(), "Resize removed running sheep");
    for (std::size_t i = 0; i < before.size(); ++i) {
        const auto &oldRunner = before[i];
        const auto &newRunner = herd.runners()[i];
        require(newRunner.sheep == oldRunner.sheep && near(newRunner.distance, oldRunner.distance),
            "Resize reset a runner's distance or starting placement");
        require(near(newRunner.path.length, oldRunner.path.length + 8.f),
            "Viewport resize did not change only the external runout length");
        // Camera resize must never change the shared prefix through the
        // pasture/gate, nor teleport any already-running sheep on that prefix.
        for (int sampleIndex = 0; sampleIndex <= 100; ++sampleIndex) {
            const float distance = oldRunner.path.length * static_cast<float>(sampleIndex) / 100.f;
            const auto oldPose = PenEscape::sample(oldRunner.path, distance);
            const auto newPose = PenEscape::sample(newRunner.path, distance);
            require(near(oldPose.position, newPose.position) && near(oldPose.tangent, newPose.tangent),
                "Resize altered the existing route prefix");
        }
    }

    // The third sheep is outside width=3, but the first two are valid. Failure
    // must be transactional, even after two replacement paths were generated.
    const auto extended = herd.runners();
    require(!herd.rebuildPaths(3, 6, 15.f), "Resize accepted an out-of-bounds runner");
    requireSameRunners(herd, extended);
    require(!herd.rebuildPaths(0, 6, 15.f), "Resize accepted invalid board dimensions");
    requireSameRunners(herd, extended);
    for (const float invalid : {
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()}) {
        require(!herd.rebuildPaths(4, 6, invalid), "Resize accepted non-finite runout");
        requireSameRunners(herd, extended);
    }
    require(herd.rebuildPaths(4, 6, PenEscape::kOutsideRunout), "Viewport route contraction failed");
    requireSameRunners(herd, before);

    herd.clear();
    require(herd.empty() && herd.takeFinished().empty(), "Clear retained animations/completions");
    require(board.empty(), "Clearing visual runners recreated logical sheep");
    board = original;
    require(herd.launch(board, 1, PenEscape::kOutsideRunout), "Restart could not reuse a sheep ID");
    require(herd.size() == 1 && near(runner(herd, 1).distance, 0.f),
        "Restart inherited a previous level's animation state");
    herd.clear();
    herd.clear();
    require(herd.empty(), "Repeated clear is not harmless");
}

void testInvalidInputsAndTime() {
    BoardModel board = makeBoard();
    EscapeHerd herd;
    for (const float invalid : {
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()}) {
        requireRejectedLaunch(herd, board, 1, invalid);
    }
    // Finite runouts shorter than the minimum are supported by PenEscape:
    // they are clamped, not treated as a failed click or an invalid route.
    require(herd.launch(board, 1, -5.f), "Finite short runout did not use the safe minimum");
    requireSamePath(runner(herd, 1).path,
        PenEscape::makePath(4, 6, {1, {1, 3}, Direction::Up}, PenEscape::kOutsideRunout));
    require(herd.launch(board, 2, PenEscape::kOutsideRunout), "Time fixture second launch failed");
    herd.advance(0.04f);
    const auto before = herd.runners();
    for (const float deltaTime : {
             0.f, -0.1f,
             std::numeric_limits<float>::quiet_NaN(),
             std::numeric_limits<float>::infinity(),
             -std::numeric_limits<float>::infinity()}) {
        herd.advance(deltaTime);
        requireSameRunners(herd, before);
    }
    herd.advance(1000.f);
    for (std::size_t i = 0; i < before.size(); ++i) {
        require(herd.runners()[i].distance >= before[i].distance &&
                herd.runners()[i].distance <= before[i].distance + 0.1f * kEscapeRunSpeed + 0.0001f,
            "A stalled frame teleported a runner instead of using the bounded time step");
    }
}

} // namespace

int main() {
    try {
        testConcurrentLaunchesAndCompletion();
        testResizeAndReset();
        testInvalidInputsAndTime();
        std::cout << "Escape herd checks passed: concurrent launches, occupancy release, "
                     "completion, resize, restart and invalid inputs\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Escape herd check failed: " << error.what() << '\n';
        return 1;
    }
}
