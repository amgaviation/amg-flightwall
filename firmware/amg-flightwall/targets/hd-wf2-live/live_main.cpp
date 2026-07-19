// AMG FlightWall live target — composition root.
//
// Arduino requires free setup()/loop() entry points; this file contains the
// one function-local runtime object allowed by the target-boundary exception
// in docs/development.md. Everything inward receives its dependencies
// explicitly.
//
// Concurrency (contract rule): the loop task owns scenes, settings, NVS and
// the display. Async web callbacks only enqueue Commands and copy the JSON
// snapshots that this file maintains inside SharedState under state_mutex.
// SSE sends (log/status/frame) all originate here, on the loop task.

#include <Arduino.h>

#include <array>
#include <cstring>
#include <ctime>
#include <vector>

#include <ArduinoJson.h>
#include <esp_timer.h>
#include <mbedtls/base64.h>

#include "amg/flightwall/diagnostics.hpp"
#include "amg/flightwall/renderer.hpp"
#include "hub75_output.h"
#include "live_compat_stubs.h"
#include "log_buffer.h"
#include "ota_update.h"
#include "providers.h"
#include "settings_store.h"
#include "web_server.h"
#include "wifi_manager.h"

namespace {

using namespace amg::flightwall;
using namespace amg::flightwall::live;

constexpr const char* kFirmwareVersion = "2.0.0-live";
constexpr std::uint64_t kRenderIntervalMs = 100;   // ~10 fps scene tick
constexpr std::uint64_t kFrameSseIntervalMs = 500;
constexpr std::uint64_t kStatusSseIntervalMs = 5000;
constexpr std::uint64_t kClockPushIntervalMs = 250;
constexpr std::uint64_t kStatusRebuildIntervalMs = 1000;

[[nodiscard]] std::uint64_t monotonicMs() noexcept {
  return static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
}

[[nodiscard]] Color parseHexColor(const String& text, const Color fallback) noexcept {
  if (text.length() != 7 || text[0] != '#') {
    return fallback;
  }
  char* end = nullptr;
  const long value = strtol(text.c_str() + 1, &end, 16);
  if (end == nullptr || *end != '\0') {
    return fallback;
  }
  return Color{static_cast<std::uint8_t>((value >> 16) & 0xff),
               static_cast<std::uint8_t>((value >> 8) & 0xff),
               static_cast<std::uint8_t>(value & 0xff)};
}

[[nodiscard]] int minutesOfDay(const String& hhmm) noexcept {
  if (hhmm.length() != 5) {
    return -1;
  }
  return (hhmm[0] - '0') * 600 + (hhmm[1] - '0') * 60 + (hhmm[3] - '0') * 10 + (hhmm[4] - '0');
}

[[nodiscard]] const char* healthLevelName(const HealthLevel level) noexcept {
  switch (level) {
    case HealthLevel::healthy:
      return "healthy";
    case HealthLevel::degraded:
      return "degraded";
    case HealthLevel::failed:
      return "failed";
    case HealthLevel::unknown:
      return "unknown";
  }
  return "unknown";
}

[[nodiscard]] const char* subsystemName(const Subsystem subsystem) noexcept {
  switch (subsystem) {
    case Subsystem::display:
      return "display";
    case Subsystem::configuration:
      return "configuration";
    case Subsystem::network:
      return "network";
    case Subsystem::storage:
      return "storage";
    case Subsystem::application:
      return "application";
    case Subsystem::count:
      break;
  }
  return "unknown";
}

class LiveRuntime final {
 public:
  void setup() {
    Serial.begin(115200);
    log_.logf("flightwall %s booting", kFirmwareVersion);

    storage_ready_ = store_.begin();
    if (!storage_ready_) {
      log_.append("settings: NVS open failed, running on defaults");
    }

    const Hub75Initialization display_result =
        hub75_.begin(store_.settings().display.geometry);
    display_ready_ = display_result == Hub75Initialization::ready;
    if (!display_ready_) {
      log_.append("display: HUB75 DMA allocation failed");
    } else {
      hub75_.setBrightness(currentBrightness());
      log_.logf("display: %dx%d ready (driver %s)", hub75_.width(), hub75_.height(),
                store_.settings().display.geometry.driver.c_str());
    }

    wifi_.begin();

    amg_provider_.setNotificationSink([this](const NotificationEvent& event) {
      showNotification(event);
    });

    applyRuntimeSettings();
    refreshSharedState();
    refreshStatusJson(monotonicMs());
    web_.begin();
    log_.append("web: server listening on :80");
  }

