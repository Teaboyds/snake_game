#pragma once

#include <vector>

#include "engine/scene/Scene.h"
#include "engine/scene/SceneManager.h"
#include "game/Session.h"

namespace snake {

class MenuScene : public engine::scene::Scene {
public:
    MenuScene(engine::scene::SceneManager& scenes, Session& session);

    void onEnter() override;
    void handleInput(const engine::input::InputFrame& input) override;
    void update(double dt) override;
    void render(engine::render::FrameBuffer& frame) override;

private:
    engine::scene::SceneManager& scenes_;
    Session& session_;
    std::vector<LeaderboardEntry> top_;
    double time_ = 0.0;
};

}  // namespace snake
