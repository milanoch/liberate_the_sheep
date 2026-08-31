#include "Game/BoardLayout.hpp"

#include <algorithm>

namespace LiberateTheSheep::Game {
namespace {

    constexpr float kMaximumBoardWidth = 10.4f;
    constexpr float kMaximumBoardHeight = 13.2f;
} // namespace

bool BoardLayout::valid() const noexcept {
    return columns > 0 && rows > 0 && viewportWidth > 0 && viewportHeight > 0 &&
           cameraOrthoSize > 0.f && cellWorldSize > 0.f && screenRect.width > 0.f &&
           screenRect.height > 0.f;
}

LayoutPoint BoardLayout::cellCenter(Cell cell) const noexcept {
    return {
        worldRect.x + (static_cast<float>(cell.x) + 0.5f) * cellWorldSize,
        worldRect.y + (static_cast<float>(cell.y) + 0.5f) * cellWorldSize,
    };
}

LayoutPoint BoardLayout::sheepCenter(const Sheep &sheep) const noexcept {
    const LayoutPoint tail = cellCenter(sheep.tail);
    const LayoutPoint head = cellCenter(sheep.head());
    return {(tail.x + head.x) * 0.5f, (tail.y + head.y) * 0.5f};
}

BoardLayout calculateBoardLayout(
    int viewportWidth,
    int viewportHeight,
    int columns,
    int rows,
    float cameraOrthoSize,
    LayoutPoint cameraPosition,
    LayoutInsets reservedScreenInsets
) noexcept {
    BoardLayout layout;
    layout.columns = columns;
    layout.rows = rows;
    layout.viewportWidth = viewportWidth;
    layout.viewportHeight = viewportHeight;
    layout.cameraOrthoSize = cameraOrthoSize;
    layout.cameraPosition = cameraPosition;

    if (viewportWidth <= 0 || viewportHeight <= 0 || columns <= 0 || rows <= 0 ||
        cameraOrthoSize <= 0.f) {
        return layout;
    }

    const float widthPixels = static_cast<float>(viewportWidth);
    const float heightPixels = static_cast<float>(viewportHeight);
    // Panda's orthographic camera stores the full visible height in orthoSize.
    const float pixelsPerWorldUnit = heightPixels / cameraOrthoSize;
    const float visibleHalfHeight = cameraOrthoSize * 0.5f;
    const float visibleHalfWidth =
        visibleHalfHeight * widthPixels / heightPixels;
    const float leftInset = std::clamp(reservedScreenInsets.left, 0.f, widthPixels);
    const float rightInset = std::clamp(reservedScreenInsets.right, 0.f, widthPixels);
    const float topInset = std::clamp(reservedScreenInsets.top, 0.f, heightPixels);
    const float bottomInset = std::clamp(reservedScreenInsets.bottom, 0.f, heightPixels);
    const float availablePixelWidth = std::max(1.f, widthPixels - leftInset - rightInset);
    const float availablePixelHeight = std::max(1.f, heightPixels - topInset - bottomInset);

    const float maximumCellPixels = std::min(
        kMaximumBoardWidth * pixelsPerWorldUnit / static_cast<float>(columns),
        kMaximumBoardHeight * pixelsPerWorldUnit / static_cast<float>(rows)
    );
    const float cellPixels = std::max(
        0.01f,
        std::min({
            availablePixelWidth / static_cast<float>(columns),
            availablePixelHeight / static_cast<float>(rows),
            maximumCellPixels,
        })
    );

    layout.cellWorldSize = cellPixels / pixelsPerWorldUnit;
    layout.screenRect.width = cellPixels * static_cast<float>(columns);
    layout.screenRect.height = cellPixels * static_cast<float>(rows);
    layout.screenRect.x =
        leftInset + (availablePixelWidth - layout.screenRect.width) * 0.5f;
    layout.screenRect.y =
        topInset + (availablePixelHeight - layout.screenRect.height) * 0.5f;

    layout.worldRect.width = layout.cellWorldSize * static_cast<float>(columns);
    layout.worldRect.height = layout.cellWorldSize * static_cast<float>(rows);
    layout.worldRect.x = cameraPosition.x +
                         (layout.screenRect.x / widthPixels * 2.f - 1.f) * visibleHalfWidth;
    const float boardWorldTop = cameraPosition.y +
                                (1.f - layout.screenRect.y / heightPixels * 2.f) *
                                    visibleHalfHeight;
    layout.worldRect.y = boardWorldTop - layout.worldRect.height;

    return layout;
}

} // namespace LiberateTheSheep::Game
