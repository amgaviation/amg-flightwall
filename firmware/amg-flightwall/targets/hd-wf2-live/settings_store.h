// AMG FlightWall live target — device settings persistence.
//
// DeviceSettings <-> ArduinoJson <-> NVS namespace "amgfw" (config schema v2,
// stored as a JSON blob under key "config"). Secrets are stored in separate
// NVS keys (wifi_ssid, wifi_pass, amg_token, admin_sha) and are NEVER
// serialized into /api/config responses — only the presence flags
// amg.token_set and wifi.configured are exposed.
//
// Thread model: all methods must be called from the loop task. Async server
// callbacks work on JSON copies and enqueue commands that the loop task
// applies through this store.
#pragma once

#include <cstdint>
#include <vector>

#include <ArduinoJson.h>
#include <Preferences.h>
#include <WString.h>

namespace amg::flightwall::live {

struct GeometrySettings {
  int panel_w{64};
  int panel_h{64};
  int chain{2};
  String driver{"FM6126A"};  // FM6126A | FM6124 | SHIFTREG | ICN2038S | MBI5124
  bool clkphase{false};
  int latch_blanking{2};
  int min_refresh{60};

  [[nodiscard]] bool operator==(const GeometrySettings& other) const noexcept;
  [[nodiscard]] bool operator!=(const GeometrySettings& other) const noexcept {
    return !(*this == other);
  }
};

struct DisplaySettings {
  int brightness{140};
  int night_brightness{30};
  String night_start{"22:00"};
  String night_end{"07:00"};
  bool off_when_idle{false};
  GeometrySettings geometry{};
};

struct TimeSettings {
  String tz{"EST5EDT,M3.2.0,M11.1.0"};  // POSIX TZ string
  String ntp{"pool.ntp.org"};
};

struct LocationSettings {
  double lat{0.0};
  double lon{0.0};
  int radius_nm{30};
};

struct FlightsSettings {
  static constexpr std::size_t kMaxWatchlist = 8;
  String provider{"adsblol"};
  int poll_s{15};
  std::vector<String> watchlist{};
};

struct MetarSettings {
  String station{"KTEB"};
  int poll_s{600};
};

struct AmgSettings {
  String base_url{};
  int poll_s{45};
  bool notify_on_submission{true};
  bool notify_on_request{true};
};

struct PlaylistEntry {
  String id{};
  bool enabled{false};
  int duration_s{10};
};

struct CountdownSettings {
  String label{"NEXT DEP"};
  std::int64_t target_epoch{0};
};

struct MessageSettings {
  String text{};
  String color{"#176CFF"};
  bool scroll{true};
};

struct DeviceSettings {
  static constexpr int kSchemaVersion = 2;
  static constexpr std::size_t kMaxPlaylist = 12;

  int schema_version{kSchemaVersion};
  DisplaySettings display{};
  TimeSettings time{};
  LocationSettings location{};
  FlightsSettings flights{};
  MetarSettings metar{};
  AmgSettings amg{};
  std::vector<PlaylistEntry> playlist{};
  CountdownSettings countdown{};
  MessageSettings message{};
};

// Returns the contract-default playlist (all seven scene slots).
[[nodiscard]] std::vector<PlaylistEntry> defaultPlaylist();

// Lower-case SHA-256 hex digest of `input`.
[[nodiscard]] String sha256Hex(const String& input);

class SettingsStore {
 public:
  // Opens NVS namespace "amgfw" and loads config + secrets; falls back to
  // defaults when the blob is absent or invalid. Returns false only when NVS
  // itself cannot be opened.
  [[nodiscard]] bool begin();

  [[nodiscard]] DeviceSettings& settings() noexcept { return settings_; }
  [[nodiscard]] const DeviceSettings& settings() const noexcept { return settings_; }

  // Serializes current settings (no secrets; adds amg.token_set and
  // wifi.configured presence flags) into `document`.
  void toJson(JsonDocument& document) const;

  // Applies fields present in `document` onto `target` with validation and
  // clamping. Returns false when the document is structurally unusable.
  [[nodiscard]] static bool applyJson(JsonObjectConst document, DeviceSettings& target);

  // Persists the current settings blob to NVS. Returns false on write failure.
  [[nodiscard]] bool save();

  // --- secrets (separate NVS keys; write-only via API) ---
  [[nodiscard]] bool wifiConfigured() const noexcept { return wifi_ssid_.length() > 0; }
  [[nodiscard]] const String& wifiSsid() const noexcept { return wifi_ssid_; }
  [[nodiscard]] const String& wifiPassword() const noexcept { return wifi_pass_; }
  [[nodiscard]] bool amgTokenSet() const noexcept { return amg_token_.length() > 0; }
  [[nodiscard]] const String& amgToken() const noexcept { return amg_token_; }
  [[nodiscard]] bool adminPasswordSet() const noexcept { return admin_sha_.length() > 0; }
  [[nodiscard]] const String& adminPasswordHash() const noexcept { return admin_sha_; }

  [[nodiscard]] bool setWifiCredentials(const String& ssid, const String& password);
  [[nodiscard]] bool setAmgToken(const String& token);
  // Stores sha256(password) hex; the plain password is never persisted.
  [[nodiscard]] bool setAdminPassword(const String& password);

 private:
  void loadSecrets();

  Preferences preferences_{};
  bool ready_{false};
  DeviceSettings settings_{};
  String wifi_ssid_{};
  String wifi_pass_{};
  String amg_token_{};
  String admin_sha_{};
};

}  // namespace amg::flightwall::live
