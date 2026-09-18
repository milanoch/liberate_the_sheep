#pragma once

#include <algorithm>
#include <numeric>

namespace LiberateTheSheep::Game::SheepRunAnimation {

// Each coat owns four complete rows: standing, 96 running poses, then padding.
inline constexpr int kColumns = 25;
inline constexpr int kRows = 12;
inline constexpr int kCoats = 3;
inline constexpr int kRunPoseCount = 96;
inline constexpr int kCellsPerCoat = 100;
inline constexpr int kPaddingCellsPerCoat = 3;

static_assert(std::gcd(kColumns, kRows) == 1, "Panda atlas indexing requires coprime dimensions");
static_assert(kColumns * kRows == kCoats * kCellsPerCoat);
static_assert(kRunPoseCount + 1 + kPaddingCellsPerCoat == kCellsPerCoat,
              "Reserve standing and transparent cells");
static_assert(kCellsPerCoat % kColumns == 0, "Keep every coat in complete atlas rows");

[[nodiscard]] constexpr int logicalCell(int coat, int pose) noexcept {
    return std::clamp(coat, 0, kCoats - 1) * kCellsPerCoat +
           std::clamp(pose, 0, kRunPoseCount);
}

// Current Panda SDK uses column=index%columns AND row=index%rows. The
// dimensions are coprime, so this index addresses the intended physical cell.
[[nodiscard]] constexpr int encodedCell(int coat, int pose) noexcept {
    const int logical = logicalCell(coat, pose);
    int encoded = logical % kColumns;
    while (encoded % kRows != logical / kColumns) { encoded += kColumns; }
    return encoded;
}

} // namespace LiberateTheSheep::Game::SheepRunAnimation
