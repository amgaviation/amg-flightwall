#pragma once

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Big HH:MM (scale 2) with a seconds tick and a weekday MM/DD date line.
// The platform pushes wall-clock via setClock; an invalid ClockInfo renders a
// labeled "NO TIME SYNC" state.
class ClockScene final : public Scene {
 public:
  void setClock(const ClockInfo& info) noexcept;

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  ClockInfo info_{};
};

}  // namespace amg::flightwall
