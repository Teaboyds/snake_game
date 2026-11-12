#pragma once

#include <string>
#include <string_view>

#include "engine/core/Vec2.h"

namespace engine::platform {

// Platform layer: โค้ดเดียวที่คุยกับ OS โดยตรง (POSIX termios)
// ถ้าจะ port ไป Windows ก็เขียนไฟล์ .cpp ใหม่ของคลาสนี้คลาสเดียว
//
// RAII: constructor เปิด raw mode / alternate screen,
// destructor คืนค่า terminal กลับเหมือนเดิมเสมอ แม้เกิด exception
class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    void write(std::string_view data);

    // อ่านทุก byte ที่ค้างอยู่ใน stdin แบบ non-blocking (คืน "" ถ้าไม่มี)
    std::string readAvailable();

    Vec2i size() const;

    // true เมื่อได้รับ SIGTERM / SIGHUP
    bool quitRequested() const;
};

}  // namespace engine::platform
