#include "wifi_manager.h"

#include <ctime>

#include <ESPmDNS.h>
#include <WiFi.h>

namespace amg::flightwall::live {

namespace {

constexpr std::uint64_t kConnectTimeoutMs = 20'000;
constexpr std::uint64_t kMinimumValidEpoch = 1'700'000'000;  // 2023-11-14; guards unsynced clock

[[nodiscard]] const char* stateName(const WifiState state) noexcept {
  switch (state) {
    case WifiState::disabled:
      return "disabled";
    case WifiState::unprovisioned:
      return "unprovisioned";
    case WifiState::idle:
      return "idle";
    case WifiState::connecting:
      return "connecting";
    case WifiState::online:
      return "online";
    case WifiState::backoff:
      return "backoff";
    case WifiState::requires_attention:
      return "requires_attention";
  }
  return "unknown";
}

}  // namespace

void WifiManager::begin() {
  WiFi.persistent(false);

  if (!store_.wifiConfigured()) {
    log_.append("wifi: no stored credentials, starting setup portal");
    startSetupAp();
    return;
  }

  supervisor_.setConfiguredProfile({1, 1});
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(kHostname);
  sta_started_ = true;
  log_.logf("wifi: STA mode, target ssid \"%s\"", store_.wifiSsid().c_str());
}

void WifiManager::startSetupAp() {
  setup_mode_ = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(kSetupSsid);  // open AP per contract; LAN-local setup only
  dns_.setErrorReplyCode(DNSReplyCode::NoError);
  dns_.start(53, "*", WiFi.softAPIP());
  log_.logf("wifi: setup AP \"%s\" at %s", kSetupSsid, WiFi.softAPIP().toString().c_str());
}

void WifiManager::applyTimeConfiguration() {
  const DeviceSettings& settings = store_.settings();
  configTzTime(settings.time.tz.c_str(), settings.time.ntp.c_str());
  log_.logf("wifi: sntp %s tz %s", settings.time.ntp.c_str(), settings.time.tz.c_str());
}

void WifiManager::observeAndReport(const WifiObservation observation,
                                   const std::uint64_t now_ms) {
  const bool accepted = supervisor_.observe(supervisor_.activeAttempt(), observation, now_ms);
  if (!accepted) {
    return;
  }
  log_.logf("wifi: state -> %s", stateName(supervisor_.status().state));
  if (supervisor_.status().state == WifiState::requires_attention && !setup_mode_) {
    log_.append("wifi: connection requires attention, raising setup portal");
    WiFi.disconnect(true);
    startSetupAp();
  }
}

void WifiManager::loop(const std::uint64_t now_ms) {
  if (setup_mode_) {
    dns_.processNextRequest();
    return;
  }
  if (!sta_started_) {
    return;
  }

  const WifiAction action = supervisor_.poll(now_ms);
  if (action == WifiAction::connect_using_configured_profile) {
    attempt_started_ms_ = now_ms;
    attempt_in_flight_ = true;
    services_started_ = false;
    WiFi.disconnect();
    WiFi.begin(store_.wifiSsid().c_str(), store_.wifiPassword().c_str());
    log_.logf("wifi: connect attempt %u", supervisor_.status().attempt_count);
  } else if (action == WifiAction::disconnect) {
    WiFi.disconnect();
    attempt_in_flight_ = false;
  }

  const wl_status_t link = WiFi.status();
  const WifiState state = supervisor_.status().state;

  if (state == WifiState::connecting && attempt_in_flight_) {
    if (link == WL_CONNECTED) {
      observeAndReport(WifiObservation::connected, now_ms);
      log_.logf("wifi: online, ip %s rssi %d", WiFi.localIP().toString().c_str(), WiFi.RSSI());
      if (!services_started_) {
        services_started_ = true;
        if (MDNS.begin(kHostname)) {
          MDNS.addService("http", "tcp", 80);
        }
        applyTimeConfiguration();
      }
    } else if (link == WL_CONNECT_FAILED) {
      log_.append("wifi: authentication rejected");
      observeAndReport(WifiObservation::authentication_rejected, now_ms);
      attempt_in_flight_ = false;
    } else if (now_ms - attempt_started_ms_ >= kConnectTimeoutMs) {
      log_.append("wifi: connect timed out");
      WiFi.disconnect();
      observeAndReport(WifiObservation::connection_timed_out, now_ms);
      attempt_in_flight_ = false;
    }
  } else if (state == WifiState::online && link != WL_CONNECTED) {
    log_.append("wifi: link lost");
    observeAndReport(WifiObservation::link_lost, now_ms);
    attempt_in_flight_ = false;
  }
}

bool WifiManager::online() const noexcept {
  return supervisor_.status().state == WifiState::online;
}

bool WifiManager::timeSynced() const noexcept {
  return static_cast<std::uint64_t>(time(nullptr)) > kMinimumValidEpoch;
}

ClockInfo WifiManager::clockInfo() const noexcept {
  ClockInfo info{};
  const time_t now = time(nullptr);
  if (static_cast<std::uint64_t>(now) <= kMinimumValidEpoch) {
    return info;
  }
  tm local{};
  localtime_r(&now, &local);
  info.hour = local.tm_hour;
  info.minute = local.tm_min;
  info.second = local.tm_sec;
  info.month = local.tm_mon + 1;
  info.day = local.tm_mday;
  info.weekday = local.tm_wday;
  info.valid = true;
  return info;
}

WifiRuntimeStatus WifiManager::status() const {
  WifiRuntimeStatus status;
  status.state = setup_mode_ ? "setup_ap" : stateName(supervisor_.status().state);
  status.setup_mode = setup_mode_;
  status.time_synced = timeSynced();
  if (setup_mode_) {
    status.ssid = kSetupSsid;
    status.ip = WiFi.softAPIP().toString();
  } else if (online()) {
    status.ssid = WiFi.SSID();
    status.rssi = WiFi.RSSI();
    status.ip = WiFi.localIP().toString();
  } else {
    status.ssid = store_.wifiSsid();
  }
  return status;
}

IPAddress WifiManager::apAddress() const noexcept { return WiFi.softAPIP(); }

}  // namespace amg::flightwall::live
