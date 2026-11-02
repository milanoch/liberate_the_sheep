#include "Game/ShepherdDogRescue.hpp"
#include "Game/ShepherdDogAnimation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iterator>
#include <limits>
#include <queue>
#include <utility>

namespace LiberateTheSheep::Game::DogRescue {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kGrid = .35f;
constexpr float kRunSpeed = 8.25f;
constexpr float kAcceleration = 21.f;
constexpr float kBraking = 25.f;
// A longer, slower canter matches the larger dog without changing travel speed.
constexpr float kStride = 4.2f;
constexpr float kMouthOffset = ShepherdDogAnimation::kMouthOffset;
constexpr float kGrabDuration = .72f;
constexpr float kGrabContact = .43f;
constexpr float kTurnDuration = .34f;
constexpr float kDogRadius = .27f * (ShepherdDogAnimation::kQuadSize / 2.f);
constexpr float kMaxVault = 3.15f;
constexpr int kTuckedRunFrame = ShepherdDogAnimation::kAirborneFrame;
constexpr float kEpsilon = .0001f;

Point add(Point a, Point b) { return {a.x + b.x, a.y + b.y}; }
Point sub(Point a, Point b) { return {a.x - b.x, a.y - b.y}; }
Point mul(Point a, float s) { return {a.x * s, a.y * s}; }
float dot(Point a, Point b) { return a.x * b.x + a.y * b.y; }
float length(Point a) { return std::hypot(a.x, a.y); }
float distance(Point a, Point b) { return length(sub(a, b)); }
Point unit(Point p) { const float n = length(p); return n > kEpsilon ? mul(p, 1.f / n) : Point{0.f, 1.f}; }
Point lerp(Point a, Point b, float t) { return add(a, mul(sub(b, a), t)); }
float smooth(float t) { t = std::clamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
float heading(Point direction) { return std::atan2(-direction.x, direction.y) * 180.f / kPi; }
Point forward(float degrees) { const float a = degrees * kPi / 180.f; return {-std::sin(a), std::cos(a)}; }
float angleDelta(float a, float b) { return std::remainder(b - a, 360.f); }
float rotateToward(float a, float b, float limit) {
    return a + std::clamp(angleDelta(a, b), -limit, limit);
}

struct Obstacle { Point a; Point b; float radius; bool vaultable{true}; };
struct RoutePoint { Point point; bool leap{}; };

float segmentDistanceSquared(Point p, Point a, Point b) {
    const auto ab = sub(b, a);
    const float denominator = dot(ab, ab);
    const float t = denominator > kEpsilon ? std::clamp(dot(sub(p, a), ab) / denominator, 0.f, 1.f) : 0.f;
    const auto d = sub(p, add(a, mul(ab, t)));
    return dot(d, d);
}

std::vector<Obstacle> obstaclesFor(const BoardModel &board, SheepId target,
                                 std::span<const OtherSheepPose> otherSheep) {
    std::vector<Obstacle> obstacles;
    obstacles.reserve(board.sheepCount() + otherSheep.size());
    for (const auto &sheep : board.sheep()) {
        // The dog deliberately reaches over this sheep's shoulder to lift its
        // accessory. All other sheep remain actual navigation obstacles.
        if (sheep.id == target) { continue; }
        const auto h = sheep.head();
        Point tail{static_cast<float>(sheep.tail.x) + .5f, static_cast<float>(sheep.tail.y) + .5f};
        Point head{static_cast<float>(h.x) + .5f, static_cast<float>(h.y) + .5f};
        const auto inward = mul(unit(sub(head, tail)), .08f);
        obstacles.push_back({add(tail, inward), sub(head, inward), .37f});
    }
    for (const auto &sheep : otherSheep) {
        if (sheep.id == target || board.findSheep(sheep.id) ||
            !std::isfinite(sheep.position.x) || !std::isfinite(sheep.position.y) ||
            !std::isfinite(sheep.rotationDegrees)) { continue; }
        const auto halfAxis = mul(forward(sheep.rotationDegrees), .42f);
        obstacles.push_back({sub(sheep.position, halfAxis), add(sheep.position, halfAxis), .37f});
    }
    // The search includes the offscreen approach as well as the pen interior.
    // Fence rails are solid: unlike sheep they cannot be used as jump edges,
    // so the dog always enters and returns through the real central opening.
    const float fenceY = -PenEscape::kFenceOffset;
    const float gateX = static_cast<float>(board.width()) * .5f;
    const float leftFence = -PenEscape::kFenceOffset;
    const float rightFence = static_cast<float>(board.width()) + PenEscape::kFenceOffset;
    const float topFence = static_cast<float>(board.height()) + PenEscape::kFenceOffset;
    obstacles.push_back({{-PenEscape::kFenceOffset, fenceY},
                         {gateX - PenEscape::kGateWidth * .5f, fenceY}, .11f, false});
    obstacles.push_back({{gateX + PenEscape::kGateWidth * .5f, fenceY},
                         {static_cast<float>(board.width()) + PenEscape::kFenceOffset, fenceY}, .11f, false});
    obstacles.push_back({{leftFence, fenceY}, {leftFence, topFence}, .11f, false});
    obstacles.push_back({{rightFence, fenceY}, {rightFence, topFence}, .11f, false});
    obstacles.push_back({{leftFence, topFence}, {rightFence, topFence}, .11f, false});
    return obstacles;
}

bool freePoint(Point p, const std::vector<Obstacle> &obstacles, float dogRadius) {
    return std::all_of(obstacles.begin(), obstacles.end(), [&](const auto &obstacle) {
        const float radius = obstacle.radius + dogRadius;
        return segmentDistanceSquared(p, obstacle.a, obstacle.b) >= radius * radius;
    });
}

bool clearLine(Point a, Point b, const std::vector<Obstacle> &obstacles, float dogRadius,
               bool solidOnly = false) {
    const int samples = std::max(1, static_cast<int>(std::ceil(distance(a, b) / .12f)));
    for (int step = 0; step <= samples; ++step) {
        const auto p = lerp(a, b, static_cast<float>(step) / samples);
        for (const auto &obstacle : obstacles) {
            if (solidOnly && obstacle.vaultable) { continue; }
            const float radius = obstacle.radius + dogRadius;
            if (segmentDistanceSquared(p, obstacle.a, obstacle.b) < radius * radius) { return false; }
        }
    }
    return true;
}

struct GridSearch {
    Point origin{};
    int width{};
    int height{};
    std::vector<float> costs;
    std::vector<int> previous;
    std::vector<bool> leapt;
    std::vector<bool> free;

    Point point(int index) const {
        return {origin.x + static_cast<float>(index % width) * kGrid,
                origin.y + static_cast<float>(index / width) * kGrid};
    }
};

GridSearch searchFromStart(const BoardModel &board, const std::vector<Obstacle> &obstacles,
                          Point start, Point target, float dogRadius, bool allowVaults) {
    GridSearch grid;
    // A target may already be running outside the logical board. Include its
    // actual position; it need not still exist in BoardModel.
    grid.origin = {std::min(-1.75f, target.x - 1.1f),
                   std::min({-1.4f, target.y - 1.1f, start.y - .3f})};
    const Point far{std::max(static_cast<float>(board.width()) + 1.75f, target.x + 1.1f),
                    std::max(static_cast<float>(board.height()) + 1.75f, target.y + 1.1f)};
    grid.width = 1 + static_cast<int>(std::ceil((far.x - grid.origin.x) / kGrid));
    grid.height = 1 + static_cast<int>(std::ceil((far.y - grid.origin.y) / kGrid));
    const auto size = static_cast<std::size_t>(grid.width * grid.height);
    grid.costs.assign(size, std::numeric_limits<float>::infinity());
    grid.previous.assign(size, -1);
    grid.leapt.assign(size, false);
    grid.free.resize(size);
    using Entry = std::pair<float, int>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> queue;
    for (std::size_t index = 0; index < size; ++index) {
        const auto p = grid.point(static_cast<int>(index));
        grid.free[index] = freePoint(p, obstacles, dogRadius);
        const float d = distance(p, start);
        if (grid.free[index] && d < kGrid * 1.6f && clearLine(start, p, obstacles, dogRadius)) {
            grid.costs[index] = d;
            queue.emplace(d, static_cast<int>(index));
        }
    }
    constexpr std::array<std::pair<int, int>, 8> directions{{
        {1,0}, {-1,0}, {0,1}, {0,-1}, {1,1}, {1,-1}, {-1,1}, {-1,-1}
    }};
    while (!queue.empty()) {
        const auto [cost, index] = queue.top();
        queue.pop();
        if (cost > grid.costs[static_cast<std::size_t>(index)]) { continue; }
        const int x = index % grid.width;
        const int y = index / grid.width;
        const Point from = grid.point(index);
        for (const auto [dx, dy] : directions) {
            bool crossedBlocked = false;
            for (int reach = 1; reach <= (allowVaults ? 9 : 1); ++reach) {
                const int nx = x + dx * reach;
                const int ny = y + dy * reach;
                if (nx < 0 || ny < 0 || nx >= grid.width || ny >= grid.height) { break; }
                const int next = ny * grid.width + nx;
                const Point to = grid.point(next);
                const float d = distance(from, to);
                if (d > kMaxVault) { break; }
                if (!grid.free[static_cast<std::size_t>(next)]) {
                    crossedBlocked = true;
                    continue;
                }
                const bool leap = crossedBlocked || !clearLine(from, to, obstacles, dogRadius);
                if (leap && !allowVaults) { break; }
                if (leap && !clearLine(from, to, obstacles, dogRadius, true)) { break; }
                if (reach > 1 && !crossedBlocked) { break; }
                // A vault is considered only across an actual obstacle, never
                // as a shortcut over open grass. Its fixed penalty makes a
                // slightly longer walk preferable to unnecessary jumping.
                const float nextCost = cost + d + (leap ? 3.3f + d * .35f : 0.f);
                if (nextCost < grid.costs[static_cast<std::size_t>(next)]) {
                    grid.costs[static_cast<std::size_t>(next)] = nextCost;
                    grid.previous[static_cast<std::size_t>(next)] = index;
                    grid.leapt[static_cast<std::size_t>(next)] = leap;
                    queue.emplace(nextCost, next);
                }
                break; // The first clear landing is the only needed jump edge.
            }
        }
    }
    return grid;
}

struct RouteChoice {
    std::vector<RoutePoint> points;
    float heading{};
};

RouteChoice chooseRoute(const BoardModel &board, SheepId targetId, Point bomb,
                       Point start, const std::vector<Obstacle> &obstacles,
                       float dogRadius, bool allowVaults) {
    const auto grid = searchFromStart(board, obstacles, start, bomb, dogRadius, allowVaults);
    int bestNode = -1;
    Point bestGoal{};
    Point bestApproach{};
    float bestHeading{};
    float bestCost = std::numeric_limits<float>::infinity();
    bool lastLeap = false;
    for (int side = 0; side < 24; ++side) {
        const float a = static_cast<float>(side) * (2.f * kPi / 24.f);
        const Point facing{std::sin(a), std::cos(a)};
        const Point goal = sub(bomb, mul(facing, kMouthOffset));
        if (!freePoint(goal, obstacles, dogRadius)) { continue; }
        const Point approach = sub(goal, mul(facing, .38f));
        const bool clearApproach = clearLine(approach, goal, obstacles, dogRadius);
        for (std::size_t index = 0; index < grid.costs.size(); ++index) {
            if (!std::isfinite(grid.costs[index])) { continue; }
            const Point from = grid.point(static_cast<int>(index));
            const float d = distance(from, goal);
            if (d > kMaxVault) { continue; }
            const bool walk = clearApproach && clearLine(from, approach, obstacles, dogRadius);
            if (!walk && !allowVaults) { continue; }
            if (!walk && !clearLine(from, goal, obstacles, dogRadius, true)) { continue; }
            const float entryDistance = walk ? distance(from, approach) + .38f : d;
            float cost = grid.costs[index] + entryDistance + (walk ? 0.f : 3.3f + d * .35f);
            // Approach from the shoulder or head, rather than bulldozing the
            // sheep's rump. This is a tie-breaker, never a reachability rule.
            if (const auto *sheep = board.findSheep(targetId)) {
                const auto direction = directionDelta(sheep->direction);
                cost += .16f * (1.f + dot(facing, {static_cast<float>(direction.x), static_cast<float>(direction.y)}));
            }
            if (cost < bestCost) {
                bestCost = cost;
                bestNode = static_cast<int>(index);
                bestGoal = goal;
                bestApproach = approach;
                bestHeading = heading(facing);
                lastLeap = !walk;
            }
        }
    }
    if (bestNode < 0) { return {}; }
    std::vector<RoutePoint> reverse;
    for (int index = bestNode; index >= 0; index = grid.previous[static_cast<std::size_t>(index)]) {
        reverse.push_back({grid.point(index), grid.leapt[static_cast<std::size_t>(index)]});
    }
    std::reverse(reverse.begin(), reverse.end());
    std::vector<RoutePoint> route{{start, false}};
    route.insert(route.end(), reverse.begin(), reverse.end());
    if (!lastLeap) { route.push_back({bestApproach, false}); }
    route.push_back({bestGoal, lastLeap});

    // Remove the grid's staircase without cutting across flock bodies. Leap
    // edges retain their exact takeoff and landing points.
    std::vector<RoutePoint> simplified{route.front()};
    for (std::size_t index = 1; index < route.size();) {
        std::size_t next = index;
        if (!route[index].leap) {
            while (next + 1 < route.size() && !route[next + 1].leap &&
                   clearLine(simplified.back().point, route[next + 1].point, obstacles, dogRadius)) {
                ++next;
            }
        }
        if (distance(simplified.back().point, route[next].point) > .015f) {
            simplified.push_back(route[next]);
        }
        index = next + 1;
    }

    // Short collision-aware quadratic fillets give continuous travel headings.
    // Densely sampling only the curve also keeps distance-based animation even.
    std::vector<RoutePoint> rounded{simplified.front()};
    for (std::size_t index = 1; index + 1 < simplified.size(); ++index) {
        const auto before = simplified[index - 1].point;
        const auto corner = simplified[index].point;
        const auto after = simplified[index + 1].point;
        if (simplified[index].leap || simplified[index + 1].leap) {
            rounded.push_back(simplified[index]);
            continue;
        }
        const float cut = std::min({.3f, distance(before, corner) * .35f, distance(corner, after) * .35f});
        const auto in = sub(corner, mul(unit(sub(corner, before)), cut));
        const auto out = add(corner, mul(unit(sub(after, corner)), cut));
        std::array<Point, 7> curve{};
        bool clear = true;
        for (std::size_t step = 0; step < curve.size(); ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(curve.size() - 1);
            curve[step] = lerp(lerp(in, corner, t), lerp(corner, out, t), t);
            if (!freePoint(curve[step], obstacles, dogRadius)) { clear = false; }
            if (step > 0 && !clearLine(curve[step - 1], curve[step], obstacles, dogRadius)) { clear = false; }
        }
        if (clear) {
            for (const auto p : curve) { rounded.push_back({p, false}); }
        } else {
            rounded.push_back(simplified[index]);
        }
    }
    if (simplified.size() > 1) { rounded.push_back(simplified.back()); }
    return {std::move(rounded), bestHeading};
}

} // namespace

bool Motion::begin(const BoardModel &board, SheepId target, Point bombPosition,
                   float bombRotationDegrees, float offscreenY,
                   std::span<const OtherSheepPose> otherSheep) {
    clear();
    if (board.width() <= 0 || board.height() <= 0 ||
        !std::isfinite(bombPosition.x) || !std::isfinite(bombPosition.y) ||
        !std::isfinite(offscreenY)) { return false; }
    originalBomb_ = bombPosition;
    originalBombRotation_ = std::isfinite(bombRotationDegrees) ? bombRotationDegrees : 0.f;
    const Point start{static_cast<float>(board.width()) * .5f,
                       std::min(offscreenY, -PenEscape::kFenceOffset - 2.2f)};
    const auto obstacles = obstaclesFor(board, target, otherSheep);
    // First exhaust ordinary ground routes. Even a much longer safe detour is
    // preferable to jumping through a flock when no jump is actually needed.
    auto choice = chooseRoute(board, target, bombPosition, start, obstacles, kDogRadius, false);
    bool clearingFlock = false;
    if (choice.points.empty()) {
        choice = chooseRoute(board, target, bombPosition, start, obstacles, kDogRadius, true);
    }
    if (choice.points.empty()) {
        // Extremely dense layouts can seal every full-width walking route.
        // A light-footed dog can use smaller landing gaps while vaulting the
        // adjacent bodies; keep the jump span bounded instead of teleporting.
        choice = chooseRoute(board, target, bombPosition, start, obstacles, .09f, false);
        if (choice.points.empty()) {
            choice = chooseRoute(board, target, bombPosition, start, obstacles, .09f, true);
        }
    }
    if (choice.points.empty()) {
        // A dense field can seal every landing pocket. The shepherd can still
        // reach it: the controller gently makes the paused flock step aside
        // along this route and restores their poses afterwards. Only sheep
        // yield; fences remain solid and entry/exit still use the same gate.
        std::vector<Obstacle> fences;
        std::copy_if(obstacles.begin(), obstacles.end(), std::back_inserter(fences),
                     [](const auto &obstacle) { return !obstacle.vaultable; });
        choice = chooseRoute(board, target, bombPosition, start, fences, kDogRadius, false);
        clearingFlock = !choice.points.empty();
    }
    if (choice.points.empty()) { return false; }
    route_.push_back({start, false});
    for (const auto &point : choice.points) {
        if (distance(route_.back().position, point.point) > .01f) {
            route_.push_back({point.point, point.leap});
        }
    }
    if (route_.size() < 2) { clear(); return false; }
    cumulative_.reserve(route_.size());
    cumulative_.push_back(0.f);
    for (std::size_t index = 1; index < route_.size(); ++index) {
        cumulative_.push_back(cumulative_.back() + distance(route_[index - 1].position, route_[index].position));
    }
    grabHeading_ = choice.heading;
    pose_.position = start;
    pose_.clearingFlock = clearingFlock;
    pose_.rotationDegrees = heading(sub(route_[1].position, start));
    pose_.bombPosition = bombPosition;
    pose_.bombRotationDegrees = originalBombRotation_;
    stage_ = Stage::Approach;
    return true;
}

bool Motion::active() const noexcept {
    return stage_ == Stage::Approach || stage_ == Stage::Grab || stage_ == Stage::Return;
}

void Motion::setOffscreenY(float y) {
    if (!active() || route_.size() < 2 || !std::isfinite(y)) { return; }
    const float nextY = std::min(y, -PenEscape::kFenceOffset - 2.2f);
    const auto oldStart = route_.front().position;
    if (nextY >= oldStart.y - .001f) { return; }
    const float extension = oldStart.y - nextY;
    const Point nextStart{oldStart.x, nextY};
    const auto firstEnd = route_[1].position;
    if (!route_[1].leapFromPrevious &&
        std::abs(firstEnd.x - oldStart.x) < .0001f && firstEnd.y > oldStart.y) {
        // The first edge is already vertical; extending it does not change
        // any point on the previously existing route.
        route_.front().position = nextStart;
        for (std::size_t index = 1; index < cumulative_.size(); ++index) {
            cumulative_[index] += extension;
        }
    } else {
        // Preserve the complete old route, including its collision detour.
        // This new edge is entirely below its previous offscreen endpoint.
        route_.insert(route_.begin(), {nextStart, false});
        cumulative_.insert(cumulative_.begin(), 0.f);
        for (std::size_t index = 1; index < cumulative_.size(); ++index) {
            cumulative_[index] += extension;
        }
    }
    distance_ += extension;
}

void Motion::clear() {
    route_.clear();
    cumulative_.clear();
    stage_ = Stage::Idle;
    pose_ = {};
    originalBomb_ = {};
    originalBombRotation_ = 0.f;
    grabHeading_ = 0.f;
    distance_ = 0.f;
    speed_ = 0.f;
    gaitDistance_ = 0.f;
    grabElapsed_ = 0.f;
    turnElapsed_ = 0.f;
    justGrabbed_ = false;
}

void Motion::advance(float deltaTime) {
    justGrabbed_ = false;
    if (!active() || !std::isfinite(deltaTime) || deltaTime <= 0.f) { return; }
    // Fixed small integration slices prevent a stalled frame from skipping the
    // pickup contact or making acceleration depend on display refresh rate.
    float remaining = std::min(deltaTime, .1f);
    while (remaining > .00001f && active()) {
        const float step = std::min(remaining, 1.f / 120.f);
        if (stage_ == Stage::Grab) {
            advanceGrab(step);
        } else {
            advanceTravel(step, stage_ == Stage::Return);
        }
        remaining -= step;
    }
}

void Motion::advanceTravel(float deltaTime, bool returning) {
    if (route_.size() < 2 || cumulative_.empty()) { stage_ = Stage::Finished; return; }
    const float total = cumulative_.back();
    if (returning && turnElapsed_ < kTurnDuration) {
        turnElapsed_ = std::min(kTurnDuration, turnElapsed_ + deltaTime);
        const float t = smooth(turnElapsed_ / kTurnDuration);
        const auto rear = unit(sub(route_[route_.size() - 2].position, route_.back().position));
        const float exitHeading = heading(rear);
        pose_.rotationDegrees = grabHeading_ + angleDelta(grabHeading_, exitHeading) * t;
        // A small planted rear-foot pivot, not a rapid full-body spin.
        const auto side = forward(grabHeading_ + 90.f);
        pose_.position = add(route_.back().position, mul(side, .055f * std::sin(kPi * t)));
        pose_.frame = ShepherdDogAnimation::kGrabFirst + ShepherdDogAnimation::kGrabFrames - 1;
        pose_.speed = 0.f;
        pose_.lift = 0.f;
        updateCarriedBomb();
        return;
    }
    const float remaining = returning ? distance_ : total - distance_;
    const float brakingSpeed = std::sqrt(std::max(0.f, 2.f * kBraking * remaining));
    float targetSpeed = returning ? kRunSpeed : std::min(kRunSpeed, brakingSpeed);
    const float at = std::clamp(distance_, 0.f, total);
    const auto upper = std::upper_bound(cumulative_.begin(), cumulative_.end(), at);
    const std::size_t segment = std::clamp<std::size_t>(
        static_cast<std::size_t>(upper - cumulative_.begin()), 1, route_.size() - 1);
    // A vault has a readable takeoff/landing. Avoid racing through a tiny gap.
    if (route_[segment].leapFromPrevious) { targetSpeed = std::min(targetSpeed, 5.5f); }
    const float acceleration = targetSpeed < speed_ ? kBraking : kAcceleration;
    speed_ += std::clamp(targetSpeed - speed_, -acceleration * deltaTime, acceleration * deltaTime);
    const float moved = std::min(remaining, std::max(.06f, speed_) * deltaTime);
    distance_ += returning ? -moved : moved;
    const auto currentUpper = std::upper_bound(cumulative_.begin(), cumulative_.end(), distance_);
    const std::size_t current = std::clamp<std::size_t>(
        static_cast<std::size_t>(currentUpper - cumulative_.begin()), 1, route_.size() - 1);
    const float span = cumulative_[current] - cumulative_[current - 1];
    const float t = span > kEpsilon ? std::clamp((distance_ - cumulative_[current - 1]) / span, 0.f, 1.f) : 1.f;
    pose_.position = lerp(route_[current - 1].position, route_[current].position, t);
    const auto tangent = returning ? sub(route_[current - 1].position, route_[current].position)
                                  : sub(route_[current].position, route_[current - 1].position);
    float desiredHeading = heading(tangent);
    if (!returning && remaining < .6f) {
        desiredHeading += angleDelta(desiredHeading, grabHeading_) * smooth(1.f - remaining / .6f);
    }
    pose_.rotationDegrees = rotateToward(pose_.rotationDegrees, desiredHeading, 600.f * deltaTime);
    pose_.lift = route_[current].leapFromPrevious
        ? std::sin(kPi * t) * std::clamp(span * .23f, .20f, .65f) : 0.f;
    const bool airborne = pose_.lift > .035f;
    // Keep the planted-foot phase while the paws are tucked in a jump. Counting
    // air travel here used to skip part of the run and snap forward on landing.
    // On the ground, the two-stride sequence follows distance, so acceleration
    // and braking change the cadence along with the dog's actual movement.
    if (!airborne) { gaitDistance_ += moved; }
    const float cycle = gaitDistance_ / (kStride * ShepherdDogAnimation::kRunStrides);
    pose_.frame = static_cast<int>(std::floor(cycle * ShepherdDogAnimation::kRunFrames)) %
        ShepherdDogAnimation::kRunFrames;
    pose_.speed = speed_;
    if (airborne) {
        // Once airborne the paws collect underneath the dog instead of
        // continuing a ground-speed run cycle in mid-air.
        pose_.frame = kTuckedRunFrame;
    }
    if (returning) { updateCarriedBomb(); }
    if (remaining - moved <= .001f) {
        speed_ = 0.f;
        pose_.speed = 0.f;
        pose_.lift = 0.f;
        if (returning) {
            pose_.position = route_.front().position;
            stage_ = Stage::Finished;
            updateCarriedBomb();
        } else {
            distance_ = total;
            pose_.position = route_.back().position;
            stage_ = Stage::Grab;
            grabElapsed_ = 0.f;
            pose_.frame = ShepherdDogAnimation::kGrabFirst;
        }
    }
}

void Motion::advanceGrab(float deltaTime) {
    const float previous = grabElapsed_ / kGrabDuration;
    grabElapsed_ = std::min(kGrabDuration, grabElapsed_ + deltaTime);
    const float t = grabElapsed_ / kGrabDuration;
    pose_.rotationDegrees = rotateToward(pose_.rotationDegrees, grabHeading_, 500.f * deltaTime);
    pose_.frame = ShepherdDogAnimation::kGrabFirst +
        std::min(ShepherdDogAnimation::kGrabFrames - 1,
                 static_cast<int>(t * ShepherdDogAnimation::kGrabFrames));
    pose_.speed = 0.f;
    pose_.lift = 0.f;
    // The artwork supplies the neck and jaw articulation; translate only a
    // little into the reach, then take a deliberate half-step back with it.
    const float reach = t < kGrabContact ? .075f * std::sin(kPi * t / (2.f * kGrabContact))
        : .075f * (1.f - smooth((t - kGrabContact) / (1.f - kGrabContact)));
    pose_.position = add(route_.back().position, mul(forward(grabHeading_), reach));
    if (previous < kGrabContact && t >= kGrabContact) {
        pose_.carrying = true;
        justGrabbed_ = true;
    }
    if (pose_.carrying) {
        updateCarriedBomb();
        const float blend = smooth((t - kGrabContact) / .25f);
        pose_.bombPosition = lerp(originalBomb_, pose_.bombPosition, blend);
        pose_.bombRotationDegrees = originalBombRotation_ +
            angleDelta(originalBombRotation_, pose_.bombRotationDegrees) * blend;
    }
    if (grabElapsed_ >= kGrabDuration) {
        stage_ = Stage::Return;
        turnElapsed_ = 0.f;
        pose_.position = route_.back().position;
    }
}

void Motion::updateCarriedBomb() {
    if (!pose_.carrying) { return; }
    const auto facing = forward(pose_.rotationDegrees);
    // The same mouth anchor is used during pickup and locomotion. Tiny gait
    // motion is linked to distance, so the bomb never swings while stationary.
    const float gait = gaitDistance_ / kStride * 2.f * kPi;
    const float running = std::clamp(pose_.speed / kRunSpeed, 0.f, 1.f);
    const float bob = std::sin(gait * 2.f) * .018f * running;
    pose_.bombPosition = add(pose_.position, mul(facing, kMouthOffset + bob));
    pose_.bombRotationDegrees = pose_.rotationDegrees + 90.f + std::sin(gait) * 2.f * running;
}

} // namespace LiberateTheSheep::Game::DogRescue
