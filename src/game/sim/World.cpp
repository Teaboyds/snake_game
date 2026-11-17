#include "game/sim/World.h"

#include <algorithm>
#include <cctype>

namespace snake {

namespace {
constexpr double kEpsilon = 1e-9;  // กัน floating-point error จากการบวก dt ซ้ำ ๆ
}

World::World(const GameConfig& config, std::uint32_t seed)
    : config_(config),
      rng_(seed),
      snake_({config.boardWidth / 2, config.boardHeight / 2}, Direction::Right, config.initialLength),
      nextWallScore_(config.wallEventScoreInterval),
      challengeFruitSpawnTimer_(config.challengeFruitInterval) {
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
        // รอดจากกำแพง (มี Life / Invulnerability) = ทะลุไปอีกฝั่งเหมือนกำแพงเปิด
        const bool wallBlocks = !config_.wrapWalls || wallState_ == WallState::Solid;
        if (wallBlocks && !survivesHit(snake_.head(), events)) return events;
        next = wrap(next);
    }

    const bool eating = fruit_ && *fruit_ == next;
    if (snake_.wouldCollide(next, eating) && !survivesHit(next, events)) return events;

    snake_.moveTo(next, eating);

    // Challenge Fruit ไม่ให้แต้ม ไม่ทำให้โต ได้แค่ Typing Challenge
    if (challengeFruit_ && *challengeFruit_ == next) {
        challengeFruit_.reset();
        challengeFruitSpawnTimer_ = config_.challengeFruitInterval;
        startChallenge(events);
    }

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
    std::vector<GameEvent> events;
    if (status_ != WorldStatus::Running) return events;

    if (invulnerable_) {
        invulnerableTimer_ -= dt;
        // หมดเวลาตอนหัวยังทับตัวเอง = ยืดไปจนหัวพ้น ไม่งั้นจะตายทันทีแบบไม่แฟร์
        if (invulnerableTimer_ <= kEpsilon && !snake_.headOverlapsBody()) invulnerable_ = false;
    }
    advanceWalls(dt, events);
    if (status_ != WorldStatus::Running) return events;  // ตายแล้ว ไม่มี event อื่นตามหลัง
    advanceChallengeFruit(dt);

    if (challenge_) {
        challenge_->timeLeft -= dt;
        if (challenge_->timeLeft <= kEpsilon) {
            challenge_.reset();
            events.push_back({GameEvent::Type::TypingChallengeFailed, snake_.head()});
        }
    }
    return events;
}

void World::advanceChallengeFruit(double dt) {
    if (challenge_) return;  // ระหว่าง Typing Challenge ไม่นับเวลา
    if (challengeFruit_) {
        challengeFruitTimeLeft_ -= dt;
        if (challengeFruitTimeLeft_ > kEpsilon) return;
        challengeFruit_.reset();  // ไม่ไปกินทันเวลา หายไป
        challengeFruitSpawnTimer_ = config_.challengeFruitInterval;
        return;
    }
    challengeFruitSpawnTimer_ -= dt;
    if (challengeFruitSpawnTimer_ > kEpsilon) return;

    challengeFruit_ = randomFreeCell();
    if (challengeFruit_) {
        challengeFruitTimeLeft_ = config_.challengeFruitLifetime;
    } else {  // กระดานเต็ม รอรอบหน้า
        challengeFruitSpawnTimer_ = config_.challengeFruitInterval;
    }
}

