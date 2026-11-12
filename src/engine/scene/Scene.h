#pragma once

#include "engine/input/InputSystem.h"
#include "engine/render/FrameBuffer.h"

namespace engine::scene {

// หนึ่ง Scene = หนึ่ง state ของเกม (Menu, Play, GameOver, ...)
// Application เรียก 3 phase นี้ตามลำดับทุกเฟรม: input -> update -> render
class Scene {
public:
    virtual ~Scene() = default;

    virtual void onEnter() {}
    virtual void onExit() {}

    // true = scene นี้ต้องการรับข้อความ (เช่น ช่องกรอกชื่อ) -> InputMode::Text
    virtual bool wantsTextInput() const { return false; }

    virtual void handleInput(const input::InputFrame& input) = 0;
    virtual void update(double dt) = 0;  // dt คงที่เสมอ (fixed timestep)
    virtual void render(render::FrameBuffer& frame) = 0;  // ห้ามแก้ state ใน render
};

}  // namespace engine::scene
