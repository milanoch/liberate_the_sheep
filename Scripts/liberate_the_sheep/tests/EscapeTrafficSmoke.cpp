#include "Game/BoardGenerator.hpp"
#include "Game/EscapeHerd.hpp"
#include "Game/LevelCatalog.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using namespace LiberateTheSheep::Game;
using Point = PenEscape::Point;
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTolerance = 0.00015f;
constexpr float kHalfWidth = 0.48f;
constexpr float kHalfLength = 0.92f;

void require(bool condition, const std::string &message) {
    if (!condition) { throw std::runtime_error(message); }
}

float dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }

struct Body {
    SheepId id{};
    Point center;
    Point forward;
    Point side;
};

// Deliberately independent of EscapeTraffic's collision implementation. These
// oriented boxes enclose every frame's visible wool, ears and hooves. The
// Sprite has a fixed size; transparent padding is not physical body geometry.
Body movingBody(const EscapingSheep &runner) {
    const auto sample = PenEscape::sample(runner.path, runner.distance);
    return {runner.sheep.id, sample.position, sample.tangent,
            {sample.tangent.y, -sample.tangent.x}};
}

Body standingBody(const Sheep &sheep) {
    const Cell direction = directionDelta(sheep.direction);
    const Point forward{static_cast<float>(direction.x), static_cast<float>(direction.y)};
    const Point center{static_cast<float>(sheep.tail.x) + 0.5f + forward.x * 0.5f,
                       static_cast<float>(sheep.tail.y) + 0.5f + forward.y * 0.5f};
    return {sheep.id, center, forward, {forward.y, -forward.x}};
}

bool overlaps(const Body &a, const Body &b) {
    const Point delta{b.center.x - a.center.x, b.center.y - a.center.y};
    for (Point axis : std::array<Point, 4>{a.forward, a.side, b.forward, b.side}) {
        const float radiusA = kHalfWidth * std::abs(dot(a.side, axis)) +
                              kHalfLength * std::abs(dot(a.forward, axis));
        const float radiusB = kHalfWidth * std::abs(dot(b.side, axis)) +
                              kHalfLength * std::abs(dot(b.forward, axis));
        if (std::abs(dot(delta, axis)) >= radiusA + radiusB - kTolerance) { return false; }
    }
    return true;
}

std::string bodyLabel(const Body &body) {
    return std::to_string(body.id) + " at (" + std::to_string(body.center.x) + ", " +
           std::to_string(body.center.y) + ")";
}

bool overlapsRectangle(const Body &body, Point minimum, Point maximum) {
    const Point rectangleCenter{(minimum.x + maximum.x) * 0.5f,
                                (minimum.y + maximum.y) * 0.5f};
    const Point rectangleHalfSize{(maximum.x - minimum.x) * 0.5f,
                                  (maximum.y - minimum.y) * 0.5f};
    const Point delta{rectangleCenter.x - body.center.x, rectangleCenter.y - body.center.y};
    for (Point axis : std::array<Point, 4>{{{1.f, 0.f}, {0.f, 1.f}, body.forward, body.side}}) {
        const float bodyRadius = kHalfWidth * std::abs(dot(body.side, axis)) +
                                 kHalfLength * std::abs(dot(body.forward, axis));
        const float rectangleRadius = rectangleHalfSize.x * std::abs(axis.x) +
                                      rectangleHalfSize.y * std::abs(axis.y);
        if (std::abs(dot(delta, axis)) >= bodyRadius + rectangleRadius - kTolerance) {
            return false;
        }
    }
    return true;
}

void requireFenceClearance(const Body &body, int columns, int rows, const std::string &context) {
    constexpr float halfThickness = 2.4f / 0.94f / 7.f * 0.5f;
    constexpr float endOverhang = (2.4f / 0.94f - 2.4f) * 0.5f;
    constexpr float left = -PenEscape::kFenceOffset;
    constexpr float bottom = -PenEscape::kFenceOffset;
    const float right = static_cast<float>(columns) + PenEscape::kFenceOffset;
    const float top = static_cast<float>(rows) + PenEscape::kFenceOffset;
    const float gate = static_cast<float>(columns) * 0.5f;
    const float halfGate = PenEscape::kGateWidth * 0.5f - endOverhang;
    const std::array<std::array<Point, 2>, 5> panels{{
        {{{left - halfThickness, bottom}, {left + halfThickness, top}}},
        {{{right - halfThickness, bottom}, {right + halfThickness, top}}},
        {{{left, top - halfThickness}, {right, top + halfThickness}}},
        {{{left, bottom - halfThickness}, {gate - halfGate, bottom + halfThickness}}},
        {{{gate + halfGate, bottom - halfThickness}, {right, bottom + halfThickness}}},
    }};
    for (const auto &panel : panels) {
        require(!overlapsRectangle(body, panel[0], panel[1]),
            context + ": animated body clips the fence: " + bodyLabel(body));
    }
}

