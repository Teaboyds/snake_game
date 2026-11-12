#include "game/scenes/MenuScene.h"

#include <memory>

#include "game/scenes/LeaderboardView.h"
#include "game/scenes/NameEntryScene.h"

namespace snake {

using engine::input::Action;
using engine::render::Color;

MenuScene::MenuScene(engine::scene::SceneManager& scenes, Session& session)
    : scenes_(scenes), session_(session) {}

void MenuScene::onEnter() { top_ = session_.leaderboard.top(5); }

void MenuScene::handleInput(const engine::input::InputFrame& input) {
    if (input.has(Action::Confirm)) scenes_.change(std::make_unique<NameEntryScene>(scenes_, session_));
}

void MenuScene::update(double dt) { time_ += dt; }

void MenuScene::render(engine::render::FrameBuffer& frame) {
    static const char* kTitle[] = {
        " ____  _   _    _    _  _______ ",
        "/ ___|| \\ | |  / \\  | |/ / ____|",
        "\\___ \\|  \\| | / _ \\ | ' /|  _|  ",
        " ___) | |\\  |/ ___ \\| . \\| |___ ",
        "|____/|_| \\_/_/   \\_\\_|\\_\\_____|",
    };

    int y = frame.height() / 2 - 11;
    for (const char* line : kTitle) frame.textCentered(y++, line, Color::Green, true);

    y += 2;
    if (static_cast<int>(time_ * 2.0) % 2 == 0) {
        frame.textCentered(y, "Press ENTER to start", Color::Yellow, true);
    }
    y += 2;
    frame.textCentered(y++, "Arrow keys / WASD : move", Color::Gray);
    frame.textCentered(y++, "P / Esc : pause      Q : quit", Color::Gray);

    y += 1;
    y = drawLeaderboard(frame, y, top_, session_.playerName);

    y += 1;
    const Color statusColor = session_.leaderboard.online() ? Color::Green : Color::Red;
    frame.textCentered(y, "leaderboard: " + session_.leaderboard.description(), statusColor);
}

}  // namespace snake
