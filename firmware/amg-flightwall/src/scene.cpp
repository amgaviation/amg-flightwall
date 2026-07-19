#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

void SceneManager::activate(Scene& scene) noexcept {
  if (active_ == &scene) {
    return;
  }
  if (active_ != nullptr) {
    active_->onExit();
  }
  active_ = &scene;
  active_->onEnter();
}

void SceneManager::render(FrameContext& context) noexcept {
  if (active_ != nullptr) {
    active_->render(context);
  }
}

Scene* SceneManager::active() const noexcept { return active_; }

}  // namespace amg::flightwall
