#pragma once

#include "Game/BoardLayout.hpp"
#include "Game/PastureWear.hpp"
#include <Bamboo/Bamboo.hpp>
#include <Bamboo/Assets/TextureAPI.hpp>
#include <array>
#include <vector>

namespace LiberateTheSheep::Game {

// The ground and fence Sprites are authored in game.world. Only a single
// opaque ground texture is composed from imported artwork for each layout.
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
    std::uint64_t surfaceSeed_{};
    int surfaceWidth_{};
    int surfaceHeight_{};
    void placeGround(const BoardLayout &board);
    void placeSide(std::size_t side, LayoutPoint from, LayoutPoint to, float cellSize);
};

} // namespace LiberateTheSheep::Game
