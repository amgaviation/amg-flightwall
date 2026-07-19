#pragma once

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Station and color-coded flight category (VFR green, MVFR blue, IFR red,
// LIFR magenta), wind/visibility/temperature rows, and the raw METAR scrolling
// on the bottom row. An invalid snapshot renders a labeled stale state.
class MetarScene final : public Scene {
 public:
  void setSnapshot(const MetarSnapshot& snapshot);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  MetarSnapshot snapshot_{};  // sanitized, upper-cased copy
};

}  // namespace amg::flightwall
