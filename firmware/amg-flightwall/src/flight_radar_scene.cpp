#include "amg/flightwall/flight_radar_scene.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "amg/flightwall/scene_support.hpp"

namespace amg::flightwall {
namespace {

constexpr int map_x = 88;
constexpr int map_y = 12;
constexpr int map_w = 40;
constexpr int map_h = 52;
constexpr std::size_t max_callsign_chars = 6;
constexpr double pi = 3.14159265358979323846;
constexpr double nm_per_degree = 60.0;

}  // namespace

void FlightRadarScene::setOwnPosition(const double lat, const double lon) noexcept {
  own_lat_ = lat;
  own_lon_ = lon;
}

void FlightRadarScene::setRangeNm(const int range_nm) noexcept {
  range_nm_ = std::max(range_nm, 1);
}

void FlightRadarScene::setSnapshot(const FlightSnapshot& snapshot) {
  valid_ = snapshot.valid;
  row_count_ = 0;
  blip_count_ = 0;
  if (!valid_) {
    return;
  }

  std::vector<const FlightContact*> ordered;
  ordered.reserve(snapshot.contacts.size());
  for (const FlightContact& contact : snapshot.contacts) {
    ordered.push_back(&contact);
  }
  std::sort(ordered.begin(), ordered.end(),
            [](const FlightContact* left, const FlightContact* right) {
              return left->distance_nm < right->distance_nm;
            });

  const double cos_lat = std::cos(own_lat_ * pi / 180.0);
  const int center_x = map_x + map_w / 2;
  const int center_y = map_y + map_h / 2;
  const double half_w = map_w / 2.0 - 2.0;
  const double half_h = map_h / 2.0 - 2.0;
  const double range = static_cast<double>(range_nm_);

  for (const FlightContact* contact : ordered) {
    if (row_count_ < max_rows) {
      Row& row = rows_[row_count_++];
      row.callsign_length = 0;
      for (const char character : contact->callsign) {
        if (row.callsign_length >= max_callsign_chars) {
          break;
        }
        row.callsign[row.callsign_length++] =
            scene_support::fontSupports(character) ? character : ' ';
      }
      row.flight_level = std::clamp(contact->altitude_ft / 100, 0, 999);
      row.speed_kt = std::clamp(contact->ground_speed_kt, 0, 999);
      row.watchlisted = contact->watchlisted;
    }
    if (blip_count_ < max_blips) {
      const double dx_nm = (contact->lon - own_lon_) * nm_per_degree * cos_lat;
      const double dy_nm = (contact->lat - own_lat_) * nm_per_degree;
      int x = center_x + static_cast<int>(dx_nm / range * half_w);
      int y = center_y - static_cast<int>(dy_nm / range * half_h);
      x = std::clamp(x, map_x + 1, map_x + map_w - 3);
      y = std::clamp(y, map_y + 1, map_y + map_h - 3);
      blips_[blip_count_++] = Blip{x, y, contact->watchlisted};
    }
  }
}

std::string_view FlightRadarScene::id() const noexcept { return "flights"; }

void FlightRadarScene::render(FrameContext& context) noexcept {
  Renderer& renderer = context.renderer;
  renderer.clear(colors::black);
  renderer.text(2, 2, "FLIGHT RADAR", colors::amg_blue);
  renderer.line(1, 10, renderer.width() - 2, 10, colors::muted);
  renderer.rectangle(map_x, map_y, map_w, map_h, colors::muted);

  if (!valid_) {
    renderer.text(2, 16, "DATA STALE", colors::muted);
    renderer.text(map_x + 8, map_y + map_h / 2 - 3, "OFF", colors::muted);
    return;
  }

  const int center_x = map_x + map_w / 2;
  const int center_y = map_y + map_h / 2;
  renderer.line(center_x - 1, center_y, center_x + 1, center_y, colors::amg_blue);
  renderer.line(center_x, center_y - 1, center_x, center_y + 1, colors::amg_blue);

  for (std::size_t index = 0; index < blip_count_; ++index) {
    const Blip& blip = blips_[index];
    renderer.fillRectangle(blip.x, blip.y, 2, 2,
                           blip.watchlisted ? colors::amber : colors::cyan);
  }

  if (row_count_ == 0) {
    renderer.text(2, 32, "NO TRAFFIC", colors::muted);
    return;
  }

  renderer.text(2, 13, "CALL", colors::muted);
  renderer.text(44, 13, "FL", colors::muted);
  renderer.text(64, 13, "KT", colors::muted);
  std::array<char, 16> number_buffer{};
  int y = 22;
  for (std::size_t index = 0; index < row_count_; ++index) {
    const Row& row = rows_[index];
    const Color color = row.watchlisted ? colors::amber : colors::white;
    renderer.text(2, y, std::string_view(row.callsign.data(), row.callsign_length), color);
    renderer.text(44, y, scene_support::formatNumber(number_buffer, row.flight_level), color);
    renderer.text(64, y, scene_support::formatNumber(number_buffer, row.speed_kt), color);
    y += 8;
  }
}

}  // namespace amg::flightwall
