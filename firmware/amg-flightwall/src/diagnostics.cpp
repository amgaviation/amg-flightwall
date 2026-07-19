#include "amg/flightwall/diagnostics.hpp"

namespace amg::flightwall {
namespace {

constexpr std::size_t subsystemCount = static_cast<std::size_t>(Subsystem::count);

std::size_t indexOf(const Subsystem subsystem) noexcept {
  return static_cast<std::size_t>(subsystem);
}

bool isReported(const HealthReport& report) noexcept {
  return report.code != HealthCode::not_initialized;
}

int severity(const HealthLevel level) noexcept {
  switch (level) {
    case HealthLevel::failed:
      return 3;
    case HealthLevel::degraded:
      return 2;
    case HealthLevel::unknown:
      return 1;
    case HealthLevel::healthy:
      return 0;
  }
  return 1;
}

HealthLevel expectedLevel(const HealthCode code) noexcept {
  switch (code) {
    case HealthCode::ok:
      return HealthLevel::healthy;
    case HealthCode::unprovisioned:
    case HealthCode::connection_timed_out:
    case HealthCode::link_lost:
      return HealthLevel::degraded;
    case HealthCode::invalid_configuration:
    case HealthCode::authentication_rejected:
    case HealthCode::allocation_failed:
      return HealthLevel::failed;
    case HealthCode::not_initialized:
    case HealthCode::stale:
    case HealthCode::timestamp_invalid:
      return HealthLevel::unknown;
  }
  return HealthLevel::unknown;
}

}  // namespace

HealthRegistry::HealthRegistry() noexcept {
  for (std::size_t index = 0; index < reports_.size(); ++index) {
    reports_[index] = {static_cast<Subsystem>(index), HealthLevel::unknown,
                       HealthCode::not_initialized, 0};
  }
}

bool HealthRegistry::report(const HealthReport report,
                            const std::uint64_t observed_at_ms) noexcept {
  const std::size_t index = indexOf(report.subsystem);
  if (index >= subsystemCount || report.level != expectedLevel(report.code) ||
      report.updated_at_ms > observed_at_ms ||
      (isReported(reports_[index]) && report.updated_at_ms < reports_[index].updated_at_ms)) {
    return false;
  }
  reports_[index] = report;
  return true;
}

DiagnosticsSnapshot HealthRegistry::snapshot(const std::uint64_t now_ms,
                                               const std::uint64_t stale_after_ms) const noexcept {
  DiagnosticsSnapshot result;
  result.entries = reports_;

  bool has_report = false;
  HealthLevel overall = HealthLevel::healthy;
  for (HealthReport& entry : result.entries) {
    if (!isReported(entry)) {
      continue;
    }

    has_report = true;
    if (entry.updated_at_ms > now_ms) {
      entry.level = HealthLevel::unknown;
      entry.code = HealthCode::timestamp_invalid;
    } else if (now_ms - entry.updated_at_ms > stale_after_ms) {
      entry.level = HealthLevel::unknown;
      entry.code = HealthCode::stale;
    }
    if (severity(entry.level) > severity(overall)) {
      overall = entry.level;
    }
  }

  result.overall = has_report ? overall : HealthLevel::unknown;
  return result;
}

}  // namespace amg::flightwall
