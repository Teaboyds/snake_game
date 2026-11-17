// Unit tests ของ simulation — รันได้โดยไม่ต้องเปิด terminal เพราะ World ไม่มี I/O
#include <cctype>
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

// กระดาน 6x6 หัวอยู่ (3,3) หันขวา ผลไม้ปกติอยู่ไกล ไม่ขวางทาง
World makeChallengeWorld(GameConfig config = GameConfig{}) {
    config.boardWidth = 6;
    config.boardHeight = 6;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    return world;
}

void testEatingChallengeFruitStartsTypingChallenge() {
    World world = makeChallengeWorld();
    world.setChallengeFruitForTesting({4, 3});
    const auto events = world.step();
    CHECK(hasEvent(events, GameEvent::Type::TypingChallengeStarted));
    CHECK(world.challenge().has_value());
    CHECK(world.challenge()->word.size() >= 4 && world.challenge()->word.size() <= 6);
    CHECK(!world.challengeFruit().has_value());
    CHECK(world.score() == 0);
    CHECK(world.snake().length() == 3);
}

void testTypingWordEarnsLife() {
    World world = makeChallengeWorld();
    world.setChallengeFruitForTesting({4, 3});
    world.step();
    CHECK(world.lives() == 0);

    const auto events = world.type(world.challenge()->word);
    CHECK(hasEvent(events, GameEvent::Type::TypingChallengePassed));
    CHECK(!world.challenge().has_value());
    CHECK(world.lives() == 1);
}

// ได้ Life แบบที่ผู้เล่นได้จริง: กิน Challenge Fruit ข้างหน้าแล้วพิมพ์คำให้ถูก (งูเดิน 1 ช่อง)
void earnLife(World& world) {
    const Vec2i ahead = world.snake().head() + snake::toVector(world.snake().direction());
    world.setChallengeFruitForTesting({(ahead.x + world.width()) % world.width(),
                                       (ahead.y + world.height()) % world.height()});
    world.step();
    world.type(world.challenge()->word);
}

void testSelfCollisionWithLifeSpendsIt() {
    GameConfig config;
    config.initialLength = 5;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    earnLife(world);
    CHECK(world.lives() == 1);

    world.submit({Direction::Up});
    world.step();
    world.submit({Direction::Left});
    world.step();
    world.submit({Direction::Down});
    const auto events = world.step();
    CHECK(hasEvent(events, GameEvent::Type::LifeLost));
    CHECK(!hasEvent(events, GameEvent::Type::SnakeDied));
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.lives() == 0);
    CHECK(world.invulnerable());
}

void testSolidWallWithLifeWrapsAndInvulnerabilityPassesAgain() {
    World world = makeChallengeWorld([] {
        GameConfig config;
        config.wallEventScoreInterval = 10;
        return config;
    }());
    earnLife(world);  // (4,3)
    world.setFruitForTesting({5, 3});
    world.step();  // (5,3) 10 แต้ม -> warning
    world.setFruitForTesting({0, 0});
    world.advance(2.0);
    CHECK(world.wallState() == snake::WallState::Solid);

    const auto events = world.step();  // ชนกำแพงแดง
    CHECK(hasEvent(events, GameEvent::Type::LifeLost));
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.snake().head() == (Vec2i{0, 3}));
    CHECK(world.lives() == 0);

    for (int i = 0; i < 5; ++i) world.step();  // (5,3)
    world.step();  // ชนอีกรอบระหว่าง Invulnerability
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.snake().head() == (Vec2i{0, 3}));
}

void testStraddlingSnakeWithLifeSurvivesWallsTurningSolid() {
    World world = makeChallengeWorld([] {
        GameConfig config;
        config.wallEventScoreInterval = 10;
        return config;
    }());
    earnLife(world);  // (4,3)
    world.setFruitForTesting({5, 3});
    world.step();  // (5,3) 10 แต้ม -> warning
    world.setFruitForTesting({0, 0});
    world.step();  // (0,3) — ตัวยังค้างอยู่อีกฝั่ง
    CHECK(world.snake().crossesEdge());

    const auto events = world.advance(2.0);
    CHECK(hasEvent(events, GameEvent::Type::LifeLost));
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.lives() == 0);
}

