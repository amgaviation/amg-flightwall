// AMG FlightWall live target — Wi-Fi + captive portal + time sync.
//
// Drives the portable core WifiSupervisor: STA connect with the credentials
// stored in the settings store. When unprovisioned, or when the supervisor
// escalates to requires_attention, the device raises the open setup AP
// "FlightWall-Setup" with a catch-all captive DNS redirect so the same web
// app serves the first-boot portal. When online: mDNS "flightwall" and SNTP
// (configurable POSIX TZ + NTP host) feeding ClockInfo for the clock scene.
//
// Loop-task only; the web server reads Wi-Fi state through the mutex-guarded
// status snapshot maintained by the composition root.
#pragma once

#include <cstdint>

#include <DNSServer.h>
#include <IPAddress.h>
#include <WString.h>

#include "amg/flightwall/wifi_supervisor.hpp"
#include "live_compat_stubs.h"
#include "log_buffer.h"
#include "settings_store.h"

namespace amg::flightwall::live {

struct WifiRuntimeStatus {
  const char* state{"unprovisioned"};
  String ssid{};
  int rssi{0};
  String ip{};
  bool setup_mode{false};
  bool time_synced{false};
};

class WifiManager {
 public:
  static constexpr const char* kSetupSsid = "FlightWall-Setup";
  static constexpr const char* kHostname = "flightwall";

  WifiManager(SettingsStore& store, LogBuffer& log) noexcept : store_(store), log_(log) {}

  void begin();
  void loop(std::uint64_t now_ms);

  [[nodiscard]] bool setupMode() const noexcept { return setup_mode_; }
  [[nodiscard]] bool online() const noexcept;
  [[nodiscard]] bool timeSynced() const noexcept;
  [[nodiscard]] ClockInfo clockInfo() const noexcept;
  [[nodiscard]] WifiRuntimeStatus status() const;
  [[nodiscard]] IPAddress apAddress() const noexcept;

 private:
  void startSetupAp();
  void applyTimeConfiguration();
  void observeAndReport(WifiObservation observation, std::uint64_t now_ms);

  SettingsStore& store_;
  LogBuffer& log_;
  WifiSupervisor supervisor_{};
  DNSServer dns_{};
  bool setup_mode_{false};
  bool sta_started_{false};
  bool services_started_{false};
  std::uint64_t attempt_started_ms_{0};
  bool attempt_in_flight_{false};
};

}  // namespace amg::flightwall::live
