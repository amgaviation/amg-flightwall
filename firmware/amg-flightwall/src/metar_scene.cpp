#include "amg/flightwall/metar_scene.hpp"

#include <array>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {
namespace {

Color categoryColor(const std::string_view category) noexcept {
  if (category == "VFR") {
    return colors::green;
  }
  if (category == "MVFR") {
    return colors::amg_blue;
  }
  if (category == "IFR") {
    return colors::red;
  }
  if (category == "LIFR") {
    return scene_support::magenta;
  }
  return colors::muted;
}

}  // namespace

void MetarScene::setSnapshot(const MetarSnapshot& snapshot) {
  snapshot_ = snapshot;
  scene_support::sanitizeForFont(snapshot_.station);
  scene_support::sanitizeForFont(snapshot_.raw);
  scene_support::sanitizeForFont(snapshot_.flight_category);
  scene_support::sanitizeForFont(snapshot_.wind);
  scene_support::sanitizeForFont(snapshot_.visibility);
  scene_support::toUpperAscii(snapshot_.station);
  scene_support::toUpperAscii(snapshot_.flight_category);
}

std::string_view MetarScene::id() const noexcept { return "metar"; }

void MetarScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);

  const std::string_view station =
      snapshot_.station.empty() ? std::string_view{"METAR"}
                                : std::string_view(snapshot_.station).substr(0, 5);
  renderer.text(2, 2, station, colors::white, 2);
  if (!snapshot_.flight_category.empty()) {
    const std::string_view category = std::string_view(snapshot_.flight_category).substr(0, 4);
    scene_support::drawTextRightAligned(renderer, renderer.width() - 2, 2, category,
                                        categoryColor(category), 2);
  }
  renderer.line(1, 18, renderer.width() - 2, 18, colors::muted);

  if (!snapshot_.valid) {
    scene_support::drawTextCentered(renderer, 32, "DATA STALE", colors::muted, 1);
    return;
  }

  renderer.text(2, 23, "WIND", colors::muted);
  renderer.text(34, 23, std::string_view(snapshot_.wind).substr(0, 15), colors::white);
  renderer.text(2, 32, "VIS", colors::muted);
  renderer.text(34, 32, std::string_view(snapshot_.visibility).substr(0, 15), colors::white);

  renderer.text(2, 41, "TMP", colors::muted);
  std::array<char, 16> number_buffer{};
  std::array<char, 16> temp_buffer{};
  std::size_t length = 0;
  length = scene_support::appendBounded(temp_buffer.data(), temp_buffer.size(), length,
                                        scene_support::formatNumber(number_buffer, snapshot_.temp_c));
  length = scene_support::appendBounded(temp_buffer.data(), temp_buffer.size(), length, "/");
  length = scene_support::appendBounded(
      temp_buffer.data(), temp_buffer.size(), length,
      scene_support::formatNumber(number_buffer, snapshot_.dewpoint_c));
  renderer.text(34, 41, std::string_view(temp_buffer.data(), length), colors::white);

  if (!snapshot_.raw.empty()) {
    if (renderer.textWidth(snapshot_.raw, 1) <= renderer.width() - 4) {
      renderer.text(2, 54, snapshot_.raw, colors::cyan);
    } else {
      scene_support::drawMarqueeText(renderer, snapshot_.raw, 54, colors::cyan, 1,
                                     context.monotonic_ms,
                                     scene_support::default_marquee_speed_px_s, 24);
    }
  }
}

}  // namespace amg::flightwall
