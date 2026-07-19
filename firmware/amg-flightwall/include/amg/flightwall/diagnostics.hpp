#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace amg::flightwall {

enum class Subsystem : std::size_t {
  display,
  configuration,
  network,
  storage,
  application,
  count,
};

enum class HealthLevel { healthy, degraded, failed, unknown };

enum class HealthCode {
  ok,
  not_initialized,
  invalid_configuration,
  unprovisioned,
  authentication_rejected,
  connection_timed_out,
  link_lost,
  allocation_failed,
  stale,
  timestamp_invalid,
};

struct HealthReport {
  Subsystem subsystem{Subsystem::application};
  HealthLevel level{HealthLevel::unknown};
  HealthCode code{HealthCode::not_initialized};
  std::uint64_t updated_at_ms{0};
};

struct DiagnosticsSnapshot {
  HealthLevel overall{HealthLevel::unknown};
  std::array<HealthReport, static_cast<std::size_t>(Subsystem::count)> entries{};
};

class HealthRegistry {
 public:
  HealthRegistry() noexcept;

  [[nodiscard]] bool report(HealthReport report, std::uint64_t observed_at_ms) noexcept;
  [[nodiscard]] DiagnosticsSnapshot snapshot(std::uint64_t now_ms,
                                             std::uint64_t stale_after_ms) const noexcept;

 private:
  std::array<HealthReport, static_cast<std::size_t>(Subsystem::count)> reports_{};
};

}  // namespace amg::flightwall
