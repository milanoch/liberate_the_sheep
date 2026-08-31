#pragma once

#include "Game/BoardModel.hpp"

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <optional>

namespace LiberateTheSheep::UI {

class BoardInputView final : public PandaUI::View {
public:
    using CellAction = std::function<void(Game::Cell)>;

    explicit BoardInputView(CellAction onCellPressed = {}, float dragThreshold = 14.f);

    void setBoardSize(int columns, int rows);
    [[nodiscard]] int columns() const noexcept;
    [[nodiscard]] int rows() const noexcept;

    void setInputEnabled(bool enabled);
    [[nodiscard]] bool isInputEnabled() const noexcept;

    void setDragThreshold(float threshold);
    [[nodiscard]] float dragThreshold() const noexcept;

    void setOnCellPressed(CellAction action);
    [[nodiscard]] std::optional<Game::Cell>
    cellAtWindowPoint(PandaUI::Point windowPoint) const;

    bool pointerDown(PandaUI::PointerEvent &event) override;
    bool pointerMove(PandaUI::PointerEvent &event) override;
    bool pointerUp(PandaUI::PointerEvent &event) override;
    void interactionCancelled() override;

private:
    [[nodiscard]] bool acceptsPointer(const PandaUI::PointerEvent &event) const noexcept;
    [[nodiscard]] bool isTrackedPointer(const PandaUI::PointerEvent &event) const noexcept;
    [[nodiscard]] bool exceededDragThreshold(PandaUI::Point position) const noexcept;
    void cancelTracking() noexcept;

    CellAction onCellPressed_;
    PandaUI::Point pointerDownPosition_;
    PandaUI::PointerType trackedPointerType_{PandaUI::PointerType::Mouse};
    int trackedPointerId_{};
    int columns_{};
    int rows_{};
    float dragThreshold_{14.f};
    bool inputEnabled_{true};
    bool trackingPointer_{};
    bool dragged_{};
};

} // namespace LiberateTheSheep::UI
