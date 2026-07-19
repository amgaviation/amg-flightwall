#include "providers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#if defined(AMG_CERT_BUNDLE_EMBEDDED)
extern const uint8_t rootca_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
#endif

namespace amg::flightwall::live {

namespace {

constexpr std::uint16_t kHttpTimeoutMs = 8000;

// Bounded streamed-read budget shared by every provider. A slow or hostile
// endpoint must not stall the render loop or compound across the three pollers,
// so each fetch aborts once the wall-clock deadline (measured from fetch start)
// or the per-endpoint byte cap is reached. Caps are sized a little above the
// largest realistic payload for each endpoint.
constexpr std::uint32_t kReadDeadlineMs = 10000;
constexpr std::size_t kFlightsMaxBytes = 48U * 1024U;
constexpr std::size_t kMetarMaxBytes = 8U * 1024U;
constexpr std::size_t kAmgMaxBytes = 16U * 1024U;

// Wraps the HTTP body stream so a streamed JSON parse is bounded by both a
// wall-clock deadline and a byte cap. Every byte ArduinoJson consumes passes
// through readBytes(); once either limit is reached the reader reports
// end-of-input, so deserialization aborts with a typed error that the caller
// maps to a bounded-read failure without blocking further.
class BoundedStream final : public Stream {
 public:
  BoundedStream(Stream& inner, const std::size_t max_bytes,
                const std::uint32_t deadline_ms) noexcept
      : inner_(inner), max_bytes_(max_bytes), deadline_ms_(deadline_ms) {}

  [[nodiscard]] bool overCap() const noexcept { return over_cap_; }
  [[nodiscard]] bool timedOut() const noexcept { return timed_out_; }

  int available() override { return (over_cap_ || timed_out_) ? 0 : inner_.available(); }
  int peek() override { return inner_.peek(); }

  int read() override {
    char c;
    return readBytes(&c, 1) == 1 ? static_cast<unsigned char>(c) : -1;
  }

  size_t readBytes(char* buffer, size_t length) override {
    if (!withinBudget()) {
      return 0;
    }
    const std::size_t remaining = max_bytes_ - bytes_read_;
    const size_t capped = static_cast<size_t>(std::min<std::size_t>(length, remaining));
    const size_t count = inner_.readBytes(buffer, capped);
    bytes_read_ += count;
    return count;
  }

  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t*, size_t) override { return 0; }

 private:
  // Latches the abort reason and returns false once the byte cap is reached or
  // the wall-clock deadline has passed. Signed millis() difference tolerates the
  // 32-bit wraparound.
  bool withinBudget() noexcept {
    if (bytes_read_ >= max_bytes_) {
      over_cap_ = true;
      return false;
    }
    if (static_cast<std::int32_t>(millis() - deadline_ms_) >= 0) {
      timed_out_ = true;
      return false;
    }
    return true;
  }

  Stream& inner_;
  std::size_t max_bytes_;
  std::uint32_t deadline_ms_;
  std::size_t bytes_read_{0};
  bool over_cap_{false};
  bool timed_out_{false};
};

void configureTls(WiFiClientSecure& client, const bool insecure) {
#if defined(AMG_CERT_BUNDLE_EMBEDDED)
  if (!insecure) {
    client.setCACertBundle(rootca_crt_bundle_start);
    return;
  }
#else
  (void)insecure;
#endif
  // Documented fallback: without the embedded certificate bundle every
  // endpoint uses an unauthenticated TLS session (LAN-appliance tradeoff).
  client.setInsecure();
}

[[nodiscard]] double degreesToRadians(const double degrees) noexcept {
  return degrees * 0.017453292519943295;
}

[[nodiscard]] double haversineNm(const double lat1, const double lon1, const double lat2,
                                 const double lon2) noexcept {
  const double dlat = degreesToRadians(lat2 - lat1);
  const double dlon = degreesToRadians(lon2 - lon1);
  const double a = std::sin(dlat / 2) * std::sin(dlat / 2) +
                   std::cos(degreesToRadians(lat1)) * std::cos(degreesToRadians(lat2)) *
                       std::sin(dlon / 2) * std::sin(dlon / 2);
  const double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
  return 3440.065 * c;  // Earth radius in nautical miles
}

[[nodiscard]] std::string truncated(const char* value, const std::size_t max_length) {
  if (value == nullptr) {
    return {};
  }
  std::string text(value);
  while (!text.empty() && text.back() == ' ') {
    text.pop_back();
  }
  if (text.size() > max_length) {
    text.resize(max_length);
  }
  return text;
}

}  // namespace

