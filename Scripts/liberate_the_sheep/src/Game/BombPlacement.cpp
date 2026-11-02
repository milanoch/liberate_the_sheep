#include "Game/BombPlacement.hpp"

#include "Game/EscapeHerd.hpp"
#include "Game/LevelCatalog.hpp"
#include "Game/PenEscapePath.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <vector>

namespace LiberateTheSheep::Game {
namespace {

constexpr std::size_t kNoIndex = std::numeric_limits<std::size_t>::max();

struct DependencyGraph {
    std::vector<std::vector<std::size_t>> blockers;
    std::vector<std::vector<bool>> closure;
    std::vector<int> waves;
    std::vector<int> incoming;
    std::vector<int> requiredMoves;
};

DependencyGraph dependencies(const BoardModel &board) {
    const auto &sheep = board.sheep();
    const auto count = sheep.size();
    DependencyGraph graph{
        std::vector<std::vector<std::size_t>>(count),
        std::vector<std::vector<bool>>(count, std::vector<bool>(count, false)),
        std::vector<int>(count, 0), std::vector<int>(count, 0),
        std::vector<int>(count, 1)
    };
    std::unordered_map<SheepId, std::size_t> indices;
    for (std::size_t index = 0; index < count; ++index) {
        indices.emplace(sheep[index].id, index);
    }

    for (std::size_t index = 0; index < count; ++index) {
        const auto &piece = sheep[index];
        if (!isValidDirection(piece.direction)) { continue; }
        const auto nearest = board.inspectMove(piece.id).blocker;
        if (nearest) {
            if (const auto found = indices.find(*nearest); found != indices.end()) {
                ++graph.incoming[found->second];
            }
        }
        const auto delta = directionDelta(piece.direction);
        auto cell = piece.head();
        cell.x += delta.x;
        cell.y += delta.y;
        for (; board.isInside(cell); cell.x += delta.x, cell.y += delta.y) {
            const auto occupant = board.sheepAt(cell);
            if (!occupant || *occupant == piece.id) { continue; }
            const auto found = indices.find(*occupant);
            if (found == indices.end()) { continue; }
            auto &blockers = graph.blockers[index];
            if (std::find(blockers.begin(), blockers.end(), found->second) == blockers.end()) {
                blockers.push_back(found->second);
            }
        }
    }

    // The constructive generator produces a DAG. Resolve its layers without
    // recursion so a malformed cyclic input cannot overflow the stack.
    for (std::size_t pass = 0; pass < count; ++pass) {
        bool advanced = false;
        for (std::size_t index = 0; index < count; ++index) {
            if (graph.waves[index] != 0) { continue; }
            const auto &blockers = graph.blockers[index];
            if (!std::all_of(blockers.begin(), blockers.end(), [&](std::size_t blocker) {
                    return graph.waves[blocker] != 0;
                })) {
                continue;
            }
            int wave = 1;
            for (const auto blocker : blockers) {
                wave = std::max(wave, graph.waves[blocker] + 1);
                graph.closure[index][blocker] = true;
                for (std::size_t dependency = 0; dependency < count; ++dependency) {
                    if (graph.closure[blocker][dependency]) {
                        graph.closure[index][dependency] = true;
                    }
                }
            }
            graph.waves[index] = wave;
            graph.requiredMoves[index] = 1 + static_cast<int>(std::count(
                graph.closure[index].begin(), graph.closure[index].end(), true));
            advanced = true;
        }
        if (!advanced) { break; }
    }
    return graph;
}

std::uint64_t tieBreaker(SheepId id, std::uint64_t seed) noexcept {
    std::uint64_t value = seed ^ (static_cast<std::uint64_t>(id) * 0x9e3779b97f4a7c15ULL);
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31u);
}

std::size_t chooseTimed(const BoardModel &board, const DependencyGraph &graph,
                        int preferredWave, std::uint64_t seed) {
    const auto &sheep = board.sheep();
    const int maximumWave = *std::max_element(graph.waves.begin(), graph.waves.end());
    const int target = std::clamp(preferredWave, 1, std::max(1, maximumWave));
    std::size_t selected = kNoIndex;
    int bestScore = std::numeric_limits<int>::min();
    std::uint64_t bestTie{};
    for (std::size_t index = 0; index < sheep.size(); ++index) {
        const int wave = graph.waves[index];
        if (wave == 0) { continue; }
        if (wave < target || wave > target + 1) { continue; }
        // Going below the intended depth is more expensive than one extra
        // layer. Within that band, require as many distinct removals as possible.
        const int distance = wave < target ? (target - wave) * 6'000 : (wave - target) * 2'500;
        const auto &piece = sheep[index];
        const auto head = piece.head();
        const int inset = std::min({piece.tail.x, head.x, piece.tail.y, head.y,
            board.width() - piece.tail.x - 1, board.width() - head.x - 1,
            board.height() - piece.tail.y - 1, board.height() - head.y - 1});
        const int score = graph.requiredMoves[index] * 120 + inset * 5 - distance;
        const auto tie = tieBreaker(piece.id, seed);
        if (selected == kNoIndex || score > bestScore || (score == bestScore && tie > bestTie)) {
            selected = index;
            bestScore = score;
            bestTie = tie;
        }
    }
    return selected;
}

float timedDeadline(const BoardModel &board, const Sheep &sheep, int moves,
                    float configuredSeconds) {
    // Geometry is calculated for the future clear lane: the dependency closure
    // accounts for every sheep which needs to be removed before this launch.
    const auto path = PenEscape::makePath(board.width(), board.height(), sheep);
    if (!path.valid()) { return configuredSeconds; }
    constexpr float tailClearY = -PenEscape::kFenceOffset - 1.10f;
    const float lastRunout = std::max(0.f, tailClearY - path.end.y);
    const float gateDistance = std::max(0.f, path.length - lastRunout);
    const float accelerationAllowance = .5f * kEscapeRunSpeed / kEscapeAcceleration;
    const float transitSeconds = gateDistance / kEscapeRunSpeed + accelerationAllowance;
    // These are small input/planning allowances, not a delay per solution wave:
    // independent sheep may be released while others are already running.
    const float floor = transitSeconds + static_cast<float>(moves) * .42f + 4.f;
    return std::max(configuredSeconds, std::ceil(floor));
}

float centerDistance(const Sheep &left, const Sheep &right) noexcept {
    const auto leftHead = left.head();
    const auto rightHead = right.head();
    return .5f * (std::abs(left.tail.x + leftHead.x - right.tail.x - rightHead.x) +
                  std::abs(left.tail.y + leftHead.y - right.tail.y - rightHead.y));
}

std::size_t chooseImpact(const BoardModel &board, const DependencyGraph &graph,
                         int preferredWave, std::size_t timed,
                         const std::vector<std::size_t> &used, std::uint64_t seed) {
    const auto &sheep = board.sheep();
    const auto unused = [&](std::size_t index) {
        return graph.waves[index] > 0 && std::find(used.begin(), used.end(), index) == used.end();
    };
    const bool hasExposed = [&] {
        for (std::size_t index = 0; index < sheep.size(); ++index) {
            if (unused(index) && graph.incoming[index] > 0) { return true; }
        }
        return false;
    }();
    std::size_t selected = kNoIndex;
    int bestScore = std::numeric_limits<int>::min();
    std::uint64_t bestTie{};
    for (std::size_t index = 0; index < sheep.size(); ++index) {
        if (!unused(index) || (hasExposed && graph.incoming[index] == 0)) { continue; }
        int score = -std::abs(graph.waves[index] - std::max(1, preferredWave)) * 800 +
            graph.incoming[index] * 120 + graph.requiredMoves[index] * 15;
        if (timed != kNoIndex && graph.closure[timed][index]) { score += 900; }
        float separation = static_cast<float>(board.width() + board.height());
        for (const auto other : used) {
            if (other == timed) { continue; }
            separation = std::min(separation, centerDistance(sheep[index], sheep[other]));
            const auto &a = sheep[index];
            const auto &b = sheep[other];
            if (a.direction == b.direction &&
                (a.tail.x == b.tail.x || a.tail.y == b.tail.y)) {
                score -= 180;
            }
        }
        score += static_cast<int>(separation * 25.f);
        const auto tie = tieBreaker(sheep[index].id, seed);
        if (selected == kNoIndex || score > bestScore || (score == bestScore && tie > bestTie)) {
            selected = index;
            bestScore = score;
            bestTie = tie;
        }
    }
    return selected;
}

} // namespace

