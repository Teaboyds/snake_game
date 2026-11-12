// Unit tests ของ simulation — รันได้โดยไม่ต้องเปิด terminal เพราะ World ไม่มี I/O
#include <cstdlib>
#include <iostream>
#include <vector>

#include <unistd.h>

#include "backend/redis/RedisClient.h"
#include "engine/input/InputSystem.h"
#include "game/services/InMemoryLeaderboard.h"
#include "game/services/RedisLeaderboard.h"
#include "game/sim/World.h"

using backend::redis::Reply;
using engine::Vec2i;
using engine::input::Action;
using snake::Direction;
using snake::GameConfig;
using snake::GameEvent;
using snake::World;
using snake::WorldStatus;

namespace {

int g_failures = 0;
bool g_skipped = false;  // test ตั้งเป็น true ถ้าข้ามเพราะไม่มี dependency (เช่น Redis ไม่เปิด)

#define CHECK(expr)                                                                  \
    do {                                                                             \
        if (!(expr)) {                                                               \
            std::cerr << "  " << __FILE__ << ":" << __LINE__ << " CHECK(" #expr ")\n"; \
            ++g_failures;                                                            \
        }                                                                            \
    } while (0)

bool hasEvent(const std::vector<GameEvent>& events, GameEvent::Type type) {
    for (const GameEvent& e : events) {
        if (e.type == type) return true;
    }
    return false;
}

void testMovesForward() {
    World world(GameConfig{}, 1);
    world.setFruitForTesting({0, 0});
    const Vec2i start = world.snake().head();
    world.step();
    CHECK(world.snake().head() == (Vec2i{start.x + 1, start.y}));
    CHECK(world.snake().length() == 3);
}

void testEatingGrowsAndScores() {
    World world(GameConfig{}, 1);
    const Vec2i start = world.snake().head();
    world.setFruitForTesting({start.x + 1, start.y});
    const auto events = world.step();
    CHECK(hasEvent(events, GameEvent::Type::FruitEaten));
    CHECK(world.snake().length() == 4);
    CHECK(world.score() == 10);
    CHECK(world.fruit().has_value());
    CHECK(!world.snake().wouldCollide(*world.fruit(), true));  // ผลไม้ใหม่ไม่เกิดบนตัวงู
}

void testReverseTurnIgnored() {
    World world(GameConfig{}, 1);
    world.setFruitForTesting({0, 0});
    const Vec2i start = world.snake().head();
    world.submit({Direction::Left});
    world.step();
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.snake().head() == (Vec2i{start.x + 1, start.y}));
}

void testSelfCollisionKills() {
    GameConfig config;
    config.initialLength = 5;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    world.submit({Direction::Up});
    world.step();
    world.submit({Direction::Left});
    world.step();
    world.submit({Direction::Down});
    const auto events = world.step();
    CHECK(hasEvent(events, GameEvent::Type::SnakeDied));
    CHECK(world.status() == WorldStatus::Dead);
}

void testChasingOwnTailIsSafe() {
    GameConfig config;
    config.initialLength = 4;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    world.submit({Direction::Up});
    world.step();
    world.submit({Direction::Left});
    world.step();
    world.submit({Direction::Down});
    world.step();  // หัวเดินเข้าช่องที่หางเพิ่งออกไป
    CHECK(world.status() == WorldStatus::Running);
}

void testWrapAroundWalls() {
    GameConfig config;
    config.boardWidth = 6;
    config.boardHeight = 6;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    world.step();
    world.step();
    world.step();  // x: 3 -> 4 -> 5 -> 0
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.snake().head() == (Vec2i{0, 3}));
}

void testSolidWallsKill() {
    GameConfig config;
    config.boardWidth = 6;
    config.boardHeight = 6;
    config.wrapWalls = false;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    world.step();
    world.step();
    world.step();
    CHECK(world.status() == WorldStatus::Dead);
}

// กระดาน 6x6 หัวอยู่ (3,3) หันขวา ผลไม้อยู่ข้างหน้า กินแล้วถึงแต้ม trigger ทันที
World makeWallEventWorld() {
    GameConfig config;
    config.boardWidth = 6;
    config.boardHeight = 6;
    config.wallEventScoreInterval = 10;
    World world(config, 1);
    world.setFruitForTesting({4, 3});
    return world;
}

