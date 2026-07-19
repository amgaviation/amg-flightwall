#pragma once

#include <cstdint>

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Label plus D:HH:MM:SS remaining. The platform supplies seconds_remaining;
// between pushes the scene counts down using injected monotonic time only
// (never wall clock). A negative seconds_remaining renders "NO TARGET".
class CountdownScene final : public Scene {
 public:
  void setCountdown(const CountdownInfo& info);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  CountdownInfo info_{};  // sanitized copy
  std::uint64_t base_ms_{0};
  bool base_captured_{false};
};

}  // namespace amg::flightwall
