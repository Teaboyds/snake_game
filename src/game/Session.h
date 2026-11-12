#pragma once

#include <string>

#include "game/GameConfig.h"
#include "game/services/Leaderboard.h"

namespace snake {

// ข้อมูลที่อยู่ข้าม scene (อายุยาวกว่ารอบเกมหนึ่งรอบ)
struct Session {
    GameConfig config;
    Leaderboard& leaderboard;
    std::string playerName;
};

}  // namespace snake
