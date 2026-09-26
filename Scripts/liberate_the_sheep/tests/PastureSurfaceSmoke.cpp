#include "Game/PastureSurface.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

using namespace LiberateTheSheep::Game;

bool expect(bool value, std::string_view message) {
    if (!value) {
        std::cerr << message << '\n';
    }
    return value;
}

std::vector<std::uint8_t> atlas(bool gradient = false, int side = 32) {
    std::vector<std::uint8_t> result(static_cast<std::size_t>(side) * side * 8);
    for (int y = 0; y < side; ++y) {
        for (int x = 0; x < 2 * side; ++x) {
            const auto offset = (static_cast<std::size_t>(y) * 2 * side + x) * 4;
            const bool soil = x >= side;
            result[offset] = gradient ? static_cast<std::uint8_t>(20 + 210 * (x % side) / (side - 1)) : (soil ? 182 : 76);
            result[offset + 1] = gradient ? static_cast<std::uint8_t>(25 + 200 * y / (side - 1)) : (soil ? 119 : 163);
            result[offset + 2] = soil ? 73 : 55;
            result[offset + 3] = 255;
        }
    }
    return result;
}

BoardLayout layout(int width = 240, int height = 360) {
    BoardLayout result;
    result.columns = 12;
    result.rows = 18;
    result.viewportWidth = width;
    result.viewportHeight = height;
    result.cameraOrthoSize = 18.f;
    result.cameraPosition = {6.f, 9.f};
    result.cellWorldSize = 1.f;
    result.worldRect = {0.f, 0.f, 12.f, 18.f};
    result.screenRect = {0.f, 0.f, static_cast<float>(width), static_cast<float>(height)};
    return result;
}

BoardModel flock() {
    BoardModel board(12, 18);
    SheepId id = 1;
    for (int y = 2; y < 16; ++y) {
        for (int x = 0; x < 6; x += 2) {
            if (!board.addSheep({id++, {x, y}, Direction::Right})) {
                std::abort();
            }
        }
    }
    return board;
}

std::array<int, 4> pixel(const PastureSurface &surface, int x, int y) {
    const auto offset = (static_cast<std::size_t>(y) * surface.width + x) * 4;
    return {surface.rgba[offset], surface.rgba[offset + 1],
            surface.rgba[offset + 2], surface.rgba[offset + 3]};
}

bool densityAndSnapshot() {
    auto board = flock();
    PastureWear wear;
    wear.capture(board, 123);
    const auto source = atlas();
    const auto first = composePastureSurface(source, 64, 32, layout(), wear, 123);
    if (!expect(first.valid(), "Valid composition rejected.")) {
        return false;
    }
    for (std::size_t i = 3; i < first.rgba.size(); i += 4) {
        if (!expect(first.rgba[i] == 255, "Surface contains transparent alpha.")) {
            return false;
        }
    }
    const auto crowded = pixel(first, 60, 180);
    const auto clear = pixel(first, 210, 180);
    if (!expect(crowded[0] > crowded[1] && crowded[0] > clear[0] + 60 &&
                clear[1] > clear[0] + 60 && crowded[2] > 50,
                "Crowded area must be warm earth; clear area must stay green.")) {
        return false;
    }
    // No discrete patch edges: the density falloff is smooth across every pixel
    // of a horizontal section through the flock boundary.
    for (int x = 1; x < first.width; ++x) {
        const auto a = pixel(first, x - 1, 180);
        const auto b = pixel(first, x, 180);
        if (!expect(std::abs(a[0] - b[0]) <= 4 && std::abs(a[1] - b[1]) <= 4,
                    "Wear contains an abrupt patch boundary.")) {
            return false;
        }
    }
    const auto same = composePastureSurface(source, 64, 32, layout(), wear, 123);
    const auto changedSeed = composePastureSurface(source, 64, 32, layout(), wear, 124);
    if (!expect(first.rgba == same.rgba && first.rgba != changedSeed.rgba,
                "Composition is not seed-deterministic.")) {
        return false;
    }
    while (!board.empty()) {
        if (!board.eraseSheep(board.sheep().back().id)) {
            return false;
        }
    }
    const auto departed = composePastureSurface(source, 64, 32, layout(), wear, 123);
    if (!expect(departed.rgba == first.rgba, "Grass regrew when sheep departed.")) {
        return false;
    }
    wear.capture(board, 123);
    const auto empty = composePastureSurface(source, 64, 32, layout(), wear, 123);
    return expect(pixel(empty, 60, 180) == clear, "Empty snapshot should have no bare ground.");
}