void requireSeparated(const EscapeHerd &herd, const BoardModel &board, const std::string &context) {
    std::vector<Body> bodies;
    bodies.reserve(herd.size() + board.sheepCount());
    for (const auto &runner : herd.runners()) {
        bodies.push_back(movingBody(runner));
        requireFenceClearance(bodies.back(), board.width(), board.height(), context);
    }
    const std::size_t movingCount = bodies.size();
    for (const Sheep &sheep : board.sheep()) { bodies.push_back(standingBody(sheep)); }
    for (std::size_t first = 0; first < movingCount; ++first) {
        for (std::size_t second = first + 1; second < bodies.size(); ++second) {
            if (overlaps(bodies[first], bodies[second])) {
                throw std::runtime_error(context + ": visible bodies overlap: " +
                    bodyLabel(bodies[first]) + " / " + bodyLabel(bodies[second]));
            }
        }
    }
}

struct Kinematics {
    SheepId id{};
    float distance{};
    float speed{};
    bool yielding{};
    Point position;
};

std::vector<Kinematics> snapshot(const EscapeHerd &herd) {
    std::vector<Kinematics> result;
    result.reserve(herd.size());
    for (const auto &runner : herd.runners()) {
        result.push_back({runner.sheep.id, runner.distance, runner.speed, runner.yielding,
            PenEscape::sample(runner.path, runner.distance).position});
    }
    return result;
}

struct Observations {
    bool concurrent{};
    bool yielding{};
    bool braking{};
    bool resumed{};
    std::unordered_set<SheepId> gaveWay;
    std::unordered_set<SheepId> completed;
    std::size_t samples{};
};

void checkedAdvance(EscapeHerd &herd, const BoardModel &board, float deltaTime,
                    Observations &observations, const std::string &context) {
    const auto before = snapshot(herd);
    herd.advance(deltaTime);
    require(before.size() == herd.size(), context + ": advance silently removed a runner");
    unsigned int moved = 0;
    const float boundedTime = std::clamp(deltaTime, 0.f, 0.1f);
    for (std::size_t i = 0; i < before.size(); ++i) {
        const auto &old = before[i];
        const auto &now = herd.runners()[i];
        const std::string label = context + ", sheep " + std::to_string(now.sheep.id);
        require(now.sheep.id == old.id, label + ": priority order changed during advance");
        require(std::isfinite(now.distance) && std::isfinite(now.speed) && now.speed >= 0.f &&
                now.speed <= kEscapeRunSpeed + kTolerance, label + ": invalid speed/distance");
        const float travelled = now.distance - old.distance;
        require(travelled >= -kTolerance && travelled <= boundedTime * kEscapeRunSpeed + kTolerance,
            label + ": went backwards or teleported");
        if (!now.finished()) {
            const float change = now.speed - old.speed;
            require(change <= boundedTime * kEscapeAcceleration + kTolerance,
                label + ": acceleration is discontinuous (" + std::to_string(change) + ")");
            require(change >= -boundedTime * kEscapeBraking - kTolerance,
                label + ": braking is discontinuous (" + std::to_string(change) + ")");
        }
        const auto sample = PenEscape::sample(now.path, now.distance);
        require(std::hypot(sample.position.x - old.position.x, sample.position.y - old.position.y) <=
                travelled + 0.0003f, label + ": route position jumped independently of distance");
        if (travelled > 0.00001f) { ++moved; }
        if (now.yielding) {
            observations.yielding = true;
            observations.gaveWay.insert(now.sheep.id);
        }
        if (now.speed + 0.00001f < old.speed && !now.finished()) { observations.braking = true; }
        if (observations.gaveWay.contains(now.sheep.id) && !now.yielding && now.speed > 1.f) {
            observations.resumed = true;
        }
        const auto gait = sampleEscapeGait(now);
        if (now.speed <= 0.02f) {
            require(gait.pose == 0 && std::abs(gait.bodyWave) <= kTolerance,
                label + ": waiting sheep is still running or bouncing in place");
        } else {
            const int expectedPose = 1 + static_cast<int>(
                std::floor(now.distance / kEscapeRunStride *
                    static_cast<float>(SheepRunAnimation::kRunPoseCount))) %
                SheepRunAnimation::kRunPoseCount;
            const float expectedWave = std::sin(now.distance / kEscapeRunStride * 2.f * kPi) *
                std::clamp(now.speed / kEscapeRunSpeed, 0.f, 1.f);
            require(gait.pose == expectedPose && std::abs(gait.bodyWave - expectedWave) < 0.0003f,
                label + ": animation is not tied to actual distance and speed");
        }
    }
    observations.concurrent = observations.concurrent || moved >= 2;
    requireSeparated(herd, board, context);
    for (SheepId id : herd.takeFinished()) {
        require(observations.completed.insert(id).second, context + ": runner completed more than once");
    }
    require(herd.takeFinished().empty(), context + ": completion reported twice");
    ++observations.samples;
}

