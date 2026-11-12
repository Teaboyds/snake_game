#include "game/scenes/PlayScene.h"

#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <string>

#include "game/scenes/GameOverScene.h"

namespace snake {

using engine::input::Action;
using engine::render::Color;

namespace {
constexpr double kEndDelay = 1.0;
}

PlayScene::PlayScene(engine::scene::SceneManager& scenes, Session& session)
    : scenes_(scenes),
      session_(session),
      world_(session.config, std::random_device{}()),
      personalBest_(0),
      moveInterval_(session.config.startMoveInterval) {
    if (const auto standing = session_.leaderboard.standing(session_.playerName)) {
        personalBest_ = standing->bestScore;
    }
}

void PlayScene::handleInput(const engine::input::InputFrame& input) {
    for (Action a : input.actions) {
        switch (a) {
            case Action::Pause: paused_ = !paused_; break;
            case Action::Up: if (!paused_) world_.submit({Direction::Up}); break;
            case Action::Down: if (!paused_) world_.submit({Direction::Down}); break;
            case Action::Left: if (!paused_) world_.submit({Direction::Left}); break;
            case Action::Right: if (!paused_) world_.submit({Direction::Right}); break;
            default: break;
        }
    }
}

void PlayScene::update(double dt) {
    if (paused_) return;

    if (world_.status() != WorldStatus::Running) {
        endTimer_ += dt;
        if (endTimer_ >= kEndDelay && !leaving_) {
            leaving_ = true;
            const bool won = world_.status() == WorldStatus::Won;
            scenes_.change(std::make_unique<GameOverScene>(scenes_, session_, world_.score(), won, newBest_));
        }
        return;
    }

    for (const GameEvent& event : world_.advance(dt)) onGameEvent(event);
    if (world_.status() != WorldStatus::Running) return;

    moveTimer_ += dt;
    while (moveTimer_ >= moveInterval_ && world_.status() == WorldStatus::Running) {
        moveTimer_ -= moveInterval_;
        for (const GameEvent& event : world_.step()) onGameEvent(event);
    }
}

void PlayScene::onGameEvent(const GameEvent& event) {
    switch (event.type) {
        case GameEvent::Type::FruitEaten:
            moveInterval_ = std::max(session_.config.minMoveInterval,
                                     moveInterval_ - session_.config.speedupPerFruit);
            break;
        case GameEvent::Type::SnakeDied:
        case GameEvent::Type::BoardCleared:
            // ส่งคะแนนทันทีที่เกมจบ (ไม่รอหน้า Game Over) กดออกระหว่างนั้นคะแนนก็ไม่หาย
            newBest_ = world_.score() > personalBest_;
            session_.leaderboard.submitScore(session_.playerName, world_.score());
            break;
        case GameEvent::Type::WallsWarning:
        case GameEvent::Type::WallsSolid:
        case GameEvent::Type::WallsOpen:
            break;  // render อ่าน wallState() เอง — ตรงนี้ไว้ต่อเสียง / effect ในอนาคต
    }
}

void PlayScene::render(engine::render::FrameBuffer& frame) {
    const int w = world_.width();
    const int h = world_.height();
    // แต่ละช่องกว้าง 2 คอลัมน์ ("@ ") เพราะตัวอักษรใน terminal สูงกว่ากว้างราว 2 เท่า
    const int boxWidth = w * 2 + 3;
    const int boxHeight = h + 2;

    if (frame.width() < boxWidth || frame.height() < boxHeight + 3) {
        frame.textCentered(frame.height() / 2, "Terminal too small - please enlarge", Color::Red, true);
        return;
    }

    const int left = (frame.width() - boxWidth) / 2;
    const int top = std::max(2, (frame.height() - boxHeight) / 2);
    auto cellX = [&](int x) { return left + 2 + x * 2; };
    auto cellY = [&](int y) { return top + 1 + y; };

    // HUD
    frame.text(left, top - 2, "PLAYER " + session_.playerName, Color::Green, true);
    frame.text(left, top - 1, "SCORE " + std::to_string(world_.score()), Color::White, true);
    const std::string best = "BEST " + std::to_string(std::max(personalBest_, world_.score()));
    frame.text(left + boxWidth - static_cast<int>(best.size()), top - 1, best, Color::Cyan);

    // สถานะกำแพง: เทา = ทะลุได้, กระพริบ = เตือน, แดง = ชนตาย
    Color wallColor = Color::Gray;
    bool wallBold = false;
    switch (world_.wallState()) {
        case WallState::Open:
            break;
        case WallState::Warning: {
            char timeLeft[32];
            std::snprintf(timeLeft, sizeof(timeLeft), "%.1fs", world_.wallTimeLeft());
            if (static_cast<int>(world_.wallTimeLeft() * 8.0) % 2 == 0) wallColor = Color::Red;
            frame.textCentered(top - 1, std::string("! WALLS CLOSING ") + timeLeft + " !", Color::Yellow, true);
            break;
        }
        case WallState::Solid:
            wallColor = Color::Red;
            wallBold = true;
            frame.textCentered(top - 1, "LOCKED! eat +" + std::to_string(world_.wallUnlockRemaining()) + " to unlock",
                               Color::Red, true);
            break;
    }

    // กรอบ + พื้น
    for (int x = 0; x < boxWidth; ++x) {
        frame.put(left + x, top, '#', wallColor, wallBold);
        frame.put(left + x, top + boxHeight - 1, '#', wallColor, wallBold);
    }
    for (int y = 0; y < h; ++y) {
        frame.put(left, cellY(y), '#', wallColor, wallBold);
        frame.put(left + boxWidth - 1, cellY(y), '#', wallColor, wallBold);
        for (int x = 0; x < w; ++x) frame.put(cellX(x), cellY(y), '.', Color::Gray);
    }

    // ผลไม้
    if (const auto fruit = world_.fruit()) {
        frame.put(cellX(fruit->x), cellY(fruit->y), '*', Color::Red, true);
    }

    // งู (วาดจากหางไปหัว ให้หัวทับตัวเสมอ)
    const WorldStatus status = world_.status();
    const Color bodyColor = status == WorldStatus::Won ? Color::Yellow : Color::Green;
    const auto& body = world_.snake().body();
    for (auto it = body.rbegin(); it != body.rend(); ++it) {
        frame.put(cellX(it->x), cellY(it->y), 'o', bodyColor);
    }
    const engine::Vec2i head = world_.snake().head();
    if (status == WorldStatus::Dead) {
        frame.put(cellX(head.x), cellY(head.y), 'X', Color::Red, true);
    } else {
        frame.put(cellX(head.x), cellY(head.y), '@', bodyColor, true);
    }

    // Overlay
    const int midY = top + boxHeight / 2;
    if (paused_) {
        frame.textCentered(midY, "  PAUSED  ", Color::Yellow, true);
    } else if (status == WorldStatus::Dead) {
        frame.textCentered(midY, "  OUCH!  ", Color::Red, true);
    } else if (status == WorldStatus::Won) {
        frame.textCentered(midY, "  PERFECT!  ", Color::Yellow, true);
    }

    frame.textCentered(top + boxHeight, "Arrows: move   P/Esc: pause   Q: quit", Color::Gray);
}

}  // namespace snake
