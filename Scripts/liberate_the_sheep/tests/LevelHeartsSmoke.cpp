#include "Game/LevelHearts.hpp"

#include <iostream>
#include <stdexcept>

namespace {
using LiberateTheSheep::Game::LevelHearts;
using LiberateTheSheep::Game::ProgressStore;

void check(bool condition, const char *message) {
    if (!condition) { throw std::runtime_error(message); }
}

void normalMode() {
    LevelHearts hearts;
    check(hearts.counterVisible(), "New game must show hearts by default");
    check(hearts.remaining() == ProgressStore::kMaximumHearts && !hearts.isDepleted(),
          "New level must start with full hearts");
    for (int expected = ProgressStore::kMaximumHearts - 1; expected >= 0; --expected) {
        hearts.blockedMove();
        check(hearts.remaining() == expected, "Normal blocked move must cost exactly one heart");
        check(hearts.isDepleted() == (expected == 0), "Only last heart should trigger defeat");
    }
    for (int i = 0; i < 100; ++i) { hearts.blockedMove(); }
    check(hearts.remaining() == 0 && hearts.isDepleted(), "Hearts must not underflow");
    hearts.reset(false);
    check(hearts.remaining() == ProgressStore::kMaximumHearts && !hearts.isDepleted(),
          "Restart must restore normal hearts");
}

void immortalModeAndSwitching() {
    LevelHearts hearts;
    hearts.blockedMove();
    hearts.reset(true);
    check(!hearts.counterVisible(), "Immortal mode must hide the counter");
    for (int mistake = 0; mistake < 10000; ++mistake) {
        hearts.blockedMove();
        check(!hearts.isDepleted(), "Immortal mistakes must never block input/hints or cause defeat");
        check(hearts.remaining() == ProgressStore::kMaximumHearts,
              "Immortal mistakes must not consume hearts");
    }
    // Restart/next level continue to honour the saved mode.
    hearts.reset(true);
    check(!hearts.counterVisible() && !hearts.isDepleted(), "Restart lost immortal mode");
    // Returning through settings and disabling restores the ordinary rules.
    hearts.reset(false);
    check(hearts.counterVisible(), "Disabling must restore the counter");
    for (int i = 0; i < ProgressStore::kMaximumHearts; ++i) { hearts.blockedMove(); }
    check(hearts.isDepleted(), "Disabling must restore three-mistake defeat");
    hearts.reset(true);
    check(!hearts.isDepleted(), "A prior normal defeat must not leak into an immortal level");
}
} // namespace

int main() {
    try {
        normalMode();
        immortalModeAndSwitching();
        std::cout << "Normal/immortal heart rules and reset transitions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
