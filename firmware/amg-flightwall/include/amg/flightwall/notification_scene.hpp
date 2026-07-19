#pragma once

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Full-panel banner: colored border, title (scale 2 when it fits), scrolling
// body. Driven only through the SceneRotator overlay stack; the platform sets
// the event payload, then pushes this scene as an overlay.
class NotificationScene final : public Scene {
 public:
  void setNotification(const NotificationEvent& event);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  NotificationEvent event_{};  // sanitized copy
};

}  // namespace amg::flightwall
