#include "game/sim/Snake.h"

#include <cstdlib>

namespace snake {

Snake::Snake(engine::Vec2i head, Direction facing, int length) : direction_(facing) {
    const engine::Vec2i back = toVector(facing);
    for (int i = 0; i < length; ++i) {
        body_.push_back({head.x - back.x * i, head.y - back.y * i});
    }
}

void Snake::queueTurn(Direction d) {
    const Direction last = turnQueue_.empty() ? direction_ : turnQueue_.back();
    if (d == last || areOpposite(d, last)) return;
    if (turnQueue_.size() >= kMaxQueuedTurns) return;
    turnQueue_.push_back(d);
}

void Snake::applyNextTurn() {
    if (turnQueue_.empty()) return;
    direction_ = turnQueue_.front();
    turnQueue_.pop_front();
}

bool Snake::wouldCollide(engine::Vec2i cell, bool growing) const {
    const std::size_t checked = growing ? body_.size() : body_.size() - 1;
    for (std::size_t i = 0; i < checked; ++i) {
        if (body_[i] == cell) return true;
    }
    return false;
}

void Snake::moveTo(engine::Vec2i newHead, bool grow) {
    body_.push_front(newHead);
    if (!grow) body_.pop_back();
}

bool Snake::crossesEdge() const {
    // ปกติปล้องที่ติดกันห่างกัน 1 ช่อง ถ้าห่างเกิน 1 แปลว่ามีการ wrap ข้ามขอบ
    for (std::size_t i = 1; i < body_.size(); ++i) {
        if (std::abs(body_[i].x - body_[i - 1].x) > 1 || std::abs(body_[i].y - body_[i - 1].y) > 1) return true;
    }
    return false;
}

}  // namespace snake
