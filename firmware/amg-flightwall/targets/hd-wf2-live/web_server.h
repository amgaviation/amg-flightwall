// AMG FlightWall live target — device HTTP API + embedded web app.
//
// ESPAsyncWebServer implementation of the contract API. Concurrency rule:
// async callbacks NEVER touch scenes, settings, or NVS. Read routes serve
// JSON strings that the loop task maintains inside SharedState (copied under
// state_mutex); mutating routes validate/authorize, enqueue a Command, and
// return immediately — the loop task drains the queue and applies changes.
//
// Auth: mutating routes require header `X-Auth: <sha256(admin_password)>`
// compared constant-time against the stored hash. Once an admin password is
// set ("claimed"), EVERY mutating route requires X-Auth — including while a
// recovery AP is up (a claimed owner authenticates with their password from
// the AP-served UI). Before the device is claimed (admin_hash empty) only two
// narrow exemptions exist: POST /api/secrets (to set the first password —
// trust-on-first-use) and the Wi-Fi provisioning routes while in AP setup
// mode. OTA, config, reboot, message, notify, and scene control are NEVER
// reachable unauthenticated, closing the first-run LAN race and the
// recovery-AP auth bypass.
//
// SSE /api/events channels: `log` (line per event), `status` (every 5 s),
// `frame` (base64 RGB565, ~500 ms cadence, only while clients are connected).
// All SSE sends originate from the loop task via the send* methods.
#pragma once

#include <cstdint>
#include <deque>
#include <mutex>

#include <ESPAsyncWebServer.h>

#include "log_buffer.h"
#include "ota_update.h"
#include "settings_store.h"

namespace amg::flightwall::live {

struct Command {
  enum class Type {
    apply_config,    // json = PUT /api/config body
    set_secrets,     // json = POST /api/secrets body (held only until applied)
    activate_scene,  // id, duration_ms
    show_message,    // json = POST /api/message body
    notify,          // json = POST /api/notify body
    reboot,
    wifi_join,  // id = ssid, secret = passphrase
  };

  Type type{Type::reboot};
  String json{};
  String id{};
  String secret{};
  std::uint32_t duration_ms{0};
};

class CommandQueue {
 public:
  static constexpr std::size_t kCapacity = 16;

  [[nodiscard]] bool push(Command command);
  [[nodiscard]] bool pop(Command& command);

 private:
  std::mutex mutex_{};
  std::deque<Command> queue_{};
};

// Loop-task-maintained state that async read callbacks copy under mutex.
struct SharedState {
  std::mutex mutex{};
  String status_json{"{}"};
  String config_json{"{}"};
  String admin_hash{};          // sha256 hex of admin password ("" = not set)
  GeometrySettings geometry{};  // active geometry, for reboot_required checks
  bool setup_mode{false};
};

class WebServer {
 public:
  WebServer(SharedState& state, CommandQueue& commands, LogBuffer& log,
            OtaUpdater& ota) noexcept
      : state_(state), commands_(commands), log_(log), ota_(ota) {}

  void begin();

  // --- SSE senders; loop task only ---
  [[nodiscard]] bool hasEventClients() const noexcept;
  // True only when at least one client is connected, its send queue is not
  // backing up, and free heap is above a safety floor. The ~22 KB frame events
  // must be dropped (never queued) when a client stalls, or the no-PSRAM heap
  // is exhausted. Checked before the caller spends CPU encoding a frame.
  [[nodiscard]] bool frameChannelReady() const noexcept;
  void sendLogLine(const char* line);
  void sendStatus(const String& json);
  void sendFrame(const char* base64_payload);

 private:
  // Route sensitivity classes for authorization.
  enum class RouteClass {
    kPrivileged,  // config/ota/reboot/message/notify/scene — always needs auth
                  // once claimed; never open before claiming.
    kClaim,       // POST /api/secrets — open only while unclaimed (TOFU).
    kProvision,   // wifi scan/join — open only in AP setup mode while unclaimed.
  };

  [[nodiscard]] bool authorized(AsyncWebServerRequest* request,
                                RouteClass route = RouteClass::kPrivileged) const;
  void enqueueOrFail(AsyncWebServerRequest* request, Command command);
  void registerAssets();
  void registerApi();
  void handleNotFound(AsyncWebServerRequest* request);

  SharedState& state_;
  CommandQueue& commands_;
  LogBuffer& log_;
  OtaUpdater& ota_;
  AsyncWebServer server_{80};
  AsyncEventSource events_{"/api/events"};
};

}  // namespace amg::flightwall::live
