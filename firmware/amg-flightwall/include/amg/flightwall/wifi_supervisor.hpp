#pragma once

#include <cstdint>

namespace amg::flightwall {

enum class WifiState { disabled, unprovisioned, idle, connecting, online, backoff, requires_attention };
enum class WifiAction { none, connect_using_configured_profile, disconnect };
enum class WifiObservation { connected, authentication_rejected, connection_timed_out, link_lost };
enum class WifiFailure {
  none,
  authentication_rejected,
  connection_timed_out,
  link_lost,
  retry_exhausted,
};

struct WifiStatus {
  WifiState state{WifiState::unprovisioned};
  WifiFailure last_failure{WifiFailure::none};
  std::uint8_t attempt_count{0};
  std::uint64_t next_attempt_ms{0};
};

struct WifiProfileReference {
  std::uint32_t key{0};
  std::uint32_t revision{0};

  [[nodiscard]] bool valid() const noexcept { return key != 0 && revision != 0; }
};

struct WifiConnectionAttempt {
  WifiProfileReference profile{};
  std::uint64_t generation{0};

  [[nodiscard]] bool valid() const noexcept { return profile.valid() && generation != 0; }
};

class WifiSupervisor {
 public:
  void setEnabled(bool enabled) noexcept;
  void setConfiguredProfile(WifiProfileReference profile) noexcept;
  [[nodiscard]] WifiProfileReference configuredProfile() const noexcept;
  [[nodiscard]] WifiConnectionAttempt activeAttempt() const noexcept;
  [[nodiscard]] WifiAction poll(std::uint64_t now_ms) noexcept;
  [[nodiscard]] bool observe(WifiConnectionAttempt attempt, WifiObservation observation,
                             std::uint64_t now_ms) noexcept;
  [[nodiscard]] const WifiStatus& status() const noexcept;

 private:
  bool enabled_{true};
  bool requires_reprovisioning_{false};
  WifiProfileReference configured_profile_{};
  WifiConnectionAttempt active_attempt_{};
  std::uint64_t attempt_generation_{0};
  WifiAction pending_action_{WifiAction::none};
  WifiStatus status_{};
};

}  // namespace amg::flightwall