  void loop() {
    const std::uint64_t now = monotonicMs();

    drainCommands(now);
    wifi_.loop(now);

    const bool online = wifi_.online();
    flight_provider_.tick(now, online);
    metar_provider_.tick(now, online);
    amg_provider_.tick(now, online);
    pushProviderUpdates();

    if (now - last_clock_push_ms_ >= kClockPushIntervalMs) {
      last_clock_push_ms_ = now;
      clock_scene_.setClock(wifi_.clockInfo());
      pushCountdown();
      hub75_.setBrightness(currentBrightness());
    }

    if (display_ready_ && now - last_render_ms_ >= kRenderIntervalMs) {
      last_render_ms_ = now;
      FrameContext context{renderer_, now, frame_number_++};
      (void)rotator_.tick(context);
      renderer_.present();
    }

    if (now - last_status_rebuild_ms_ >= kStatusRebuildIntervalMs) {
      last_status_rebuild_ms_ = now;
      refreshStatusJson(now);
    }

    log_.drainNew([this](const char* line) { web_.sendLogLine(line); });

    if (web_.hasEventClients()) {
      if (now - last_status_sse_ms_ >= kStatusSseIntervalMs) {
        last_status_sse_ms_ = now;
        String status;
        {
          const std::lock_guard<std::mutex> guard(shared_.mutex);
          status = shared_.status_json;
        }
        web_.sendStatus(status);
      }
      if (now - last_frame_sse_ms_ >= kFrameSseIntervalMs) {
        last_frame_sse_ms_ = now;
        sendFrameEvent();
      }
    }

    handleRebootRequests(now);
    delay(10);
  }

 private:
  // --- command handling -------------------------------------------------

  void drainCommands(const std::uint64_t now) {
    Command command;
    while (commands_.pop(command)) {
      switch (command.type) {
        case Command::Type::apply_config:
          applyConfigCommand(command.json);
          break;
        case Command::Type::set_secrets:
          applySecretsCommand(command.json);
          break;
        case Command::Type::activate_scene: {
          const bool activated =
              rotator_.activateNow(std::string_view(command.id.c_str()), command.duration_ms);
          log_.logf("scene: activate %s -> %s", command.id.c_str(),
                    activated ? "ok" : "unknown id");
          break;
        }
        case Command::Type::show_message:
          applyMessageCommand(command.json);
          break;
        case Command::Type::notify:
          applyNotifyCommand(command.json);
          break;
        case Command::Type::reboot:
          log_.append("system: reboot requested");
          reboot_at_ms_ = now + 500;
          break;
        case Command::Type::wifi_join:
          if (store_.setWifiCredentials(command.id, command.secret)) {
            log_.logf("wifi: credentials stored for \"%s\", rebooting into STA",
                      command.id.c_str());
            reboot_at_ms_ = now + 1000;
          } else {
            log_.append("wifi: storing credentials failed");
          }
          break;
      }
    }
  }

  void applyConfigCommand(const String& body) {
    JsonDocument document;
    if (deserializeJson(document, body) != DeserializationError::Ok) {
      log_.append("config: rejected unparseable body");
      return;
    }
    if (!SettingsStore::applyJson(document.as<JsonObjectConst>(), store_.settings())) {
      log_.append("config: rejected invalid document");
      return;
    }
    if (!store_.save()) {
      log_.append("config: persist failed");
    }
    applyRuntimeSettings();
    refreshSharedState();
    log_.append("config: applied");
  }

  void applySecretsCommand(const String& body) {
    JsonDocument document;
    if (deserializeJson(document, body) != DeserializationError::Ok) {
      return;
    }
    JsonObjectConst object = document.as<JsonObjectConst>();
    if (object["wifi_ssid"].is<const char*>()) {
      const String ssid = object["wifi_ssid"].as<const char*>();
      const String pass =
          object["wifi_pass"].is<const char*>() ? object["wifi_pass"].as<const char*>() : "";
      if (store_.setWifiCredentials(ssid, pass)) {
        log_.append("secrets: wifi credentials updated (effective after reboot)");
      }
    }
    if (object["amg_token"].is<const char*>()) {
      if (store_.setAmgToken(object["amg_token"].as<const char*>())) {
        log_.append("secrets: amg bridge token updated");
        amg_provider_.pollNow();
      }
    }
    if (object["admin_password"].is<const char*>()) {
      if (store_.setAdminPassword(object["admin_password"].as<const char*>())) {
        log_.append("secrets: admin password updated");
      }
    }
    refreshSharedState();
  }

