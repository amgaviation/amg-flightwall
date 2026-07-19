#pragma once

#include "amg/flightwall/data_models.hpp"
#include "amg/flightwall/scene.hpp"

namespace amg::flightwall {

// Headline counters (NEW REQ n in amber when > 0, ACT MSN n, MTD USD compact)
// plus a detail line rotating through latest requests and missions. The '$'
// glyph does not exist in the 5x7 font, so revenue renders as "MTD USD 12.4K".
// An invalid snapshot renders a labeled "BRIDGE OFFLINE" state.
class AmgOpsScene final : public Scene {
 public:
  static constexpr std::size_t max_items = 6;

  void setSnapshot(const AmgMetricsSnapshot& snapshot);

  [[nodiscard]] std::string_view id() const noexcept override;
  void render(FrameContext& context) noexcept override;

 private:
  AmgMetricsSnapshot snapshot_{};  // sanitized, bounded copy
};

}  // namespace amg::flightwall