std::vector<BombPlacement> planBombPlacement(const BoardModel &board, const LevelDefinition &level) {
    std::vector<BombPlacement> placements;
    if (board.empty() || (level.bombSeconds <= 0.f && level.impactBombHits <= 0)) {
        return placements;
    }
    const auto graph = dependencies(board);
    const auto &sheep = board.sheep();
    std::vector<std::size_t> used;
    std::size_t timed = kNoIndex;
    if (level.bombSeconds > 0.f) {
        timed = chooseTimed(board, graph, level.bombPreferredWave, level.generation.seed);
        if (timed != kNoIndex) {
            placements.push_back({sheep[timed].id, false,
                timedDeadline(board, sheep[timed], graph.requiredMoves[timed], level.bombSeconds),
                0, graph.requiredMoves[timed]});
            used.push_back(timed);
        }
    }
    if (level.impactBombHits > 0) {
        const int count = std::clamp(level.impactBombCount, 0, 2);
        for (int slot = 0; slot < count; ++slot) {
            const auto selected = chooseImpact(board, graph, level.impactBombPreferredWave,
                timed, used, level.generation.seed + static_cast<std::uint64_t>(slot));
            if (selected == kNoIndex) { break; }
            placements.push_back({sheep[selected].id, true, 0.f, level.impactBombHits,
                graph.requiredMoves[selected]});
            used.push_back(selected);
        }
    }
    return placements;
}

} // namespace LiberateTheSheep::Game