  void applyMessageCommand(const String& body) {
    JsonDocument document;
    if (deserializeJson(document, body) != DeserializationError::Ok) {
      return;
    }
    JsonObjectConst object = document.as<JsonObjectConst>();
    MessagePayload payload;
    payload.text = object["text"].is<const char*>() ? object["text"].as<const char*>() : "";
    payload.color = parseHexColor(
        object["color"].is<const char*>() ? String(object["color"].as<const char*>()) : String(),
        colors::amg_blue);
    payload.scroll = object["scroll"].is<bool>() ? object["scroll"].as<bool>() : true;
    const int duration_s = object["duration_s"].is<int>() ? object["duration_s"].as<int>() : 15;
    payload.duration_ms = static_cast<std::uint32_t>(duration_s > 0 ? duration_s : 15) * 1000U;
    MessageScene& slot = message_overlay_pool_[message_overlay_index_];
    slot.setMessage(payload);
    if (rotator_.pushOverlay(slot, payload.duration_ms, 1)) {
      message_overlay_index_ = (message_overlay_index_ + 1) % message_overlay_pool_.size();
      log_.append("message: overlay shown");
    } else {
      log_.append("message: overlay queue full");
    }
  }

  void applyNotifyCommand(const String& body) {
    JsonDocument document;
    if (deserializeJson(document, body) != DeserializationError::Ok) {
      return;
    }
    JsonObjectConst object = document.as<JsonObjectConst>();
    NotificationEvent event;
    event.title = object["title"].is<const char*>() ? object["title"].as<const char*>() : "";
    event.body = object["body"].is<const char*>() ? object["body"].as<const char*>() : "";
    const String level =
        object["level"].is<const char*>() ? object["level"].as<const char*>() : "info";
    if (level == "success") {
      event.color = colors::green;
      event.priority = 1;
    } else if (level == "warn") {
      event.color = colors::amber;
      event.priority = 2;
    } else if (level == "alert") {
      event.color = colors::red;
      event.priority = 3;
    } else {
      event.color = colors::amg_blue;
      event.priority = 1;
    }
    const int duration_s = object["duration_s"].is<int>() ? object["duration_s"].as<int>() : 15;
    event.duration_ms = static_cast<std::uint32_t>(duration_s > 0 ? duration_s : 15) * 1000U;
    showNotification(event);
  }

  void showNotification(const NotificationEvent& event) {
    NotificationScene& slot = notification_pool_[notification_index_];
    slot.setNotification(event);
    if (rotator_.pushOverlay(slot, event.duration_ms, event.priority)) {
      notification_index_ = (notification_index_ + 1) % notification_pool_.size();
      log_.logf("notify: %s", event.title.c_str());
    } else {
      log_.append("notify: overlay queue full, dropped");
    }
  }

  // --- settings application --------------------------------------------

  void applyRuntimeSettings() {
    const DeviceSettings& settings = store_.settings();

    std::vector<SceneSlot> slots;
    slots.reserve(settings.playlist.size());
    for (const PlaylistEntry& entry : settings.playlist) {
      Scene* scene = sceneById(entry.id);
      if (scene != nullptr) {
        slots.push_back(SceneSlot{scene, entry.enabled,
                                  static_cast<std::uint32_t>(entry.duration_s) * 1000U});
      }
    }
    rotator_.setPlaylist(std::move(slots));

    flight_radar_scene_.setOwnPosition(settings.location.lat, settings.location.lon);
    flight_radar_scene_.setRangeNm(settings.location.radius_nm);

    MessagePayload baseline;
    baseline.text = settings.message.text.c_str();
    baseline.color = parseHexColor(settings.message.color, colors::amg_blue);
    baseline.scroll = settings.message.scroll;
    baseline.duration_ms = 0;
    message_scene_.setMessage(baseline);
    pushCountdown();

    flight_provider_.pollNow();
    metar_provider_.pollNow();
    amg_provider_.pollNow();
  }

  [[nodiscard]] Scene* sceneById(const String& id) noexcept {
    if (id == "clock") {
      return &clock_scene_;
    }
    if (id == "flights") {
      return &flight_radar_scene_;
    }
    if (id == "metar") {
      return &metar_scene_;
    }
    if (id == "amg_ops") {
      return &amg_ops_scene_;
    }
    if (id == "missions") {
      return &mission_board_scene_;
    }
    if (id == "countdown") {
      return &countdown_scene_;
    }
    if (id == "message") {
      return &message_scene_;
    }
    return nullptr;
  }

