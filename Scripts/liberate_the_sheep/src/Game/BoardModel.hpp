#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace LiberateTheSheep::Game {

using SheepId = std::uint32_t;

inline constexpr SheepId kInvalidSheepId = 0;

struct Cell {
    int x{};
    int y{};

    friend constexpr bool operator==(const Cell &, const Cell &) = default;
};

enum class Direction : std::uint8_t {
    Up = 0,
    Right = 1,
    Down = 2,
    Left = 3,
};

using DirectionMask = std::uint8_t;

inline constexpr DirectionMask kAllDirections = 0x0f;

[[nodiscard]] constexpr bool isValidDirection(Direction direction) noexcept {
    return static_cast<std::uint8_t>(direction) <= static_cast<std::uint8_t>(Direction::Left);
}

[[nodiscard]] constexpr DirectionMask directionBit(Direction direction) noexcept {
    return isValidDirection(direction)
               ? static_cast<DirectionMask>(1u << static_cast<std::uint8_t>(direction))
               : DirectionMask{0};
}

[[nodiscard]] constexpr bool containsDirection(DirectionMask mask, Direction direction) noexcept {
    return (mask & directionBit(direction)) != 0;
}

[[nodiscard]] constexpr Cell directionDelta(Direction direction) noexcept {
    switch (direction) {
        case Direction::Up:
            return {0, 1};
        case Direction::Right:
            return {1, 0};
        case Direction::Down:
            return {0, -1};
        case Direction::Left:
            return {-1, 0};
    }
    return {0, 0};
}

struct Sheep {
    SheepId id{kInvalidSheepId};
    Cell tail{};
    Direction direction{Direction::Up};

    [[nodiscard]] constexpr Cell head() const noexcept {
        const Cell delta = directionDelta(direction);
        return {tail.x + delta.x, tail.y + delta.y};
    }

    [[nodiscard]] constexpr std::pair<Cell, Cell> occupiedCells() const noexcept {
        return {tail, head()};
    }

    friend constexpr bool operator==(const Sheep &, const Sheep &) = default;
};

enum class PlacementState {
    Valid,
    InvalidId,
    DuplicateId,
    InvalidDirection,
    OutOfBounds,
    Occupied,
};

struct PlacementCheck {
    PlacementState state{PlacementState::Valid};
    std::optional<Cell> problemCell;
    std::optional<SheepId> occupyingSheep;

    [[nodiscard]] bool valid() const noexcept {
        return state == PlacementState::Valid;
    }
};

enum class MoveState {
    Clear,
    Blocked,
    SheepNotFound,
};

struct MoveCheck {
    MoveState state{MoveState::SheepNotFound};
    std::optional<SheepId> blocker;
    std::optional<Cell> blockerCell;

    [[nodiscard]] bool canEscape() const noexcept {
        return state == MoveState::Clear;
    }
};

enum class EscapeResult {
    Escaped,
    Blocked,
    SheepNotFound,
};

enum class BoardValidationCode {
    InvalidDimensions,
    InvalidOccupancySize,
    InvalidSheepId,
    DuplicateSheepId,
    InvalidDirection,
    SheepOutOfBounds,
    OverlappingSheep,
    OccupancyMismatch,
};

struct BoardValidationIssue {
    BoardValidationCode code{BoardValidationCode::InvalidDimensions};
    SheepId sheep{kInvalidSheepId};
    std::optional<Cell> cell;
    std::string message;
};

struct BoardValidationReport {
    std::vector<BoardValidationIssue> issues;

    [[nodiscard]] bool valid() const noexcept {
        return issues.empty();
    }
};

struct SolvabilityReport {
    bool solvable{false};
    std::size_t initialEscapableCount{};
    std::size_t solutionWaveCount{};
    std::size_t widestSolutionWave{};
    std::vector<std::vector<SheepId>> solutionWaves;
    std::vector<SheepId> removalOrder;
    std::vector<SheepId> permanentlyBlocked;
};

// Coordinates use an ordinary Cartesian grid: (0, 0) is the lower-left cell
// and y grows upwards. A sheep's tail plus its direction defines its head cell.
class BoardModel {
public:
    BoardModel(int width, int height);

    [[nodiscard]] int width() const noexcept;
    [[nodiscard]] int height() const noexcept;
    [[nodiscard]] int cellCount() const noexcept;
    [[nodiscard]] int occupiedCellCount() const noexcept;
    [[nodiscard]] double density() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t sheepCount() const noexcept;

    [[nodiscard]] bool isInside(Cell cell) const noexcept;
    [[nodiscard]] std::optional<SheepId> sheepAt(Cell cell) const noexcept;
    [[nodiscard]] bool isOccupied(Cell cell) const noexcept;
    [[nodiscard]] const Sheep *findSheep(SheepId id) const noexcept;
    [[nodiscard]] const std::vector<Sheep> &sheep() const noexcept;

    [[nodiscard]] PlacementCheck inspectPlacement(const Sheep &sheep) const noexcept;
    [[nodiscard]] bool addSheep(const Sheep &sheep);
    [[nodiscard]] bool eraseSheep(SheepId id) noexcept;

    [[nodiscard]] MoveCheck inspectMove(SheepId id) const noexcept;
    [[nodiscard]] EscapeResult tryEscape(SheepId id) noexcept;
    [[nodiscard]] std::vector<SheepId> escapableSheep() const;

    // Finds the nearest occupied cell strictly beyond origin in direction.
    // ignoredSheep is useful when querying geometry associated with one piece.
    [[nodiscard]] MoveCheck inspectRay(
        Cell origin, Direction direction, SheepId ignoredSheep = kInvalidSheepId
    ) const noexcept;

    [[nodiscard]] BoardValidationReport validateStructure() const;
    [[nodiscard]] SolvabilityReport analyzeSolvability() const;
    [[nodiscard]] std::uint64_t layoutFingerprint() const;

private:
    [[nodiscard]] std::size_t cellIndex(Cell cell) const noexcept;

    int width_{};
    int height_{};
    std::vector<Sheep> sheep_;
    std::vector<SheepId> occupancy_;
};

} // namespace LiberateTheSheep::Game
