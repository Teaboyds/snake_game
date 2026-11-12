#include "engine/input/InputSystem.h"

#include <algorithm>

#include "engine/platform/Terminal.h"

namespace engine::input {

bool InputFrame::has(Action action) const {
    return std::find(actions.begin(), actions.end(), action) != actions.end();
}

InputFrame parseInput(std::string_view bytes, InputMode mode) {
    InputFrame frame;
    std::vector<Action>& actions = frame.actions;

    for (size_t i = 0; i < bytes.size(); ++i) {
        const char c = bytes[i];

        if (c == '\x1b') {
            // ปุ่มลูกศรส่งมาเป็น escape sequence: ESC [ A..D (หรือ ESC O A..D)
            if (i + 2 < bytes.size() && (bytes[i + 1] == '[' || bytes[i + 1] == 'O')) {
                switch (bytes[i + 2]) {
                    case 'A': actions.push_back(Action::Up); break;
                    case 'B': actions.push_back(Action::Down); break;
                    case 'C': actions.push_back(Action::Right); break;
                    case 'D': actions.push_back(Action::Left); break;
                    default: break;  // sequence อื่นที่ไม่รู้จัก ข้ามไป
                }
                i += 2;
            } else {
                actions.push_back(Action::Pause);  // กด ESC เดี่ยว ๆ
            }
            continue;
        }

        if (mode == InputMode::Text) {
            if (c == '\r' || c == '\n') {
                actions.push_back(Action::Confirm);
            } else if (c == '\x7f' || c == '\b') {
                frame.text += '\b';
            } else if (c == '\x03') {
                actions.push_back(Action::Quit);
            } else if (c >= 0x20 && c < 0x7f) {
                frame.text += c;  // ASCII ที่พิมพ์ได้เท่านั้น
            }
            continue;
        }

        switch (c) {
            case 'w': case 'W': actions.push_back(Action::Up); break;
            case 's': case 'S': actions.push_back(Action::Down); break;
            case 'a': case 'A': actions.push_back(Action::Left); break;
            case 'd': case 'D': actions.push_back(Action::Right); break;
            case '\r': case '\n': case ' ': actions.push_back(Action::Confirm); break;
            case 'p': case 'P': actions.push_back(Action::Pause); break;
            case 'q': case 'Q': case '\x03': actions.push_back(Action::Quit); break;  // \x03 = Ctrl+C
            default: break;
        }
    }
    return frame;
}

InputSystem::InputSystem(platform::Terminal& terminal) : terminal_(terminal) {}

InputFrame InputSystem::poll(InputMode mode) {
    InputFrame frame = parseInput(terminal_.readAvailable(), mode);
    if (terminal_.quitRequested()) frame.actions.push_back(Action::Quit);
    return frame;
}

}  // namespace engine::input
