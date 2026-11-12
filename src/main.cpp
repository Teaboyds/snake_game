#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <string>

#include "engine/core/Application.h"
#include "game/Session.h"
#include "game/scenes/MenuScene.h"
#include "game/services/InMemoryLeaderboard.h"
#include "game/services/RedisLeaderboard.h"

namespace {

std::string envOr(const char* name, const std::string& fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? value : fallback;
}

// ต่อ Redis ไม่ได้ก็ยังเล่นได้ (graceful degradation) แค่คะแนนไม่ถูกบันทึกถาวร
std::unique_ptr<snake::Leaderboard> createLeaderboard() {
    const std::string host = envOr("SNAKE_REDIS_HOST", "127.0.0.1");
    const int port = std::atoi(envOr("SNAKE_REDIS_PORT", "6380").c_str());

    auto redis = std::make_unique<snake::RedisLeaderboard>(host, port);
    if (redis->connect()) return redis;

    std::cerr << "warning: cannot connect to redis " << host << ":" << port << " (" << redis->lastError()
              << "), using offline leaderboard\n";
    return std::make_unique<snake::InMemoryLeaderboard>();
}

}  // namespace

int main() {
    try {
        const std::unique_ptr<snake::Leaderboard> leaderboard = createLeaderboard();
        snake::Session session{{}, *leaderboard, {}};

        engine::Application app;
        app.scenes().change(std::make_unique<snake::MenuScene>(app.scenes(), session));
        return app.run();
    } catch (const std::exception& e) {
        // Terminal ถูกคืนค่าแล้วโดย destructor (RAII) ก่อนมาถึงตรงนี้
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
}