BoardModel boardWith(std::initializer_list<Sheep> sheep) {
    BoardModel board(4, 6);
    for (const auto &value : sheep) {
        require(board.addSheep(value), "Cannot construct traffic fixture");
    }
    return board;
}

Observations simulateFixture(const std::string &name, BoardModel board,
                             const std::vector<SheepId> &order, float launchDelay = 0.f,
                             bool resize = false) {
    EscapeHerd herd;
    Observations observations;
    std::size_t next = 0;
    float elapsed = 0.f;
    bool extended = false;
    bool contracted = false;
    for (int tick = 0; tick < 120 * 100 &&
             (next < order.size() || !herd.empty()); ++tick) {
        while (next < order.size() && elapsed + 0.00001f >= launchDelay * static_cast<float>(next)) {
            require(herd.launch(board, order[next], PenEscape::kOutsideRunout), name + ": launch failed");
            ++next;
            requireSeparated(herd, board, name + " at launch");
        }
        if (resize && !extended && elapsed > 1.25f) {
            const auto before = snapshot(herd);
            require(herd.rebuildPaths(4, 6, 12.f), name + ": resize extension failed");
            for (std::size_t i = 0; i < before.size(); ++i) {
                require(std::abs(before[i].distance - herd.runners()[i].distance) < kTolerance &&
                        std::abs(before[i].speed - herd.runners()[i].speed) < kTolerance,
                    name + ": resize reset traffic kinematics");
            }
            extended = true;
        }
        if (resize && !contracted && elapsed > 2.5f) {
            require(herd.rebuildPaths(4, 6, PenEscape::kOutsideRunout), name + ": resize contraction failed");
            contracted = true;
        }
        checkedAdvance(herd, board, kEscapeTrafficStep, observations,
            name + " t=" + std::to_string(elapsed));
        elapsed += kEscapeTrafficStep;
    }
    require(board.empty() && herd.empty() && observations.completed.size() == order.size(),
        name + ": traffic deadlocked or lost sheep");
    return observations;
}

