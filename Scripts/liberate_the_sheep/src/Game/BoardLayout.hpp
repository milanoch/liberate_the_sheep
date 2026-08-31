#pragma once

#include "Game/BoardModel.hpp"

namespace LiberateTheSheep::Game {

struct LayoutPoint {
    float x{};
    float y{};
};

struct LayoutRect {
    float x{};
    float y{};
    float width{};
    float height{};
};

struct LayoutInsets {
    float top{};
    float right{};
    float bottom{};
    float left{};
};

struct BoardLayout {
    int columns{};
    int rows{};
    int viewportWidth{};
    int viewportHeight{};
    float cameraOrthoSize{};
    LayoutPoint cameraPosition{};

    float cellWorldSize{};
    LayoutRect worldRect{};
    LayoutRect screenRect{};

    [[nodiscard]] bool valid() const noexcept;
    [[nodiscard]] LayoutPoint cellCenter(Cell cell) const noexcept;
    [[nodiscard]] LayoutPoint sheepCenter(const Sheep &sheep) const noexcept;
};

// The camera is expected to be unrotated and orthographic. Panda defines
// orthoSize as the full visible world height.
[[nodiscard]] BoardLayout calculateBoardLayout(
    int viewportWidth,
    int viewportHeight,
    int columns,
    int rows,
    float cameraOrthoSize,
    LayoutPoint cameraPosition = {},
    LayoutInsets reservedScreenInsets = {}
) noexcept;

} // namespace LiberateTheSheep::Game