  void pushCountdown() {
    const CountdownSettings& countdown = store_.settings().countdown;
    CountdownInfo info;
    info.label = countdown.label.c_str();
    if (countdown.target_epoch > 0 && wifi_.timeSynced()) {
      info.seconds_remaining = countdown.target_epoch - static_cast<std::int64_t>(time(nullptr));
    } else {
      info.seconds_remaining = -1;
    }
    countdown_scene_.setCountdown(info);
  }

  void pushProviderUpdates() {
    // Contract rule: provider results reach scenes under the state mutex so
    // async status readers never observe a half-updated surface.
    if (flight_provider_.consumeUpdated()) {
      const std::lock_guard<std::mutex> guard(shared_.mutex);
      flight_radar_scene_.setSnapshot(flight_provider_.snapshot());
    }
    if (metar_provider_.consumeUpdated()) {
      const std::lock_guard<std::mutex> guard(shared_.mutex);
      metar_scene_.setSnapshot(metar_provider_.snapshot());
    }
    if (amg_provider_.consumeUpdated()) {
      const std::lock_guard<std::mutex> guard(shared_.mutex);
      amg_ops_scene_.setSnapshot(amg_provider_.snapshot());
      mission_board_scene_.setSnapshot(amg_provider_.snapshot());
    }
  }

  [[nodiscard]] std::uint8_t currentBrightness() {
    const DisplaySettings& display = store_.settings().display;
    const ClockInfo clock = wifi_.clockInfo();
    if (!clock.valid) {
      return static_cast<std::uint8_t>(display.brightness);
    }
    const int now_minutes = clock.hour * 60 + clock.minute;
    const int start = minutesOfDay(display.night_start);
    const int end = minutesOfDay(display.night_end);
    bool night = false;
    if (start >= 0 && end >= 0 && start != end) {
      night = start < end ? (now_minutes >= start && now_minutes < end)
                          : (now_minutes >= start || now_minutes < end);
    }
    return static_cast<std::uint8_t>(night ? display.night_brightness : display.brightness);
  }

  // --- shared state / SSE ----------------------------------------------

  void refreshSharedState() {
    JsonDocument document;
    store_.toJson(document);
    String config;
    serializeJson(document, config);

    const std::lock_guard<std::mutex> guard(shared_.mutex);
    shared_.config_json = config;
    shared_.admin_hash = store_.adminPasswordHash();
    shared_.geometry = store_.settings().display.geometry;
    shared_.setup_mode = wifi_.setupMode();
  }

