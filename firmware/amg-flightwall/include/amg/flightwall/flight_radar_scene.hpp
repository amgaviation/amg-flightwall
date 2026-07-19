#pragma once

#include <array>
#include <cstddef>

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Nearest contacts as rows (callsign, flight level, ground speed) plus a mini
// plan-view map with own position at the center. Watchlisted contacts render
// in colors::amber. Row/blip layout is precomputed in setSnapshot so the
// render path is heap-free; an invalid snapshot renders a labeled stale state.
class FlightRadarScene final : public Scene {
 public:
  static constexpr std::size_t max_rows = 5;
  static constexpr std::size_t max_blips = 8;

  void setOwnPosition(double lat, double lon) noexcept;
  void setRangeNm(int range_nm) noexcept;
  void setSnapshot(const FlightSnapshot& snapshot);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  struct Row {
    std::array<char, 8> callsign{};
    std::size_t callsign_length{0};
    int flight_level{0};
    int speed_kt{0};
    bool watchlisted{false};
  };

  struct Blip {
    int x{0};
    int y{0};
    bool watchlisted{false};
  };

  double own_lat_{0.0};
  double own_lon_{0.0};
  int range_nm_{30};
  bool valid_{false};
  std::array<Row, max_rows> rows_{};
  std::size_t row_count_{0};
  std::array<Blip, max_blips> blips_{};
  std::size_t blip_count_{0};
};

}  // namespace amg::flightwall
