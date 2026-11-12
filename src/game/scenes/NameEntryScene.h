#pragma once

#include <string>

#include "engine/scene/Scene.h"
#include "engine/scene/SceneManager.h"
#include "game/Session.h"

namespace snake {

class NameEntryScene : public engine::scene::Scene {
public:
    NameEntryScene(engine::scene::SceneManager& scenes, Session& session);

    bool wantsTextInput() const override { return true; }

    void handleInput(const engine::input::InputFrame& input) override;
    void update(double dt) override;
    void render(engine::render::FrameBuffer& frame) override;

private:
    engine::scene::SceneManager& scenes_;
    Session& session_;
    std::string name_;
    std::string error_;
    double time_ = 0.0;
};

}  // namespace snake
