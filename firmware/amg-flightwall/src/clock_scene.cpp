#include "amg/flightwall/clock_scene.hpp"

#include <algorithm>
#include <array>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {
namespace {

constexpr std::array<std::string_view, 7> weekday_names{"SUN", "MON", "TUE", "WED",
                                                        "THU", "FRI", "SAT"};

}  // namespace

void ClockScene::setClock(const ClockInfo& info) noexcept { info_ = info; }

std::string_view ClockScene::id() const noexcept { return "clock"; }

void ClockScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);

  if (!info_.valid) {
    scene_support::drawTextCentered(renderer, 18, "CLOCK", colors::amg_blue, 2);
    scene_support::drawTextCentered(renderer, 42, "NO TIME SYNC", colors::muted, 1);
    return;
  }

  const int hour = std::clamp(info_.hour, 0, 23);
  const int minute = std::clamp(info_.minute, 0, 59);
  const int second = std::clamp(info_.second, 0, 59);

  std::array<char, 8> time_buffer{};
  time_buffer[0] = static_cast<char>('0' + hour / 10);
  time_buffer[1] = static_cast<char>('0' + hour % 10);
  time_buffer[2] = second % 2 == 0 ? ':' : ' ';  // seconds tick: blinking colon
  time_buffer[3] = static_cast<char>('0' + minute / 10);
  time_buffer[4] = static_cast<char>('0' + minute % 10);
  const std::string_view time_text(time_buffer.data(), 5);

  const int time_width = renderer.textWidth(time_text, 2);
  const int time_x = (renderer.width() - time_width) / 2;
  renderer.text(time_x, 12, time_text, colors::white, 2);

  std::array<char, 4> seconds_buffer{};
  renderer.text(time_x + time_width + 4, 19,
                scene_support::formatTwoDigits(seconds_buffer, second), colors::muted, 1);

  renderer.line(24, 34, renderer.width() - 25, 34, colors::muted);

  std::array<char, 16> date_buffer{};
  std::array<char, 4> two_digit_buffer{};
  const std::size_t weekday_index = static_cast<std::size_t>(((info_.weekday % 7) + 7) % 7);
  std::size_t length = 0;
  length = scene_support::appendBounded(date_buffer.data(), date_buffer.size(), length,
                                        weekday_names[weekday_index]);
  length = scene_support::appendBounded(date_buffer.data(), date_buffer.size(), length, " ");
  length = scene_support::appendBounded(date_buffer.data(), date_buffer.size(), length,
                                        scene_support::formatTwoDigits(two_digit_buffer, info_.month));
  length = scene_support::appendBounded(date_buffer.data(), date_buffer.size(), length, "/");
  length = scene_support::appendBounded(date_buffer.data(), date_buffer.size(), length,
                                        scene_support::formatTwoDigits(two_digit_buffer, info_.day));
  scene_support::drawTextCentered(renderer, 42, std::string_view(date_buffer.data(), length),
                                  colors::amg_blue, 1);
}

}  // namespace amg::flightwall