void testTargetedTraffic() {
    const auto queue = simulateFixture("same lane queue", boardWith({
        {1, {0, 3}, Direction::Up}, {2, {0, 1}, Direction::Up}}), {1, 2});
    require(queue.concurrent, "Following sheep waited for the entire previous route");
    require(queue.yielding && queue.resumed, "Same-lane follower never yielded and resumed");

    const auto crossing = simulateFixture("perpendicular crossing", boardWith({
        {1, {0, 3}, Direction::Right}, {2, {2, 1}, Direction::Up}}), {1, 2});
    require(crossing.concurrent, "Independent approaches were globally serialized");
    require(crossing.yielding && crossing.resumed, "Crossing did not yield and resume");

    const auto gate = simulateFixture("gate merge", boardWith({
        {1, {0, 3}, Direction::Down}, {2, {3, 3}, Direction::Down}}), {1, 2}, 0.f, true);
    require(gate.concurrent && gate.yielding && gate.braking && gate.resumed,
        "Gate traffic did not move concurrently, brake, yield and resume");

    // The newer runner reaches the older runner's future path first. A policy
    // that only looks at current overlap, or flips priority by distance to the
    // junction every frame, can create a head-on encounter/deadlock here.
    simulateFixture("older route reservation", boardWith({
        {1, {0, 1}, Direction::Up}, {2, {2, 0}, Direction::Left}}), {1, 2});
    simulateFixture("late crossing launch", boardWith({
        {1, {0, 3}, Direction::Right}, {2, {2, 1}, Direction::Up}}), {1, 2}, 0.37f);
    simulateFixture("late queue launch", boardWith({
        {1, {0, 3}, Direction::Up}, {2, {0, 1}, Direction::Up},
        {3, {3, 3}, Direction::Down}}), {1, 2, 3}, 0.27f, true);
}

void testDistanceDrivenGait() {
    EscapingSheep runner{{1, {0, 0}, Direction::Up},
        PenEscape::makePath(4, 6, {1, {0, 0}, Direction::Up}), 0.19f, kEscapeRunSpeed, false};
    const auto fast = sampleEscapeGait(runner);
    runner.speed = 0.5f * kEscapeRunSpeed;
    const auto slow = sampleEscapeGait(runner);
    require(fast.pose == slow.pose && std::abs(slow.bodyWave - fast.bodyWave * 0.5f) < kTolerance,
        "Changing speed reset animation phase instead of scaling body movement");
    runner.speed = 0.f;
    const auto stopped = sampleEscapeGait(runner);
    require(stopped.pose == 0 && stopped.bodyWave == 0.f, "Stationary sheep does not stand still");
    runner.speed = kEscapeRunSpeed;
    runner.distance += kEscapeRunStride;
    const auto cycled = sampleEscapeGait(runner);
    require(cycled.pose == fast.pose && std::abs(cycled.bodyWave - fast.bodyWave) < kTolerance,
        "Running gait does not repeat once per travelled stride");
    runner.distance += kEscapeRunStride / static_cast<float>(SheepRunAnimation::kRunPoseCount);
    require(sampleEscapeGait(runner).pose == fast.pose % SheepRunAnimation::kRunPoseCount + 1,
        "Running frames do not advance in authored order");
}

void testAnimationAtlasLayout() {
    namespace Animation = SheepRunAnimation;
    std::unordered_set<int> encodedCells;
    for (int coat = 0; coat < Animation::kCoats; ++coat) {
        for (int pose = 0; pose <= Animation::kRunPoseCount; ++pose) {
            const int logical = Animation::logicalCell(coat, pose);
            const int encoded = Animation::encodedCell(coat, pose);
            require(logical == coat * Animation::kCellsPerCoat + pose,
                "Incorrect per-coat animation sequence");
            require(encoded >= 0 && encoded < Animation::kColumns * Animation::kRows,
                "Encoded animation cell lies outside the atlas");
            const int column = encoded % Animation::kColumns;
            const int row = encoded % Animation::kRows;
            require(row / (Animation::kRows / Animation::kCoats) == coat &&
                    row * Animation::kColumns + column == logical,
                "Panda atlas indexing changes coat or displays the wrong pose");
            require(encodedCells.insert(encoded).second,
                "Different coat/pose pairs address the same atlas cell");
            require(logical % Animation::kCellsPerCoat <= Animation::kRunPoseCount,
                "Animation displays one of the transparent padding cells");
        }
    }
    require(encodedCells.size() == Animation::kCoats * (Animation::kRunPoseCount + 1),
        "Not all visible atlas poses were checked");
    // Check the complete physical atlas as Panda addresses it, including the
    // three transparent cells at the end of each coat's four-row block.
    int paddingCells = 0;
    for (int encoded = 0; encoded < Animation::kColumns * Animation::kRows; ++encoded) {
        const int physical = (encoded % Animation::kRows) * Animation::kColumns +
                             encoded % Animation::kColumns;
        const bool padding = physical % Animation::kCellsPerCoat > Animation::kRunPoseCount;
        require(encodedCells.contains(encoded) != padding,
            "Atlas coverage omits a visible frame or selects a padding cell");
        if (padding) { ++paddingCells; }
    }
    require(paddingCells == Animation::kCoats * Animation::kPaddingCellsPerCoat,
        "Atlas does not reserve the expected number of transparent cells");
    require(Animation::logicalCell(-1, -1) == 0 &&
            Animation::logicalCell(99, 99) ==
                (Animation::kCoats - 1) * Animation::kCellsPerCoat + Animation::kRunPoseCount,
        "Invalid animation arguments do not clamp to visible cells");
    for (int coat = 0; coat < Animation::kCoats; ++coat) {
        require(Animation::encodedCell(coat, -1) == Animation::encodedCell(coat, 0) &&
                Animation::encodedCell(coat, Animation::kRunPoseCount + 1) ==
                    Animation::encodedCell(coat, Animation::kRunPoseCount),
            "Out-of-range poses select another coat or a padding cell");
    }

    EscapingSheep runner{{1, {0, 0}, Direction::Up},
        PenEscape::makePath(4, 6, {1, {0, 0}, Direction::Up}), 0.f, kEscapeRunSpeed, false};
    for (int cycle = 0; cycle < 4; ++cycle) {
        for (int frame = 0; frame < Animation::kRunPoseCount; ++frame) {
            // Sample in the middle of a cel, away from floating-point edges.
            runner.distance = (static_cast<float>(cycle) +
                (static_cast<float>(frame) + 0.5f) /
                static_cast<float>(Animation::kRunPoseCount)) * kEscapeRunStride;
            require(sampleEscapeGait(runner).pose == frame + 1,
                "Running gait skips, reverses or fails to wrap in authored order");
        }
    }
}

