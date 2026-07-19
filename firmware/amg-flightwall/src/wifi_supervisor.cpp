#include "amg/flightwall/wifi_supervisor.hpp"

#include <limits>

namespace amg::flightwall {
namespace {

constexpr std::uint64_t base_backoff_ms = 1'000;
constexpr std::uint64_t maximum_base_backoff_ms = 50'000;
constexpr std::uint64_t maximum_backoff_ms = 60'000;
constexpr std::uint8_t maximum_attempts = 8;

bool sameProfile(const WifiProfileReference left, const WifiProfileReference right) noexcept {
  return left.key == right.key && left.revision == right.revision;
}

bool sameAttempt(const WifiConnectionAttempt left, const WifiConnectionAttempt right) noexcept {
  return sameProfile(left.profile, right.profile) && left.generation == right.generation;
}

bool connectionMayBeActive(const WifiState state) noexcept {
  return state == WifiState::connecting || state == WifiState::online;
}

std::uint64_t retryDelay(const std::uint8_t attempt_count,
                         const WifiProfileReference profile) noexcept {
  std::uint64_t delay = base_backoff_ms;
  for (std::uint8_t attempt = 1;
       attempt < attempt_count && delay < maximum_base_backoff_ms;
       ++attempt) {
    delay = delay > maximum_base_backoff_ms / 2 ? maximum_base_backoff_ms : delay * 2;
  }

  const std::uint64_t spread = delay / 5;
  const std::uint64_t seed = static_cast<std::uint64_t>(profile.key) * 0x9E3779B1U ^
                             static_cast<std::uint64_t>(profile.revision) * 0x85EBCA77U ^
                             static_cast<std::uint64_t>(attempt_count) * 0xC2B2AE3DU;
  const std::uint64_t jittered = delay - spread + seed % (2 * spread + 1);
  return jittered > maximum_backoff_ms ? maximum_backoff_ms : jittered;
}

std::uint64_t saturatingAdd(const std::uint64_t left, const std::uint64_t right) noexcept {
  const std::uint64_t maximum = std::numeric_limits<std::uint64_t>::max();
  return left > maximum - right ? maximum : left + right;
}

}  // namespace

void WifiSupervisor::setEnabled(const bool enabled) noexcept {
  if (enabled_ && !enabled && connectionMayBeActive(status_.state)) {
    pending_action_ = WifiAction::disconnect;
  }
  if (!enabled) {
    active_attempt_ = {};
  }
  enabled_ = enabled;
  if (!enabled_) {
    status_.state = WifiState::disabled;
  } else if (requires_reprovisioning_) {
    status_.state = WifiState::requires_attention;
  } else {
    status_.state = configured_profile_.valid() ? WifiState::idle : WifiState::unprovisioned;
  }
}

void WifiSupervisor::setConfiguredProfile(const WifiProfileReference profile) noexcept {
  const WifiProfileReference normalized = profile.valid() ? profile : WifiProfileReference{};
  if (sameProfile(configured_profile_, normalized)) {
    return;
  }

  if (connectionMayBeActive(status_.state)) {
    pending_action_ = WifiAction::disconnect;
  }
  active_attempt_ = {};
  configured_profile_ = normalized;
  requires_reprovisioning_ = false;
  status_ = {};
  status_.state = enabled_ ? (configured_profile_.valid() ? WifiState::idle
                                                          : WifiState::unprovisioned)
                           : WifiState::disabled;
}

WifiProfileReference WifiSupervisor::configuredProfile() const noexcept {
  return configured_profile_;
}

WifiConnectionAttempt WifiSupervisor::activeAttempt() const noexcept { return active_attempt_; }

WifiAction WifiSupervisor::poll(const std::uint64_t now_ms) noexcept {
  if (pending_action_ != WifiAction::none) {
    const WifiAction action = pending_action_;
    pending_action_ = WifiAction::none;
    return action;
  }
  if (!enabled_ || !configured_profile_.valid()) {
    return WifiAction::none;
  }

  const bool ready_to_connect = status_.state == WifiState::idle ||
                                (status_.state == WifiState::backoff &&
                                 now_ms >= status_.next_attempt_ms);
  if (!ready_to_connect) {
    return WifiAction::none;
  }

  status_.state = WifiState::connecting;
  if (status_.attempt_count < std::numeric_limits<std::uint8_t>::max()) {
    ++status_.attempt_count;
  }
  attempt_generation_ = attempt_generation_ == std::numeric_limits<std::uint64_t>::max()
                            ? 1
                            : attempt_generation_ + 1;
  active_attempt_ = {configured_profile_, attempt_generation_};
  return WifiAction::connect_using_configured_profile;
}

bool WifiSupervisor::observe(const WifiConnectionAttempt attempt,
                             const WifiObservation observation,
                             const std::uint64_t now_ms) noexcept {
  if (!enabled_ || !configured_profile_.valid() || !attempt.valid() ||
      !sameAttempt(attempt, active_attempt_)) {
    return false;
  }

  const bool connecting_observation =
      status_.state == WifiState::connecting &&
      (observation == WifiObservation::connected ||
       observation == WifiObservation::authentication_rejected ||
       observation == WifiObservation::connection_timed_out);
  const bool online_observation =
      status_.state == WifiState::online &&
      (observation == WifiObservation::connected || observation == WifiObservation::link_lost);
  if (!connecting_observation && !online_observation) {
    return false;
  }

  if (observation == WifiObservation::connected) {
    status_.state = WifiState::online;
    status_.last_failure = WifiFailure::none;
    status_.attempt_count = 0;
    status_.next_attempt_ms = 0;
    return true;
  }

  if (observation == WifiObservation::authentication_rejected) {
    active_attempt_ = {};
    requires_reprovisioning_ = true;
    status_.state = WifiState::requires_attention;
    status_.last_failure = WifiFailure::authentication_rejected;
    status_.next_attempt_ms = 0;
    return true;
  }

  if (observation == WifiObservation::connection_timed_out ||
      observation == WifiObservation::link_lost) {
    if (status_.attempt_count >= maximum_attempts) {
      active_attempt_ = {};
      requires_reprovisioning_ = true;
      status_.state = WifiState::requires_attention;
      status_.last_failure = WifiFailure::retry_exhausted;
      status_.next_attempt_ms = 0;
      return true;
    }
    active_attempt_ = {};
    status_.state = WifiState::backoff;
    status_.last_failure = observation == WifiObservation::connection_timed_out
                               ? WifiFailure::connection_timed_out
                               : WifiFailure::link_lost;
    status_.next_attempt_ms =
        saturatingAdd(now_ms, retryDelay(status_.attempt_count, configured_profile_));
    return true;
  }
  return false;
}

const WifiStatus& WifiSupervisor::status() const noexcept { return status_; }

}  // namespace amg::flightwall
