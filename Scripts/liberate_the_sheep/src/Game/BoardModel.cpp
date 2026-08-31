#include "Game/BoardModel.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace LiberateTheSheep::Game {
namespace {

    void addIssue(
        BoardValidationReport &report,
        BoardValidationCode code,
        SheepId sheep,
        std::optional<Cell> cell,
        std::string message
    ) {
        report.issues.push_back({code, sheep, cell, std::move(message)});
    }

    void hashByte(std::uint64_t &hash, std::uint8_t value) noexcept {
        constexpr std::uint64_t kFnvPrime = 1099511628211ULL;
        hash ^= value;
        hash *= kFnvPrime;
    }

    void hashUint32(std::uint64_t &hash, std::uint32_t value) noexcept {
        for (int shift = 0; shift < 32; shift += 8) {
            hashByte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffu));
        }
    }

} // namespace

BoardModel::BoardModel(int width, int height)
    : width_(width)
    , height_(height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument("Board dimensions must be positive");
    }

    const auto unsignedWidth = static_cast<std::size_t>(width);
    const auto unsignedHeight = static_cast<std::size_t>(height);
    if (unsignedWidth >
        static_cast<std::size_t>(std::numeric_limits<int>::max()) / unsignedHeight) {
        throw std::length_error("Board dimensions are too large");
    }

    occupancy_.assign(unsignedWidth * unsignedHeight, kInvalidSheepId);
}

int BoardModel::width() const noexcept {
    return width_;
}

int BoardModel::height() const noexcept {
    return height_;
}

int BoardModel::cellCount() const noexcept {
    return width_ * height_;
}

int BoardModel::occupiedCellCount() const noexcept {
    return static_cast<int>(sheep_.size() * 2);
}

double BoardModel::density() const noexcept {
    return static_cast<double>(occupiedCellCount()) / static_cast<double>(cellCount());
}

bool BoardModel::empty() const noexcept {
    return sheep_.empty();
}

std::size_t BoardModel::sheepCount() const noexcept {
    return sheep_.size();
}

bool BoardModel::isInside(Cell cell) const noexcept {
    return cell.x >= 0 && cell.x < width_ && cell.y >= 0 && cell.y < height_;
}

std::optional<SheepId> BoardModel::sheepAt(Cell cell) const noexcept {
    if (!isInside(cell)) {
        return std::nullopt;
    }

    const SheepId id = occupancy_[cellIndex(cell)];
    return id == kInvalidSheepId ? std::nullopt : std::optional<SheepId>{id};
}

bool BoardModel::isOccupied(Cell cell) const noexcept {
    return sheepAt(cell).has_value();
}

const Sheep *BoardModel::findSheep(SheepId id) const noexcept {
    const auto found = std::find_if(sheep_.begin(), sheep_.end(), [id](const Sheep &sheep) {
        return sheep.id == id;
    });
    return found == sheep_.end() ? nullptr : &*found;
}

const std::vector<Sheep> &BoardModel::sheep() const noexcept {
    return sheep_;
}

PlacementCheck BoardModel::inspectPlacement(const Sheep &sheep) const noexcept {
    if (sheep.id == kInvalidSheepId) {
        return {PlacementState::InvalidId, std::nullopt, std::nullopt};
    }
    if (findSheep(sheep.id) != nullptr) {
        return {PlacementState::DuplicateId, std::nullopt, sheep.id};
    }
    if (!isValidDirection(sheep.direction)) {
        return {PlacementState::InvalidDirection, sheep.tail, std::nullopt};
    }

    const auto [tail, head] = sheep.occupiedCells();
    for (const Cell cell : {tail, head}) {
        if (!isInside(cell)) {
            return {PlacementState::OutOfBounds, cell, std::nullopt};
        }
        if (const auto occupant = sheepAt(cell)) {
            return {PlacementState::Occupied, cell, occupant};
        }
    }

    return {};
}

bool BoardModel::addSheep(const Sheep &sheep) {
    if (!inspectPlacement(sheep).valid()) {
        return false;
    }

    const auto [tail, head] = sheep.occupiedCells();
    occupancy_[cellIndex(tail)] = sheep.id;
    occupancy_[cellIndex(head)] = sheep.id;
    sheep_.push_back(sheep);
    return true;
}

bool BoardModel::eraseSheep(SheepId id) noexcept {
    const auto found = std::find_if(sheep_.begin(), sheep_.end(), [id](const Sheep &sheep) {
        return sheep.id == id;
    });
    if (found == sheep_.end()) {
        return false;
    }

    const auto [tail, head] = found->occupiedCells();
    if (isInside(tail) && occupancy_[cellIndex(tail)] == id) {
        occupancy_[cellIndex(tail)] = kInvalidSheepId;
    }
    if (isInside(head) && occupancy_[cellIndex(head)] == id) {
        occupancy_[cellIndex(head)] = kInvalidSheepId;
    }
    sheep_.erase(found);
    return true;
}

MoveCheck
BoardModel::inspectRay(Cell origin, Direction direction, SheepId ignoredSheep) const noexcept {
    if (!isValidDirection(direction)) {
        return {MoveState::Blocked, std::nullopt, std::nullopt};
    }

    const Cell delta = directionDelta(direction);
    Cell current{origin.x + delta.x, origin.y + delta.y};
    while (isInside(current)) {
        if (const auto occupant = sheepAt(current); occupant && *occupant != ignoredSheep) {
            return {MoveState::Blocked, occupant, current};
        }
        current.x += delta.x;
        current.y += delta.y;
    }

    return {MoveState::Clear, std::nullopt, std::nullopt};
}

