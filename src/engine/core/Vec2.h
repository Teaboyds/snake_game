#pragma once

namespace engine {

struct Vec2i {
    int x = 0;
    int y = 0;

    constexpr Vec2i operator+(const Vec2i& o) const { return {x + o.x, y + o.y}; }
    constexpr bool operator==(const Vec2i& o) const { return x == o.x && y == o.y; }
    constexpr bool operator!=(const Vec2i& o) const { return !(*this == o); }
};

}  // namespace engine
