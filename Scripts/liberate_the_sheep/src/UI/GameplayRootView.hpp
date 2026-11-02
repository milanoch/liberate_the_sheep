#pragma once

#include "UI/BoardInputView.hpp"
#include "UI/UIStyle.hpp"

#include <PandaUI/PandaUI.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace LiberateTheSheep::UI {

class BombSheepPicture;
class BombFieldOverlay;
class RescueNoticeView;

struct BombDisplay {
    PandaUI::Point center{};
    float size{};
    float rotationDegrees{};
    int value{};
    bool impact{};
};

struct BombHUDState {
    float seconds{};
    int hits{};
    bool impact{};
    bool rescued{};
    bool defused{};
};

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
    std::string loadErrorTitle = "COULD NOT PREPARE LEVEL";
    std::string loadErrorMessage = "Try loading this level again or return to the menu.";
    std::string nextLevelButton = "NEXT LEVEL";
    std::string replayButton = "REPLAY";

    std::string bombTitle = "A SHEEP WITH A BOMB";
    std::string bombInstructions;
    std::string bombImmortalInstructions;
    std::string bombStart = "LET'S GO!";
    std::string bombTimer = "BOMB TIMER";
    std::string bombRescued = "SHEEP RESCUED";
    std::string bombDefused = "BOMB DEFUSED";
    std::string bombExpired = "TIME IS UP";
    std::string bombDefeatMessage;
    std::string impactBombTitle = "A SHEEP WITH A BLACK BOMB";
    std::string impactBombInstructions;
    std::string impactBombImmortalInstructions;
    std::string impactBombHits = "HITS UNTIL EXPLOSION";
    std::string impactBombExpired = "THE BOMB WAS HIT";
    std::string impactBombDefeatMessage;
    std::string dogRescueTitle = "SHEEPDOG TO THE RESCUE";
    std::string dogRescueMessage =
        "Immortal mode is on. The sheepdog will take the bomb away from the flock.";
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
        Action startBombLevel;
    };

    explicit GameplayRootView(Callbacks callbacks = {}, GameplayText text = {}, bool showHearts = true);

    // frame is expressed in root/window logical points and must match the
    // rendered world-board rectangle.
    void setBoardFrame(PandaUI::Rect frame);
    void setBoardSize(int columns, int rows);
    void updateHUD(int levelNumber, int hearts, int sheepLeft);

    void showVictory(int heartsRemaining, bool hasNextLevel = true);
    void showDefeat();
    void showLoadError();
    void hideResult();
    void setBoardInputEnabled(bool enabled);

    void setBombArtwork(PandaUI::TextureHandle sheepAtlas, PandaUI::TextureHandle bomb);
    void showBombTutorial(bool immortal, float seconds, int coat, bool impact = false, int hits = 0,
                          int bombCount = 1);
    void hideBombTutorial();
    void updateBombTimer(float seconds, bool visible, bool rescued, bool defused, bool immortal,
                         bool impact = false, int hits = 0);
    void setBombMarker(PandaUI::Point center, float bombSize, float rotationDegrees,
                       int seconds, bool visible, bool impact = false);
    void setBombMarkers(const std::vector<BombDisplay> &displays);
    void updateBombStatus(const std::vector<BombHUDState> &bombs, bool immortal);
    void playBombBurst(PandaUI::Point center, bool harmless);
    void showBombDefeat(bool impact = false);
    void showDogRescueNotice();
    void hideDogRescueNotice();

    [[nodiscard]] std::shared_ptr<BoardInputView> boardInputView() const;

private:
    std::shared_ptr<PandaUI::Panel> makeHud(Callbacks &callbacks);
    std::shared_ptr<PandaUI::Panel> makeResultOverlay(Callbacks &callbacks);
    std::shared_ptr<PandaUI::Panel> makeBombTutorial(Callbacks &callbacks);

    GameplayText text_;
    bool showHearts_{true};
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
    std::shared_ptr<PandaUI::Label> bombTimerLabel_;
    float bombTimerFontSize_{12.f};
    std::shared_ptr<BombFieldOverlay> bombFieldOverlay_;
    std::shared_ptr<RescueNoticeView> dogRescueNotice_;
    std::shared_ptr<PandaUI::Panel> bombTutorial_;
    std::shared_ptr<BombSheepPicture> bombPicture_;
    std::shared_ptr<PandaUI::Label> bombTitleLabel_;
    std::shared_ptr<PandaUI::Label> bombInstructionsLabel_;
    std::shared_ptr<PandaUI::Label> bombImmortalLabel_;
    std::shared_ptr<PandaUI::Button> bombStartButton_;
};

} // namespace LiberateTheSheep::UI