void testWallEventCycle() {
    GameConfig config;
    config.boardWidth = 6;
    config.boardHeight = 6;
    config.wallEventScoreInterval = 20;
    config.wallUnlockScore = 10;
    World world(config, 1);

    world.setFruitForTesting({4, 3});
    world.step();  // 10 แต้ม ยังไม่ถึง
    CHECK(world.wallState() == snake::WallState::Open);

    world.setFruitForTesting({5, 3});
    CHECK(hasEvent(world.step(), GameEvent::Type::WallsWarning));  // 20 แต้ม
    CHECK(world.wallState() == snake::WallState::Warning);

    CHECK(world.advance(1.0).empty());
    CHECK(hasEvent(world.advance(1.0), GameEvent::Type::WallsSolid));
    CHECK(world.wallState() == snake::WallState::Solid);
    CHECK(world.wallUnlockRemaining() == 10);

    CHECK(world.advance(60.0).empty());  // เวลาผ่านไปเท่าไหร่ก็ไม่ปลดล็อค
    CHECK(world.wallState() == snake::WallState::Solid);

    world.setFruitForTesting({5, 4});
    world.submit({Direction::Down});
    CHECK(hasEvent(world.step(), GameEvent::Type::WallsOpen));  // กินครบ 10 แต้ม
    CHECK(world.wallState() == snake::WallState::Open);
    CHECK(world.wallUnlockRemaining() == 0);
}

void testWarningWallsStillWrap() {
    World world = makeWallEventWorld();
    world.step();  // (4,3) + warning
    world.step();  // (5,3)
    world.step();  // ทะลุไป (0,3)
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.snake().head() == (Vec2i{0, 3}));
}

void testSolidWallsKillOnHit() {
    World world = makeWallEventWorld();
    world.step();      // (4,3)
    world.advance(2.0);  // solid — งูยังไม่พาดขอบ จึงรอด
    CHECK(world.status() == WorldStatus::Running);
    world.step();  // (5,3)
    world.step();  // ชนกำแพงแดง
    CHECK(world.status() == WorldStatus::Dead);
}

void testSnakeStraddlingEdgeDiesWhenWallsTurnSolid() {
    World world = makeWallEventWorld();
    world.step();  // (4,3)
    world.step();  // (5,3)
    world.step();  // (0,3) — ตัวยังค้างอยู่อีกฝั่ง
    CHECK(world.snake().crossesEdge());
    const auto events = world.advance(2.0);
    CHECK(hasEvent(events, GameEvent::Type::SnakeDied));
    CHECK(world.status() == WorldStatus::Dead);
}

void testSameSeedIsDeterministic() {
    World a(GameConfig{}, 42);
    World b(GameConfig{}, 42);
    CHECK(a.fruit() == b.fruit());
}

void testParseArrowKeys() {
    const auto actions = engine::input::parseInput("\x1b[A\x1b[D\x1bOCq").actions;
    CHECK((actions == std::vector<Action>{Action::Up, Action::Left, Action::Right, Action::Quit}));
    CHECK((engine::input::parseInput("\x1b").actions == std::vector<Action>{Action::Pause}));
}

void testTextModeTypesLettersInsteadOfActions() {
    const auto frame = engine::input::parseInput("qwa\x7fz\r", engine::input::InputMode::Text);
    CHECK(frame.text == "qwa\bz");  // q ไม่ใช่ Quit, w ไม่ใช่ Up
    CHECK((frame.actions == std::vector<Action>{Action::Confirm}));
    CHECK(engine::input::parseInput("\x03", engine::input::InputMode::Text).has(Action::Quit));
}

// ---------- Redis protocol ----------

std::optional<Reply> parseAll(std::string_view data) {
    std::size_t consumed = 0;
    auto reply = backend::redis::parseReply(data, consumed);
    if (reply && consumed != data.size()) return std::nullopt;
    return reply;
}

void testEncodeCommand() {
    CHECK(backend::redis::encodeCommand({"SET", "k", "v"}) == "*3\r\n$3\r\nSET\r\n$1\r\nk\r\n$1\r\nv\r\n");
}

void testParseReplies() {
    CHECK(parseAll("+OK\r\n")->str == "OK");
    CHECK(parseAll("-ERR bad\r\n")->isError());
    CHECK(parseAll(":42\r\n")->integer == 42);
    CHECK(parseAll("$5\r\nhello\r\n")->str == "hello");
    CHECK(parseAll("$-1\r\n")->isNil());

    const auto array = parseAll("*2\r\n$3\r\nbob\r\n$2\r\n80\r\n");
    CHECK(array && array->elements.size() == 2);
    CHECK(array && array->elements[0].str == "bob" && array->elements[1].str == "80");
}

