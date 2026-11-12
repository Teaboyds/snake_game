#pragma once

#include <string>
#include <vector>

#include "engine/render/FrameBuffer.h"
#include "game/services/Leaderboard.h"

namespace snake {

// วาดตาราง TOP N ตรงกลางจอ ไฮไลต์แถวของผู้เล่น คืนค่า y บรรทัดถัดไป
int drawLeaderboard(engine::render::FrameBuffer& frame, int y, const std::vector<LeaderboardEntry>& entries,
                    const std::string& highlightName);

}  // namespace snake
