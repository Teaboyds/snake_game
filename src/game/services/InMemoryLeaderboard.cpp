#include "game/services/InMemoryLeaderboard.h"

#include <algorithm>

namespace snake {

void InMemoryLeaderboard::submitScore(const std::string& player, int score) {
    auto [it, inserted] = players_.try_emplace(player, Record{score, 0});
    Record& record = it->second;
    record.best = std::max(record.best, score);
    ++record.gamesPlayed;
}

std::vector<LeaderboardEntry> InMemoryLeaderboard::top(int count) {
    std::vector<LeaderboardEntry> entries = ranked();
    if (static_cast<int>(entries.size()) > count) entries.resize(static_cast<std::size_t>(std::max(0, count)));
    return entries;
}

std::optional<PlayerStanding> InMemoryLeaderboard::standing(const std::string& player) {
    const auto it = players_.find(player);
    if (it == players_.end()) return std::nullopt;

    const std::vector<LeaderboardEntry> entries = ranked();
    const auto pos = std::find_if(entries.begin(), entries.end(),
                                  [&](const LeaderboardEntry& e) { return e.name == player; });
    return PlayerStanding{it->second.best, static_cast<int>(pos - entries.begin()) + 1, it->second.gamesPlayed};
}

std::vector<LeaderboardEntry> InMemoryLeaderboard::ranked() const {
    std::vector<LeaderboardEntry> entries;
    for (const auto& [name, record] : players_) entries.push_back({name, record.best});
    // เรียงแบบเดียวกับ Redis ZREVRANGE: คะแนนมากก่อน, คะแนนเท่ากันเรียงชื่อจากหลังไปหน้า
    std::sort(entries.begin(), entries.end(), [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
        return a.score != b.score ? a.score > b.score : a.name > b.name;
    });
    return entries;
}

}  // namespace snake
