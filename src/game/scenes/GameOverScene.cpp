#include "game/scenes/GameOverScene.h"

#include <memory>
#include <string>

#include "game/scenes/LeaderboardView.h"
#include "game/scenes/MenuScene.h"
#include "game/scenes/PlayScene.h"

namespace snake {

using engine::input::Action;
using engine::render::Color;

GameOverScene::GameOverScene(engine::scene::SceneManager& scenes, Session& session, int score, bool won,
                             bool newBest)
    : scenes_(scenes), session_(session), score_(score), won_(won), newBest_(newBest) {}

void GameOverScene::onEnter() {
    standing_ = session_.leaderboard.standing(session_.playerName);
    top_ = session_.leaderboard.top(5);
}

void GameOverScene::handleInput(const engine::input::InputFrame& input) {
    for (Action a : input.actions) {
        if (a == Action::Confirm) scenes_.change(std::make_unique<PlayScene>(scenes_, session_));
        if (a == Action::Pause) scenes_.change(std::make_unique<MenuScene>(scenes_, session_));
    }
}

void GameOverScene::update(double dt) { time_ += dt; }

void GameOverScene::render(engine::render::FrameBuffer& frame) {
    int y = frame.height() / 2 - 10;
    if (won_) {
        frame.textCentered(y, "Y O U   W I N !", Color::Yellow, true);
    } else {
        frame.textCentered(y, "G A M E   O V E R", Color::Red, true);
    }

    y += 2;
    frame.textCentered(y++, session_.playerName + "  -  SCORE " + std::to_string(score_), Color::White, true);
    if (standing_) {
        frame.textCentered(y++,
                           "BEST " + std::to_string(standing_->bestScore) + "   RANK #" +
                               std::to_string(standing_->rank) + "   GAMES " + std::to_string(standing_->gamesPlayed),
                           Color::Cyan);
    } else {
        frame.textCentered(y++, "(leaderboard unavailable)", Color::Red);
    }
    if (newBest_ && static_cast<int>(time_ * 3.0) % 2 == 0) {
        frame.textCentered(y, "NEW BEST!", Color::Magenta, true);
    }

    y += 2;
    y = drawLeaderboard(frame, y, top_, session_.playerName);

    y += 1;
    frame.textCentered(y++, "ENTER : play again", Color::Gray);
    frame.textCentered(y++, "Esc   : main menu ", Color::Gray);
    frame.textCentered(y, "Q     : quit      ", Color::Gray);
}

}  // namespace snake