void testGaitSamplingFrameRates() {
    const std::array<std::vector<float>, 4> schedules{{
        {1.f / 30.f}, {1.f / 60.f}, {1.f / 120.f},
        {0.0083f, 0.014f, 0.031f, 0.004f, 0.022f, 0.041f},
    }};
    for (std::size_t schedule = 0; schedule < schedules.size(); ++schedule) {
        EscapingSheep runner{{1, {0, 0}, Direction::Up},
            PenEscape::makePath(4, 6, {1, {0, 0}, Direction::Up}), 0.013f, kEscapeRunSpeed, false};
        std::unordered_set<int> fastPoses;
        double elapsed = 0.;
        float stoppedDistance = -1.f;
        int lastMovingBin = -1;
        int lastMovingPose = -1;
        std::size_t fastTicks = 0;
        std::size_t slowTicks = 0;
        std::size_t stoppedTicks = 0;
        for (std::size_t tick = 0; elapsed < 12.; ++tick) {
            const float deltaTime = schedules[schedule][tick % schedules[schedule].size()];
            runner.speed = elapsed < 4. || elapsed >= 8. ? kEscapeRunSpeed :
                           elapsed < 6. ? kEscapeRunSpeed * 0.35f : 0.f;
            const float before = runner.distance;
            runner.distance += runner.speed * deltaTime;
            const auto gait = sampleEscapeGait(runner);
            if (runner.speed == 0.f) {
                if (stoppedDistance < 0.f) { stoppedDistance = before; }
                require(runner.distance == stoppedDistance && gait.pose == 0 && gait.bodyWave == 0.f,
                    "Waiting gait advances at fixed animation FPS instead of standing still");
                ++stoppedTicks;
            } else {
                const int bin = static_cast<int>(std::floor(runner.distance / kEscapeRunStride *
                    static_cast<float>(SheepRunAnimation::kRunPoseCount)));
                require(gait.pose == bin % SheepRunAnimation::kRunPoseCount + 1,
                    "Frame-rate-dependent animation phase");
                if (lastMovingBin >= 0) {
                    const int advance = bin - lastMovingBin;
                    const int maxAdvance = static_cast<int>(std::ceil(
                        runner.speed * deltaTime / kEscapeRunStride *
                        static_cast<float>(SheepRunAnimation::kRunPoseCount))) + 1;
                    require(advance >= 0 && advance <= maxAdvance &&
                            gait.pose == (lastMovingPose - 1 + advance) %
                                SheepRunAnimation::kRunPoseCount + 1,
                        "Animation runs backwards, skips out of order, or resets after yielding");
                }
                const float activity = runner.speed / kEscapeRunSpeed;
                require(std::abs(gait.bodyWave) <= activity + kTolerance,
                    "Gait movement fails to diminish at slower speed");
                lastMovingBin = bin;
                lastMovingPose = gait.pose;
                if (runner.speed == kEscapeRunSpeed) {
                    fastPoses.insert(gait.pose);
                    ++fastTicks;
                } else {
                    ++slowTicks;
                }
            }
            elapsed += deltaTime;
        }
        require(fastTicks > 0 && slowTicks > 0 && stoppedTicks > 0,
            "Frame-rate gait fixture did not exercise run, slowdown and stop");
        require(fastPoses.size() == SheepRunAnimation::kRunPoseCount,
            "Frame-rate gait loop never displays one of the authored running poses");
    }
    std::cout << "Animation: all "
              << SheepRunAnimation::kCoats * (SheepRunAnimation::kRunPoseCount + 1)
              << " visible atlas cells and " << SheepRunAnimation::kRunPoseCount
              << "-pose loops at 30/60/120/irregular FPS, slowdown, stop and resume passed\n";
}

