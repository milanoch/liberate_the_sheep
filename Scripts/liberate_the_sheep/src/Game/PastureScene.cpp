#include "Game/PastureScene.hpp"
#include "Game/PastureSurface.hpp"
#include "Game/PastureColorSpace.hpp"
#include <Bamboo/Assets/AssetManagerAPI.hpp>
#include "Game/PenEscapePath.hpp"

#include <Bamboo/Components/SpriteRendererComponentAPI.hpp>
#include <Bamboo/Components/TransformComponentAPI.hpp>
#include <Bamboo/Logger.hpp>
#include <Bamboo/WorldAPI.hpp>
#include <algorithm>
#include <cmath>

namespace LiberateTheSheep::Game {
namespace {

constexpr float kFencePanelLengthCells = 3.f;
constexpr float kFenceThicknessCells = 0.70f;
constexpr float kFencePostSpan = 0.94f;
constexpr float kDegreesToRadians = 0.01745329252f;

float fencePostDistance(std::size_t side, int index, int count, float length) {
    if (index == 0) { return 0.f; }
    if (index == count) { return length; }
    const float step = length / static_cast<float>(count);
    const float post = static_cast<float>(index);
    const float phase = 0.6f + static_cast<float>(side) * 1.73f;
    return post * step + 0.08f * step *
        (0.62f * std::sin(post * 1.13f + phase) +
         0.38f * std::sin(post * 0.47f - phase * 0.7f));
}

float fencePostOffset(std::size_t side, int index, int count,
                      float length, float cellSize) {
    if (index == 0 || index == count || count < 2) {
        return 0.f;
    }
    // A few unequal, deliberately placed joints, not an alternating zigzag:
    // signed heights mix inward/outward runs; both gate posts stay pinned.
    constexpr std::array<float, 5> positions{0.60f, 0.38f, 0.24f, 0.58f, 0.43f};
    constexpr std::array<float, 5> offsets{-0.16f, 0.12f, -0.12f, 0.10f, -0.10f};
    constexpr std::array<float, 5> angleBudgets{4.0f, 3.24f, 3.6f, 3.1f, 2.9f};
    const int joint = std::clamp(static_cast<int>(std::round(
        static_cast<float>(count) * positions[side])), 1, count - 1);
    const float jointDistance = fencePostDistance(side, joint, count, length);
    const float remaining = length - jointDistance;
    const float turnBudget = angleBudgets[side] * kDegreesToRadians;
    const float height = std::copysign(std::min(std::abs(offsets[side]) * cellSize,
        turnBudget * jointDistance * remaining / length), offsets[side]);
    const float along = fencePostDistance(side, index, count, length);
    if (index <= joint) { return height * along / jointDistance; }
    if (side == 2 && count >= 3) {
        // A small outward correction farther up the right side adds a reverse
        // turn, with a long straight run between it and the inward joint.
        const int secondJoint = std::clamp(static_cast<int>(std::round(
            static_cast<float>(count) * 0.73f)), joint + 1, count - 1);
        const float secondDistance = fencePostDistance(side, secondJoint, count, length);
        const float secondHeight = 0.030f * cellSize;
        if (index <= secondJoint) {
            const float t = (along - jointDistance) / (secondDistance - jointDistance);
            return height + (secondHeight - height) * t;
        }
        return secondHeight * (length - along) / (length - secondDistance);
    }
    // Actual-distance interpolation keeps all other posts exactly collinear.
    return height * (length - along) / remaining;
}

// One shared post per joint: the rails meet, rather than rotating independently.
// All bends are angular at posts, never a curved deformation of the timber.
LayoutPoint fenceAnchor(std::size_t side, int index, int count,
                        LayoutPoint from, LayoutPoint to, float cellSize) {
    if (index == 0) { return from; }
    if (index == count) { return to; }
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    const float along = fencePostDistance(side, index, count, length);
    const float offset = fencePostOffset(side, index, count, length, cellSize);
    constexpr std::array<LayoutPoint, 5> outward{{
        {0.f, 1.f}, {-1.f, 0.f}, {1.f, 0.f}, {0.f, -1.f}, {0.f, -1.f}
    }};
    return {
        from.x + dx / length * along + outward[side].x * offset,
        from.y + dy / length * along + outward[side].y * offset
    };
}

// Stable ordering keeps authored fence placement deterministic.
void sortPool(std::vector<Bamboo::EntityHandle> &pool) {
    // SDK's generic Bamboo::swap conflicts with std::sort's ADL swap.
    for (std::size_t i = 1; i < pool.size(); ++i) {
        const auto value = pool[i];
        std::size_t j = i;
        while (j > 0 && pool[j - 1].id > value.id) {
            pool[j] = pool[j - 1];
            --j;
        }
        pool[j] = value;
    }
}

} // namespace

void PastureScene::resolve() {
    shutdown();
    background_ = Bamboo::WorldAPI::findByTag("GameBackground");
    if (background_.isValid()) {
        sourceTexture_ = Bamboo::SpriteRendererComponentAPI::getTexture(background_);
        if (sourceTexture_.isValid()) {
            // Read the imported atlas before assigning any runtime texture.
            sourcePixels_ = Bamboo::TextureAPI::readPixelsRGBA8(sourceTexture_);
        }
    }
    if (!sourcePixels_) {
        LOG_WARN("PastureScene: cannot read imported ground artwork; reload assets");
    }
    constexpr std::array<const char *, 5> tags{
        "PenFenceTop", "PenFenceLeft", "PenFenceRight",
        "PenFenceBottomLeft", "PenFenceBottomRight"
    };
    for (std::size_t side = 0; side < tags.size(); ++side) {
        fencePanels_[side] = Bamboo::WorldAPI::findAllByTag(tags[side]);
        sortPool(fencePanels_[side]);
        if (fencePanels_[side].empty()) {
            LOG_WARN("PastureScene: missing authored fence panels %s", tags[side]);
        }
        for (auto panel : fencePanels_[side]) {
            if (!panel.isValid()) { continue; }
            authoredFenceColors_.push_back({panel,
                Bamboo::SpriteRendererComponentAPI::getColor(panel)});
            if (!fenceSourceTexture_.isValid()) {
                fenceSourceTexture_ = Bamboo::SpriteRendererComponentAPI::getTexture(panel);
            }
        }
    }
    if (fenceSourceTexture_.isValid()) {
        fencePixels_ = Bamboo::TextureAPI::readPixelsRGBA8(fenceSourceTexture_);
        const auto count = static_cast<std::uint64_t>(fencePixels_.width) * fencePixels_.height;
        if (count == 0 || count > 8u * 1024u * 1024u ||
            fencePixels_.pixels.size() != count * 4u) {
            fencePixels_ = {};
        } else {
            // Cache the imported straight-alpha wood in the same linear colour
            // space as the runtime background. Coverage must stay unchanged.
            linearizePastureForUpload(fencePixels_.pixels);
        }
    }
}

void PastureScene::shutdown() {
    for (auto &original : authoredFenceColors_) {
        if (original.entity.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setColor(original.entity, original.color);
        }
    }
    authoredFenceColors_.clear();
    activeFencePanels_.clear();
    fenceFootprints_.clear();
    fencePixels_ = {};
    fenceSourceTexture_ = {};
    canCompositeFence_ = false;
    if (surfaceTexture_.isValid()) {
        if (background_.isValid() && sourceTexture_.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setTexture(background_, sourceTexture_);
            Bamboo::SpriteRendererComponentAPI::setCell(background_, 2, 1, 0);
        }
        Bamboo::AssetManagerAPI::deleteTexture(surfaceTexture_);
    }
    surfaceTexture_ = {};
    sourceTexture_ = {};
    sourcePixels_ = {};
    background_ = {};
    surfaceWidth_ = 0;
    surfaceHeight_ = 0;
}

void PastureScene::captureFlock(const BoardModel &board, std::uint64_t seed) {
    wear_.capture(board, seed);
    surfaceSeed_ = seed;
}

void PastureScene::placeGround(const BoardLayout &board) {
    if (!background_.isValid()) { return; }
    const float width = board.cameraOrthoSize * board.viewportWidth / board.viewportHeight;
    Bamboo::TransformComponentAPI::setPosition(background_,
        {board.cameraPosition.x, board.cameraPosition.y, -2.f});
    Bamboo::TransformComponentAPI::setScale(background_, {width, board.cameraOrthoSize, 1.f});
    Bamboo::SpriteRendererComponentAPI::setColor(background_, {1.f, 1.f, 1.f, 1.f});

    auto surface = composePastureSurface(sourcePixels_.pixels,
        static_cast<int>(sourcePixels_.width), static_cast<int>(sourcePixels_.height),
        board, wear_, surfaceSeed_);
    if (!surface.valid()) {
        // Never replace a valid image by an empty upload or display the soil
        // half of the source atlas as the entire background.
        if (sourceTexture_.isValid()) {
            Bamboo::SpriteRendererComponentAPI::setTexture(background_, sourceTexture_);
            Bamboo::SpriteRendererComponentAPI::setCell(background_, 2, 1, 0);
        }
        LOG_WARN("PastureScene: opaque ground composition failed; reload the new grass asset");
        return;
    }

    Bamboo::TextureHandle next = surfaceTexture_;
    const bool replace = !next.isValid() || surfaceWidth_ != surface.width ||
                         surfaceHeight_ != surface.height;
    if (replace) {
        next = Bamboo::AssetManagerAPI::createTextureRGBA8(
            static_cast<std::uint32_t>(surface.width), static_cast<std::uint32_t>(surface.height));
        if (!next.isValid()) {
            if (sourceTexture_.isValid()) {
                Bamboo::SpriteRendererComponentAPI::setTexture(background_, sourceTexture_);
                Bamboo::SpriteRendererComponentAPI::setCell(background_, 2, 1, 0);
            }
            LOG_WARN("PastureScene: cannot allocate opaque ground texture");
            return;
        }
    }
    // Panda creates linear UNORM runtime textures; imported artwork is sRGB.
    // Keep the compositor/preview in sRGB and decode once for the GPU.
    linearizePastureForUpload(surface.rgba);
    // Source-over the imported fence onto actual grass, not the editor panel.
    // The resulting image stays opaque, including its antialiased wood edges.
    const bool paintedFence = canCompositeFence_ && compositePastureFence(
        surface, fencePixels_.pixels, static_cast<int>(fencePixels_.width),
        static_cast<int>(fencePixels_.height), board, fenceFootprints_);
    if (!paintedFence && !activeFencePanels_.empty()) {
        LOG_WARN("PastureScene: fence composition unavailable; retaining authored Sprites");
    }
    Bamboo::TextureAPI::updateRGBA8(next, surface.rgba.data(),
        static_cast<std::uint32_t>(surface.rgba.size()));
    Bamboo::SpriteRendererComponentAPI::setTexture(background_, next);
    Bamboo::SpriteRendererComponentAPI::setCell(background_, 1, 1, 0);
    if (replace && surfaceTexture_.isValid()) {
        Bamboo::AssetManagerAPI::deleteTexture(surfaceTexture_);
    }
    surfaceTexture_ = next;
    surfaceWidth_ = surface.width;
    surfaceHeight_ = surface.height;
    if (paintedFence) {
        // Hide only after a complete texture has been assigned. On the next
        // layout pass placeSide restores colours before attempting composition.
        for (const auto panel : activeFencePanels_) {
            Bamboo::Color color = Bamboo::SpriteRendererComponentAPI::getColor(panel);
            color.a = 0.f;
            Bamboo::SpriteRendererComponentAPI::setColor(panel, color);
        }
    }
}

void PastureScene::placeSide(std::size_t side, LayoutPoint from, LayoutPoint to, float cellSize) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    // Even a small pen needs one real joint on each half beside the gate.
    // Reuse the authored pool rather than bending a single Sprite.
    const int required = std::max(side >= 3 ? 2 : 1, static_cast<int>(
        std::round(length / (kFencePanelLengthCells * cellSize))));
    auto &panels = fencePanels_[side];
    const int count = std::min(required, static_cast<int>(panels.size()));
    if (count == 0) { return; }
    if (count < required) {
        LOG_WARN("PastureScene: fence pool is too small for this board");
    }
    for (std::size_t index = 0; index < panels.size(); ++index) {
        auto &panel = panels[index];
        if (!panel.isValid()) { continue; }
        const bool active = static_cast<int>(index) < count;
        const float tint = active ? 0.62f + 0.025f * std::sin(
            static_cast<float>(index) * 1.37f + static_cast<float>(side) * 1.91f + 0.4f) : 1.f;
        Bamboo::SpriteRendererComponentAPI::setColor(panel, {tint, tint, tint, active ? 1.f : 0.f});
        if (!active) { continue; }
        const auto start = fenceAnchor(side, static_cast<int>(index), count, from, to, cellSize);
        const auto end = fenceAnchor(side, static_cast<int>(index) + 1, count, from, to, cellSize);
        const float panelDx = end.x - start.x;
        const float panelDy = end.y - start.y;
        // Length follows the shared posts; timber thickness stays in sheep-cell
        // units, including shorter sections beside the gate.
        const float panelWidth = std::hypot(panelDx, panelDy) / kFencePostSpan;
        const float radians = std::atan2(panelDy, panelDx);
        const LayoutPoint center{(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f};
        const float angle = radians / kDegreesToRadians;
        Bamboo::TransformComponentAPI::setPosition(panel, {center.x, center.y, 0.15f});
        Bamboo::TransformComponentAPI::setScale(
            panel, {panelWidth, kFenceThicknessCells * cellSize, 1.f});
        Bamboo::TransformComponentAPI::setRotationEuler(panel,
            {0.f, 0.f, angle});
        activeFencePanels_.push_back(panel);
        fenceFootprints_.push_back({center.x, center.y,
            panelWidth, kFenceThicknessCells * cellSize, angle, tint});
        // This world uses one full-cell texture for all fence Sprites. Do not
        // silently replace a differently authored panel with the cached wood.
        if (Bamboo::SpriteRendererComponentAPI::getTexture(panel).id != fenceSourceTexture_.id) {
            canCompositeFence_ = false;
        }
    }
}

void PastureScene::apply(const BoardLayout &board) {
    if (!board.valid()) { return; }
    activeFencePanels_.clear();
    fenceFootprints_.clear();
    canCompositeFence_ = static_cast<bool>(fencePixels_);
    const float c = board.cellWorldSize;
    const float left = board.worldRect.x - PenEscape::kFenceOffset * c;
    const float right = board.worldRect.x + board.worldRect.width + PenEscape::kFenceOffset * c;
    const float bottom = board.worldRect.y - PenEscape::kFenceOffset * c;
    const float top = board.worldRect.y + board.worldRect.height + PenEscape::kFenceOffset * c;
    const float middle = board.worldRect.x + board.worldRect.width * 0.5f;
    const float halfGate = PenEscape::kGateWidth * c * 0.5f;
    // Small, unequal corner setting errors avoid a perfectly drafted rectangle.
    // Both sides still share the same post; gate anchors and paths stay fixed.
    const LayoutPoint topLeft{left + 0.035f * c, top - 0.015f * c};
    const LayoutPoint topRight{right - 0.045f * c, top + 0.020f * c};
    const LayoutPoint bottomLeft{left - 0.025f * c, bottom + 0.055f * c};
    const LayoutPoint bottomRight{right + 0.015f * c, bottom - 0.035f * c};
    placeSide(0, topLeft, topRight, c);
    placeSide(1, bottomLeft, topLeft, c);
    placeSide(2, bottomRight, topRight, c);
    placeSide(3, bottomLeft, {middle - halfGate, bottom}, c);
    placeSide(4, {middle + halfGate, bottom}, bottomRight, c);
    placeGround(board);
}

} // namespace LiberateTheSheep::Game
