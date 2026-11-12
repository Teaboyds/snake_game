#include "game/scenes/LeaderboardView.h"

#include <cstdio>

namespace snake {

using engine::render::Color;

int drawLeaderboard(engine::render::FrameBuffer& frame, int y, const std::vector<LeaderboardEntry>& entries,
                    const std::string& highlightName) {
    frame.textCentered(y++, "-- LEADERBOARD --", Color::Cyan, true);
    if (entries.empty()) {
        frame.textCentered(y++, "no scores yet", Color::Gray);
        return y;
    }
    for (std::size_t i = 0; i < entries.size(); ++i) {
        char line[64];
        std::snprintf(line, sizeof(line), "%2zu. %-12s %6d", i + 1, entries[i].name.c_str(), entries[i].score);
        const bool me = entries[i].name == highlightName;
        frame.textCentered(y++, line, me ? Color::Yellow : Color::White, me);
    }
    return y;
}

}  // namespace snake
