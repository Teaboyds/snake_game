#pragma once

#include <optional>
#include <string>
#include <vector>

namespace snake {

struct LeaderboardEntry {
    std::string name;
    int score = 0;
};

struct PlayerStanding {
    int bestScore = 0;
    int rank = 0;  // เริ่มที่ 1
    int gamesPlayed = 0;
};

// Interface (Dependency Inversion): scene รู้จักแค่ตัวนี้
// ไม่รู้ว่าข้างหลังเป็น Redis, in-memory, หรือ HTTP API ของ game server
// ผู้เล่นระบุตัวด้วยชื่อ — ชื่อซ้ำ = ผู้เล่นคนเดิม
class Leaderboard {
public:
    virtual ~Leaderboard() = default;

    virtual std::string description() const = 0;
    virtual bool online() const = 0;

    // เก็บเฉพาะคะแนนที่ดีที่สุดของแต่ละคน และนับจำนวนเกมที่เล่น
    virtual void submitScore(const std::string& player, int score) = 0;
    virtual std::vector<LeaderboardEntry> top(int count) = 0;
    virtual std::optional<PlayerStanding> standing(const std::string& player) = 0;  // nullopt = ยังไม่เคยเล่น
};

}  // namespace snake
