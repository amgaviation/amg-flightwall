#include "amg/flightwall/mode.hpp"

namespace amg::flightwall {

void ModeController::setRequestedMode(const Mode mode) noexcept { requested_ = mode; }

Mode ModeController::requestedMode() const noexcept { return requested_; }

Mode ModeController::effectiveMode(const std::uint64_t now_ms) const noexcept {
  if (requested_ != Mode::automatic) {
    return requested_;
  }
  if (trigger_.has_value() && now_ms >= trigger_->received_at_ms && now_ms < trigger_->expires_at_ms) {
    return Mode::operations;
  }
  return Mode::classic;
}

void ModeController::observe(const TriggerEvent& event) {
  if (event.expires_at_ms <= event.received_at_ms || event.reason.empty()) {
    return;
  }
  if (!trigger_.has_value() || event.priority >= trigger_->priority || event.received_at_ms >= trigger_->expires_at_ms) {
    trigger_ = event;
  }
}

void ModeController::clearTrigger() noexcept { trigger_.reset(); }

const std::optional<TriggerEvent>& ModeController::activeTrigger() const noexcept { return trigger_; }

}  // namespace amg::flightwall
