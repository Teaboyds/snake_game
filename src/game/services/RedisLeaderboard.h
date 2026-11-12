#pragma once

#include "backend/redis/RedisClient.h"
#include "game/services/Leaderboard.h"

namespace snake {

// Redis data model:
//   {prefix}leaderboard      ZSET  member = ชื่อผู้เล่น, score = คะแนนดีที่สุด
//   {prefix}player:{name}    HASH  games_played, last_score
//
// ถ้า connection หลุด จะลองต่อใหม่อัตโนมัติในการเรียกครั้งถัดไป
class RedisLeaderboard : public Leaderboard {
public:
    RedisLeaderboard(std::string host, int port, std::string keyPrefix = "snake:");

    bool connect() { return client_.connect(); }
    const std::string& lastError() const { return client_.lastError(); }

    std::string description() const override;
    bool online() const override { return client_.connected(); }

    void submitScore(const std::string& player, int score) override;
    std::vector<LeaderboardEntry> top(int count) override;
    std::optional<PlayerStanding> standing(const std::string& player) override;

    std::string leaderboardKey() const { return prefix_ + "leaderboard"; }
    std::string playerKey(const std::string& player) const { return prefix_ + "player:" + player; }

private:
    bool ensureConnected();

    backend::redis::Client client_;
    std::string prefix_;
};

}  // namespace snake
