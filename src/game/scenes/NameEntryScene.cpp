#include "game/scenes/NameEntryScene.h"

#include <memory>

#include "game/PlayerName.h"
#include "game/scenes/MenuScene.h"
#include "game/scenes/PlayScene.h"

namespace snake {

using engine::input::Action;
using engine::render::Color;

NameEntryScene::NameEntryScene(engine::scene::SceneManager& scenes, Session& session)
    : scenes_(scenes), session_(session), name_(session.playerName) {}

void NameEntryScene::handleInput(const engine::input::InputFrame& input) {
    for (char c : input.text) {
        if (c == '\b') {
            if (!name_.empty()) name_.pop_back();
            error_.clear();
        } else if (!isAllowedNameChar(c)) {
            error_ = "Only A-Z a-z 0-9 _ - allowed";
        } else if (name_.size() < kMaxPlayerNameLength) {
            name_ += c;
            error_.clear();
        }
    }

    for (Action a : input.actions) {
        if (a == Action::Confirm) {
            if (name_.empty()) {
                error_ = "Name cannot be empty";
            } else {
                session_.playerName = name_;
                scenes_.change(std::make_unique<PlayScene>(scenes_, session_));
            }
        } else if (a == Action::Pause) {
            scenes_.change(std::make_unique<MenuScene>(scenes_, session_));
        }
    }
}

void NameEntryScene::update(double dt) { time_ += dt; }

void NameEntryScene::render(engine::render::FrameBuffer& frame) {
    int y = frame.height() / 2 - 5;
    frame.textCentered(y, "ENTER YOUR NAME", Color::Green, true);

    y += 2;
    // จัดกลางจากชื่ออย่างเดียว แล้วค่อยวาง cursor ต่อท้าย (cursor ไม่มีผลกับตำแหน่งชื่อ)
    const std::size_t fieldWidth = kMaxPlayerNameLength + 1;
    std::string field((fieldWidth - name_.size()) / 2, ' ');
    field += name_;
    if (name_.size() < kMaxPlayerNameLength && static_cast<int>(time_ * 2.0) % 2 == 0) field += '_';
    field.resize(fieldWidth, ' ');
    frame.textCentered(y, "[ " + field + " ]", Color::Cyan, true);

    y += 2;
    frame.textCentered(y++, "A-Z a-z 0-9 _ -   (max 12)", Color::Gray);
    frame.textCentered(y++, "Same name = same player on the leaderboard", Color::Gray);

    y += 1;
    if (!error_.empty()) frame.textCentered(y, error_, Color::Red, true);

    y += 2;
    frame.textCentered(y, "ENTER : play     ESC : back", Color::Gray);
}

}  // namespace snake
