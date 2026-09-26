// Offline diagnostic only; no Panda runtime or editor is linked/launched.
// Uses the same opaque ground compositor as the game, then exports scene sprites.
#include "Game/BoardGenerator.hpp"
#include "Game/BoardLayout.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/PastureSurface.hpp"
#include "Game/PastureWear.hpp"
#include "Game/PenEscapePath.hpp"
#include "Game/SheepAppearance.hpp"
#include "Game/SheepRunAnimation.hpp"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace LiberateTheSheep::Game;

namespace {
void writePreview(std::ostream &out, const std::vector<std::uint8_t> &atlas,
                  const std::filesystem::path &directory, int level, int width, int height) {
    const auto *definition = findLevel(level);
    if (!definition) { throw std::runtime_error("Unknown preview level"); }
    auto result = BoardGenerator::generate(definition->generation);
    if (!result.succeeded()) { throw std::runtime_error(result.error); }
    const auto &board = *result.board;
    const auto seed = definition->generation.seed ^ result.metrics.layoutFingerprint;
    const auto layout = calculatePastureLayout(width, height, board.width(), board.height(),
                                               9.f, {}, {154.f, 18.f, 20.f, 18.f});
    PastureWear wear;
    wear.capture(board, seed);
    const float c = layout.cellWorldSize;
    const auto surface = composePastureSurface(atlas, 2048, 1024, layout, wear, seed);
    if (surface.width <= 0 || surface.height <= 0 || surface.rgba.empty()) {
        throw std::runtime_error("Ground compositor returned an empty preview");
    }
    const std::string groundFile = "ground-level-" + std::to_string(level) + "-" +
                                  std::to_string(width) + "x" + std::to_string(height) + ".rgba";
    std::ofstream ground(directory / groundFile, std::ios::binary);
    ground.write(reinterpret_cast<const char *>(surface.rgba.data()),
                 static_cast<std::streamsize>(surface.rgba.size()));
    if (!ground) { throw std::runtime_error("Cannot save ground RGBA preview"); }
    out << "{\"level\":" << level << ",\"width\":" << width << ",\"height\":" << height
        << ",\"ortho\":9,\"cellSize\":" << c << ",\"boardWidth\":" << board.width()
        << ",\"boardHeight\":" << board.height()
        << ",\"groundFile\":\"" << groundFile << "\",\"groundWidth\":" << surface.width
        << ",\"groundHeight\":" << surface.height << ",\"sprites\":[";
    bool first = true;
    auto sprite = [&](const char *kind, float x, float y, float w, float h,
                      float rotation, float opacity, int index = 0) {
        if (!first) { out << ','; }
        first = false;
        out << "{\"kind\":\"" << kind << "\",\"x\":" << x << ",\"y\":" << y
            << ",\"width\":" << w << ",\"height\":" << h << ",\"rotation\":" << rotation
            << ",\"opacity\":" << opacity << ",\"index\":" << index << '}';
    };
    const float left = layout.worldRect.x - PenEscape::kFenceOffset * c;
    const float right = layout.worldRect.x + layout.worldRect.width + PenEscape::kFenceOffset * c;
    const float bottom = layout.worldRect.y - PenEscape::kFenceOffset * c;
    const float top = layout.worldRect.y + layout.worldRect.height + PenEscape::kFenceOffset * c;
    const float middle = layout.worldRect.x + layout.worldRect.width * .5f;
    const float halfGate = PenEscape::kGateWidth * c * .5f;
    auto fence = [&](float ax, float ay, float bx, float by) {
        const float dx = bx - ax;
        const float dy = by - ay;
        const float length = std::hypot(dx, dy);
        const int count = std::max(1, static_cast<int>(std::ceil(length / (2.4f * c))));
        const float panelWidth = (length / count) / .94f;
        for (int i = 0; i < count; ++i) {
            const float t = (i + .5f) / count;
            sprite("fence", ax + dx * t, ay + dy * t, panelWidth, panelWidth / 7.f,
                   std::atan2(dy, dx) * 57.295779513f, 1.f);
        }
    };
    fence(left, top, right, top);
    fence(left, bottom, left, top);
    fence(right, bottom, right, top);
    fence(left, bottom, middle - halfGate, bottom);
    fence(middle + halfGate, bottom, right, bottom);
    const auto coats = makeBalancedCoatOrder(board.sheepCount(), seed);
    std::size_t index = 0;
    for (const auto &sheep : board.sheep()) {
        const auto center = layout.sheepCenter(sheep);
        const auto angle = sheep.direction == Direction::Right ? -90.f :
                           sheep.direction == Direction::Down ? 180.f :
                           sheep.direction == Direction::Left ? 90.f : 0.f;
        sprite("sheep", center.x, center.y, 1.23f * c, 1.97f * c, angle, 1.f,
               SheepRunAnimation::encodedCell(static_cast<int>(coats[index++]), 0));
    }
    out << "],\"sheepCount\":" << board.sheepCount() << '}';
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc != 3) {
            throw std::runtime_error("Usage: ExportPasturePreview ATLAS.rgba OUTPUT_DIRECTORY");
        }
        std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
        constexpr std::size_t expectedBytes = 2048u * 1024u * 4u;
        if (!input || input.tellg() != static_cast<std::streamoff>(expectedBytes)) {
            throw std::runtime_error("Expected a 2048x1024 tightly packed RGBA8 atlas");
        }
        input.seekg(0);
        std::vector<std::uint8_t> atlas(expectedBytes);
        input.read(reinterpret_cast<char *>(atlas.data()), static_cast<std::streamsize>(atlas.size()));
        if (!input) { throw std::runtime_error("Cannot read atlas pixels"); }
        const std::filesystem::path directory(argv[2]);
        std::filesystem::create_directories(directory);
        std::ofstream out(directory / "preview.json");
        if (!out) { throw std::runtime_error("Cannot open output JSON"); }
        out << std::setprecision(9) << "[";
        writePreview(out, atlas, directory, 1, 1200, 800); out << ',';
        writePreview(out, atlas, directory, 6, 1200, 800); out << ',';
        writePreview(out, atlas, directory, 9, 1200, 800); out << ',';
        writePreview(out, atlas, directory, 9, 540, 960); out << "]\n";
        std::cout << (directory / "preview.json").string() << '\n';
        return out ? 0 : 1;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
