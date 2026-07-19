#include "amg/flightwall/countdown_scene.hpp"

#include <algorithm>
#include <array>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {

void CountdownScene::setCountdown(const CountdownInfo& info) {
  info_ = info;
  scene_support::sanitizeForFont(info_.label);
  base_ms_ = 0;
  base_captured_ = false;
}

std::string_view CountdownScene::id() const noexcept { return "countdown"; }

void CountdownScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);

  const std::string_view label = info_.label.empty()
                                     ? std::string_view{"COUNTDOWN"}
                                     : std::string_view(info_.label).substr(0, 21);
  scene_support::drawTextCentered(renderer, 6, label, colors::amg_blue, 1);

  if (info_.seconds_remaining < 0) {
    scene_support::drawTextCentered(renderer, 30, "NO TARGET", colors::muted, 1);
    return;
  }

  if (!base_captured_) {
    base_ms_ = context.monotonic_ms;
    base_captured_ = true;
  }
  const std::uint64_t elapsed_ms =
      context.monotonic_ms >= base_ms_ ? context.monotonic_ms - base_ms_ : 0;
  std::int64_t remaining = info_.seconds_remaining - static_cast<std::int64_t>(elapsed_ms / 1'000);
  remaining = std::max<std::int64_t>(remaining, 0);

  const std::int64_t days = std::min<std::int64_t>(remaining / 86'400, 99);
  const int hours = static_cast<int>((remaining / 3'600) % 24);
  const int minutes = static_cast<int>((remaining / 60) % 60);
  const int seconds = static_cast<int>(remaining % 60);

  std::array<char, 16> number_buffer{};
  std::array<char, 4> two_digit_buffer{};
  std::array<char, 16> text_buffer{};
  std::size_t length = 0;
  if (days > 0) {
    length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length,
                                          scene_support::formatNumber(number_buffer, days));
    length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length, ":");
  }
  length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length,
                                        scene_support::formatTwoDigits(two_digit_buffer, hours));
  length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length, ":");
  length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length,
                                        scene_support::formatTwoDigits(two_digit_buffer, minutes));
  length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length, ":");
  length = scene_support::appendBounded(text_buffer.data(), text_buffer.size(), length,
                                        scene_support::formatTwoDigits(two_digit_buffer, seconds));
  const std::string_view countdown_text(text_buffer.data(), length);

  const Color color = remaining == 0
                          ? colors::red
                          : (remaining <= 600 ? colors::amber : colors::white);
  const int scale = renderer.textWidth(countdown_text, 2) <= renderer.width() - 4 ? 2 : 1;
  scene_support::drawTextCentered(renderer, 26, countdown_text, color, scale);
  scene_support::drawTextCentered(renderer, 48, "REMAINING", colors::muted, 1);
}

}  // namespace amg::flightwall
