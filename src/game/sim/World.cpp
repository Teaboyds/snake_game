#include "game/sim/World.h"

#include <algorithm>

namespace snake {

World::World(const GameConfig& config, std::uint32_t seed)
    : config_(config),
      rng_(seed),
      snake_({config.boardWidth / 2, config.boardHeight / 2}, Direction::Right, config.initialLength),
      nextWallScore_(config.wallEventScoreInterval) {
    spawnFruit();
}

void World::submit(const TurnCommand& command) {
    if (status_ != WorldStatus::Running) return;
    snake_.queueTurn(command.direction);
}

std::vector<GameEvent> World::step() {
    std::vector<GameEvent> events;
    if (status_ != WorldStatus::Running) return events;
    ++tick_;

    snake_.applyNextTurn();
    engine::Vec2i next = snake_.head() + toVector(snake_.direction());

    if (!inBounds(next)) {
        if (!config_.wrapWalls || wallState_ == WallState::Solid) {
            status_ = WorldStatus::Dead;
            events.push_back({GameEvent::Type::SnakeDied, snake_.head()});
            return events;
        }
        next = wrap(next);
    }

    const bool eating = fruit_ && *fruit_ == next;
    if (snake_.wouldCollide(next, eating)) {
        status_ = WorldStatus::Dead;
        events.push_back({GameEvent::Type::SnakeDied, next});
        return events;
    }

    snake_.moveTo(next, eating);

    if (eating) {
        score_ += config_.scorePerFruit;
        events.push_back({GameEvent::Type::FruitEaten, next});
        checkWallUnlock(events);
        checkWallEventTrigger(events);
        spawnFruit();
        if (!fruit_) {  // ไม่มีช่องว่างเหลือแล้ว = ชนะ
            status_ = WorldStatus::Won;
            events.push_back({GameEvent::Type::BoardCleared, next});
        }
    }
    return events;
}

std::vector<GameEvent> World::advance(double dt) {
    constexpr double kEpsilon = 1e-9;  // กัน floating-point error จากการบวก dt ซ้ำ ๆ

    std::vector<GameEvent> events;
    if (status_ != WorldStatus::Running || wallState_ != WallState::Warning) return events;

    wallTimer_ -= dt;
    if (wallTimer_ > kEpsilon) return events;

    wallState_ = WallState::Solid;
    wallTimer_ = 0.0;
    wallUnlockAtScore_ = score_ + config_.wallUnlockScore;
    events.push_back({GameEvent::Type::WallsSolid, snake_.head()});
    // กำแพงปิดตอนที่งูยังพาดข้ามขอบอยู่ = โดนกำแพงหนีบตาย
    if (snake_.crossesEdge()) {
        status_ = WorldStatus::Dead;
        events.push_back({GameEvent::Type::SnakeDied, snake_.head()});
    }
    return events;
}

int World::wallUnlockRemaining() const {
    if (wallState_ != WallState::Solid) return 0;
    return std::max(0, wallUnlockAtScore_ - score_);
}

void World::checkWallUnlock(std::vector<GameEvent>& events) {
    if (wallState_ != WallState::Solid || score_ < wallUnlockAtScore_) return;
    wallState_ = WallState::Open;
    events.push_back({GameEvent::Type::WallsOpen, snake_.head()});
}

void World::checkWallEventTrigger(std::vector<GameEvent>& events) {
    const int interval = config_.wallEventScoreInterval;
    if (interval <= 0 || score_ < nextWallScore_) return;

    while (score_ >= nextWallScore_) nextWallScore_ += interval;
    if (wallState_ != WallState::Open) return;  // event ก่อนหน้ายังไม่จบ ข้ามรอบนี้

    wallState_ = WallState::Warning;
    wallTimer_ = config_.wallWarningDuration;
    events.push_back({GameEvent::Type::WallsWarning, snake_.head()});
}

bool World::inBounds(engine::Vec2i p) const {
    return p.x >= 0 && p.y >= 0 && p.x < config_.boardWidth && p.y < config_.boardHeight;
}

engine::Vec2i World::wrap(engine::Vec2i p) const {
    const int w = config_.boardWidth;
    const int h = config_.boardHeight;
    return {((p.x % w) + w) % w, ((p.y % h) + h) % h};
}

void World::spawnFruit() {
    std::vector<engine::Vec2i> freeCells;
    for (int y = 0; y < config_.boardHeight; ++y) {
        for (int x = 0; x < config_.boardWidth; ++x) {
            const engine::Vec2i cell{x, y};
            if (!snake_.wouldCollide(cell, true)) freeCells.push_back(cell);
        }
    }
    if (freeCells.empty()) {
        fruit_.reset();
        return;
    }
    std::uniform_int_distribution<std::size_t> pick(0, freeCells.size() - 1);
    fruit_ = freeCells[pick(rng_)];
}

}  // namespace snake
