#include "Game/PenEscapePath.hpp"

#include <algorithm>
#include <cmath>

namespace LiberateTheSheep::Game::PenEscape {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kEpsilon = 0.00001f;

[[nodiscard]] Point add(Point a, Point b) noexcept { return {a.x + b.x, a.y + b.y}; }
[[nodiscard]] Point subtract(Point a, Point b) noexcept { return {a.x - b.x, a.y - b.y}; }
[[nodiscard]] Point multiply(Point value, float factor) noexcept {
    return {value.x * factor, value.y * factor};
}
[[nodiscard]] float magnitude(Point point) noexcept { return std::hypot(point.x, point.y); }
[[nodiscard]] Point normalized(Point point) noexcept {
    const float length = magnitude(point);
    return length > kEpsilon ? multiply(point, 1.f / length) : Point{};
}
[[nodiscard]] float cross(Point a, Point b) noexcept { return a.x * b.y - a.y * b.x; }

void appendPoint(std::vector<Point> &points, Point next) {
    if (points.empty() || magnitude(subtract(points.back(), next)) > kEpsilon) {
        points.push_back(next);
    }
}

void appendLine(Path &path, Point start, Point end) {
    const float length = magnitude(subtract(end, start));
    if (length <= kEpsilon) {
        return;
    }
    path.segments.push_back({Segment::Kind::Line, start, end, {}, 0.f, 0.f, 0.f, length});
    path.length += length;
}

// Polyline edges are axis-aligned. Each 90-degree turn uses an exact circular
// fillet; shortening both neighboring edges by the radius makes it tangent.
// Half-edge limits keep adjacent fillets from overlapping near the gate.
[[nodiscard]] Path roundedPath(const std::vector<Point> &points) {
    Path path;
    if (points.size() < 2) {
        return path;
    }
    path.start = points.front();
    path.end = points.back();
    Point cursor = path.start;
    for (std::size_t index = 1; index + 1 < points.size(); ++index) {
        const Point vertex = points[index];
        const Point incoming = subtract(vertex, points[index - 1]);
        const Point outgoing = subtract(points[index + 1], vertex);
        const float incomingLength = magnitude(incoming);
        const float outgoingLength = magnitude(outgoing);
        const Point inDirection = normalized(incoming);
        const Point outDirection = normalized(outgoing);
        const float turn = cross(inDirection, outDirection);
        if (std::abs(turn) < kEpsilon) {
            // A collinear waypoint is not a corner. Such points occur when a
            // sheep starts in the gate column, so it can simply run straight.
            continue;
        }
        const float radius = std::min({
            kCornerRadius, incomingLength * 0.5f, outgoingLength * 0.5f,
        });
        const Point arcStart = subtract(vertex, multiply(inDirection, radius));
        const Point arcEnd = add(vertex, multiply(outDirection, radius));
        const Point center = add(arcStart, multiply(outDirection, radius));
        appendLine(path, cursor, arcStart);
        const float startAngle = std::atan2(arcStart.y - center.y, arcStart.x - center.x);
        const float sweep = std::copysign(kPi * 0.5f, turn);
        const float length = radius * std::abs(sweep);
        path.segments.push_back({
            Segment::Kind::Arc, arcStart, arcEnd, center, radius, startAngle, sweep, length,
        });
        path.length += length;
        cursor = arcEnd;
    }
    appendLine(path, cursor, path.end);
    return path;
}

[[nodiscard]] Sample sampleSegment(const Segment &segment, float distance) noexcept {
    Sample result;
    const float t = segment.length > kEpsilon
                        ? std::clamp(distance / segment.length, 0.f, 1.f)
                        : 1.f;
    if (segment.kind == Segment::Kind::Arc) {
        const float angle = segment.startAngleRadians + segment.sweepRadians * t;
        const float sine = std::sin(angle);
        const float cosine = std::cos(angle);
        const float sign = std::copysign(1.f, segment.sweepRadians);
        result.position = add(segment.center, {cosine * segment.radius, sine * segment.radius});
        result.tangent = {-sine * sign, cosine * sign};
    } else {
        const Point delta = subtract(segment.end, segment.start);
        result.position = add(segment.start, multiply(delta, t));
        result.tangent = normalized(delta);
    }
    // Exact stored endpoints avoid tiny trigonometric gaps at joins.
    if (t <= 0.f) {
        result.position = segment.start;
    } else if (t >= 1.f) {
        result.position = segment.end;
    }
    result.rotationDegrees = std::atan2(-result.tangent.x, result.tangent.y) * 180.f / kPi;
    return result;
}

} // namespace

bool Path::valid() const noexcept {
    return !segments.empty() && length > 0.f && std::isfinite(length);
}

Path makePath(int columns, int rows, const Sheep &sheep, float outsideRunout) {
    if (columns <= 0 || rows <= 0 || !isValidDirection(sheep.direction) ||
        !std::isfinite(outsideRunout)) {
        return {};
    }
    outsideRunout = std::max(kOutsideRunout, outsideRunout);
    const auto inside = [columns, rows](Cell cell) {
        return cell.x >= 0 && cell.y >= 0 && cell.x < columns && cell.y < rows;
    };
    if (!inside(sheep.tail) || !inside(sheep.head())) {
        return {};
    }
    const Cell head = sheep.head();
    const Point start{
        (static_cast<float>(sheep.tail.x) + static_cast<float>(head.x) + 1.f) * 0.5f,
        (static_cast<float>(sheep.tail.y) + static_cast<float>(head.y) + 1.f) * 0.5f,
    };
    const float width = static_cast<float>(columns);
    const float height = static_cast<float>(rows);
    const float left = -kCorridorOffset;
    const float right = width + kCorridorOffset;
    const float top = height + kCorridorOffset;
    const float bottom = -kCorridorOffset;
    const float gateX = width * 0.5f;
    std::vector<Point> waypoints;
    waypoints.reserve(7);
    appendPoint(waypoints, start);

    switch (sheep.direction) {
        case Direction::Up: {
            const float side = start.x <= gateX ? left : right;
            appendPoint(waypoints, {start.x, top});
            appendPoint(waypoints, {side, top});
            appendPoint(waypoints, {side, bottom});
            break;
        }
        case Direction::Right:
            appendPoint(waypoints, {right, start.y});
            appendPoint(waypoints, {right, bottom});
            break;
        case Direction::Down:
            appendPoint(waypoints, {start.x, bottom});
            break;
        case Direction::Left:
            appendPoint(waypoints, {left, start.y});
            appendPoint(waypoints, {left, bottom});
            break;
    }
    appendPoint(waypoints, {gateX, bottom});
    appendPoint(waypoints, {gateX, -kFenceOffset - outsideRunout});
    return roundedPath(waypoints);
}

Sample sample(const Path &path, float travelledDistance) noexcept {
    if (!path.valid()) {
        return {path.start, {0.f, 1.f}, 0.f, true};
    }
    if (std::isnan(travelledDistance) || travelledDistance <= 0.f) {
        return sampleSegment(path.segments.front(), 0.f);
    }
    if (travelledDistance >= path.length) {
        Sample result = sampleSegment(path.segments.back(), path.segments.back().length);
        result.finished = true;
        return result;
    }
    float remaining = travelledDistance;
    for (const Segment &segment : path.segments) {
        if (remaining <= segment.length) {
            return sampleSegment(segment, remaining);
        }
        remaining -= segment.length;
    }
    Sample result = sampleSegment(path.segments.back(), path.segments.back().length);
    result.finished = true;
    return result;
}

} // namespace LiberateTheSheep::Game::PenEscape
