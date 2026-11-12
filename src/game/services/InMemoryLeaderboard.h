#pragma once

#include <map>

#include "game/services/Leaderboard.h"

namespace snake {

// ใช้ตอนต่อ Redis ไม่ได้ (เล่นได้ปกติ แต่คะแนนหายเมื่อปิดเกม) และใช้ใน unit test
class InMemoryLeaderboard : public Leaderboard {
public:
    std::string description() const override { return "offline - scores kept until exit"; }
    bool online() const override { return false; }

    void submitScore(const std::string& player, int score) override;
    std::vector<LeaderboardEntry> top(int count) override;
    std::optional<PlayerStanding> standing(const std::string& player) override;

private:
    struct Record {
        int best = 0;
        int gamesPlayed = 0;
    };

    std::vector<LeaderboardEntry> ranked() const;

    std::map<std::string, Record> players_;
};

}  // namespace snake
