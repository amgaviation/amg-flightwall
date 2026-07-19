#pragma once

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Mission rows with a status color chip, label, and ETA. An invalid snapshot
// renders a labeled "BRIDGE OFFLINE" state; a valid empty board renders
// "NO ACTIVE MISSIONS".
class AmgMissionBoardScene final : public Scene {
 public:
  static constexpr std::size_t max_rows = 6;

  void setSnapshot(const AmgMetricsSnapshot& snapshot);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  bool valid_{false};
  std::vector<AmgMissionItem> missions_{};  // sanitized, bounded copy
};

}  // namespace amg::flightwall
