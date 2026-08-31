#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace LiberateTheSheep::Game {

enum class SheepCoat : std::uint8_t {
    White = 0,
    Black = 1,
    Brown = 2,
};

inline constexpr std::size_t kSheepCoatCount = 3u;

// Returns a deterministic shuffled order whose coat counts differ by at most
// one. A three-sheep level therefore always contains all three coat colors.
[[nodiscard]] std::vector<SheepCoat> makeBalancedCoatOrder(
    std::size_t sheepCount,
    std::uint64_t seed
);

} // namespace LiberateTheSheep::Game
