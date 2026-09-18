#include "Game/PastureScene.hpp"
#include "Game/PenEscapePath.hpp"

#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <algorithm>
#include <cmath>

namespace LiberateTheSheep::Game {

void PastureScene::resolve() {
    background_ = Bamboo::WorldAPI::findByTag("GameBackground");
    constexpr std::array<const char *, 5> tags{
        "PenFenceTop", "PenFenceLeft", "PenFenceRight",
        "PenFenceBottomLeft", "PenFenceBottomRight"
    };
    for (std::size_t side = 0; side < tags.size(); ++side) {
        fencePanels_[side] = Bamboo::WorldAPI::findAllByTag(tags[side]);
        // SDK's generic Bamboo::swap conflicts with std::sort's ADL swap for
        // EntityHandle. This tiny authored pool only needs insertion sort.
        auto &panels = fencePanels_[side];
        for (std::size_t i = 1; i < panels.size(); ++i) {
            const auto value = panels[i];
            std::size_t j = i;
            while (j > 0 && panels[j - 1].id > value.id) {
                panels[j] = panels[j - 1];
                --j;
            }
            panels[j] = value;
        }
        if (fencePanels_[side].empty()) {
            LOG_WARN("PastureScene: missing authored fence panels %s", tags[side]);
        }
    }
}

void PastureScene::placeSide(std::size_t side, LayoutPoint from, LayoutPoint to, float cellSize) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    const int required = std::max(1, static_cast<int>(std::ceil(length / (2.4f * cellSize))));
    auto &panels = fencePanels_[side];
    const int count = std::min(required, static_cast<int>(panels.size()));
    if (count == 0) { return; }
    if (count < required) {
        LOG_WARN("PastureScene: fence pool is too small for this board");
    }
    const float step = length / static_cast<float>(count);
    for (std::size_t index = 0; index < panels.size(); ++index) {
        auto &panel = panels[index];
        if (!panel.isValid()) { continue; }
        const bool active = static_cast<int>(index) < count;
        Bamboo::SpriteRendererComponentAPI::setColor(panel, {1.f, 1.f, 1.f, active ? 1.f : 0.f});
        if (!active) { continue; }
        const float t = (static_cast<float>(index) + 0.5f) / static_cast<float>(count);
        Bamboo::TransformComponentAPI::setPosition(panel, {from.x + dx * t, from.y + dy * t, 0.15f});
        // Shared end posts overlap, so the enclosure has no unintended gaps.
        const float panelWidth = step / 0.94f;
        Bamboo::TransformComponentAPI::setScale(panel, {panelWidth, panelWidth / 7.f, 1.f});
        Bamboo::TransformComponentAPI::setRotationEuler(panel, {0.f, 0.f, std::atan2(dy, dx) * 57.295779513f});
    }
}

void PastureScene::apply(const BoardLayout &board) {
    if (!board.valid()) { return; }
    if (background_.isValid()) {
        const float width = board.cameraOrthoSize * board.viewportWidth / board.viewportHeight;
        const float cover = std::max(width, board.cameraOrthoSize) + 0.1f;
        Bamboo::TransformComponentAPI::setPosition(background_, {board.cameraPosition.x, board.cameraPosition.y, -2.f});
        Bamboo::TransformComponentAPI::setScale(background_, {cover, cover, 1.f});
    }
    const float c = board.cellWorldSize;
    const float left = board.worldRect.x - PenEscape::kFenceOffset * c;
    const float right = board.worldRect.x + board.worldRect.width + PenEscape::kFenceOffset * c;
    const float bottom = board.worldRect.y - PenEscape::kFenceOffset * c;
    const float top = board.worldRect.y + board.worldRect.height + PenEscape::kFenceOffset * c;
    const float middle = board.worldRect.x + board.worldRect.width * 0.5f;
    const float halfGate = PenEscape::kGateWidth * c * 0.5f;
    placeSide(0, {left, top}, {right, top}, c);
    placeSide(1, {left, bottom}, {left, top}, c);
    placeSide(2, {right, bottom}, {right, top}, c);
    placeSide(3, {left, bottom}, {middle - halfGate, bottom}, c);
    placeSide(4, {middle + halfGate, bottom}, {right, bottom}, c);
}

} // namespace LiberateTheSheep::Game