  void refreshStatusJson(const std::uint64_t now) {
    (void)health_.report({Subsystem::display,
                          display_ready_ ? HealthLevel::healthy : HealthLevel::failed,
                          display_ready_ ? HealthCode::ok : HealthCode::allocation_failed, now},
                         now);
    (void)health_.report({Subsystem::storage,
                          storage_ready_ ? HealthLevel::healthy : HealthLevel::degraded,
                          storage_ready_ ? HealthCode::ok : HealthCode::not_initialized, now},
                         now);
    const bool online = wifi_.online();
    const bool setup = wifi_.setupMode();
    (void)health_.report(
        {Subsystem::network,
         online ? HealthLevel::healthy : (setup ? HealthLevel::degraded : HealthLevel::degraded),
         online ? HealthCode::ok : (setup ? HealthCode::unprovisioned : HealthCode::link_lost),
         now},
        now);
    (void)health_.report({Subsystem::configuration, HealthLevel::healthy, HealthCode::ok, now},
                         now);
    (void)health_.report({Subsystem::application, HealthLevel::healthy, HealthCode::ok, now},
                         now);
    const DiagnosticsSnapshot snapshot = health_.snapshot(now, 60'000);

    const WifiRuntimeStatus wifi_status = wifi_.status();
    JsonDocument document;
    document["version"] = kFirmwareVersion;
    document["uptime_s"] = static_cast<std::uint32_t>(now / 1000);
    document["heap"] = ESP.getFreeHeap();
    JsonObject wifi = document["wifi"].to<JsonObject>();
    wifi["state"] = wifi_status.state;
    wifi["ssid"] = wifi_status.ssid;
    wifi["rssi"] = wifi_status.rssi;
    wifi["ip"] = wifi_status.ip;
    const std::string_view active = rotator_.activeSceneId();
    document["scene"] = String(active.data(), active.size());
    JsonObject health = document["health"].to<JsonObject>();
    health["overall"] = healthLevelName(snapshot.overall);
    JsonArray subsystems = health["subsystems"].to<JsonArray>();
    for (const HealthReport& report : snapshot.entries) {
      JsonObject entry = subsystems.add<JsonObject>();
      entry["name"] = subsystemName(report.subsystem);
      entry["level"] = healthLevelName(report.level);
    }
    document["time_synced"] = wifi_status.time_synced;
    document["setup_mode"] = wifi_status.setup_mode;
    document["password_set"] = store_.adminPasswordHash().length() > 0;

    String status;
    serializeJson(document, status);
    const std::lock_guard<std::mutex> guard(shared_.mutex);
    shared_.status_json = status;
    shared_.setup_mode = wifi_status.setup_mode;
  }

  void sendFrameEvent() {
    // Skip the encode+base64 work entirely if the channel can't take a frame
    // right now (no client, a client backing up, or low heap). Frames are
    // ephemeral, so dropping is always correct.
    if (!web_.frameChannelReady()) {
      return;
    }
    hub75_.encodeRgb565(frame_rgb565_);
    const std::size_t needed = 4 * ((frame_rgb565_.size() + 2) / 3) + 1;
    frame_base64_.resize(needed);
    std::size_t written = 0;
    const int result =
        mbedtls_base64_encode(frame_base64_.data(), frame_base64_.size(), &written,
                              frame_rgb565_.data(), frame_rgb565_.size());
    if (result != 0) {
      return;
    }
    frame_base64_[written] = 0;
    web_.sendFrame(reinterpret_cast<const char*>(frame_base64_.data()));
  }

  void handleRebootRequests(const std::uint64_t now) {
    if (ota_.rebootPending() && reboot_at_ms_ == 0) {
      log_.append("ota: rebooting into new image");
      reboot_at_ms_ = now + 1500;  // let the HTTP response flush first
    }
    if (reboot_at_ms_ != 0 && now >= reboot_at_ms_) {
      log_.append("system: restarting");
      delay(100);
      ESP.restart();
    }
  }

  // --- members ----------------------------------------------------------

  LogBuffer log_{};
  SettingsStore store_{};
  Hub75Output hub75_{};
  Renderer renderer_{hub75_};
  WifiManager wifi_{store_, log_};

  ClockScene clock_scene_{};
  FlightRadarScene flight_radar_scene_{};
  MetarScene metar_scene_{};
  AmgOpsScene amg_ops_scene_{};
  AmgMissionBoardScene mission_board_scene_{};
  MessageScene message_scene_{};  // playlist slot
  CountdownScene countdown_scene_{};
  SceneRotator rotator_{};

  // Overlay scene pools. A queued overlay in the rotator holds a Scene* into
  // one of these slots, so concurrently-queued overlays must not alias one
  // shared object (that would make an earlier banner render a later one's
  // content). Each pool is sized to the rotator's overlay capacity and the
  // round-robin index advances ONLY on a successful push, which guarantees the
  // slot handed out is never one still referenced by a live overlay.
  std::array<MessageScene, SceneRotator::max_overlays> message_overlay_pool_{};
  std::size_t message_overlay_index_{0};
  std::array<NotificationScene, SceneRotator::max_overlays> notification_pool_{};
  std::size_t notification_index_{0};

  FlightProvider flight_provider_{store_, log_};
  MetarProvider metar_provider_{store_, log_};
  AmgProvider amg_provider_{store_, log_};

  HealthRegistry health_{};
  SharedState shared_{};
  CommandQueue commands_{};
  OtaUpdater ota_{log_};
  WebServer web_{shared_, commands_, log_, ota_};

  std::vector<std::uint8_t> frame_rgb565_{};
  std::vector<unsigned char> frame_base64_{};

  bool display_ready_{false};
  bool storage_ready_{false};
  std::uint64_t frame_number_{0};
  std::uint64_t last_render_ms_{0};
  std::uint64_t last_clock_push_ms_{0};
  std::uint64_t last_status_rebuild_ms_{0};
  std::uint64_t last_status_sse_ms_{0};
  std::uint64_t last_frame_sse_ms_{0};
  std::uint64_t reboot_at_ms_{0};
};

LiveRuntime& runtime() {
  static LiveRuntime instance;
  return instance;
}

}  // namespace

void setup() { runtime().setup(); }

void loop() { runtime().loop(); }