void testCatalogTraffic() {
    std::size_t totalSamples = 0;
    std::size_t totalCompleted = 0;
    for (const auto &level : levelCatalog()) {
        const auto generated = BoardGenerator::generate(level.generation);
        require(generated.succeeded(), "Cannot generate traffic catalog fixture");
        // 120 FPS observes every simulation tick; 30 FPS exercises subdivided
        // advance calls. 60 FPS adds deterministic staggered clicks, including
        // clicks after earlier runners/completions have changed reservations.
        for (const int fps : {120, 30, 60}) {
            BoardModel board = *generated.board;
            EscapeHerd herd;
            Observations observations;
            std::size_t launches = 0;
            std::uint32_t random = static_cast<std::uint32_t>(level.number) * 7331u;
            const bool delayed = fps == 60;
            const float step = 1.f / static_cast<float>(fps);
            float elapsed = 0.f;
            float nextLaunchTime = 0.f;
            const std::string label = "level " + std::to_string(level.number) +
                ", " + std::to_string(fps) + " FPS";
            const int maxFrames = (40 + static_cast<int>(generated.guaranteedSolution.size()) * 20) * fps;
            for (int frame = 0; frame < maxFrames && (!board.empty() || !herd.empty()); ++frame) {
                while (!board.empty() && elapsed + 0.00001f >= nextLaunchTime) {
                    SheepId id{};
                    if (delayed) {
                        const auto legal = board.escapableSheep();
                        require(!legal.empty(), label + ": generated puzzle is not solvable");
                        random = random * 1664525u + 1013904223u;
                        id = legal[random % legal.size()];
                    } else {
                        id = generated.guaranteedSolution[launches];
                    }
                    require(herd.launch(board, id, PenEscape::kOutsideRunout), label + ": legal click rejected");
                    ++launches;
                    requireSeparated(herd, board, label + " at launch");
                    if (delayed) {
                        random = random * 1664525u + 1013904223u;
                        nextLaunchTime = elapsed + 0.025f + static_cast<float>(random % 83u) * 0.007f;
                    }
                }
                checkedAdvance(herd, board, step, observations, label + " t=" + std::to_string(elapsed));
                elapsed += step;
            }
            require(board.empty() && herd.empty() && observations.completed.size() == launches &&
                    launches == generated.guaranteedSolution.size(), label + ": deadlock or missing completion");
            require(observations.concurrent, label + ": no two sheep ever moved simultaneously");
            totalSamples += observations.samples;
            totalCompleted += observations.completed.size();
        }
    }
    std::cout << "Catalog traffic: " << totalCompleted << " completed sheep, " << totalSamples
              << " sampled frames across all 9 levels at 30/60/120 FPS\n";
}

} // namespace

int main() {
    try {
        testAnimationAtlasLayout();
        testDistanceDrivenGait();
        testGaitSamplingFrameRates();
        testTargetedTraffic();
        testCatalogTraffic();
        std::cout << "Escape traffic checks passed: run silhouette/fence separation, smooth yielding/resume, "
                     "independent motion, distance-driven gait, late launches and resize\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Escape traffic check failed: " << error.what() << '\n';
        return 1;
    }
}
