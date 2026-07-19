#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace amg::flightwall {

enum class Mode { classic, operations, automatic };
enum class AutoTrigger { watchlist_aircraft, active_mission, maintenance_alert, crew_notification, airport_event };

struct TriggerEvent {
  AutoTrigger type;
  std::uint8_t priority{0};
  std::uint64_t received_at_ms{0};
  std::uint64_t expires_at_ms{0};
  std::string reason{};
};

class ModeController {
 public:
  void setRequestedMode(Mode mode) noexcept;
  [[nodiscard]] Mode requestedMode() const noexcept;
  [[nodiscard]] Mode effectiveMode(std::uint64_t now_ms) const noexcept;

  void observe(const TriggerEvent& event);
  void clearTrigger() noexcept;
  [[nodiscard]] const std::optional<TriggerEvent>& activeTrigger() const noexcept;

 private:
  Mode requested_{Mode::classic};
  std::optional<TriggerEvent> trigger_{};
};

}  // namespace amg::flightwall