// งูยาว 5 เพิ่งชนตัวเองแล้วเสีย Life: หัวอยู่ (12,8) ทับหางพอดี หันลง
World makeSnakeThatJustSpentLife() {
    GameConfig config;
    config.initialLength = 5;
    World world(config, 1);
    world.setFruitForTesting({0, 0});
    earnLife(world);
    world.submit({Direction::Up});
    world.step();
    world.submit({Direction::Left});
    world.step();
    world.submit({Direction::Down});
    world.step();
    return world;
}

void testInvulnerabilityWearsOff() {
    World world = makeSnakeThatJustSpentLife();
    world.step();  // หัวลงไป (12,9) พ้นตัวเองแล้ว
    world.advance(1.9);
    CHECK(world.invulnerable());
    world.advance(0.1);
    CHECK(!world.invulnerable());
}

void testInvulnerabilityLastsUntilHeadLeavesBody() {
    World world = makeSnakeThatJustSpentLife();
    world.advance(2.0);  // หมดเวลาแล้ว แต่หัวยังทับหางอยู่
    CHECK(world.invulnerable());
    world.step();  // หัวลงไป (12,9) พ้นตัวเองแล้ว
    world.advance(1.0 / 60.0);
    CHECK(!world.invulnerable());
}

void testTypingChallengeFailsWhenTimeRunsOut() {
    World world = makeChallengeWorld();
    world.setChallengeFruitForTesting({4, 3});
    world.step();
    const double timeLimit = world.challenge()->word.size() * 1.0;  // 1 วินาทีต่อตัวอักษร

    world.advance(timeLimit - 0.1);
    CHECK(world.challenge().has_value());
    const auto events = world.advance(0.1);
    CHECK(hasEvent(events, GameEvent::Type::TypingChallengeFailed));
    CHECK(!world.challenge().has_value());
    CHECK(world.lives() == 0);
}

void testPassingWithFullLivesGivesBonusScore() {
    World world = makeChallengeWorld();
    for (int i = 0; i < 3; ++i) earnLife(world);
    CHECK(world.lives() == 3);

    earnLife(world);
    CHECK(world.lives() == 3);
    CHECK(world.score() == 20);
}

void testChallengeFruitAppearsEvery20SecondsAndLasts5() {
    World world = makeChallengeWorld();
    world.advance(19.9);
    CHECK(!world.challengeFruit().has_value());
    world.advance(0.1);
    CHECK(world.challengeFruit().has_value());
    CHECK(world.challengeFruit() != world.fruit());
    CHECK(!world.snake().wouldCollide(*world.challengeFruit(), true));

    world.advance(4.9);
    CHECK(world.challengeFruit().has_value());
    world.advance(0.1);
    CHECK(!world.challengeFruit().has_value());

    // นับ 20 วินาทีใหม่ตั้งแต่ตอนที่หายไป
    world.advance(19.9);
    CHECK(!world.challengeFruit().has_value());
    world.advance(0.1);
    CHECK(world.challengeFruit().has_value());
}

void testChallengeFruitTimerPausesDuringTypingChallenge() {
    World world = makeChallengeWorld([] {
        GameConfig config;
        config.challengeSecondsPerLetter = 10.0;  // challenge ยาวอย่างน้อย 40 วินาที
        return config;
    }());
    world.setChallengeFruitForTesting({4, 3});
    world.step();
    world.advance(30.0);
    CHECK(world.challenge().has_value());
    CHECK(!world.challengeFruit().has_value());

    world.type(world.challenge()->word);
    world.advance(19.9);
    CHECK(!world.challengeFruit().has_value());
    world.advance(0.1);
    CHECK(world.challengeFruit().has_value());
}

