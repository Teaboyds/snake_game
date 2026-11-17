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
// หลัง challenge จบ ยังอยู่ในโหมดพิมพ์ต่ออีกนิด ตัวอักษรที่ยังพิมพ์ค้างอยู่จะได้ไม่กลายเป็นปุ่มเลี้ยว (เช่น d ใน adder)
constexpr double kTypingGrace = 0.5;
constexpr double kChallengeTimeLow = 2.0;  // เวลาเหลือน้อยกว่านี้ ตัวเลขเป็นสีแดง
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
    if (!paused_) {
        for (const GameEvent& event : world_.type(input.text)) onGameEvent(event);
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

    elapsed_ += dt;
    for (const GameEvent& event : world_.advance(dt)) onGameEvent(event);
    if (world_.status() != WorldStatus::Running) return;

    if (typingGrace_ > 0.0) typingGrace_ -= dt;

    auto interval = [&] {
        const double factor = session_.config.challengeSpeedFactor;
        return world_.challenge() && factor > 0.0 ? moveInterval_ / factor : moveInterval_;
    };
    // challenge จบ = interval สั้นลงทันที ตัดเวลาที่สะสมไว้ ไม่งั้นงูกระโดด 2 ช่องในเฟรมเดียว
    moveTimer_ = std::min(moveTimer_ + dt, interval());
    while (moveTimer_ >= interval() && world_.status() == WorldStatus::Running) {
        moveTimer_ -= interval();
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
            newBest_ = world_.score() > personalBest_;
            session_.leaderboard.submitScore(session_.playerName, world_.score());
            break;
        case GameEvent::Type::TypingChallengePassed:
        case GameEvent::Type::TypingChallengeFailed:
            typingGrace_ = kTypingGrace;
            break;
        case GameEvent::Type::WallsWarning:
        case GameEvent::Type::WallsSolid:
        case GameEvent::Type::WallsOpen:
        case GameEvent::Type::TypingChallengeStarted:
        case GameEvent::Type::LifeLost:
            break;  // render อ่าน state จาก World เอง — ตรงนี้ไว้ต่อเสียง / effect ในอนาคต
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
    const int maxLives = session_.config.maxLives;
    int livesX = left + boxWidth - (6 + maxLives * 3);
    frame.text(livesX, top - 2, "LIFE ", Color::White, true);
    livesX += 5;
    for (int i = 0; i < maxLives; ++i, livesX += 3) {
        if (i < world_.lives()) frame.text(livesX + 1, top - 2, "<3", Color::Red, true);
        else frame.text(livesX + 1, top - 2, "--", Color::Gray);
    }
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

    const bool blinkOn = static_cast<int>(elapsed_ * 8.0) % 2 == 0;

    // ผลไม้
    if (const auto fruit = world_.fruit()) {
        frame.put(cellX(fruit->x), cellY(fruit->y), '*', Color::Red, true);
    }
    // Challenge Fruit กะพริบช่วงท้ายก่อนหายไป
    if (const auto fruit = world_.challengeFruit();
        fruit && (world_.challengeFruitTimeLeft() > session_.config.challengeFruitBlinkSeconds || blinkOn)) {
        frame.put(cellX(fruit->x), cellY(fruit->y), '$', Color::Yellow, true);
    }

    // งู (วาดจากหางไปหัว ให้หัวทับตัวเสมอ)
    const WorldStatus status = world_.status();
    Color bodyColor = status == WorldStatus::Won ? Color::Yellow : Color::Green;
    if (world_.invulnerable() && !blinkOn) bodyColor = Color::Gray;  // กะพริบตอน Invulnerability
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

    // ระหว่าง Typing Challenge บรรทัดคำแนะนำปุ่มกลายเป็นโจทย์ (ตอน pause ซ่อนคำ กันแอบดู)
    if (const auto& challenge = world_.challenge(); challenge && status == WorldStatus::Running) {
        char timeLeft[32];
        std::snprintf(timeLeft, sizeof(timeLeft), "  %.1fs", std::max(0.0, challenge->timeLeft));
        const std::string label = "TYPE: ";
        const int lineWidth = static_cast<int>(label.size() + challenge->word.size() + std::string(timeLeft).size());
        int x = (frame.width() - lineWidth) / 2;
        frame.text(x, top + boxHeight, label, Color::White, true);
        x += static_cast<int>(label.size());
        for (std::size_t i = 0; i < challenge->word.size(); ++i, ++x) {
            if (paused_) {
                frame.put(x, top + boxHeight, '?', Color::Gray);
            } else if (i >= challenge->typed.size()) {
                frame.put(x, top + boxHeight, challenge->word[i], Color::White, true);
            } else if (challenge->typed[i] == challenge->word[i]) {
                frame.put(x, top + boxHeight, challenge->word[i], Color::Green, true);
            } else {
                frame.put(x, top + boxHeight, challenge->typed[i], Color::Red, true);
            }
        }
        frame.text(x, top + boxHeight, timeLeft, challenge->timeLeft < kChallengeTimeLow ? Color::Red : Color::Yellow);
    } else {
        frame.textCentered(top + boxHeight, "Arrows: move   P/Esc: pause   Q: quit", Color::Gray);
    }
}

}  // namespace snake
