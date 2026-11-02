#pragma once

#include <algorithm>

namespace LiberateTheSheep::Game::ShepherdDogAnimation {

// Registered 256 px cells: two complete canter strides with a calm tail,
// followed by the already accepted pickup poses and airborne silhouette.
inline constexpr int kColumns = 15;
inline constexpr int kRows = 14;
inline constexpr int kFrames = kColumns * kRows;
inline constexpr int kRunFrames = 192;
inline constexpr int kRunStrides = 2;
inline constexpr int kGrabFirst = kRunFrames;
inline constexpr int kGrabFrames = 12;
inline constexpr int kAirborneFrame = kGrabFirst + kGrabFrames;
inline constexpr float kQuadSize = 2.4f;
// Every pose is registered with its muzzle at (128,36) in a 256 px cell.
// Keep the pickup destination and carried accessory tied to the rendered size.
inline constexpr float kMouthOffset = (.5f - 36.f / 256.f) * kQuadSize;

struct Pivot { float x; float y; };

// Normalized image coordinates (Y downward), deliberately independent of the
// moving tail and paws. The muzzle offset follows kQuadSize above.
[[nodiscard]] constexpr Pivot pivot(int) noexcept { return {.5f, .5f}; }

// Panda samples column=index%columns and row=index%rows. Since 15 and 14 are
// coprime, each logical row-major pose has exactly one encoded index.
[[nodiscard]] constexpr int encodedCell(int frame) noexcept {
    const int logical = std::clamp(frame, 0, kAirborneFrame);
    int index = logical % kColumns;
    while (index % kRows != logical / kColumns) { index += kColumns; }
    return index;
}

} // namespace LiberateTheSheep::Game::ShepherdDogAnimation
