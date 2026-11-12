#pragma once

#include "engine/core/Vec2.h"

namespace snake {

enum class Direction { Up, Down, Left, Right };

constexpr engine::Vec2i toVector(Direction d) {
    switch (d) {
        case Direction::Up: return {0, -1};
        case Direction::Down: return {0, 1};
        case Direction::Left: return {-1, 0};
        case Direction::Right: return {1, 0};
    }
    return {0, 0};
}

constexpr bool areOpposite(Direction a, Direction b) {
    return toVector(a) + toVector(b) == engine::Vec2i{0, 0};
}

}  // namespace snake