void testParseIncompleteReplyWaitsForMoreData() {
    std::size_t consumed = 0;
    CHECK(!backend::redis::parseReply("$5\r\nhel", consumed));
    CHECK(!backend::redis::parseReply("*2\r\n$3\r\nbob\r\n", consumed));
    CHECK(!backend::redis::parseReply(":4", consumed));
}

// ---------- Leaderboard ----------

// สัญญา (contract) ที่ Leaderboard ทุกตัวต้องทำตาม — รันกับทั้ง in-memory และ Redis จริง
void checkLeaderboardContract(snake::Leaderboard& board) {
    CHECK(!board.standing("alice").has_value());

    board.submitScore("alice", 50);
    board.submitScore("bob", 80);
    board.submitScore("alice", 30);  // ชื่อซ้ำ = คนเดิม, คะแนนต่ำกว่า best ไม่ทับ
    board.submitScore("carol", 10);

    const auto alice = board.standing("alice");
    CHECK(alice && alice->bestScore == 50);
    CHECK(alice && alice->gamesPlayed == 2);
    CHECK(alice && alice->rank == 2);

    const auto top = board.top(2);
    CHECK(top.size() == 2);
    CHECK(top.size() == 2 && top[0].name == "bob" && top[0].score == 80);
    CHECK(top.size() == 2 && top[1].name == "alice" && top[1].score == 50);

    board.submitScore("alice", 120);
    const auto aliceAgain = board.standing("alice");
    CHECK(aliceAgain && aliceAgain->bestScore == 120 && aliceAgain->rank == 1 && aliceAgain->gamesPlayed == 3);
    CHECK(board.top(10).size() == 3);
}

void testInMemoryLeaderboard() {
    snake::InMemoryLeaderboard board;
    checkLeaderboardContract(board);
}

void testRedisLeaderboard() {
    const char* hostEnv = std::getenv("SNAKE_REDIS_HOST");
    const char* portEnv = std::getenv("SNAKE_REDIS_PORT");
    const std::string host = hostEnv ? hostEnv : "127.0.0.1";
    const int port = portEnv ? std::atoi(portEnv) : 6380;

    // prefix เฉพาะของ test run นี้ ไม่ปนกับข้อมูลจริง
    const std::string prefix = "snake:test:" + std::to_string(::getpid()) + ":";
    snake::RedisLeaderboard board(host, port, prefix);
    if (!board.connect()) {
        g_skipped = true;
        return;
    }

    checkLeaderboardContract(board);

    backend::redis::Client cleanup(host, port, std::chrono::milliseconds(300));
    if (cleanup.connect()) {
        cleanup.command({"DEL", board.leaderboardKey(), board.playerKey("alice"), board.playerKey("bob"),
                         board.playerKey("carol")});
    }
}

}  // namespace

int main() {
    struct Test {
        const char* name;
        void (*fn)();
    };
    const Test tests[] = {
        {"moves forward", testMovesForward},
        {"eating grows and scores", testEatingGrowsAndScores},
        {"reverse turn ignored", testReverseTurnIgnored},
        {"self collision kills", testSelfCollisionKills},
        {"chasing own tail is safe", testChasingOwnTailIsSafe},
        {"wrap around walls", testWrapAroundWalls},
        {"solid walls kill", testSolidWallsKill},
        {"wall event cycle", testWallEventCycle},
        {"warning walls still wrap", testWarningWallsStillWrap},
        {"solid walls kill on hit", testSolidWallsKillOnHit},
        {"straddling edge dies when walls turn solid", testSnakeStraddlingEdgeDiesWhenWallsTurnSolid},
        {"same seed is deterministic", testSameSeedIsDeterministic},
        {"parse arrow keys", testParseArrowKeys},
        {"text mode types letters instead of actions", testTextModeTypesLettersInsteadOfActions},
        {"redis: encode command", testEncodeCommand},
        {"redis: parse replies", testParseReplies},
        {"redis: incomplete reply waits for more data", testParseIncompleteReplyWaitsForMoreData},
        {"leaderboard: in-memory", testInMemoryLeaderboard},
        {"leaderboard: redis (live)", testRedisLeaderboard},
    };

    for (const Test& t : tests) {
        const int before = g_failures;
        g_skipped = false;
        t.fn();
        const char* status = g_failures != before ? "[FAIL] " : g_skipped ? "[SKIP] " : "[PASS] ";
        std::cout << status << t.name << '\n';
    }
    std::cout << (g_failures == 0 ? "All tests passed\n" : "Some tests failed\n");
    return g_failures == 0 ? 0 : 1;
}
