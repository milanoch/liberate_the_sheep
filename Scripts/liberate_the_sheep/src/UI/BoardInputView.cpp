#include "UI/BoardInputView.hpp"

#include "UI/UIStyle.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace LiberateTheSheep::UI {

BoardInputView::BoardInputView(CellAction onCellPressed, float dragThreshold)
    : onCellPressed_(std::move(onCellPressed)) {
    setBackgroundColor(Colors::transparent());
    setClipsToBounds(true);
    setUserInteractionEnabled(true);
    setDragThreshold(dragThreshold);
}

void BoardInputView::setBoardSize(int columns, int rows) {
    columns_ = std::max(0, columns);
    rows_ = std::max(0, rows);
    cancelTracking();
}

int BoardInputView::columns() const noexcept {
    return columns_;
}

int BoardInputView::rows() const noexcept {
    return rows_;
}

void BoardInputView::setInputEnabled(bool enabled) {
    inputEnabled_ = enabled;
    setUserInteractionEnabled(enabled);
    if (!enabled) {
        cancelTracking();
    }
}

bool BoardInputView::isInputEnabled() const noexcept {
    return inputEnabled_;
}

void BoardInputView::setDragThreshold(float threshold) {
    dragThreshold_ = std::max(0.f, threshold);
}

float BoardInputView::dragThreshold() const noexcept {
    return dragThreshold_;
}

void BoardInputView::setOnCellPressed(CellAction action) {
    onCellPressed_ = std::move(action);
}

std::optional<Game::Cell>
BoardInputView::cellAtWindowPoint(PandaUI::Point windowPoint) const {
    if (columns_ <= 0 || rows_ <= 0) {
        return std::nullopt;
    }

    const PandaUI::Rect frame = getWorldFrame();
    if (frame.size.width <= 0.f || frame.size.height <= 0.f) {
        return std::nullopt;
    }

    const float localX = windowPoint.x - frame.origin.x;
    const float localY = windowPoint.y - frame.origin.y;
    if (localX < 0.f || localY < 0.f || localX >= frame.size.width ||
        localY >= frame.size.height) {
        return std::nullopt;
    }

    const float normalizedX = localX / frame.size.width;
    const float normalizedY = localY / frame.size.height;
    const int column = std::clamp(static_cast<int>(normalizedX * columns_), 0, columns_ - 1);
    const int rowFromTop = std::clamp(static_cast<int>(normalizedY * rows_), 0, rows_ - 1);

    // PandaUI's window coordinates grow downwards. BoardModel uses a Cartesian
    // grid with (0, 0) in the lower-left corner, so the row is inverted here.
    return Game::Cell{column, rows_ - 1 - rowFromTop};
}

bool BoardInputView::pointerDown(PandaUI::PointerEvent &event) {
    if (!inputEnabled_ || trackingPointer_ || !acceptsPointer(event) ||
        !cellAtWindowPoint(event.position)) {
        return false;
    }

    pointerDownPosition_ = event.position;
    trackedPointerType_ = event.type;
    trackedPointerId_ = event.pointerId;
    trackingPointer_ = true;
    dragged_ = false;
    return true;
}

bool BoardInputView::pointerMove(PandaUI::PointerEvent &event) {
    if (!isTrackedPointer(event)) {
        return false;
    }
    dragged_ = dragged_ || exceededDragThreshold(event.position);
    return true;
}

bool BoardInputView::pointerUp(PandaUI::PointerEvent &event) {
    if (!isTrackedPointer(event)) {
        return false;
    }

    dragged_ = dragged_ || exceededDragThreshold(event.position);
    const bool shouldTrigger = inputEnabled_ && !dragged_;
    const std::optional<Game::Cell> cell =
        shouldTrigger ? cellAtWindowPoint(event.position) : std::nullopt;
    CellAction action = onCellPressed_;
    cancelTracking();

    if (cell && action) {
        action(*cell);
    }
    return true;
}

void BoardInputView::interactionCancelled() {
    cancelTracking();
}

bool BoardInputView::acceptsPointer(const PandaUI::PointerEvent &event) const noexcept {
    if (event.type == PandaUI::PointerType::Touch) {
        return event.isPrimary;
    }
    return event.button == PandaUI::PointerButton::Left;
}

bool BoardInputView::isTrackedPointer(const PandaUI::PointerEvent &event) const noexcept {
    return trackingPointer_ && event.type == trackedPointerType_ &&
           event.pointerId == trackedPointerId_;
}

bool BoardInputView::exceededDragThreshold(PandaUI::Point position) const noexcept {
    const float dx = position.x - pointerDownPosition_.x;
    const float dy = position.y - pointerDownPosition_.y;
    return dx * dx + dy * dy > dragThreshold_ * dragThreshold_;
}

void BoardInputView::cancelTracking() noexcept {
    trackingPointer_ = false;
    dragged_ = false;
}

} // namespace LiberateTheSheep::UI
