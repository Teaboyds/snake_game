#pragma once

#include <cstddef>
#include <deque>

#include "engine/core/Vec2.h"
#include "game/sim/Direction.h"

namespace snake {

class Snake {
public:
    Snake(engine::Vec2i head, Direction facing, int length);

    // บัฟเฟอร์การเลี้ยวไว้สูงสุด 2 ครั้ง: กด ขึ้น+ซ้าย เร็ว ๆ ภายใน tick เดียวก็ไม่หาย
    // การเลี้ยวกลับหลัง 180° ถูกปฏิเสธ (ไม่งั้นจะชนคอตัวเองทันที)
    void queueTurn(Direction d);
    void applyNextTurn();

    // growing = true ถ้า tick นี้จะกินผลไม้ (หางไม่ขยับ)
    // ถ้าไม่โต การเดินเข้าช่องหางตัวเองถือว่าปลอดภัย เพราะหางจะขยับออกพอดี
    bool wouldCollide(engine::Vec2i cell, bool growing) const;
    void moveTo(engine::Vec2i newHead, bool grow);

    // true ถ้าตัวงูกำลังพาดข้ามขอบจอ (ส่วนหนึ่งทะลุไปอีกฝั่งแล้ว)
    bool crossesEdge() const;

    engine::Vec2i head() const { return body_.front(); }
    Direction direction() const { return direction_; }
    const std::deque<engine::Vec2i>& body() const { return body_; }
    int length() const { return static_cast<int>(body_.size()); }

private:
    static constexpr std::size_t kMaxQueuedTurns = 2;

    std::deque<engine::Vec2i> body_;  // body_[0] = หัว
    Direction direction_;
    std::deque<Direction> turnQueue_;
};

}  // namespace snake
