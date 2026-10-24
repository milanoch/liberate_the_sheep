#pragma once

#include "Game/BoardLayout.hpp"
#include "Game/PastureWear.hpp"
#include "Game/PastureFenceComposite.hpp"
#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Assets/TextureAPI.hpp>
#include <array>
#include <vector>

namespace LiberateTheSheep::Game {

// The ground and fence Sprites are authored in game.world. Only a single
// opaque ground texture is composed from imported artwork for each layout.
// During play it also carries the static fence, avoiding broken Sprite alpha
// blending. Authored fence Sprites remain editable and are restored on shutdown.
class PastureScene {
public:
    void resolve();
    void shutdown();
    void captureFlock(const BoardModel &board, std::uint64_t seed);
    void apply(const BoardLayout &board);

private:
    std::array<std::vector<Bamboo::EntityHandle>, 5> fencePanels_;
    PastureWear wear_;
    Bamboo::EntityHandle background_;
    Bamboo::TextureHandle sourceTexture_;
    Bamboo::TextureHandle surfaceTexture_;
    Bamboo::TextureAPI::TexturePixelsRGBA8 sourcePixels_;
    Bamboo::TextureHandle fenceSourceTexture_;
    Bamboo::TextureAPI::TexturePixelsRGBA8 fencePixels_;
    struct FenceAppearance {
        Bamboo::EntityHandle entity;
        Bamboo::Color color;
    };
    std::vector<FenceAppearance> authoredFenceColors_;
    std::vector<Bamboo::EntityHandle> activeFencePanels_;
    std::vector<PastureFencePanel> fenceFootprints_;
    bool canCompositeFence_{};
    std::uint64_t surfaceSeed_{};
    int surfaceWidth_{};
    int surfaceHeight_{};
    void placeGround(const BoardLayout &board);
    void placeSide(std::size_t side, LayoutPoint from, LayoutPoint to, float cellSize);
};

} // namespace LiberateTheSheep::Game
