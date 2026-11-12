#pragma once

#include <cstddef>

namespace snake {

constexpr std::size_t kMaxPlayerNameLength = 12;

// จำกัดเป็น ASCII เพราะ FrameBuffer เก็บ 1 byte ต่อ 1 ช่อง (ภาษาไทยใน UTF-8 ใช้ 3 byte)
// และไม่มีช่องว่าง จะได้ไม่มีปัญหา "Bob" กับ "Bob " เป็นคนละคน
constexpr bool isAllowedNameChar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
}

}  // namespace snake
