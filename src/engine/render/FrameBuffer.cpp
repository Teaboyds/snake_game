#include "engine/render/FrameBuffer.h"

namespace engine::render {

FrameBuffer::FrameBuffer(int width, int height)
    : width_(width), height_(height), cells_(static_cast<size_t>(width * height)) {}

void FrameBuffer::clear() {
    for (Cell& cell : cells_) cell = Cell{};
}

void FrameBuffer::put(int x, int y, char glyph, Color fg, bool bold) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;  // clip นอกจอทิ้ง
    cells_[static_cast<size_t>(y * width_ + x)] = Cell{glyph, fg, bold};
}

void FrameBuffer::text(int x, int y, std::string_view s, Color fg, bool bold) {
    for (size_t i = 0; i < s.size(); ++i) put(x + static_cast<int>(i), y, s[i], fg, bold);
}

void FrameBuffer::textCentered(int y, std::string_view s, Color fg, bool bold) {
    text((width_ - static_cast<int>(s.size())) / 2, y, s, fg, bold);
}

}  // namespace engine::render
