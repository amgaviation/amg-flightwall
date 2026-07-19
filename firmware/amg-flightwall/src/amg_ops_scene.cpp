#include "amg/flightwall/amg_ops_scene.hpp"

#include <algorithm>
#include <array>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {
namespace {

constexpr std::uint64_t detail_rotate_ms = 4'000;
constexpr std::size_t detail_max_chars = 21;

Color siteStateColor(const std::string_view site_state) noexcept {
  if (site_state.empty()) {
    return colors::muted;
  }
  if (site_state == "OK") {
    return colors::green;
  }
  return colors::amber;
}

}  // namespace

void AmgOpsScene::setSnapshot(const AmgMetricsSnapshot& snapshot) {
  snapshot_ = snapshot;
  if (snapshot_.latest_requests.size() > max_items) {
    snapshot_.latest_requests.resize(max_items);
  }
  if (snapshot_.missions.size() > max_items) {
    snapshot_.missions.resize(max_items);
  }
  if (snapshot_.recent_submissions.size() > max_items) {
    snapshot_.recent_submissions.resize(max_items);
  }
  for (AmgRequestItem& request : snapshot_.latest_requests) {
    scene_support::sanitizeForFont(request.label);
    scene_support::sanitizeForFont(request.name);
  }
  for (AmgMissionItem& mission : snapshot_.missions) {
    scene_support::sanitizeForFont(mission.label);
    scene_support::sanitizeForFont(mission.status);
  }
  for (AmgSubmissionItem& submission : snapshot_.recent_submissions) {
    scene_support::sanitizeForFont(submission.kind);
    scene_support::sanitizeForFont(submission.name);
  }
  scene_support::sanitizeForFont(snapshot_.site_state);
  scene_support::toUpperAscii(snapshot_.site_state);
}

std::string_view AmgOpsScene::id() const noexcept { return "amg_ops"; }

void AmgOpsScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);
  renderer.text(2, 2, "AMG OPS", colors::amg_blue);
  renderer.fillRectangle(renderer.width() - 7, 2, 5, 5, siteStateColor(snapshot_.site_state));
  renderer.line(1, 10, renderer.width() - 2, 10, colors::muted);

  if (!snapshot_.valid) {
    scene_support::drawTextCentered(renderer, 30, "BRIDGE OFFLINE", colors::muted, 1);
    return;
  }

  std::array<char, 16> number_buffer{};
  const int right_x = renderer.width() - 2;

  renderer.text(2, 14, "NEW REQ", colors::muted);
  const int new_requests = std::clamp(snapshot_.new_request_count, 0, 999);
  scene_support::drawTextRightAligned(renderer, right_x, 14,
                                      scene_support::formatNumber(number_buffer, new_requests),
                                      new_requests > 0 ? colors::amber : colors::white, 1);

  renderer.text(2, 23, "ACT MSN", colors::muted);
  const int active_missions = std::clamp(snapshot_.active_mission_count, 0, 999);
  scene_support::drawTextRightAligned(
      renderer, right_x, 23, scene_support::formatNumber(number_buffer, active_missions),
      colors::white, 1);

  renderer.text(2, 32, "MTD USD", colors::muted);
  scene_support::drawTextRightAligned(
      renderer, right_x, 32,
      scene_support::formatCompactUsd(number_buffer, snapshot_.revenue_mtd_cents),
      snapshot_.revenue_mtd_cents >= 0 ? colors::white : colors::muted, 1);

  renderer.line(1, 42, renderer.width() - 2, 42, colors::muted);

  const std::size_t request_count = snapshot_.latest_requests.size();
  const std::size_t mission_count = snapshot_.missions.size();
  const std::size_t total = request_count + mission_count;
  if (total == 0) {
    renderer.text(2, 48, "NO RECENT ACTIVITY", colors::muted);
    return;
  }

  const std::size_t rotation =
      static_cast<std::size_t>((context.monotonic_ms / detail_rotate_ms) %
                               static_cast<std::uint64_t>(total));
  std::array<char, 24> detail_buffer{};
  std::size_t length = 0;
  if (rotation < request_count) {
    const AmgRequestItem& request = snapshot_.latest_requests[rotation];
    length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length,
                                          request.label);
    length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length, " ");
    length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length,
                                          request.name);
    if (request.age_min >= 0) {
      length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length, " ");
      length = scene_support::appendBounded(
          detail_buffer.data(), detail_buffer.size(), length,
          scene_support::formatNumber(number_buffer, std::clamp(request.age_min, 0, 999)));
      length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length, "M");
    }
  } else {
    const AmgMissionItem& mission = snapshot_.missions[rotation - request_count];
    length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length,
                                          mission.label);
    length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length, " ");
    length = scene_support::appendBounded(detail_buffer.data(), detail_buffer.size(), length,
                                          mission.status);
  }
  renderer.text(2, 48, std::string_view(detail_buffer.data(), std::min(length, detail_max_chars)),
                colors::white);
}

}  // namespace amg::flightwall