void World::advanceWalls(double dt, std::vector<GameEvent>& events) {
    if (wallState_ != WallState::Warning) return;

    wallTimer_ -= dt;
    if (wallTimer_ > kEpsilon) return;

    wallState_ = WallState::Solid;
    wallTimer_ = 0.0;
    wallUnlockAtScore_ = score_ + config_.wallUnlockScore;
    events.push_back({GameEvent::Type::WallsSolid, snake_.head()});
    // กำแพงปิดตอนที่งูยังพาดข้ามขอบอยู่ = โดนกำแพงหนีบ (ตาย ถ้าไม่มี Life)
    if (snake_.crossesEdge()) survivesHit(snake_.head(), events);
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

std::vector<GameEvent> World::type(std::string_view text) {
    std::vector<GameEvent> events;
    if (status_ != WorldStatus::Running || !challenge_) return events;

    std::string& typed = challenge_->typed;
    for (char c : text) {
        if (c == '\b') {
            if (!typed.empty()) typed.pop_back();
            continue;
        }
        if (typed.size() >= challenge_->word.size()) continue;  // พิมพ์เกินความยาวคำ ไม่รับ
        typed.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        if (typed == challenge_->word) {
            challenge_.reset();
            events.push_back({GameEvent::Type::TypingChallengePassed, snake_.head()});
            if (lives_ < config_.maxLives) {
                ++lives_;
            } else {  // Life เต็มแล้ว ได้แต้มแทน (แต้มนี้นับรวมกับ wall event ด้วย)
                score_ += config_.fullLivesBonusScore;
                checkWallUnlock(events);
                checkWallEventTrigger(events);
            }
            break;
        }
    }
    return events;
}

bool World::survivesHit(engine::Vec2i at, std::vector<GameEvent>& events) {
    if (invulnerable()) return true;
    if (lives_ > 0) {
        --lives_;
        invulnerable_ = true;
        invulnerableTimer_ = config_.invulnerabilitySeconds;
        events.push_back({GameEvent::Type::LifeLost, at});
        return true;
    }
    status_ = WorldStatus::Dead;
    events.push_back({GameEvent::Type::SnakeDied, at});
    return false;
}

void World::startChallenge(std::vector<GameEvent>& events) {
    if (config_.challengeWords.empty()) return;  // config ไม่มีคำ = ปิด challenge
    std::uniform_int_distribution<std::size_t> pick(0, config_.challengeWords.size() - 1);
    const std::string& word = config_.challengeWords[pick(rng_)];
    challenge_ = TypingChallenge{word, "", word.size() * config_.challengeSecondsPerLetter};
    events.push_back({GameEvent::Type::TypingChallengeStarted, snake_.head()});
}

bool World::inBounds(engine::Vec2i p) const {
    return p.x >= 0 && p.y >= 0 && p.x < config_.boardWidth && p.y < config_.boardHeight;
}

engine::Vec2i World::wrap(engine::Vec2i p) const {
    const int w = config_.boardWidth;
    const int h = config_.boardHeight;
    return {((p.x % w) + w) % w, ((p.y % h) + h) % h};
}

// สุ่มช่องที่ไม่มีงูและไม่มีผลไม้ใด ๆ อยู่ (ไม่มีเลย = กระดานเต็ม)
std::optional<engine::Vec2i> World::randomFreeCell() {
    std::vector<engine::Vec2i> cells;
    for (int y = 0; y < config_.boardHeight; ++y) {
        for (int x = 0; x < config_.boardWidth; ++x) {
            const engine::Vec2i cell{x, y};
            if (snake_.wouldCollide(cell, true) || fruit_ == cell || challengeFruit_ == cell) continue;
            cells.push_back(cell);
        }
    }
    if (cells.empty()) return std::nullopt;
    std::uniform_int_distribution<std::size_t> pick(0, cells.size() - 1);
    return cells[pick(rng_)];
}

void World::spawnFruit() {
    fruit_ = randomFreeCell();
    // ช่องว่างสุดท้ายมี Challenge Fruit อยู่: ผลไม้ปกติแทนที่ ไม่งั้นเกมจะคิดว่ากระดานเต็มแล้ว
    if (!fruit_ && challengeFruit_) {
        fruit_ = challengeFruit_;
        challengeFruit_.reset();
        challengeFruitSpawnTimer_ = config_.challengeFruitInterval;
    }
}

}  // namespace snake
