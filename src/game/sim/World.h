#pragma once

#include <cstdint>
#include <optional>
#include <random>
#include <vector>

#include "engine/core/Vec2.h"
#include "game/GameConfig.h"
#include "game/sim/Snake.h"

namespace snake {

// Command: สิ่งที่ "ผู้เล่นขอ" ส่งเข้า simulation
// ในเกม online client จะส่ง command แบบนี้ไป server ไม่ได้ส่งปุ่มที่กด
struct TurnCommand {
    Direction direction;
};

// Event: สิ่งที่ "เกิดขึ้นแล้ว" ใน simulation ส่งออกไปให้ระบบอื่น (UI, เสียง, analytics, network)
struct GameEvent {
    enum class Type { FruitEaten, SnakeDied, BoardCleared, WallsWarning, WallsSolid, WallsOpen };
    Type type;
    engine::Vec2i position;
};

enum class WorldStatus { Running, Dead, Won };

// Open -> (ถึงแต้มที่กำหนด) -> Warning (กระพริบ, ยังทะลุได้) -> Solid (แดง, ชน = ตาย)
//      -> (เก็บแต้มเพิ่มครบ) -> Open
enum class WallState { Open, Warning, Solid };

// Simulation ล้วน ๆ: ไม่มี I/O, ไม่มีเวลาจริง, ไม่มี render
// input = commands + seed, output = state + events  =>  deterministic 100%
// โค้ดนี้ยกไปรันบน game server (authoritative server) ได้ทันที
class World {
public:
    World(const GameConfig& config, std::uint32_t seed);

    void submit(const TurnCommand& command);
    std::vector<GameEvent> step();  // เดินเกม 1 tick (งูขยับ 1 ช่อง)

    // เดินเวลาของระบบที่นับเป็นวินาที (wall event) — caller ต้องส่ง dt คงที่
    // เพื่อให้ยัง deterministic (PlayScene ส่ง fixed dt จาก Application)
    std::vector<GameEvent> advance(double dt);

    const Snake& snake() const { return snake_; }
    std::optional<engine::Vec2i> fruit() const { return fruit_; }
    int score() const { return score_; }
    WorldStatus status() const { return status_; }
    std::uint64_t tick() const { return tick_; }
    int width() const { return config_.boardWidth; }
    int height() const { return config_.boardHeight; }
    WallState wallState() const { return wallState_; }
    double wallTimeLeft() const { return wallTimer_; }
    int wallUnlockRemaining() const;  // แต้มที่ยังต้องเก็บเพื่อปลดล็อคกำแพงแดง

    void setFruitForTesting(engine::Vec2i position) { fruit_ = position; }

private:
    bool inBounds(engine::Vec2i p) const;
    engine::Vec2i wrap(engine::Vec2i p) const;
    void spawnFruit();
    void checkWallEventTrigger(std::vector<GameEvent>& events);
    void checkWallUnlock(std::vector<GameEvent>& events);

    GameConfig config_;
    std::mt19937 rng_;
    Snake snake_;
    std::optional<engine::Vec2i> fruit_;
    int score_ = 0;
    WorldStatus status_ = WorldStatus::Running;
    std::uint64_t tick_ = 0;

    WallState wallState_ = WallState::Open;
    double wallTimer_ = 0.0;  // เวลาที่เหลือของ Warning
    int wallUnlockAtScore_ = 0;
    int nextWallScore_;
};

}  // namespace snake