MoveCheck BoardModel::inspectMove(SheepId id) const noexcept {
    const Sheep *sheep = findSheep(id);
    if (sheep == nullptr) {
        return {MoveState::SheepNotFound, std::nullopt, std::nullopt};
    }
    return inspectRay(sheep->head(), sheep->direction, sheep->id);
}

EscapeResult BoardModel::tryEscape(SheepId id) noexcept {
    const MoveCheck check = inspectMove(id);
    if (check.state == MoveState::SheepNotFound) {
        return EscapeResult::SheepNotFound;
    }
    if (!check.canEscape()) {
        return EscapeResult::Blocked;
    }

    return eraseSheep(id) ? EscapeResult::Escaped : EscapeResult::SheepNotFound;
}

std::vector<SheepId> BoardModel::escapableSheep() const {
    std::vector<SheepId> result;
    result.reserve(sheep_.size());
    for (const Sheep &sheep : sheep_) {
        if (inspectMove(sheep.id).canEscape()) {
            result.push_back(sheep.id);
        }
    }
    return result;
}

BoardValidationReport BoardModel::validateStructure() const {
    BoardValidationReport report;
    if (width_ <= 0 || height_ <= 0) {
        addIssue(
            report,
            BoardValidationCode::InvalidDimensions,
            kInvalidSheepId,
            std::nullopt,
            "Board dimensions must be positive"
        );
        return report;
    }

    const std::size_t expectedCellCount =
        static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_);
    if (occupancy_.size() != expectedCellCount) {
        addIssue(
            report,
            BoardValidationCode::InvalidOccupancySize,
            kInvalidSheepId,
            std::nullopt,
            "Occupancy storage size does not match board dimensions"
        );
        return report;
    }

    std::vector<SheepId> expectedOccupancy(expectedCellCount, kInvalidSheepId);
    std::unordered_set<SheepId> ids;
    for (const Sheep &sheep : sheep_) {
        if (sheep.id == kInvalidSheepId) {
            addIssue(
                report,
                BoardValidationCode::InvalidSheepId,
                sheep.id,
                std::nullopt,
                "Sheep id zero is reserved for an empty cell"
            );
        } else if (!ids.insert(sheep.id).second) {
            addIssue(
                report,
                BoardValidationCode::DuplicateSheepId,
                sheep.id,
                std::nullopt,
                "Duplicate sheep id"
            );
        }

        if (!isValidDirection(sheep.direction)) {
            addIssue(
                report,
                BoardValidationCode::InvalidDirection,
                sheep.id,
                sheep.tail,
                "Sheep direction is invalid"
            );
            continue;
        }

        const auto [tail, head] = sheep.occupiedCells();
        for (const Cell cell : {tail, head}) {
            if (!isInside(cell)) {
                addIssue(
                    report,
                    BoardValidationCode::SheepOutOfBounds,
                    sheep.id,
                    cell,
                    "A sheep cell lies outside the board"
                );
                continue;
            }

            SheepId &expected = expectedOccupancy[cellIndex(cell)];
            if (expected != kInvalidSheepId && expected != sheep.id) {
                addIssue(
                    report,
                    BoardValidationCode::OverlappingSheep,
                    sheep.id,
                    cell,
                    "Two sheep occupy the same cell"
                );
            } else {
                expected = sheep.id;
            }
        }
    }

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const Cell cell{x, y};
            const std::size_t index = cellIndex(cell);
            if (occupancy_[index] != expectedOccupancy[index]) {
                addIssue(
                    report,
                    BoardValidationCode::OccupancyMismatch,
                    occupancy_[index],
                    cell,
                    "Occupancy storage does not match the sheep list"
                );
            }
        }
    }

    return report;
}

SolvabilityReport BoardModel::analyzeSolvability() const {
    SolvabilityReport report;
    BoardModel remaining = *this;
    report.initialEscapableCount = remaining.escapableSheep().size();

    while (!remaining.empty()) {
        std::vector<SheepId> wave = remaining.escapableSheep();
        if (wave.empty()) {
            report.permanentlyBlocked.reserve(remaining.sheepCount());
            for (const Sheep &sheep : remaining.sheep()) {
                report.permanentlyBlocked.push_back(sheep.id);
            }
            return report;
        }

        report.widestSolutionWave = std::max(report.widestSolutionWave, wave.size());
        for (const SheepId id : wave) {
            report.removalOrder.push_back(id);
            (void)remaining.eraseSheep(id);
        }
        report.solutionWaves.push_back(std::move(wave));
        report.solutionWaveCount = report.solutionWaves.size();
    }

    report.solvable = true;
    return report;
}

std::uint64_t BoardModel::layoutFingerprint() const {
    constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
    std::uint64_t hash = kFnvOffset;
    hashUint32(hash, static_cast<std::uint32_t>(width_));
    hashUint32(hash, static_cast<std::uint32_t>(height_));

    std::vector<Sheep> ordered = sheep_;
    std::sort(ordered.begin(), ordered.end(), [](const Sheep &left, const Sheep &right) {
        return left.id < right.id;
    });
    for (const Sheep &sheep : ordered) {
        hashUint32(hash, sheep.id);
        hashUint32(hash, static_cast<std::uint32_t>(sheep.tail.x));
        hashUint32(hash, static_cast<std::uint32_t>(sheep.tail.y));
        hashByte(hash, static_cast<std::uint8_t>(sheep.direction));
    }
    return hash;
}

std::size_t BoardModel::cellIndex(Cell cell) const noexcept {
    return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(cell.x);
}

} // namespace LiberateTheSheep::Game
