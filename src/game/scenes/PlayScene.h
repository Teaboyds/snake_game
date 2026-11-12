#pragma once

#include "engine/scene/Scene.h"
#include "engine/scene/SceneManager.h"
#include "game/Session.h"
#include "game/sim/World.h"

namespace snake {

// PlayScene = "ตัวเชื่อม" ระหว่าง engine กับ simulation
//   - แปลง input action -> command ส่งให้ World
//   - กำหนดจังหวะ tick ของ World (ความเร็วงู)
//   - รับ event จาก World มาอัปเดต session / UI
//   - วาด state ของ World ลง FrameBuffer
class PlayScene : public engine::scene::Scene {
public:
    PlayScene(engine::scene::SceneManager& scenes, Session& session);

    void handleInput(const engine::input::InputFrame& input) override;
    void update(double dt) override;
    void render(engine::render::FrameBuffer& frame) override;

private:
    void onGameEvent(const GameEvent& event);

    engine::scene::SceneManager& scenes_;
    Session& session_;
    World world_;

    int personalBest_;  // คะแนนดีที่สุดของผู้เล่นคนนี้ก่อนเริ่มรอบนี้ (จาก leaderboard)
    double moveInterval_;
    double moveTimer_ = 0.0;
    double endTimer_ = 0.0;  // หน่วงให้เห็นตอนตายก่อนเข้าหน้า Game Over
    bool paused_ = false;
    bool newBest_ = false;
    bool leaving_ = false;
};

}  // namespace snake
