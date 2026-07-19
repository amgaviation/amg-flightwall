#include "amg/flightwall/notification_scene.hpp"

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {

void NotificationScene::setNotification(const NotificationEvent& event) {
  event_ = event;
  scene_support::sanitizeForFont(event_.title);
  scene_support::sanitizeForFont(event_.body);
}

std::string_view NotificationScene::id() const noexcept { return "notification"; }

void NotificationScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);

  // A black border would be invisible; fall back to the AMG accent.
  const Color border = event_.color == colors::black ? colors::amg_blue : event_.color;
  const int inner_width = renderer.width() - 12;

  const std::string_view title =
      event_.title.empty() ? std::string_view{"NOTICE"} : std::string_view(event_.title);
  if (renderer.textWidth(title, 2) <= inner_width) {
    scene_support::drawTextCentered(renderer, 10, title, colors::white, 2);
  } else if (renderer.textWidth(title, 1) <= inner_width) {
    scene_support::drawTextCentered(renderer, 13, title, colors::white, 1);
  } else {
    scene_support::drawMarqueeText(renderer, title, 13, colors::white, 1, context.monotonic_ms,
                                   scene_support::default_marquee_speed_px_s, 24);
  }

  if (!event_.body.empty()) {
    if (renderer.textWidth(event_.body, 1) <= inner_width) {
      scene_support::drawTextCentered(renderer, 40, event_.body, colors::white, 1);
    } else {
      scene_support::drawMarqueeText(renderer, event_.body, 40, colors::white, 1,
                                     context.monotonic_ms,
                                     scene_support::default_marquee_speed_px_s, 24);
    }
  }

  // Border last so scrolling text never bleeds over the frame.
  renderer.rectangle(0, 0, renderer.width(), renderer.height(), border);
  renderer.rectangle(1, 1, renderer.width() - 2, renderer.height() - 2, border);
}

}  // namespace amg::flightwall
