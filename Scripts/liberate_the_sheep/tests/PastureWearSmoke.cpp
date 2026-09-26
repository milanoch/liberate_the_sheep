#include "Game/PastureWear.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>

namespace {

using namespace LiberateTheSheep::Game;

bool fillColumns(BoardModel &board, int from, int to, SheepId &nextId) {
    for (int y = 0; y < board.height(); ++y) {
        for (int x = from; x + 1 < to; x += 2) {
            if (!board.addSheep({nextId++, {x, y}, Direction::Right})) {
                return false;
            }
        }
    }
    return true;
}

bool localDensityChecks() {
    BoardModel board(12, 18);
    SheepId id = 1;
    if (!fillColumns(board, 0, 6, id) ||
        !board.addSheep({id, {10, 9}, Direction::Right})) {
        return false;
    }
    PastureWear wear;
    wear.capture(board, 0);
    if (wear.densityAt(3.f, 9.f) < 0.99f ||
        wear.densityAt(11.f, 9.5f) >= 0.34f ||
        wear.densityAt(10.f, 2.f) != 0.f ||
        wear.densityAt(-100.f, -100.f) != 0.f || wear.patches().empty()) {
        return false;
    }
    // Neighboring sample points have a smoothly changing density across the
    // flock boundary, without the discontinuities of per-cell occupancy.
    for (float x = 5.f; x < 8.5f; x += 0.01f) {
        const float before = wear.densityAt(x, 8.f);
        const float after = wear.densityAt(x + 0.01f, 8.f);
        if (after > before + 0.00001f || std::abs(after - before) > 0.01f) {
            return false;
        }
    }
    for (const GrassWearPatch patch : wear.patches()) {
        if (patch.x > 7.5f || wear.densityAt(patch.x, patch.y) <= 0.34f) {
            return false;
        }
    }
    return true;
}

bool monotonicAndSnapshotChecks() {
    BoardModel sparse(12, 18);
    SheepId id = 1;
    if (!fillColumns(sparse, 0, 4, id)) {
        return false;
    }
    BoardModel crowded = sparse;
    if (!fillColumns(crowded, 4, 12, id)) {
        return false;
    }
    PastureWear sparseWear;
    PastureWear crowdedWear;
    sparseWear.capture(sparse, 987654321);
    crowdedWear.capture(crowded, 987654321);
    for (float y = 0.f; y < 18.f; y += 0.31f) {
        for (float x = 0.f; x < 12.f; x += 0.31f) {
            if (crowdedWear.densityAt(x, y) + 0.00001f < sparseWear.densityAt(x, y)) {
                return false;
            }
        }
    }
    for (const auto patch : sparseWear.patches()) {
        const auto sameSite = std::find_if(crowdedWear.patches().begin(),
                                          crowdedWear.patches().end(),
            [patch](GrassWearPatch other) { return other.x == patch.x && other.y == patch.y; });
        if (sameSite == crowdedWear.patches().end() || sameSite->opacity < patch.opacity ||
            sameSite->diameter < patch.diameter || sameSite->variant != patch.variant ||
            sameSite->rotationDegrees != patch.rotationDegrees) {
            return false;
        }
    }
    const auto capturedPatches = crowdedWear.patches();
    const float capturedDensity = crowdedWear.densityAt(5.f, 5.f);
    while (!crowded.empty()) {
        if (!crowded.eraseSheep(crowded.sheep().back().id)) {
            return false;
        }
    }
    if (crowdedWear.patches() != capturedPatches ||
        crowdedWear.densityAt(5.f, 5.f) != capturedDensity) {
        return false;
    }
    crowdedWear.capture(crowded, 987654321);
    return crowdedWear.patches().empty() && crowdedWear.densityAt(5.f, 5.f) == 0.f;
}

bool dimensionsAndDeterminismChecks() {
    for (const Cell size : {Cell{4, 4}, Cell{4, 6}, Cell{12, 18}, Cell{40, 60}}) {
        BoardModel board(size.x, size.y);
        SheepId id = 1;
        if (!fillColumns(board, 0, board.width(), id)) {
            return false;
        }
        for (const std::uint64_t seed : {0ULL, 1ULL, 0xffffffffffffffffULL}) {
            PastureWear wear;
            PastureWear repeated;
            PastureWear different;
            wear.capture(board, seed);
            repeated.capture(board, seed);
            different.capture(board, seed + 1);
            if (wear.patches().empty() || wear.patches().size() > PastureWear::kMaxPatchCount ||
                wear.patches() != repeated.patches() || wear.patches() == different.patches()) {
                return false;
            }
            for (const GrassWearPatch patch : wear.patches()) {
                if (!std::isfinite(patch.x) || !std::isfinite(patch.y) ||
                    !std::isfinite(patch.opacity) || !std::isfinite(patch.diameter) ||
                    !std::isfinite(patch.rotationDegrees) || patch.x < 0.f || patch.x > size.x ||
                    patch.y < 0.f || patch.y > size.y || patch.diameter < 2.f ||
                    patch.diameter > 4.f || patch.opacity < 0.15f || patch.opacity > 0.90f ||
                    patch.rotationDegrees < 0.f || patch.rotationDegrees >= 360.f ||
                    patch.variant < 0 || patch.variant > 2) {
                    return false;
                }
            }
            if (wear.densityAt(std::numeric_limits<float>::infinity(), 0.f) != 0.f ||
                wear.densityAt(0.f, std::numeric_limits<float>::quiet_NaN()) != 0.f ||
                wear.densityAt(std::numeric_limits<float>::max(), 0.f) != 0.f) {
                return false;
            }
        }
    }
    BoardModel isolated(4, 4);
    if (!isolated.addSheep({1, {0, 0}, Direction::Right})) {
        return false;
    }
    PastureWear isolatedWear;
    isolatedWear.capture(isolated, 0);
    if (!isolatedWear.patches().empty() || isolatedWear.densityAt(1.f, 0.5f) >= 0.34f) {
        return false;
    }
    PastureWear notCaptured;
    return notCaptured.patches().empty() && notCaptured.densityAt(0.f, 0.f) == 0.f;
}

} // namespace

int main() {
    if (!localDensityChecks()) {
        std::cerr << "Pasture wear local density checks failed.\n";
        return 1;
    }
    if (!monotonicAndSnapshotChecks()) {
        std::cerr << "Pasture wear monotonicity / snapshot checks failed.\n";
        return 1;
    }
    if (!dimensionsAndDeterminismChecks()) {
        std::cerr << "Pasture wear dimensions / determinism checks failed.\n";
        return 1;
    }
    std::cout << "Pasture wear density, persistence, determinism and bounds passed.\n";
    return 0;
}
