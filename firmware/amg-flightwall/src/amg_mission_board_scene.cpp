#include "amg/flightwall/amg_mission_board_scene.hpp"

#include <algorithm>
#include <array>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {
namespace {

// Statuses arrive sanitized (underscores become spaces) and upper-cased.
Color missionStatusColor(const std::string_view status) noexcept {
  if (status == "ENROUTE" || status == "IN PROGRESS") {
    return colors::green;
  }
  if (status == "SCHEDULED") {
    return colors::cyan;
  }
  if (status == "CREW ASSIGNED") {
    return colors::amber;
  }
  if (status == "APPROVED") {
    return colors::amg_blue;
  }
  return colors::muted;
}

}  // namespace

void AmgMissionBoardScene::setSnapshot(const AmgMetricsSnapshot& snapshot) {
  valid_ = snapshot.valid;
  missions_ = snapshot.missions;
  if (missions_.size() > max_rows) {
    missions_.resize(max_rows);
  }
  for (AmgMissionItem& mission : missions_) {
    scene_support::sanitizeForFont(mission.label);
    scene_support::sanitizeForFont(mission.status);
    scene_support::toUpperAscii(mission.status);
  }
}

std::string_view AmgMissionBoardScene::id() const noexcept { return "missions"; }

void AmgMissionBoardScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);
  renderer.text(2, 2, "MISSION BOARD", colors::amg_blue);
  renderer.line(1, 10, renderer.width() - 2, 10, colors::muted);

  if (!valid_) {
    scene_support::drawTextCentered(renderer, 32, "BRIDGE OFFLINE", colors::muted, 1);
    return;
  }
  if (missions_.empty()) {
    scene_support::drawTextCentered(renderer, 32, "NO ACTIVE MISSIONS", colors::muted, 1);
    return;
  }

  std::array<char, 16> number_buffer{};
  std::array<char, 8> eta_buffer{};
  int y = 14;
  for (const AmgMissionItem& mission : missions_) {
    renderer.fillRectangle(2, y + 1, 5, 5, missionStatusColor(mission.status));
    renderer.text(10, y, std::string_view(mission.label).substr(0, 13), colors::white);
    if (mission.eta_min >= 0) {
      std::size_t length = 0;
      length = scene_support::appendBounded(
          eta_buffer.data(), eta_buffer.size(), length,
          scene_support::formatNumber(number_buffer, std::clamp(mission.eta_min, 0, 999)));
      length = scene_support::appendBounded(eta_buffer.data(), eta_buffer.size(), length, "M");
      scene_support::drawTextRightAligned(renderer, renderer.width() - 2, y,
                                          std::string_view(eta_buffer.data(), length),
                                          colors::muted, 1);
    }
    y += 8;
  }
}

}  // namespace amg::flightwall
