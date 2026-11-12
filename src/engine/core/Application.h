#pragma once

#include "engine/scene/SceneManager.h"

namespace engine {

struct AppConfig {
    double fixedDt = 1.0 / 60.0;  // simulation เดินทีละ 1/60 วินาทีเสมอ
    double maxFrameTime = 0.25;   // กัน "spiral of death" ตอนเครื่องค้าง
    int targetFps = 60;
};

// เจ้าของ game loop และระบบหลักทั้งหมดของ engine
class Application {
public:
    explicit Application(AppConfig config = {});

    scene::SceneManager& scenes() { return scenes_; }

    int run();

private:
    AppConfig config_;
    scene::SceneManager scenes_;
};

}  // namespace engine
