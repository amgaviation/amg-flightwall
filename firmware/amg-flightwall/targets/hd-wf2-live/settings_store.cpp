#include "settings_store.h"

#include <mbedtls/sha256.h>

namespace amg::flightwall::live {

namespace {

constexpr const char* kNamespace = "amgfw";
constexpr const char* kConfigKey = "config";
constexpr const char* kWifiSsidKey = "wifi_ssid";
constexpr const char* kWifiPassKey = "wifi_pass";
constexpr const char* kAmgTokenKey = "amg_token";
constexpr const char* kAdminShaKey = "admin_sha";

[[nodiscard]] int clampInt(const int value, const int low, const int high) noexcept {
  if (value < low) {
    return low;
  }
  if (value > high) {
    return high;
  }
  return value;
}

[[nodiscard]] bool validTimeOfDay(const String& value) noexcept {
  if (value.length() != 5 || value[2] != ':') {
    return false;
  }
  const int hour = (value[0] - '0') * 10 + (value[1] - '0');
  const int minute = (value[3] - '0') * 10 + (value[4] - '0');
  return isDigit(value[0]) && isDigit(value[1]) && isDigit(value[3]) && isDigit(value[4]) &&
         hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}

[[nodiscard]] bool knownDriver(const String& value) noexcept {
  return value == "FM6126A" || value == "FM6124" || value == "SHIFTREG" || value == "ICN2038S" ||
         value == "MBI5124";
}

[[nodiscard]] bool knownSceneId(const String& value) noexcept {
  return value == "clock" || value == "flights" || value == "metar" || value == "amg_ops" ||
         value == "missions" || value == "countdown" || value == "message";
}

void readString(JsonObjectConst object, const char* key, String& target,
                const unsigned int max_length) {
  if (object[key].is<const char*>()) {
    const char* value = object[key].as<const char*>();
    String candidate(value != nullptr ? value : "");
    if (candidate.length() <= max_length) {
      target = candidate;
    }
  }
}

void readBool(JsonObjectConst object, const char* key, bool& target) {
  if (object[key].is<bool>()) {
    target = object[key].as<bool>();
  }
}

void readClampedInt(JsonObjectConst object, const char* key, int& target, const int low,
                    const int high) {
  if (object[key].is<int>()) {
    target = clampInt(object[key].as<int>(), low, high);
  }
}

}  // namespace

bool GeometrySettings::operator==(const GeometrySettings& other) const noexcept {
  return panel_w == other.panel_w && panel_h == other.panel_h && chain == other.chain &&
         driver == other.driver && clkphase == other.clkphase &&
         latch_blanking == other.latch_blanking && min_refresh == other.min_refresh;
}

std::vector<PlaylistEntry> defaultPlaylist() {
  return {
      {"clock", true, 10},   {"flights", true, 20},   {"metar", true, 10},
      {"amg_ops", true, 15}, {"missions", false, 15}, {"countdown", false, 10},
      {"message", false, 10},
  };
}

String sha256Hex(const String& input) {
  std::uint8_t digest[32] = {};
  mbedtls_sha256_context context;
  mbedtls_sha256_init(&context);
  mbedtls_sha256_starts(&context, 0);
  mbedtls_sha256_update(&context, reinterpret_cast<const unsigned char*>(input.c_str()),
                        input.length());
  mbedtls_sha256_finish(&context, digest);
  mbedtls_sha256_free(&context);

  static constexpr char kHex[] = "0123456789abcdef";
  char output[65];
  for (std::size_t index = 0; index < sizeof(digest); ++index) {
    output[index * 2] = kHex[digest[index] >> 4];
    output[index * 2 + 1] = kHex[digest[index] & 0x0f];
  }
  output[64] = '\0';
  return String(output);
}

bool SettingsStore::begin() {
  ready_ = preferences_.begin(kNamespace, false);
  if (!ready_) {
    return false;
  }

  settings_ = DeviceSettings{};
  settings_.playlist = defaultPlaylist();

  const String blob = preferences_.getString(kConfigKey, "");
  if (blob.length() > 0) {
    JsonDocument document;
    const DeserializationError error = deserializeJson(document, blob);
    if (error == DeserializationError::Ok) {
      (void)applyJson(document.as<JsonObjectConst>(), settings_);
    }
  }

  loadSecrets();
  return true;
}

void SettingsStore::loadSecrets() {
  wifi_ssid_ = preferences_.getString(kWifiSsidKey, "");
  wifi_pass_ = preferences_.getString(kWifiPassKey, "");
  amg_token_ = preferences_.getString(kAmgTokenKey, "");
  admin_sha_ = preferences_.getString(kAdminShaKey, "");
}

void SettingsStore::toJson(JsonDocument& document) const {
  document["schema_version"] = settings_.schema_version;

  JsonObject display = document["display"].to<JsonObject>();
  display["brightness"] = settings_.display.brightness;
  display["night_brightness"] = settings_.display.night_brightness;
  display["night_start"] = settings_.display.night_start;
  display["night_end"] = settings_.display.night_end;
  display["off_when_idle"] = settings_.display.off_when_idle;
  JsonObject geometry = display["geometry"].to<JsonObject>();
  geometry["panel_w"] = settings_.display.geometry.panel_w;
  geometry["panel_h"] = settings_.display.geometry.panel_h;
  geometry["chain"] = settings_.display.geometry.chain;
  geometry["driver"] = settings_.display.geometry.driver;
  geometry["clkphase"] = settings_.display.geometry.clkphase;
  geometry["latch_blanking"] = settings_.display.geometry.latch_blanking;
  geometry["min_refresh"] = settings_.display.geometry.min_refresh;

  JsonObject time = document["time"].to<JsonObject>();
  time["tz"] = settings_.time.tz;
  time["ntp"] = settings_.time.ntp;

  JsonObject location = document["location"].to<JsonObject>();
  location["lat"] = settings_.location.lat;
  location["lon"] = settings_.location.lon;
  location["radius_nm"] = settings_.location.radius_nm;

  JsonObject flights = document["flights"].to<JsonObject>();
  flights["provider"] = settings_.flights.provider;
  flights["poll_s"] = settings_.flights.poll_s;
  JsonArray watchlist = flights["watchlist"].to<JsonArray>();
  for (const String& entry : settings_.flights.watchlist) {
    watchlist.add(entry);
  }

  JsonObject metar = document["metar"].to<JsonObject>();
  metar["station"] = settings_.metar.station;
  metar["poll_s"] = settings_.metar.poll_s;

  JsonObject amg = document["amg"].to<JsonObject>();
  amg["base_url"] = settings_.amg.base_url;
  amg["poll_s"] = settings_.amg.poll_s;
  amg["notify_on_submission"] = settings_.amg.notify_on_submission;
  amg["notify_on_request"] = settings_.amg.notify_on_request;
  amg["token_set"] = amgTokenSet();  // presence flag only — never the token

  JsonArray playlist = document["playlist"].to<JsonArray>();
  for (const PlaylistEntry& entry : settings_.playlist) {
    JsonObject slot = playlist.add<JsonObject>();
    slot["id"] = entry.id;
    slot["enabled"] = entry.enabled;
    slot["duration_s"] = entry.duration_s;
  }

  JsonObject countdown = document["countdown"].to<JsonObject>();
  countdown["label"] = settings_.countdown.label;
  countdown["target_epoch"] = settings_.countdown.target_epoch;

  JsonObject message = document["message"].to<JsonObject>();
  message["text"] = settings_.message.text;
  message["color"] = settings_.message.color;
  message["scroll"] = settings_.message.scroll;

  JsonObject wifi = document["wifi"].to<JsonObject>();
  wifi["configured"] = wifiConfigured();  // presence flag only — never the creds
}

bool SettingsStore::applyJson(JsonObjectConst document, DeviceSettings& target) {
  if (document.isNull()) {
    return false;
  }

  if (document["schema_version"].is<int>() &&
      document["schema_version"].as<int>() > DeviceSettings::kSchemaVersion) {
    return false;
  }

  JsonObjectConst display = document["display"];
  if (!display.isNull()) {
    readClampedInt(display, "brightness", target.display.brightness, 0, 255);
    readClampedInt(display, "night_brightness", target.display.night_brightness, 0, 255);
    String night_start = target.display.night_start;
    readString(display, "night_start", night_start, 5);
    if (validTimeOfDay(night_start)) {
      target.display.night_start = night_start;
    }
    String night_end = target.display.night_end;
    readString(display, "night_end", night_end, 5);
    if (validTimeOfDay(night_end)) {
      target.display.night_end = night_end;
    }
    readBool(display, "off_when_idle", target.display.off_when_idle);

    JsonObjectConst geometry = display["geometry"];
    if (!geometry.isNull()) {
      readClampedInt(geometry, "panel_w", target.display.geometry.panel_w, 16, 256);
      readClampedInt(geometry, "panel_h", target.display.geometry.panel_h, 16, 128);
      readClampedInt(geometry, "chain", target.display.geometry.chain, 1, 4);
      String driver = target.display.geometry.driver;
      readString(geometry, "driver", driver, 16);
      if (knownDriver(driver)) {
        target.display.geometry.driver = driver;
      }
      readBool(geometry, "clkphase", target.display.geometry.clkphase);
      readClampedInt(geometry, "latch_blanking", target.display.geometry.latch_blanking, 0, 4);
      readClampedInt(geometry, "min_refresh", target.display.geometry.min_refresh, 30, 250);
    }
  }

  JsonObjectConst time = document["time"];
  if (!time.isNull()) {
    readString(time, "tz", target.time.tz, 64);
    readString(time, "ntp", target.time.ntp, 64);
  }

  JsonObjectConst location = document["location"];
  if (!location.isNull()) {
    if (location["lat"].is<double>()) {
      const double lat = location["lat"].as<double>();
      if (lat >= -90.0 && lat <= 90.0) {
        target.location.lat = lat;
      }
    }
    if (location["lon"].is<double>()) {
      const double lon = location["lon"].as<double>();
      if (lon >= -180.0 && lon <= 180.0) {
        target.location.lon = lon;
      }
    }
    readClampedInt(location, "radius_nm", target.location.radius_nm, 1, 250);
  }

  JsonObjectConst flights = document["flights"];
  if (!flights.isNull()) {
    String provider = target.flights.provider;
    readString(flights, "provider", provider, 16);
    if (provider == "adsblol" || provider == "opensky") {
      target.flights.provider = provider;
    }
    readClampedInt(flights, "poll_s", target.flights.poll_s, 5, 3600);
    if (flights["watchlist"].is<JsonArrayConst>()) {
      target.flights.watchlist.clear();
      for (JsonVariantConst entry : flights["watchlist"].as<JsonArrayConst>()) {
        if (!entry.is<const char*>()) {
          continue;
        }
        String value(entry.as<const char*>());
        value.trim();
        value.toUpperCase();
        if (value.length() > 0 && value.length() <= 8 &&
            target.flights.watchlist.size() < FlightsSettings::kMaxWatchlist) {
          target.flights.watchlist.push_back(value);
        }
      }
    }
  }

  JsonObjectConst metar = document["metar"];
  if (!metar.isNull()) {
    String station = target.metar.station;
    readString(metar, "station", station, 8);
    station.trim();
    station.toUpperCase();
    if (station.length() >= 3 && station.length() <= 8) {
      target.metar.station = station;
    }
    readClampedInt(metar, "poll_s", target.metar.poll_s, 60, 7200);
  }

  JsonObjectConst amg = document["amg"];
  if (!amg.isNull()) {
    String base_url = target.amg.base_url;
    readString(amg, "base_url", base_url, 128);
    if (base_url.length() == 0 || base_url.startsWith("http://") ||
        base_url.startsWith("https://")) {
      target.amg.base_url = base_url;
    }
    readClampedInt(amg, "poll_s", target.amg.poll_s, 15, 3600);
    readBool(amg, "notify_on_submission", target.amg.notify_on_submission);
    readBool(amg, "notify_on_request", target.amg.notify_on_request);
  }

  if (document["playlist"].is<JsonArrayConst>()) {
    std::vector<PlaylistEntry> playlist;
    for (JsonObjectConst slot : document["playlist"].as<JsonArrayConst>()) {
      if (slot.isNull() || !slot["id"].is<const char*>()) {
        continue;
      }
      PlaylistEntry entry;
      entry.id = slot["id"].as<const char*>();
      if (!knownSceneId(entry.id)) {
        continue;
      }
      readBool(slot, "enabled", entry.enabled);
      readClampedInt(slot, "duration_s", entry.duration_s, 3, 600);
      if (playlist.size() < DeviceSettings::kMaxPlaylist) {
        playlist.push_back(entry);
      }
    }
    if (!playlist.empty()) {
      target.playlist = playlist;
    }
  }

  JsonObjectConst countdown = document["countdown"];
  if (!countdown.isNull()) {
    readString(countdown, "label", target.countdown.label, 24);
    if (countdown["target_epoch"].is<std::int64_t>()) {
      const std::int64_t epoch = countdown["target_epoch"].as<std::int64_t>();
      if (epoch >= 0) {
        target.countdown.target_epoch = epoch;
      }
    }
  }

  JsonObjectConst message = document["message"];
  if (!message.isNull()) {
    readString(message, "text", target.message.text, 120);
    String color = target.message.color;
    readString(message, "color", color, 7);
    if (color.length() == 7 && color[0] == '#') {
      target.message.color = color;
    }
    readBool(message, "scroll", target.message.scroll);
  }

  target.schema_version = DeviceSettings::kSchemaVersion;
  return true;
}

bool SettingsStore::save() {
  if (!ready_) {
    return false;
  }
  JsonDocument document;
  toJson(document);
  // Presence flags are API response decoration — keep them out of the blob.
  document["amg"].as<JsonObject>().remove("token_set");
  document.remove("wifi");
  String blob;
  serializeJson(document, blob);
  return preferences_.putString(kConfigKey, blob) == blob.length();
}

bool SettingsStore::setWifiCredentials(const String& ssid, const String& password) {
  if (!ready_ || ssid.length() == 0 || ssid.length() > 32 || password.length() > 64) {
    return false;
  }

  // SSID and password live in two independent NVS keys, so the pair is written
  // in two steps. Snapshot the prior SSID and roll it back if the password
  // write fails — otherwise a partial write would persist the new SSID against
  // the previous (untouched) password, a mismatched pair loaded on next boot.
  const String prev_ssid = wifi_ssid_;

  if (preferences_.putString(kWifiSsidKey, ssid) != ssid.length()) {
    // First write did not commit; the key keeps its prior value. Nothing to undo.
    return false;
  }
  if (preferences_.putString(kWifiPassKey, password) != password.length()) {
    // Password write failed; the password key still holds the prior value, so
    // restore the SSID to the prior value to keep the persisted pair consistent.
    if (prev_ssid.length() > 0) {
      preferences_.putString(kWifiSsidKey, prev_ssid);
    } else {
      preferences_.remove(kWifiSsidKey);
    }
    return false;
  }

  wifi_ssid_ = ssid;
  wifi_pass_ = password;
  return true;
}

bool SettingsStore::setAmgToken(const String& token) {
  if (!ready_ || token.length() > 128) {
    return false;
  }
  const bool stored = preferences_.putString(kAmgTokenKey, token) == token.length();
  if (stored) {
    amg_token_ = token;
  }
  return stored;
}

bool SettingsStore::setAdminPassword(const String& password) {
  if (!ready_ || password.length() == 0 || password.length() > 64) {
    return false;
  }
  const String hash = sha256Hex(password);
  const bool stored = preferences_.putString(kAdminShaKey, hash) == hash.length();
  if (stored) {
    admin_sha_ = hash;
  }
  return stored;
}

}  // namespace amg::flightwall::live
