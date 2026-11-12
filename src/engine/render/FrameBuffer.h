#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace engine::render {

enum class Color : std::uint8_t { Default, Red, Green, Yellow, Blue, Magenta, Cyan, White, Gray };

struct Cell {
    char glyph = ' ';
    Color fg = Color::Default;
    bool bold = false;

    bool operator==(const Cell& o) const { return glyph == o.glyph && fg == o.fg && bold == o.bold; }
    bool operator!=(const Cell& o) const { return !(*this == o); }
};

// "หน้าจอเสมือน" ขนาดเท่า terminal เกมวาดลงที่นี่ ไม่ได้เขียนลงจอตรง ๆ
// แนวคิดเดียวกับ back buffer ของ GPU: วาดให้เสร็จทั้งเฟรมก่อนแล้วค่อย present
class FrameBuffer {
public:
    FrameBuffer(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    void clear();
    void put(int x, int y, char glyph, Color fg = Color::Default, bool bold = false);
    void text(int x, int y, std::string_view s, Color fg = Color::Default, bool bold = false);
    void textCentered(int y, std::string_view s, Color fg = Color::Default, bool bold = false);

    const Cell& at(int x, int y) const { return cells_[static_cast<size_t>(y * width_ + x)]; }

private:
    int width_;
    int height_;
    std::vector<Cell> cells_;
};

}  // namespace engine::render
