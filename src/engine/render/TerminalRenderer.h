#pragma once

#include "engine/render/FrameBuffer.h"

namespace engine::platform {
class Terminal;
}

namespace engine::render {

// เอา FrameBuffer ไปแสดงบน terminal ด้วย ANSI escape codes
// ใช้ delta rendering: เทียบกับเฟรมก่อน แล้วส่งเฉพาะ cell ที่เปลี่ยน
// (หลักเดียวกับ delta compression ที่ game server ใช้ส่ง state ให้ client)
class TerminalRenderer {
public:
    explicit TerminalRenderer(platform::Terminal& terminal);

    void present(const FrameBuffer& frame);

private:
    platform::Terminal& terminal_;
    FrameBuffer previous_{0, 0};
};

}  // namespace engine::render
