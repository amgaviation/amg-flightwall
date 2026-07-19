#include "amg/flightwall/message_scene.hpp"

#include <algorithm>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {

void MessageScene::setMessage(const MessagePayload& payload) {
  payload_ = payload;
  scene_support::sanitizeForFont(payload_.text);

  line_count_ = 0;
  std::string_view remaining(payload_.text);
  while (!remaining.empty() && line_count_ < max_lines) {
    std::size_t take = std::min<std::size_t>(remaining.size(), max_line_chars);
    if (take < remaining.size()) {
      const std::size_t space = remaining.rfind(' ', take);
      if (space != std::string_view::npos && space > 0) {
        take = space;
      }
    }
    std::size_t length = 0;
    for (std::size_t index = 0; index < take && length < max_line_chars; ++index) {
      lines_[line_count_][length++] = remaining[index];
    }
    line_lengths_[line_count_] = length;
    ++line_count_;
    remaining.remove_prefix(std::min(remaining.size(), take));
    while (!remaining.empty() && remaining.front() == ' ') {
      remaining.remove_prefix(1);
    }
  }
}

std::string_view MessageScene::id() const noexcept { return "message"; }

void MessageScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);

  if (payload_.text.empty()) {
    scene_support::drawTextCentered(renderer, 30, "NO MESSAGE", colors::muted, 1);
    return;
  }

  // A black message would be invisible on the panel; fall back to white.
  const Color color = payload_.color == colors::black ? colors::white : payload_.color;

  if (payload_.scroll) {
    scene_support::drawMarqueeText(renderer, payload_.text, 25, color, 2, context.monotonic_ms,
                                   scene_support::default_marquee_speed_px_s, 32);
    return;
  }

  if (renderer.textWidth(payload_.text, 2) <= renderer.width() - 4) {
    scene_support::drawTextCentered(renderer, 25, payload_.text, color, 2);
    return;
  }

  const int total_height = static_cast<int>(line_count_) * 9 - 2;
  int y = (renderer.height() - total_height) / 2;
  for (std::size_t index = 0; index < line_count_; ++index) {
    scene_support::drawTextCentered(
        renderer, y, std::string_view(lines_[index].data(), line_lengths_[index]), color, 1);
    y += 9;
  }
}

}  // namespace amg::flightwall
