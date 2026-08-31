#include "Game/SheepAppearance.hpp"

#include <algorithm>

namespace LiberateTheSheep::Game {
namespace {

    [[nodiscard]] std::uint64_t nextRandom(std::uint64_t &state) noexcept {
        state += 0x9e3779b97f4a7c15ULL;
        std::uint64_t value = state;
        value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31u);
    }

} // namespace

std::vector<SheepCoat> makeBalancedCoatOrder(
    std::size_t sheepCount,
    std::uint64_t seed
) {
    std::vector<SheepCoat> coats;
    coats.reserve(sheepCount);
    for (std::size_t index = 0; index < sheepCount; ++index) {
        coats.push_back(static_cast<SheepCoat>(index % kSheepCoatCount));
    }

    for (std::size_t remaining = coats.size(); remaining > 1u; --remaining) {
        const std::size_t chosen = static_cast<std::size_t>(
            nextRandom(seed) % static_cast<std::uint64_t>(remaining)
        );
        std::swap(coats[remaining - 1u], coats[chosen]);
    }
    return coats;
}

} // namespace LiberateTheSheep::Game
