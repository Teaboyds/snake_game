#pragma once

#include <memory>

#include "engine/scene/Scene.h"

namespace engine::scene {

// State machine ของเกม การเปลี่ยน scene เป็นแบบ deferred:
// scene ขอเปลี่ยนระหว่าง update ได้ แต่การสลับจริงเกิดตอนต้นเฟรมถัดไป
// ป้องกันบั๊กคลาสสิก "ลบ object ตัวเองทิ้งขณะที่ method ของมันยังรันอยู่"
class SceneManager {
public:
    void change(std::unique_ptr<Scene> next);
    void applyPending();

    Scene* current() const { return current_.get(); }

private:
    std::unique_ptr<Scene> current_;
    std::unique_ptr<Scene> pending_;
};

}  // namespace engine::scene
