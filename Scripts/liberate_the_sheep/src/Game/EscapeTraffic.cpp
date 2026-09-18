#include "Game/EscapeTraffic.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace LiberateTheSheep::Game::EscapeTraffic {
namespace {

// Enclose all localized run-frame motion without rotating or stretching the
// whole Sprite: measured visible half-extents stay below 0.4781 x 0.9100 cells.
constexpr float kHalfWidth = 0.48f;
constexpr float kHalfLength = 0.92f;
constexpr float kHalfGap = 0.01f;
constexpr float kLineStep = 0.08f;
constexpr float kArcStep = 0.02f;
constexpr float kArcAngleStep = 0.1f;
constexpr float kBucketSize = 2.f;

[[nodiscard]] float dot(PenEscape::Point a, PenEscape::Point b) noexcept {
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] int bucketCoordinate(float coordinate) noexcept {
    return static_cast<int>(std::floor(coordinate / kBucketSize));
}

[[nodiscard]] std::uint64_t bucketKey(int x, int y) noexcept {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u) |
        static_cast<std::uint32_t>(y);
}

void calculateBounds(SweptFootprint &footprint) noexcept {
    const float halfX = std::abs(footprint.forward.x) * footprint.halfLength +
        std::abs(footprint.sideways.x) * footprint.halfWidth;
    const float halfY = std::abs(footprint.forward.y) * footprint.halfLength +
        std::abs(footprint.sideways.y) * footprint.halfWidth;
    footprint.minimum = {footprint.center.x - halfX, footprint.center.y - halfY};
    footprint.maximum = {footprint.center.x + halfX, footprint.center.y + halfY};
}

[[nodiscard]] bool overlaps(const SweptFootprint &a, const SweptFootprint &b) noexcept {
    if (a.maximum.x < b.minimum.x || b.maximum.x < a.minimum.x ||
        a.maximum.y < b.minimum.y || b.maximum.y < a.minimum.y) {
        return false;
    }
    const PenEscape::Point displacement{b.center.x - a.center.x, b.center.y - a.center.y};
    // Separating-axis test on both rectangles' axes. Touching the padded
    // envelopes counts as a conflict, leaving a small visible breathing gap.
    for (const PenEscape::Point axis : {a.forward, a.sideways, b.forward, b.sideways}) {
        const float extentA = std::abs(dot(axis, a.forward)) * a.halfLength +
            std::abs(dot(axis, a.sideways)) * a.halfWidth;
        const float extentB = std::abs(dot(axis, b.forward)) * b.halfLength +
            std::abs(dot(axis, b.sideways)) * b.halfWidth;
        if (std::abs(dot(axis, displacement)) > extentA + extentB) { return false; }
    }
    return true;
}

void indexInterval(FootprintRoute &route, std::size_t index) {
    const auto &footprint = route.intervals[index];
    for (int y = bucketCoordinate(footprint.minimum.y);
         y <= bucketCoordinate(footprint.maximum.y); ++y) {
        for (int x = bucketCoordinate(footprint.minimum.x);
             x <= bucketCoordinate(footprint.maximum.x); ++x) {
            route.buckets[bucketKey(x, y)].push_back(index);
        }
    }
}

} // namespace