void testTypoCanBeFixedWithBackspaceAndCaseIsIgnored() {
    World world = makeChallengeWorld();
    world.setChallengeFruitForTesting({4, 3});
    world.step();
    std::string upper = world.challenge()->word;
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    world.type("z");  // ไม่มีคำไหนขึ้นต้นด้วย z
    CHECK(world.challenge()->typed == "z");
    world.type("\b");
    CHECK(world.challenge()->typed.empty());

    const auto events = world.type(upper);
    CHECK(hasEvent(events, GameEvent::Type::TypingChallengePassed));
    CHECK(world.lives() == 1);
}

void testChallengeFruitOnLastFreeCellDoesNotEndTheGame() {
    GameConfig config;
    config.boardWidth = 5;
    config.boardHeight = 1;
    World world(config, 1);  // งู (2,0) (1,0) (0,0) หันขวา เหลือช่องว่าง (3,0) (4,0)
    world.setFruitForTesting({3, 0});
    world.setChallengeFruitForTesting({4, 0});

    const auto events = world.step();  // กินผลไม้ ช่องว่างที่เหลือมีแต่ Challenge Fruit
    CHECK(!hasEvent(events, GameEvent::Type::BoardCleared));
    CHECK(world.status() == WorldStatus::Running);
    CHECK(world.fruit() == (Vec2i{4, 0}));
    CHECK(!world.challengeFruit().has_value());
}

void testNoEventsAfterSnakeDiesInSameFrame() {
    GameConfig config;
    config.wallEventScoreInterval = 10;
    config.challengeSecondsPerLetter = 0.1;
    World world = makeChallengeWorld(config);
    world.setChallengeFruitForTesting({4, 3});
    world.step();  // (4,3) เริ่ม challenge
    world.setFruitForTesting({5, 3});
    world.step();  // (5,3) 10 แต้ม -> warning
    world.setFruitForTesting({0, 0});
    world.step();  // (0,3) ตัวพาดขอบ

    const auto events = world.advance(2.0);  // กำแพงหนีบตาย + challenge หมดเวลาในเฟรมเดียวกัน
    CHECK(!events.empty() && events.back().type == GameEvent::Type::SnakeDied);
}

void testNoChallengeWithoutWords() {
    GameConfig config;
    config.challengeWords.clear();
    World world = makeChallengeWorld(config);
    world.setChallengeFruitForTesting({4, 3});
    const auto events = world.step();
    CHECK(!hasEvent(events, GameEvent::Type::TypingChallengeStarted));
    CHECK(!world.challenge().has_value());
    CHECK(world.status() == WorldStatus::Running);
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
        {"eating challenge fruit starts typing challenge", testEatingChallengeFruitStartsTypingChallenge},
        {"typing word earns life", testTypingWordEarnsLife},
        {"self collision with life spends it", testSelfCollisionWithLifeSpendsIt},
        {"solid wall with life wraps, invulnerability passes again",
         testSolidWallWithLifeWrapsAndInvulnerabilityPassesAgain},
        {"straddling snake with life survives walls turning solid",
         testStraddlingSnakeWithLifeSurvivesWallsTurningSolid},
        {"invulnerability wears off", testInvulnerabilityWearsOff},
        {"invulnerability lasts until head leaves body", testInvulnerabilityLastsUntilHeadLeavesBody},
        {"typing challenge fails when time runs out", testTypingChallengeFailsWhenTimeRunsOut},
        {"passing with full lives gives bonus score", testPassingWithFullLivesGivesBonusScore},
        {"challenge fruit appears every 20s and lasts 5s", testChallengeFruitAppearsEvery20SecondsAndLasts5},
        {"challenge fruit timer pauses during typing challenge", testChallengeFruitTimerPausesDuringTypingChallenge},
        {"typo can be fixed with backspace, case is ignored", testTypoCanBeFixedWithBackspaceAndCaseIsIgnored},
        {"challenge fruit on last free cell does not end the game", testChallengeFruitOnLastFreeCellDoesNotEndTheGame},
        {"no events after snake dies in same frame", testNoEventsAfterSnakeDiesInSameFrame},
        {"no challenge without words", testNoChallengeWithoutWords},
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