bool seamlessAndAnchored() {
    PastureWear empty;
    const auto source = atlas(true);
    const auto baseLayout = layout(384, 576);
    const auto base = composePastureSurface(source, 64, 32, baseLayout, empty, 0);
    // The deliberately non-tileable source has opposite-edge RGB differences
    // of 200+. They must never survive as a seam at six-cell/half-period joins.
    for (int y = 1; y < base.height; ++y) {
        for (int x = 1; x < base.width; ++x) {
            const auto value = pixel(base, x, y);
            const auto left = pixel(base, x - 1, y);
            const auto above = pixel(base, x, y - 1);
            for (std::size_t c = 0; c < 3; ++c) {
                if (!expect(std::abs(value[c] - left[c]) <= 5 && std::abs(value[c] - above[c]) <= 5,
                            "Source texture edge produced a seam.")) {
                    return false;
                }
            }
        }
    }
    auto movedLayout = baseLayout;
    movedLayout.cameraPosition.x += 2.f;
    const auto moved = composePastureSurface(source, 64, 32, movedLayout, empty, 0);
    for (int y = 0; y < base.height; y += 17) {
        for (int x = 0; x < base.width - 64; x += 7) {
            if (!expect(pixel(base, x + 64, y) == pixel(moved, x, y),
                        "Grass texture swims when camera moves.")) {
                return false;
            }
        }
    }
    PastureWear wear;
    wear.capture(flock(), 67);
    const auto small = composePastureSurface(source, 64, 32, layout(201, 301), wear, 67);
    const auto large = composePastureSurface(source, 64, 32, layout(603, 903), wear, 67);
    for (int y = 0; y < small.height; y += 13) {
        for (int x = 0; x < small.width; x += 11) {
            if (!expect(pixel(small, x, y) == pixel(large, x * 3 + 1, y * 3 + 1),
                        "Grass detail or wear changes physical scale on resize.")) {
                return false;
            }
        }
    }
    auto translated = layout(201, 301);
    translated.cameraPosition.x += 100.f;
    translated.cameraPosition.y -= 80.f;
    translated.worldRect.x += 100.f;
    translated.worldRect.y -= 80.f;
    const auto translatedSurface = composePastureSurface(source, 64, 32, translated, wear, 67);
    return expect(small.rgba == translatedSurface.rgba, "Board-cell origin mapping is inconsistent.");
}

bool validationAndBounds() {
    PastureWear empty;
    auto source = atlas();
    const auto goodLayout = layout();
    for (const auto shape : {std::array{0, 0}, std::array{32, 32}, std::array{63, 32},
                             std::array{-64, -32}, std::array{1000000000, 500000000}}) {
        if (!expect(!composePastureSurface(source, shape[0], shape[1], goodLayout, empty, 0).valid(),
                    "Malformed atlas shape accepted.")) {
            return false;
        }
    }
    source.back() = 254;
    if (!expect(!composePastureSurface(source, 64, 32, goodLayout, empty, 0).valid(),
                "Transparent source accepted; black premultiplication risk.")) {
        return false;
    }
    source.back() = 255;
    source.pop_back();
    if (!expect(!composePastureSurface(source, 64, 32, goodLayout, empty, 0).valid(),
                "Undersized byte span accepted.")) {
        return false;
    }
    source.push_back(255);
    for (const float invalid : {0.f, -1.f, std::numeric_limits<float>::infinity(),
                                std::numeric_limits<float>::quiet_NaN()}) {
        auto bad = goodLayout;
        bad.cellWorldSize = invalid;
        if (!expect(!composePastureSurface(source, 64, 32, bad, empty, 0).valid(),
                    "Invalid cell scale accepted.")) {
            return false;
        }
    }
    auto invalidCoordinate = goodLayout;
    invalidCoordinate.cameraPosition.x = std::numeric_limits<float>::max();
    if (!expect(!composePastureSurface(source, 64, 32, invalidCoordinate, empty, 0).valid(),
                "Overflowing coordinate accepted.")) {
        return false;
    }
    const auto capped = composePastureSurface(source, 64, 32, layout(12000, 12000), empty, 0);
    if (!expect(capped.valid() && capped.width <= kPastureSurfaceMaxDimension &&
                capped.height <= kPastureSurfaceMaxDimension &&
                static_cast<std::size_t>(capped.width) * capped.height <= kPastureSurfaceMaxPixels &&
                capped.width == capped.height, "Output allocation was not capped/aspect-preserving.")) {
        return false;
    }
    const auto wide = composePastureSurface(source, 64, 32, layout(12000, 100), empty, 0);
    return expect(wide.valid() && wide.width == 2048 && wide.height >= 1,
                  "Extreme viewport aspect failed.");
}

} // namespace

int main() {
    if (!densityAndSnapshot() || !seamlessAndAnchored() || !validationAndBounds()) {
        return 1;
    }
    std::cout << "Opaque pasture composition: density, warm earth, persistence, seams, "
                 "physical scale, seed determinism and allocation bounds passed.\n";
    return 0;
}