FootprintRoute buildRoute(const PenEscape::Path &path, int columns, int rows) {
    FootprintRoute route;
    if (!path.valid() || columns <= 0 || rows <= 0) { return route; }
    float segmentBegin = 0.f;
    for (const PenEscape::Segment &segment : path.segments) {
        const bool arc = segment.kind == PenEscape::Segment::Kind::Arc;
        const float divisions = arc
            ? std::max(segment.length / kArcStep,
                std::abs(segment.sweepRadians) / kArcAngleStep)
            : segment.length / kLineStep;
        const int count = std::max(1, static_cast<int>(std::ceil(divisions)));
        for (int index = 0; index < count; ++index) {
            const float begin = segmentBegin + segment.length *
                (static_cast<float>(index) / static_cast<float>(count));
            const float end = segmentBegin + segment.length *
                (static_cast<float>(index + 1) / static_cast<float>(count));
            const float halfStep = (end - begin) * 0.5f;
            const auto pose = PenEscape::sample(path, (begin + end) * 0.5f);
            SweptFootprint footprint;
            footprint.begin = begin;
            footprint.end = end;
            footprint.center = pose.position;
            footprint.forward = pose.tangent;
            footprint.sideways = {-pose.tangent.y, pose.tangent.x};
            footprint.halfWidth = kHalfWidth + kHalfGap;
            footprint.halfLength = kHalfLength + kHalfGap;

            if (arc) {
                const float halfAngle = std::abs(segment.sweepRadians) /
                    (2.f * static_cast<float>(count));
                const float sine = std::sin(halfAngle);
                const float cosine = std::cos(halfAngle);
                const float width = footprint.halfWidth;
                const float length = footprint.halfLength;
                // In midpoint-local axes, rotating the body by |a|<=halfAngle
                // adds at most length*sin(a) laterally and width*sin(a)
                // longitudinally (cos(a)<=1). The arc center additionally
                // moves <=r*(1-cos(a)) laterally and <=r*sin(a) forward. This
                // encloses the continuous turn, not just sampled poses.
                footprint.halfWidth += length * sine + segment.radius * (1.f - cosine);
                footprint.halfLength += width * sine + segment.radius * sine;
            } else {
                // A straight sweep expands ONLY the length. Expanding both
                // axes would falsely block valid sheep in adjacent columns.
                footprint.halfLength += halfStep;
            }
            calculateBounds(footprint);
            route.intervals.push_back(footprint);
            indexInterval(route, route.intervals.size() - 1);
        }
        segmentBegin += segment.length;
    }
    return route;
}

std::vector<Conflict> reserveAgainst(
    const FootprintRoute &follower, const FootprintRoute &leader
) {
    std::vector<Conflict> conflicts;
    if (follower.intervals.empty() || leader.intervals.empty()) { return conflicts; }
    // One leader interval may be found in several overlapping buckets. Reuse
    // a stamp array instead of allocating a set for every follower interval.
    std::vector<std::size_t> seen(leader.intervals.size(), 0);
    std::size_t stamp = 0;
    for (const SweptFootprint &candidate : follower.intervals) {
        ++stamp;
        float clearDistance = -1.f;
        for (int y = bucketCoordinate(candidate.minimum.y);
             y <= bucketCoordinate(candidate.maximum.y); ++y) {
            for (int x = bucketCoordinate(candidate.minimum.x);
                 x <= bucketCoordinate(candidate.maximum.x); ++x) {
                const auto bucket = leader.buckets.find(bucketKey(x, y));
                if (bucket == leader.buckets.end()) { continue; }
                // Furthest-along intervals first: once one overlaps, earlier
                // intervals cannot raise the release distance. Bucket entries
                // are sorted by end distance, so do not scan their full tails.
                for (auto index = bucket->second.rbegin(); index != bucket->second.rend(); ++index) {
                    const SweptFootprint &reserved = leader.intervals[*index];
                    if (reserved.end <= clearDistance) { break; }
                    if (seen[*index] == stamp) { continue; }
                    seen[*index] = stamp;
                    if (!overlaps(candidate, reserved)) { continue; }
                    clearDistance = reserved.end;
                    break;
                }
            }
        }
        if (clearDistance >= 0.f) {
            conflicts.push_back({candidate.begin, clearDistance});
        }
    }
    return conflicts;
}

float stopDistance(const std::vector<Conflict> &conflicts, float leaderDistance) noexcept {
    for (const Conflict &conflict : conflicts) {
        if (!(leaderDistance >= conflict.leaderClearDistance)) { return conflict.followerDistance; }
    }
    return std::numeric_limits<float>::infinity();
}

} // namespace LiberateTheSheep::Game::EscapeTraffic
