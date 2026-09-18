#include "Game/BoardGenerator.hpp"
#include "Game/BoardLayout.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/PenEscapePath.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

using namespace LiberateTheSheep::Game;
namespace Escape = LiberateTheSheep::Game::PenEscape;

constexpr float kTolerance = 0.001f;
// Encloses every run frame's wool, ears and hooves with a small margin. The
// fixed-size 1.23 x 1.97 Sprite quad also contains transparent atlas padding,
// which is not collision geometry.
constexpr float kHalfWidth = 0.48f;
constexpr float kHalfLength = 0.92f;
constexpr float kFenceHalfThickness = 2.4f / 0.94f / 7.f * 0.5f;
constexpr float kFenceEndOverhang = (2.4f / 0.94f - 2.4f) * 0.5f;

[[nodiscard]] float distance(Escape::Point a, Escape::Point b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

[[nodiscard]] float dot(Escape::Point a, Escape::Point b) {
    return a.x * b.x + a.y * b.y;
}

[[nodiscard]] std::array<Escape::Point, 4> bodyCorners(const Escape::Sample &pose) {
    const Escape::Point forward = pose.tangent;
    const Escape::Point side{forward.y, -forward.x};
    std::array<Escape::Point, 4> corners{};
    // Perimeter order is also used below when clipping the rotated body to
    // the bottom fence's horizontal band.
    const std::array<Escape::Point, 4> localCorners{{
        {-kHalfWidth, -kHalfLength}, {kHalfWidth, -kHalfLength},
        {kHalfWidth, kHalfLength}, {-kHalfWidth, kHalfLength},
    }};
    for (std::size_t index = 0; index < corners.size(); ++index) {
        const Escape::Point local = localCorners[index];
        corners[index] = {
            pose.position.x + side.x * local.x + forward.x * local.y,
            pose.position.y + side.y * local.x + forward.y * local.y,
        };
    }
    return corners;
}

// Separating-axis test: the complete oriented visible silhouette versus an
// occupied grid cell, not merely the moving sheep's center against occupancy.
[[nodiscard]] bool overlapsCell(const Escape::Sample &pose, Cell cell) {
    const Escape::Point delta{
        static_cast<float>(cell.x) + 0.5f - pose.position.x,
        static_cast<float>(cell.y) + 0.5f - pose.position.y,
    };
    const Escape::Point forward = pose.tangent;
    const Escape::Point side{forward.y, -forward.x};
    const std::array<Escape::Point, 4> axes{{{1.f, 0.f}, {0.f, 1.f}, forward, side}};
    for (Escape::Point axis : axes) {
        const float bodyRadius = kHalfWidth * std::abs(dot(side, axis)) +
                                 kHalfLength * std::abs(dot(forward, axis));
        const float cellRadius = 0.5f * (std::abs(axis.x) + std::abs(axis.y));
        if (std::abs(dot(delta, axis)) >= bodyRadius + cellRadius - kTolerance) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool safelyInsideFence(const Escape::Sample &pose, int width, int height) {
    float minimumX = std::numeric_limits<float>::max();
    float maximumX = std::numeric_limits<float>::lowest();
    float minimumY = std::numeric_limits<float>::max();
    float maximumY = std::numeric_limits<float>::lowest();
    const auto corners = bodyCorners(pose);
    for (const Escape::Point corner : corners) {
        minimumX = std::min(minimumX, corner.x);
        maximumX = std::max(maximumX, corner.x);
        minimumY = std::min(minimumY, corner.y);
        maximumY = std::max(maximumY, corner.y);
    }
    constexpr float kInsideOffset = Escape::kFenceOffset - kFenceHalfThickness;
    if (minimumX < -kInsideOffset ||
        maximumX > static_cast<float>(width) + kInsideOffset ||
        maximumY > static_cast<float>(height) + kInsideOffset) {
        return false;
    }
    // Once any part reaches the bottom fence it must fit the single doorway.
    // Check the full panel thickness, and reduce the gate by both overlapping
    // end posts. This also applies before the whole silhouette exits the pen.
    if (minimumY <= -Escape::kFenceOffset + kFenceHalfThickness &&
        maximumY >= -Escape::kFenceOffset - kFenceHalfThickness) {
        constexpr float bandBottom = -Escape::kFenceOffset - kFenceHalfThickness;
        constexpr float bandTop = -Escape::kFenceOffset + kFenceHalfThickness;
        float bandMinimumX = std::numeric_limits<float>::max();
        float bandMaximumX = std::numeric_limits<float>::lowest();
        const auto includeX = [&](float x) {
            bandMinimumX = std::min(bandMinimumX, x);
            bandMaximumX = std::max(bandMaximumX, x);
        };
        // Only the part of the body intersecting the fence band must fit the
        // doorway. The full body's X bounds produce false collisions while
        // turning: an upper corner can still be safely above the fence.
        // Vertices of the clipped polygon are its existing in-band corners
        // plus the intersections of its edges with either band boundary.
        for (std::size_t index = 0; index < corners.size(); ++index) {
            const Escape::Point a = corners[index];
            const Escape::Point b = corners[(index + 1) % corners.size()];
            if (a.y >= bandBottom && a.y <= bandTop) {
                includeX(a.x);
            }
            const float deltaY = b.y - a.y;
            if (std::abs(deltaY) <= std::numeric_limits<float>::epsilon()) {
                continue;
            }
            for (const float boundary : {bandBottom, bandTop}) {
                const float t = (boundary - a.y) / deltaY;
                if (t >= 0.f && t <= 1.f) {
                    includeX(a.x + (b.x - a.x) * t);
                }
            }
        }
        const float gateX = static_cast<float>(width) * 0.5f;
        if (bandMinimumX < gateX - Escape::kGateWidth * 0.5f + kFenceEndOverhang ||
            bandMaximumX > gateX + Escape::kGateWidth * 0.5f - kFenceEndOverhang) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool fenceBandChecks() {
    // Regression: only the lower corner touches the fence band during this
    // turn. The leftmost corner is outside the gate's X range but above it.
    const Escape::Sample turning{{1.87668f, -1.48854f}, {0.748752f, -0.66285f}};
    const Escape::Sample throughGate{{2.f, -Escape::kFenceOffset}, {0.f, -1.f}};
    const Escape::Sample throughPanel{{0.f, -Escape::kFenceOffset}, {0.f, -1.f}};
    const Escape::Sample clippingPost{{1.f, -Escape::kFenceOffset}, {0.f, -1.f}};
    const Escape::Sample clippingSide{{-Escape::kFenceOffset, 2.f}, {0.f, -1.f}};
    if (!safelyInsideFence(turning, 4, 4) || !safelyInsideFence(throughGate, 4, 4) ||
        safelyInsideFence(throughPanel, 4, 4) || safelyInsideFence(clippingPost, 4, 4) ||
        safelyInsideFence(clippingSide, 4, 4)) {
        std::cerr << "Fence-band clipping rejected a clear turn or missed a real collision\n";
        return false;
    }
    return true;
}

[[nodiscard]] bool pathChecks(int width, int height, const Sheep &sheep,
                              const BoardModel *board = nullptr) {
    const Escape::Path path = Escape::makePath(width, height, sheep);
    if (!path.valid()) {
        std::cerr << "Invalid path for legal sheep\n";
        return false;
    }
    const Cell delta = directionDelta(sheep.direction);
    const Escape::Sample start = Escape::sample(path, 0.f);
    if (distance(start.tangent, {static_cast<float>(delta.x), static_cast<float>(delta.y)}) >
            kTolerance ||
        start.finished || distance(start.position, path.start) > kTolerance) {
        std::cerr << "Route does not begin on the inspected straight ray\n";
        return false;
    }
    const Escape::Sample end = Escape::sample(path, path.length + 10.f);
    if (!end.finished || distance(end.position, path.end) > kTolerance ||
        distance(end.tangent, {0.f, -1.f}) > kTolerance ||
        std::abs(end.position.x - static_cast<float>(width) * 0.5f) > kTolerance ||
        end.position.y > -Escape::kFenceOffset - kHalfLength) {
        std::cerr << "Route does not leave completely through the bottom gate\n";
        return false;
    }

    float cumulativeDistance = 0.f;
    for (const Escape::Segment &segment : path.segments) {
        if (segment.length <= 0.f || !std::isfinite(segment.length)) {
            return false;
        }
        cumulativeDistance += segment.length;
        if (cumulativeDistance < path.length - kTolerance) {
            const Escape::Sample before = Escape::sample(path, cumulativeDistance - 0.0001f);
            const Escape::Sample after = Escape::sample(path, cumulativeDistance + 0.0001f);
            if (distance(before.position, after.position) > 0.0003f ||
                distance(before.tangent, after.tangent) > 0.002f) {
                std::cerr << "Position or heading discontinuity at a corner\n";
                return false;
            }
        }
    }
    if (std::abs(cumulativeDistance - path.length) > kTolerance) {
        return false;
    }

    const int steps = static_cast<int>(std::ceil(path.length / 0.025f));
    const float stepLength = path.length / static_cast<float>(steps);
    Escape::Sample previous = start;
    for (int step = 0; step <= steps; ++step) {
        const float travelled = stepLength * static_cast<float>(step);
        const Escape::Sample pose = Escape::sample(path, travelled);
        if (!safelyInsideFence(pose, width, height)) {
            std::cerr << "Run silhouette clipped the thick fence outside the gate: "
                      << width << 'x' << height << ", tail " << sheep.tail.x << ','
                      << sheep.tail.y << ", direction " << static_cast<int>(sheep.direction)
                      << ", distance " << travelled << ", center " << pose.position.x
                      << ',' << pose.position.y << ", tangent " << pose.tangent.x
                      << ',' << pose.tangent.y << '\n';
            return false;
        }
        if (step > 0) {
            const float chord = distance(previous.position, pose.position);
            if (chord < stepLength * 0.99f || chord > stepLength * 1.005f) {
                std::cerr << "Distance sampling changes ground speed\n";
                return false;
            }
        }
        if (board != nullptr) {
            for (const Sheep &other : board->sheep()) {
                if (other.id == sheep.id) {
                    continue;
                }
                if (overlapsCell(pose, other.tail) || overlapsCell(pose, other.head())) {
                    std::cerr << "Run silhouette clipped a sheep while leaving the flock at "
                              << pose.position.x << ", " << pose.position.y << '\n';
                    return false;
                }
            }
        }
        previous = pose;
    }
    return true;
}

[[nodiscard]] bool invalidInputChecks() {
    const Sheep sheep{1, {0, 0}, Direction::Up};
    const Escape::Path invalid = Escape::makePath(0, 4, sheep);
    return !invalid.valid() && Escape::sample(invalid, 1.f).finished &&
           !Escape::makePath(4, 0, sheep).valid() &&
           !Escape::makePath(4, 4, {1, {-1, 0}, Direction::Up}).valid() &&
           !Escape::makePath(4, 4, {1, {3, 3}, Direction::Up}).valid() &&
           !Escape::makePath(4, 4, {1, {0, 0}, static_cast<Direction>(255)}).valid() &&
           !Escape::makePath(4, 4, sheep, std::numeric_limits<float>::quiet_NaN()).valid() &&
           !Escape::makePath(4, 4, sheep, std::numeric_limits<float>::infinity()).valid();
}

[[nodiscard]] bool extendedRunoutChecks() {
    constexpr float kRequiredBottom = -12.f;
    constexpr float kRunout = -kRequiredBottom + kHalfLength + 0.1f - Escape::kFenceOffset;
    for (const Direction direction : {Direction::Up, Direction::Right,
                                     Direction::Down, Direction::Left}) {
        const Sheep sheep{1, {2, 2}, direction};
        const Escape::Path original = Escape::makePath(4, 4, sheep);
        const Escape::Path extended = Escape::makePath(4, 4, sheep, kRunout);
        const Escape::Path clamped = Escape::makePath(4, 4, sheep, -10.f);
        if (!original.valid() || !extended.valid() || !clamped.valid() ||
            std::abs(clamped.length - original.length) > kTolerance ||
            std::abs(extended.length - original.length -
                     (kRunout - Escape::kOutsideRunout)) > kTolerance) {
            std::cerr << "Camera runout did not extend only the final straight segment\n";
            return false;
        }
        const Escape::Sample end = Escape::sample(extended, extended.length);
        if (!end.finished || end.position.y + kHalfLength >= kRequiredBottom ||
            distance(end.tangent, {0.f, -1.f}) > kTolerance ||
            std::abs(end.position.x - 2.f) > kTolerance) {
            std::cerr << "Extended runout leaves part of the sheep inside the view\n";
            return false;
        }
        // A resize may rebuild the route while an escape is already in flight.
        // Keeping travelled distance must retain both position and heading.
        for (float travelled = 0.f; travelled <= original.length; travelled += 0.025f) {
            const Escape::Sample before = Escape::sample(original, travelled);
            const Escape::Sample after = Escape::sample(extended, travelled);
            if (distance(before.position, after.position) > kTolerance ||
                distance(before.tangent, after.tangent) > kTolerance) {
                std::cerr << "Viewport-dependent runout changed the existing route prefix\n";
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] bool allPositionsChecks(std::size_t &routesChecked) {
    const std::array<Cell, 4> dimensions{{{4, 4}, {4, 6}, {8, 12}, {12, 18}}};
    for (Cell dimensionsForLevel : dimensions) {
        BoardModel emptyBoard(dimensionsForLevel.x, dimensionsForLevel.y);
        for (int y = 0; y < dimensionsForLevel.y; ++y) {
            for (int x = 0; x < dimensionsForLevel.x; ++x) {
                for (Direction direction : {Direction::Up, Direction::Right,
                                            Direction::Down, Direction::Left}) {
                    const Sheep sheep{1, {x, y}, direction};
                    if (!emptyBoard.inspectPlacement(sheep).valid()) {
                        continue;
                    }
                    if (!pathChecks(dimensionsForLevel.x, dimensionsForLevel.y, sheep)) {
                        return false;
                    }
                    ++routesChecked;
                }
            }
        }
    }
    return true;
}

[[nodiscard]] bool generatedFlockChecks(std::size_t &routesChecked) {
    for (const LevelDefinition &level : levelCatalog()) {
        GenerationResult generated = BoardGenerator::generate(level.generation);
        if (!generated.succeeded()) {
            return false;
        }
        BoardModel &board = *generated.board;
        for (SheepId id : generated.guaranteedSolution) {
            const Sheep *sheep = board.findSheep(id);
            if (sheep == nullptr || !board.inspectMove(id).canEscape() ||
                !pathChecks(board.width(), board.height(), *sheep, &board)) {
                std::cerr << "Catalog level " << level.number << ", sheep " << id << '\n';
                return false;
            }
            ++routesChecked;
            if (!board.eraseSheep(id)) {
                return false;
            }
        }
        if (!board.empty()) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool pastureLayoutChecks(std::size_t &layoutsChecked) {
    struct ViewportCase {
        int width;
        int height;
        LayoutInsets safeArea;
    };
    const std::array<ViewportCase, 7> viewports{{
        {375, 812, {44.f, 0.f, 34.f, 0.f}},
        {390, 844, {47.f, 0.f, 34.f, 0.f}},
        {375, 667, {}},
        {320, 568, {}},
        {1400, 700, {}},
        {812, 375, {0.f, 44.f, 21.f, 44.f}},
        {1024, 768, {24.f, 0.f, 20.f, 0.f}},
    }};
    const std::array<Cell, 4> dimensions{{{4, 4}, {4, 6}, {8, 12}, {12, 18}}};
    // PastureScene uses at most 2.4-cell fence panels, overlapping their end
    // posts by 6%, with a 7:1 aspect ratio. Include their complete Sprite,
    // not only the fence centerline, when checking HUD and viewport bounds.
    constexpr float kFenceOuterOffset = Escape::kFenceOffset + kFenceHalfThickness;
    for (const ViewportCase &viewport : viewports) {
        const LayoutInsets insets{
            viewport.safeArea.top + 154.f,
            viewport.safeArea.right + 18.f,
            viewport.safeArea.bottom + 20.f,
            viewport.safeArea.left + 18.f,
        };
        for (const Cell size : dimensions) {
            for (const float orthoSize : {9.f, 17.f}) {
                for (const LayoutPoint camera : {LayoutPoint{}, LayoutPoint{3.f, -2.f}}) {
                    const BoardLayout layout = calculatePastureLayout(
                        viewport.width, viewport.height, size.x, size.y,
                        orthoSize, camera, insets
                    );
                    const BoardLayout enclosure = calculateBoardLayout(
                        viewport.width, viewport.height, size.x + 6, size.y + 6,
                        orthoSize, camera, insets
                    );
                    const float pixelsPerWorld = static_cast<float>(viewport.height) / orthoSize;
                    const float cellPixels = layout.cellWorldSize * pixelsPerWorld;
                    const float outerMargin = kFenceOuterOffset * cellPixels;
                    const auto close = [](float a, float b) {
                        return std::abs(a - b) <= kTolerance;
                    };
                    if (!layout.valid() || layout.columns != size.x || layout.rows != size.y ||
                        !close(layout.cellWorldSize, enclosure.cellWorldSize) ||
                        !close(layout.screenRect.x, enclosure.screenRect.x + 3.f * cellPixels) ||
                        !close(layout.screenRect.y, enclosure.screenRect.y + 3.f * cellPixels) ||
                        !close(layout.worldRect.x, enclosure.worldRect.x + 3.f * layout.cellWorldSize) ||
                        !close(layout.worldRect.y, enclosure.worldRect.y + 3.f * layout.cellWorldSize) ||
                        layout.screenRect.x - outerMargin < insets.left - kTolerance ||
                        layout.screenRect.y - outerMargin < insets.top - kTolerance ||
                        layout.screenRect.x + layout.screenRect.width + outerMargin >
                            static_cast<float>(viewport.width) - insets.right + kTolerance ||
                        layout.screenRect.y + layout.screenRect.height + outerMargin >
                            static_cast<float>(viewport.height) - insets.bottom + kTolerance) {
                        std::cerr << "Pasture fence does not fit reserved viewport: "
                                  << viewport.width << 'x' << viewport.height << ", "
                                  << size.x << 'x' << size.y << '\n';
                        return false;
                    }
                    for (int y = 0; y < size.y; ++y) {
                        for (int x = 0; x < size.x; ++x) {
                            const LayoutPoint world = layout.cellCenter({x, y});
                            const LayoutPoint screen{
                                static_cast<float>(viewport.width) * 0.5f +
                                    (world.x - camera.x) * pixelsPerWorld,
                                static_cast<float>(viewport.height) * 0.5f -
                                    (world.y - camera.y) * pixelsPerWorld,
                            };
                            // BoardInputView maps its actual screen frame to
                            // cells and reverses Y; enclosure margins must not
                            // shift a click to a neighboring cell.
                            const int clickedX = static_cast<int>(
                                (screen.x - layout.screenRect.x) /
                                layout.screenRect.width * static_cast<float>(size.x)
                            );
                            const int clickedY = size.y - 1 - static_cast<int>(
                                (screen.y - layout.screenRect.y) /
                                layout.screenRect.height * static_cast<float>(size.y)
                            );
                            if (clickedX != x || clickedY != y ||
                                !close(screen.x, layout.screenRect.x +
                                    (static_cast<float>(x) + 0.5f) * cellPixels) ||
                                !close(screen.y, layout.screenRect.y +
                                    (static_cast<float>(size.y - y) - 0.5f) * cellPixels)) {
                                std::cerr << "Pasture input and rendered cell centers differ\n";
                                return false;
                            }
                        }
                    }
                    ++layoutsChecked;
                }
            }
        }
    }
    return !calculatePastureLayout(0, 812, 4, 6, 9.f).valid() &&
           !calculatePastureLayout(375, 0, 4, 6, 9.f).valid() &&
           !calculatePastureLayout(375, 812, 0, 6, 9.f).valid() &&
           !calculatePastureLayout(375, 812, 4, -1, 9.f).valid() &&
           !calculatePastureLayout(375, 812, 4, 6, 0.f).valid() &&
           !calculatePastureLayout(375, 812, std::numeric_limits<int>::max(), 6, 9.f).valid();
}

} // namespace

int main() {
    std::size_t routesChecked = 0;
    std::size_t layoutsChecked = 0;
    if (!fenceBandChecks() || !invalidInputChecks() || !extendedRunoutChecks() ||
        !allPositionsChecks(routesChecked) ||
        !generatedFlockChecks(routesChecked) || !pastureLayoutChecks(layoutsChecked)) {
        return 1;
    }
    std::cout << "Pen routes passed: " << routesChecked
              << " routes, all headings, animated silhouette/thick-fence clearance, one gate,"
                 " tangent joins and constant distance speed.\n";
    std::cout << "Pasture layouts passed: " << layoutsChecked
              << " viewport/board/camera combinations, full fence bounds and cell input alignment.\n";
    return 0;
}
