// AMG FlightWall live target — data providers.
//
// Non-blocking pollers driven from loop(): each provider owns a due-time and
// exponential backoff; when due (and Wi-Fi is online) it performs one bounded
// HTTP fetch, normalizes the payload into the portable snapshot structs, and
// flags the composition root to push the result into the scenes under the
// state mutex. Individual fetches use HTTPClient with short timeouts; the
// scene tick rate (~10 fps) tolerates the fetch pause per the contract.
//
// TLS: WiFiClientSecure::setCACertBundle is used when the firmware is built
// with -DAMG_CERT_BUNDLE_EMBEDDED and the Arduino x509 bundle embedded via
// board_build.embed_files (platformio.ini is orchestrator-owned — see the
// build report). Without the bundle each endpoint falls back to
// setInsecure(), controllable per endpoint with setInsecureTls(). LAN
// deployments accept this; the fallback is logged once per provider.
#pragma once

#include <cstdint>
#include <functional>

#include "live_compat_stubs.h"
#include "log_buffer.h"
#include "settings_store.h"

namespace amg::flightwall::live {

// Shared scheduling + backoff state machine. loop() calls tick(); when the
// provider is due, fetch() runs once. Failure doubles the retry delay
// (interval * 2^fails, capped at kMaxBackoffMs).
class PollerBase {
 public:
  virtual ~PollerBase() = default;

  void tick(std::uint64_t now_ms, bool online);

  void setInsecureTls(const bool insecure) noexcept { insecure_tls_ = insecure; }
  [[nodiscard]] bool insecureTls() const noexcept { return insecure_tls_; }

  // Forces the next tick to fetch immediately (e.g. after a config change).
  void pollNow() noexcept { next_due_ms_ = 0; }

 protected:
  static constexpr std::uint64_t kMaxBackoffMs = 10 * 60 * 1000;

  [[nodiscard]] virtual bool fetch(std::uint64_t now_ms) = 0;
  [[nodiscard]] virtual std::uint32_t intervalMs() const = 0;
  [[nodiscard]] virtual bool enabled() const = 0;

 private:
  std::uint64_t next_due_ms_{0};
  std::uint8_t failure_count_{0};
  bool insecure_tls_{true};  // default true until the cert bundle is embedded
};

class FlightProvider final : public PollerBase {
 public:
  static constexpr std::size_t kMaxContacts = 10;

  FlightProvider(SettingsStore& store, LogBuffer& log) noexcept : store_(store), log_(log) {}

  [[nodiscard]] const FlightSnapshot& snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] bool consumeUpdated() noexcept;

 protected:
  [[nodiscard]] bool fetch(std::uint64_t now_ms) override;
  [[nodiscard]] std::uint32_t intervalMs() const override;
  [[nodiscard]] bool enabled() const override;

 private:
  SettingsStore& store_;
  LogBuffer& log_;
  FlightSnapshot snapshot_{};
  bool updated_{false};
};

class MetarProvider final : public PollerBase {
 public:
  MetarProvider(SettingsStore& store, LogBuffer& log) noexcept : store_(store), log_(log) {}

  [[nodiscard]] const MetarSnapshot& snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] bool consumeUpdated() noexcept;

 protected:
  [[nodiscard]] bool fetch(std::uint64_t now_ms) override;
  [[nodiscard]] std::uint32_t intervalMs() const override;
  [[nodiscard]] bool enabled() const override;

 private:
  SettingsStore& store_;
  LogBuffer& log_;
  MetarSnapshot snapshot_{};
  bool updated_{false};
};

class AmgProvider final : public PollerBase {
 public:
  static constexpr std::size_t kMaxItems = 8;

  using NotificationSink = std::function<void(const NotificationEvent&)>;

  AmgProvider(SettingsStore& store, LogBuffer& log) noexcept : store_(store), log_(log) {}

  // `sink` runs on the loop task from within tick().
  void setNotificationSink(NotificationSink sink) { notification_sink_ = std::move(sink); }

  [[nodiscard]] const AmgMetricsSnapshot& snapshot() const noexcept { return snapshot_; }
  [[nodiscard]] bool consumeUpdated() noexcept;

 protected:
  [[nodiscard]] bool fetch(std::uint64_t now_ms) override;
  [[nodiscard]] std::uint32_t intervalMs() const override;
  [[nodiscard]] bool enabled() const override;

 private:
  SettingsStore& store_;
  LogBuffer& log_;
  AmgMetricsSnapshot snapshot_{};
  NotificationSink notification_sink_{};
  std::string last_cursor_{};
  int last_request_count_{-1};
  bool updated_{false};
};

}  // namespace amg::flightwall::live
