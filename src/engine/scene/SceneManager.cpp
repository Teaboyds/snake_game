#include "engine/scene/SceneManager.h"

namespace engine::scene {

void SceneManager::change(std::unique_ptr<Scene> next) { pending_ = std::move(next); }

void SceneManager::applyPending() {
    if (!pending_) return;
    if (current_) current_->onExit();
    current_ = std::move(pending_);
    current_->onEnter();
}

}  // namespace engine::scene