void PollerBase::tick(const std::uint64_t now_ms, const bool online) {
  if (!online || !enabled() || now_ms < next_due_ms_) {
    return;
  }

  if (fetch(now_ms)) {
    failure_count_ = 0;
    next_due_ms_ = now_ms + intervalMs();
    return;
  }

  if (failure_count_ < 10) {
    ++failure_count_;
  }
  std::uint64_t delay = intervalMs();
  for (std::uint8_t step = 0; step < failure_count_ && delay < kMaxBackoffMs; ++step) {
    delay *= 2;
  }
  next_due_ms_ = now_ms + std::min<std::uint64_t>(delay, kMaxBackoffMs);
}

// --- FlightProvider (adsb.lol point query) ---

bool FlightProvider::consumeUpdated() noexcept {
  const bool updated = updated_;
  updated_ = false;
  return updated;
}

std::uint32_t FlightProvider::intervalMs() const {
  return static_cast<std::uint32_t>(store_.settings().flights.poll_s) * 1000U;
}

bool FlightProvider::enabled() const {
  const LocationSettings& location = store_.settings().location;
  return location.lat != 0.0 || location.lon != 0.0;
}

bool FlightProvider::fetch(const std::uint64_t now_ms) {
  const std::uint32_t fetch_start_ms = millis();
  const DeviceSettings& settings = store_.settings();
  char url[128];
  snprintf(url, sizeof(url), "https://api.adsb.lol/v2/point/%.4f/%.4f/%d",
           settings.location.lat, settings.location.lon, settings.location.radius_nm);

  WiFiClientSecure client;
  configureTls(client, insecureTls());
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setConnectTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    log_.append("flights: http begin failed");
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  const int status = http.GET();
  if (status != 200) {
    log_.logf("flights: fetch failed, http %d", status);
    http.end();
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  JsonDocument filter;
  JsonObject aircraft_filter = filter["ac"].add<JsonObject>();
  aircraft_filter["flight"] = true;
  aircraft_filter["r"] = true;
  aircraft_filter["lat"] = true;
  aircraft_filter["lon"] = true;
  aircraft_filter["alt_baro"] = true;
  aircraft_filter["gs"] = true;
  aircraft_filter["track"] = true;

  JsonDocument document;
  BoundedStream bounded(http.getStream(), kFlightsMaxBytes, fetch_start_ms + kReadDeadlineMs);
  const DeserializationError error =
      deserializeJson(document, bounded, DeserializationOption::Filter(filter));
  http.end();
  if (error != DeserializationError::Ok) {
    if (bounded.overCap()) {
      log_.append("flights: response exceeded byte cap");
    } else if (bounded.timedOut()) {
      log_.append("flights: read deadline exceeded");
    } else {
      log_.logf("flights: parse failed (%s)", error.c_str());
    }
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  FlightSnapshot next;
  next.contacts.reserve(kMaxContacts);
  for (JsonObjectConst aircraft : document["ac"].as<JsonArrayConst>()) {
    if (!aircraft["lat"].is<double>() || !aircraft["lon"].is<double>()) {
      continue;
    }
    FlightContact contact{};
    contact.callsign = truncated(aircraft["flight"].as<const char*>(), 8);
    if (contact.callsign.empty()) {
      contact.callsign = truncated(aircraft["r"].as<const char*>(), 8);
    }
    if (contact.callsign.empty()) {
      continue;
    }
    contact.lat = aircraft["lat"].as<double>();
    contact.lon = aircraft["lon"].as<double>();
    // alt_baro is the string "ground" for surface traffic.
    contact.altitude_ft = aircraft["alt_baro"].is<int>() ? aircraft["alt_baro"].as<int>() : 0;
    contact.ground_speed_kt =
        aircraft["gs"].is<float>() ? static_cast<int>(aircraft["gs"].as<float>()) : 0;
    contact.heading_deg =
        aircraft["track"].is<float>() ? static_cast<int>(aircraft["track"].as<float>()) : 0;
    contact.distance_nm =
        haversineNm(settings.location.lat, settings.location.lon, contact.lat, contact.lon);
    contact.watchlisted = false;
    for (const String& watched : settings.flights.watchlist) {
      if (contact.callsign == watched.c_str()) {
        contact.watchlisted = true;
        break;
      }
    }

    // Keep the nearest kMaxContacts, ordered by distance.
    const auto position =
        std::find_if(next.contacts.begin(), next.contacts.end(),
                     [&](const FlightContact& existing) {
                       return contact.distance_nm < existing.distance_nm;
                     });
    if (position != next.contacts.end() || next.contacts.size() < kMaxContacts) {
      next.contacts.insert(position, contact);
      if (next.contacts.size() > kMaxContacts) {
        next.contacts.pop_back();
      }
    }
  }

  next.fetched_at_ms = now_ms;
  next.valid = true;
  snapshot_ = std::move(next);
  updated_ = true;
  return true;
}

// --- MetarProvider (aviationweather.gov) ---

bool MetarProvider::consumeUpdated() noexcept {
  const bool updated = updated_;
  updated_ = false;
  return updated;
}

std::uint32_t MetarProvider::intervalMs() const {
  return static_cast<std::uint32_t>(store_.settings().metar.poll_s) * 1000U;
}

bool MetarProvider::enabled() const { return store_.settings().metar.station.length() >= 3; }

bool MetarProvider::fetch(const std::uint64_t now_ms) {
  const std::uint32_t fetch_start_ms = millis();
  const MetarSettings& metar = store_.settings().metar;
  String url = "https://aviationweather.gov/api/data/metar?format=json&ids=";
  url += metar.station;

  WiFiClientSecure client;
  configureTls(client, insecureTls());
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setConnectTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    log_.append("metar: http begin failed");
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  const int status = http.GET();
  if (status != 200) {
    log_.logf("metar: fetch failed, http %d", status);
    http.end();
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  // Bound the document to the fields actually read below (the API returns a
  // one-element array of report objects).
  JsonDocument filter;
  JsonObject report_filter = filter.add<JsonObject>();
  report_filter["icaoId"] = true;
  report_filter["rawOb"] = true;
  report_filter["temp"] = true;
  report_filter["dewp"] = true;
  report_filter["wspd"] = true;
  report_filter["wdir"] = true;
  report_filter["visib"] = true;
  report_filter["fltCat"] = true;
  report_filter["fltcat"] = true;
  JsonObject cloud_filter = report_filter["clouds"].add<JsonObject>();
  cloud_filter["cover"] = true;
  cloud_filter["base"] = true;

  JsonDocument document;
  BoundedStream bounded(http.getStream(), kMetarMaxBytes, fetch_start_ms + kReadDeadlineMs);
  const DeserializationError error =
      deserializeJson(document, bounded, DeserializationOption::Filter(filter));
  http.end();
  if (error != DeserializationError::Ok || !document.is<JsonArrayConst>() ||
      document.as<JsonArrayConst>().size() == 0) {
    if (bounded.overCap()) {
      log_.append("metar: response exceeded byte cap");
    } else if (bounded.timedOut()) {
      log_.append("metar: read deadline exceeded");
    } else {
      log_.append("metar: parse failed or empty report");
    }
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  JsonObjectConst report = document.as<JsonArrayConst>()[0];
  MetarSnapshot next;
  next.station = truncated(report["icaoId"].as<const char*>(), 8);
  if (next.station.empty()) {
    next.station = metar.station.c_str();
  }
  next.raw = truncated(report["rawOb"].as<const char*>(), 160);
  next.temp_c = report["temp"].is<float>() ? static_cast<int>(report["temp"].as<float>()) : 0;
  next.dewpoint_c = report["dewp"].is<float>() ? static_cast<int>(report["dewp"].as<float>()) : 0;

  char wind[32] = "CALM";
  const int wind_speed = report["wspd"].is<int>() ? report["wspd"].as<int>() : 0;
  if (wind_speed > 0) {
    if (report["wdir"].is<int>()) {
      snprintf(wind, sizeof(wind), "%03d@%dKT", report["wdir"].as<int>(), wind_speed);
    } else {
      snprintf(wind, sizeof(wind), "VRB@%dKT", wind_speed);  // wdir "VRB" arrives as string
    }
  }
  next.wind = wind;

  double visibility_sm = 10.0;
  if (report["visib"].is<float>()) {
    visibility_sm = report["visib"].as<float>();
    char text[12];
    snprintf(text, sizeof(text), "%.1fSM", visibility_sm);
    next.visibility = text;
  } else if (report["visib"].is<const char*>()) {
    next.visibility = truncated(report["visib"].as<const char*>(), 8);  // e.g. "10+"
    next.visibility += "SM";
  }

  // Flight category: prefer the API's own field when present, otherwise
  // derive from ceiling and visibility per the standard FAA thresholds.
  const char* category = report["fltCat"].as<const char*>();
  if (category == nullptr) {
    category = report["fltcat"].as<const char*>();
  }
  if (category != nullptr) {
    next.flight_category = truncated(category, 4);
  } else {
    int ceiling_ft = 100000;
    for (JsonObjectConst layer : report["clouds"].as<JsonArrayConst>()) {
      const char* cover = layer["cover"].as<const char*>();
      if (cover == nullptr || layer["base"].isNull()) {
        continue;
      }
      const String cover_text(cover);
      if ((cover_text == "BKN" || cover_text == "OVC" || cover_text == "VV") &&
          layer["base"].is<int>()) {
        ceiling_ft = std::min(ceiling_ft, layer["base"].as<int>());
      }
    }
    if (visibility_sm > 5.0 && ceiling_ft > 3000) {
      next.flight_category = "VFR";
    } else if (visibility_sm >= 3.0 && ceiling_ft >= 1000) {
      next.flight_category = "MVFR";
    } else if (visibility_sm >= 1.0 && ceiling_ft >= 500) {
      next.flight_category = "IFR";
    } else {
      next.flight_category = "LIFR";
    }
  }

  next.fetched_at_ms = now_ms;
  next.valid = true;
  snapshot_ = std::move(next);
  updated_ = true;
  return true;
}

// --- AmgProvider (amg1 bridge) ---

bool AmgProvider::consumeUpdated() noexcept {
  const bool updated = updated_;
  updated_ = false;
  return updated;
}

std::uint32_t AmgProvider::intervalMs() const {
  return static_cast<std::uint32_t>(store_.settings().amg.poll_s) * 1000U;
}

bool AmgProvider::enabled() const {
  return store_.settings().amg.base_url.length() > 0 && store_.amgTokenSet();
}

bool AmgProvider::fetch(const std::uint64_t now_ms) {
  const std::uint32_t fetch_start_ms = millis();
  const AmgSettings& amg = store_.settings().amg;
  String url = amg.base_url;
  if (!url.endsWith("/")) {
    url += "/";
  }
  url += "api/flightwall/summary";

  // The bridge bearer token must never travel in cleartext. If the resolved
  // base_url is not https we refuse the fetch outright rather than attach the
  // Authorization header over an unencrypted link. Logged without the token.
  if (!url.startsWith("https://")) {
    log_.append("amg: base_url is not https; refusing to send bridge token");
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  WiFiClientSecure client;
  configureTls(client, insecureTls());
  HTTPClient http;
  http.setTimeout(kHttpTimeoutMs);
  http.setConnectTimeout(kHttpTimeoutMs);
  http.useHTTP10(true);
  if (!http.begin(client, url)) {
    log_.append("amg: http begin failed");
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  // Token travels only in this https request header; it is never logged.
  http.addHeader("Authorization", String("Bearer ") + store_.amgToken());
  const int status = http.GET();
  if (status != 200) {
    log_.logf("amg: fetch failed, http %d", status);
    http.end();
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  // Bound the document to the fields consumed below.
  JsonDocument filter;
  filter["requests"]["new_count"] = true;
  JsonObject latest_filter = filter["requests"]["latest"].add<JsonObject>();
  latest_filter["label"] = true;
  latest_filter["name"] = true;
  latest_filter["age_min"] = true;
  filter["missions"]["active_count"] = true;
  JsonObject mission_filter = filter["missions"]["items"].add<JsonObject>();
  mission_filter["label"] = true;
  mission_filter["status"] = true;
  mission_filter["eta_min"] = true;
  filter["submissions"]["cursor"] = true;
  JsonObject submission_filter = filter["submissions"]["recent"].add<JsonObject>();
  submission_filter["kind"] = true;
  submission_filter["name"] = true;
  submission_filter["age_min"] = true;
  filter["revenue"]["today_cents"] = true;
  filter["revenue"]["mtd_cents"] = true;
  filter["revenue"]["currency"] = true;
  filter["site"]["state"] = true;

  JsonDocument document;
  BoundedStream bounded(http.getStream(), kAmgMaxBytes, fetch_start_ms + kReadDeadlineMs);
  const DeserializationError error =
      deserializeJson(document, bounded, DeserializationOption::Filter(filter));
  http.end();
  if (error != DeserializationError::Ok) {
    if (bounded.overCap()) {
      log_.append("amg: response exceeded byte cap");
    } else if (bounded.timedOut()) {
      log_.append("amg: read deadline exceeded");
    } else {
      log_.logf("amg: parse failed (%s)", error.c_str());
    }
    snapshot_.valid = false;
    updated_ = true;
    return false;
  }

  AmgMetricsSnapshot next;
  JsonObjectConst requests = document["requests"];
  next.new_request_count = requests["new_count"].is<int>() ? requests["new_count"].as<int>() : 0;
  for (JsonObjectConst item : requests["latest"].as<JsonArrayConst>()) {
    if (next.latest_requests.size() >= kMaxItems) {
      break;
    }
    AmgRequestItem request;
    request.label = truncated(item["label"].as<const char*>(), 24);
    request.name = truncated(item["name"].as<const char*>(), 24);
    request.age_min = item["age_min"].is<int>() ? item["age_min"].as<int>() : -1;
    next.latest_requests.push_back(std::move(request));
  }

  JsonObjectConst missions = document["missions"];
  next.active_mission_count =
      missions["active_count"].is<int>() ? missions["active_count"].as<int>() : 0;
  for (JsonObjectConst item : missions["items"].as<JsonArrayConst>()) {
    if (next.missions.size() >= kMaxItems) {
      break;
    }
    AmgMissionItem mission;
    mission.label = truncated(item["label"].as<const char*>(), 24);
    mission.status = truncated(item["status"].as<const char*>(), 16);
    mission.eta_min = item["eta_min"].is<int>() ? item["eta_min"].as<int>() : -1;
    next.missions.push_back(std::move(mission));
  }

  JsonObjectConst submissions = document["submissions"];
  next.submissions_cursor = truncated(submissions["cursor"].as<const char*>(), 64);
  for (JsonObjectConst item : submissions["recent"].as<JsonArrayConst>()) {
    if (next.recent_submissions.size() >= kMaxItems) {
      break;
    }
    AmgSubmissionItem submission;
    submission.kind = truncated(item["kind"].as<const char*>(), 16);
    submission.name = truncated(item["name"].as<const char*>(), 24);
    submission.age_min = item["age_min"].is<int>() ? item["age_min"].as<int>() : -1;
    next.recent_submissions.push_back(std::move(submission));
  }

  JsonObjectConst revenue = document["revenue"];
  next.revenue_today_cents = revenue["today_cents"].is<std::int64_t>()
                                 ? revenue["today_cents"].as<std::int64_t>()
                                 : -1;
  next.revenue_mtd_cents =
      revenue["mtd_cents"].is<std::int64_t>() ? revenue["mtd_cents"].as<std::int64_t>() : -1;
  next.currency = truncated(revenue["currency"].as<const char*>(), 8);
  next.site_state = truncated(document["site"]["state"].as<const char*>(), 16);
  next.fetched_at_ms = now_ms;
  next.valid = true;

  // Submissions-cursor change detection -> notification overlay.
  if (notification_sink_ && amg.notify_on_submission && !last_cursor_.empty() &&
      !next.submissions_cursor.empty() && next.submissions_cursor != last_cursor_) {
    NotificationEvent event;
    event.title = "NEW SUBMISSION";
    if (!next.recent_submissions.empty()) {
      event.body = next.recent_submissions.front().kind;
      if (!next.recent_submissions.front().name.empty()) {
        event.body += " ";
        event.body += next.recent_submissions.front().name;
      }
    }
    event.color = Color{255, 174, 0};  // colors::amber
    event.priority = 1;
    event.duration_ms = 15000;
    notification_sink_(event);
  }
  if (notification_sink_ && amg.notify_on_request && last_request_count_ >= 0 &&
      next.new_request_count > last_request_count_) {
    NotificationEvent event;
    event.title = "NEW REQUEST";
    if (!next.latest_requests.empty()) {
      event.body = next.latest_requests.front().label;
    }
    event.color = Color{23, 108, 255};  // colors::amg_blue
    event.priority = 1;
    event.duration_ms = 15000;
    notification_sink_(event);
  }
  last_cursor_ = next.submissions_cursor;
  last_request_count_ = next.new_request_count;

  snapshot_ = std::move(next);
  updated_ = true;
  return true;
}

}  // namespace amg::flightwall::live
