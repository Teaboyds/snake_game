#include "game/services/RedisLeaderboard.h"

#include <cstdlib>

namespace snake {
namespace {

using backend::redis::Reply;

constexpr std::chrono::milliseconds kTimeout{300};

int toInt(const Reply& reply) {
    if (reply.type == Reply::Type::Integer) return static_cast<int>(reply.integer);
    // ZSCORE คืนคะแนนเป็น bulk string (Redis เก็บ score เป็น double)
    if (reply.type == Reply::Type::Bulk) return static_cast<int>(std::strtod(reply.str.c_str(), nullptr));
    return 0;
}

}  // namespace

RedisLeaderboard::RedisLeaderboard(std::string host, int port, std::string keyPrefix)
    : client_(std::move(host), port, kTimeout), prefix_(std::move(keyPrefix)) {}

std::string RedisLeaderboard::description() const {
    const std::string address = "redis " + client_.host() + ":" + std::to_string(client_.port());
    return online() ? address : address + " (disconnected)";
}

bool RedisLeaderboard::ensureConnected() { return client_.connected() || client_.connect(); }

void RedisLeaderboard::submitScore(const std::string& player, int score) {
    if (!ensureConnected()) return;

    const std::string value = std::to_string(score);
    // MULTI/EXEC = transaction: คำสั่งทั้งหมดถูกรันติดกันแบบ atomic
    // ZADD GT = อัปเดตเฉพาะเมื่อคะแนนใหม่มากกว่าเดิม (ชื่อซ้ำจึงเก็บแค่ best score)
    client_.pipeline({
        {"MULTI"},
        {"ZADD", leaderboardKey(), "GT", value, player},
        {"HINCRBY", playerKey(player), "games_played", "1"},
        {"HSET", playerKey(player), "last_score", value},
        {"EXEC"},
    });
}

std::vector<LeaderboardEntry> RedisLeaderboard::top(int count) {
    if (count <= 0 || !ensureConnected()) return {};

    const auto reply =
        client_.command({"ZREVRANGE", leaderboardKey(), "0", std::to_string(count - 1), "WITHSCORES"});
    if (!reply || reply->type != Reply::Type::Array) return {};

    // WITHSCORES คืนแบบสลับกัน: [name1, score1, name2, score2, ...]
    std::vector<LeaderboardEntry> entries;
    for (std::size_t i = 0; i + 1 < reply->elements.size(); i += 2) {
        entries.push_back({reply->elements[i].str, toInt(reply->elements[i + 1])});
    }
    return entries;
}

std::optional<PlayerStanding> RedisLeaderboard::standing(const std::string& player) {
    if (!ensureConnected()) return std::nullopt;

    const auto replies = client_.pipeline({
        {"ZSCORE", leaderboardKey(), player},
        {"ZREVRANK", leaderboardKey(), player},
        {"HGET", playerKey(player), "games_played"},
    });
    if (!replies) return std::nullopt;

    const Reply& score = (*replies)[0];
    const Reply& rank = (*replies)[1];
    const Reply& games = (*replies)[2];
    if (score.isNil() || score.isError() || rank.isError()) return std::nullopt;

    return PlayerStanding{toInt(score), toInt(rank) + 1, toInt(games)};
}

}  // namespace snake
