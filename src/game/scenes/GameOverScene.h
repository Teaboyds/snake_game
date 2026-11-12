#pragma once

#include <optional>
#include <vector>

#include "engine/scene/Scene.h"
#include "engine/scene/SceneManager.h"
#include "game/Session.h"

namespace snake {

class GameOverScene : public engine::scene::Scene {
public:
    GameOverScene(engine::scene::SceneManager& scenes, Session& session, int score, bool won, bool newBest);

    void onEnter() override;
    void handleInput(const engine::input::InputFrame& input) override;
    void update(double dt) override;
    void render(engine::render::FrameBuffer& frame) override;

private:
    engine::scene::SceneManager& scenes_;
    Session& session_;
    int score_;
    bool won_;
    bool newBest_;
    std::optional<PlayerStanding> standing_;
    std::vector<LeaderboardEntry> top_;
    double time_ = 0.0;
};

}  // namespace snake
