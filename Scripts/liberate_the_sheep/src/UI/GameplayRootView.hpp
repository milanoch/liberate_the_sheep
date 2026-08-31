#pragma once

#include "UI/BoardInputView.hpp"
#include "UI/UIStyle.hpp"

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>
#include <string>

namespace LiberateTheSheep::UI {

struct GameplayText {
    std::string levelStat = "LEVEL";
    std::string heartsStat = "HEARTS";
    std::string sheepLeftStat = "SHEEP LEFT";
    std::string hintButton = "HINT";
    std::string restartButton = "RESTART";
    std::string menuButton = "MENU";

    std::string fieldCleared = "FIELD CLEARED";
    std::string victoryTitle = "THE FLOCK IS FREE!";
    std::string victoryMessage = "You cleared the field.";
    std::string noHeartsLeft = "NO HEARTS LEFT";
    std::string defeatTitle = "THE SHEEP NEED YOU";
    std::string defeatMessage =
        "Try the same field again and look for a clear lane first.";
    std::string nextLevelButton = "NEXT LEVEL";
    std::string replayButton = "REPLAY";
};

class GameplayRootView final : public PassThroughPanel {
public:
    using Action = std::function<void()>;
    using CellAction = BoardInputView::CellAction;

    struct Callbacks {
        CellAction selectCell;
        Action hint;
        Action restart;
        Action menu;
        Action next;
        Action replay;
    };

    explicit GameplayRootView(Callbacks callbacks = {}, GameplayText text = {});

    // frame is expressed in root/window logical points and must match the
    // rendered world-board rectangle.
    void setBoardFrame(PandaUI::Rect frame);
    void setBoardSize(int columns, int rows);
    void updateHUD(int levelNumber, int hearts, int sheepLeft);

    void showVictory(int heartsRemaining, bool hasNextLevel = true);
    void showDefeat();
    void hideResult();
    void setBoardInputEnabled(bool enabled);

    [[nodiscard]] std::shared_ptr<BoardInputView> boardInputView() const;

private:
    std::shared_ptr<PandaUI::Panel> makeHud(Callbacks &callbacks);
    std::shared_ptr<PandaUI::Panel> makeResultOverlay(Callbacks &callbacks);

    GameplayText text_;
    std::shared_ptr<BoardInputView> boardInput_;
    std::shared_ptr<PandaUI::Label> levelLabel_;
    std::shared_ptr<PandaUI::Label> heartsLabel_;
    std::shared_ptr<PandaUI::Label> sheepLabel_;
    std::shared_ptr<PandaUI::Panel> resultOverlay_;
    std::shared_ptr<PandaUI::Label> resultEyebrowLabel_;
    std::shared_ptr<PandaUI::Label> resultTitleLabel_;
    std::shared_ptr<PandaUI::Label> resultMessageLabel_;
    std::shared_ptr<PandaUI::Button> nextButton_;
    std::shared_ptr<PandaUI::Button> replayButton_;
};

} // namespace LiberateTheSheep::UI
