#include "engine/render/TerminalRenderer.h"

#include <string>

#include "engine/platform/Terminal.h"

namespace engine::render {
namespace {

const char* colorCode(Color c) {
    switch (c) {
        case Color::Red: return "31";
        case Color::Green: return "32";
        case Color::Yellow: return "33";
        case Color::Blue: return "34";
        case Color::Magenta: return "35";
        case Color::Cyan: return "36";
        case Color::White: return "37";
        case Color::Gray: return "90";
        case Color::Default: break;
    }
    return "39";
}

}  // namespace

TerminalRenderer::TerminalRenderer(platform::Terminal& terminal) : terminal_(terminal) {}

void TerminalRenderer::present(const FrameBuffer& frame) {
    std::string out;
    const bool fullRedraw = previous_.width() != frame.width() || previous_.height() != frame.height();
    if (fullRedraw) out += "\x1b[0m\x1b[2J";

    bool styleKnown = false;
    Color currentFg = Color::Default;
    bool currentBold = false;
    int cursorX = -1;
    int cursorY = -1;

    for (int y = 0; y < frame.height(); ++y) {
        for (int x = 0; x < frame.width(); ++x) {
            const Cell& cell = frame.at(x, y);
            if (!fullRedraw && cell == previous_.at(x, y)) continue;

            if (cursorX != x || cursorY != y) {
                out += "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
            }
            if (!styleKnown || cell.fg != currentFg || cell.bold != currentBold) {
                out += "\x1b[";
                out += cell.bold ? "1;" : "22;";
                out += colorCode(cell.fg);
                out += "m";
                styleKnown = true;
                currentFg = cell.fg;
                currentBold = cell.bold;
            }
            out += cell.glyph;
            cursorX = x + 1;
            cursorY = y;
        }
    }

    // เขียนครั้งเดียวต่อเฟรม (1 syscall) ลดการกระพริบ
    if (!out.empty()) terminal_.write(out);
    previous_ = frame;
}

}  // namespace engine::render
