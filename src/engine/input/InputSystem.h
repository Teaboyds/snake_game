#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace engine::platform {
class Terminal;
}

namespace engine::input {

// Action mapping: เกมไม่สนใจว่ากดปุ่มอะไร สนใจแค่ "ผู้เล่นตั้งใจทำอะไร"
// (เหมือน Input Actions ใน Unreal / Unity) เปลี่ยนปุ่มได้โดยไม่แตะโค้ดเกม
enum class Action { Up, Down, Left, Right, Confirm, Pause, Quit };

// Input context: โหมด Text ใช้ตอนพิมพ์ชื่อ ปุ่มตัวอักษรกลายเป็น "ข้อความ" แทน action
// (ไม่งั้นพิมพ์ตัว q แล้วเกมปิด) — Enter, Esc, ลูกศร, Ctrl+C ยังเป็น action เหมือนเดิม
enum class InputMode { Game, Text };

struct InputFrame {
    std::vector<Action> actions;
    std::string text;  // โหมด Text เท่านั้น: ตัวอักษรที่พิมพ์ตามลำดับ, '\b' = backspace

    bool has(Action action) const;
};

// แปลง raw bytes จาก terminal เป็น input — เป็น pure function จึงเขียน test ได้
InputFrame parseInput(std::string_view bytes, InputMode mode = InputMode::Game);

class InputSystem {
public:
    explicit InputSystem(platform::Terminal& terminal);

    // เรียกครั้งเดียวต่อเฟรม คืน input ทั้งหมดที่เกิดขึ้นตั้งแต่เฟรมก่อน
    InputFrame poll(InputMode mode);

private:
    platform::Terminal& terminal_;
};

}  // namespace engine::input
