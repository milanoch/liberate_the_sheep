#pragma once

#include "Game/BoardLayout.hpp"
#include <Bamboo/Bamboo.hpp>
#include <array>
#include <vector>

namespace LiberateTheSheep::Game {

// Only positions Sprite panels authored in game.world; owns no scene resources.
class PastureScene {
public:
    void resolve();
    void apply(const BoardLayout &board);

private:
    std::array<std::vector<Bamboo::EntityHandle>, 5> fencePanels_;
    Bamboo::EntityHandle background_;
    void placeSide(std::size_t side, LayoutPoint from, LayoutPoint to, float cellSize);
};

} // namespace LiberateTheSheep::Game
