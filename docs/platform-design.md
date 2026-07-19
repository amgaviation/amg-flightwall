# AMG FlightWall Platform — Build Contract (2026-07-19)

Single source of truth for the parallel platform build. Build agents follow
this exactly; deviations require updating this document in the same commit.

## Goals

One firmware image that: renders a rotating playlist of scenes on the 128×64
HUB75 panel (75EX2 port — the X1 color buffer is dead, see
`docs/diagnosis-2026-07-19.md`); serves a full local web UI + JSON API on the
LAN (`http://flightwall.local` / device IP); joins Wi-Fi via a first-boot
captive portal; polls public flight/weather APIs and the authenticated amg1
bridge; supports OTA upload from the browser (A/B app slots). Host simulator
and tests keep working.

## Hardware truth (do not re-litigate)

- Panel: 128×64, 1/32-scan (HUB75-E), FM6124E column drivers. Proven working.
- Port: **75EX2 only.** Pins: R1 4, G1 8, B1 12, R2 5, G2 9, B2 13,
  A 39, B 38, C 37, D 36, **E 21 (verified working on V7.2.0-2)**,
  LAT 33, OE 35, CLK 34. Never drive the X1 RGB pins (2,6,10,3,7,11).
- Factory MatrixPanel config (from factory binary): 64×64 panel, chain 2,
  FM6126A driver init, 8 MHz clock, `clkphase=false`, latch blanking 2,
  min refresh 60 Hz, 8-bit depth. Raw linear 128×64 addressing photo-verified.
  Display geometry/driver/clkphase MUST be runtime-overridable from config so
  tuning never requires a reflash.
- Partition map: factory A/B layout (see `target_profile.hpp`). app0/app1
  2.75 MB each; `spiffs` region is NOT touched (factory data preserved). Web
  assets are gzipped and embedded in the app binary; config lives in NVS
  namespace `amgfw`.
- Flashing over USB stays manual esptool (safety gate stays). OTA uses the
  standard esp_ota API onto the inactive slot.

## Repo layout (ownership per agent — do not edit outside your area)

- `firmware/amg-flightwall/include|src` (core, portable, no Arduino):
  **Agent SCENES** owns new files; may not modify existing files except
  `scenes.hpp/.cpp` (additive) and adding new headers/sources.
- `firmware/amg-flightwall/targets/hd-wf2-live/`: **Agent PLATFORM** owns.
- `firmware/amg-flightwall/webui/`: **Agent WEBUI** owns (sources +
  `tools/build_webui.py` that emits `targets/hd-wf2-live/webui_assets.h`;
  the generated header is committed).
- amg1 repo: **Agent BRIDGE** owns (separate repository, new branch).
- `platformio.ini`, `Makefile`, integration glue, this doc: **orchestrator
  only** (already scaffolded; do not edit).

## Portable core additions (Agent SCENES)

All portable: C++17, no Arduino/ESP-IDF includes, compiles in host `make`.

### Data models — `include/amg/flightwall/data_models.hpp`

```cpp
struct FlightContact { std::string callsign; double lat, lon; int altitude_ft;
  int ground_speed_kt; int heading_deg; double distance_nm; bool watchlisted; };
struct FlightSnapshot { std::vector<FlightContact> contacts;
  std::uint64_t fetched_at_ms{0}; bool valid{false}; };
struct MetarSnapshot { std::string station, raw, flight_category, wind, visibility;
  int temp_c{0}, dewpoint_c{0}; std::uint64_t fetched_at_ms{0}; bool valid{false}; };
struct AmgMissionItem { std::string label, status; int eta_min{-1}; };
struct AmgRequestItem { std::string label, name; int age_min{-1}; };
struct AmgSubmissionItem { std::string kind, name; int age_min{-1}; };
struct AmgMetricsSnapshot {
  int new_request_count{0}; std::vector<AmgRequestItem> latest_requests;
  int active_mission_count{0}; std::vector<AmgMissionItem> missions;
  std::vector<AmgSubmissionItem> recent_submissions; std::string submissions_cursor;
  std::int64_t revenue_today_cents{-1}, revenue_mtd_cents{-1}; std::string currency;
  std::string site_state; std::uint64_t fetched_at_ms{0}; bool valid{false}; };
struct NotificationEvent { std::string title, body; Color color;
  std::uint8_t priority{0}; std::uint32_t duration_ms{15000}; };
struct MessagePayload { std::string text; Color color; std::uint32_t duration_ms{0};
  bool scroll{true}; };
struct ClockInfo { int hour{0}, minute{0}, second{0}, month{0}, day{0}, weekday{0};
  bool valid{false}; };  // platform supplies wall-clock; core never does time math
struct CountdownInfo { std::string label; std::int64_t seconds_remaining{-1}; };
```

### Scene engine — `include/amg/flightwall/scene_rotator.hpp`

`SceneRotator` (new, does not replace `SceneManager`):
- Holds an ordered list of `SceneSlot { Scene* scene; bool enabled;
  std::uint32_t duration_ms; }`.
- `tick(FrameContext&)` renders the active slot, advances when its duration
  elapses (skips disabled slots; single-slot lists just stay).
- Overlay stack: `pushOverlay(Scene&, duration_ms, priority)` — highest
  priority renders instead of the playlist; expires by duration; lower-priority
  pending overlays queue. `NotificationScene` is fed by overlays.
- `activateNow(id, duration_ms)` for the web API's "show this now".
- Deterministic and fully host-testable (time injected via FrameContext).

### Scenes (each a `Scene`, constructor takes no platform deps; data pushed
via setters; render defensively when snapshot `!valid` — show a labeled
"stale/offline" state, never garbage):

- `ClockScene` — big HH:MM (scale 2), date line, seconds tick; uses ClockInfo.
- `FlightRadarScene` — replaces/extends ClassicScene: nearest N contacts as
  rows (callsign, alt, speed) + a mini plan-view map (relative positions
  scaled into a box, own position at center); watchlisted contacts in
  `colors::amber`.
- `MetarScene` — station + flight category color-coded (VFR green, MVFR blue,
  IFR red, LIFR magenta), scrolling raw METAR on the bottom row.
- `AmgOpsScene` — headline counters: NEW REQ n (amber if >0), ACT MSN n,
  MTD $k; rotating detail line through latest_requests/missions.
- `AmgMissionBoardScene` — mission rows with status color chips.
- `MessageScene` — static or horizontally scrolling text, settable color.
- `CountdownScene` — label + D:HH:MM:SS remaining.
- `NotificationScene` — full-panel banner: colored border, title (scale 2 if
  it fits), body scroll; driven only through the overlay stack.
- Keep `HardwareSmokeScene` compiling untouched.

Shared drawing helpers allowed in `src/scene_support.cpp` (e.g. scroll state,
right-aligned text, tiny 3×5 digits if needed — must stay portable).

### Host tests (extend existing `tests/`, same style/framework as present):
rotator scheduling/overlay preemption/expiry; each scene renders into
`FrameBufferDisplay` without OOB writes (the FrameBufferDisplay ignores OOB,
assert litPixelCount>0 for valid snapshots and ==stale-marker for invalid);
deterministic golden checks where cheap. `make test` must pass.

### Simulator: extend `simulator/main.cpp` minimally to instantiate the
rotator with fixture data for every scene and print/dump frames (keep existing
behavior working; add `--scene <id>` arg to render one scene's frame as ASCII).

## Platform target (Agent PLATFORM) — `targets/hd-wf2-live/`

Arduino / espressif32@6.12.0, board esp32-s3-devkitc-1, flags as in the
existing envs (`ARDUINO_USB_CDC_ON_BOOT=1`, C++17, -Wall -Wextra -Werror for
our sources). Libraries (pinned in the env, already scaffolded by
orchestrator): ESP32-HUB75-MatrixPanel-DMA (same commit pin as build_only
env), Adafruit GFX + BusIO, ESPAsyncWebServer + AsyncTCP (mathieucarbou
forks pinned), ArduinoJson@^7.

Modules (files under `targets/hd-wf2-live/`, integrate via `live_main.cpp`
which the orchestrator finalizes):

- `hub75_output.{h,cpp}` — implements core `Display` over
  `MatrixPanel_I2S_DMA`, X2 pins from `hdWf2MiniX2Profile()`; geometry/driver/
  clkphase/brightness read from `DeviceSettings`; `present()` blits RGB888;
  double-buffer OFF (single buffer + full repaint per frame at scene tick
  ~10 fps is fine); exposes `setBrightness(0..255)`.
- `settings_store.{h,cpp}` — `DeviceSettings` struct (see Config schema) ⇄
  ArduinoJson ⇄ NVS blob (`amgfw/config`), plus separate NVS keys for wifi
  creds (`amgfw/wifi_ssid`, `amgfw/wifi_pass`) and admin password hash —
  wifi credentials and password hash NEVER appear in `/api/config` responses.
- `wifi_manager.{h,cpp}` — drives core `WifiSupervisor`: STA connect with
  stored creds; if unprovisioned or `requires_attention` → AP mode
  `FlightWall-Setup` (open AP + captive DNS) serving the same web app in
  setup mode; mDNS `flightwall` when online; NTP sync (configurable tz
  offset, POSIX TZ string) feeding `ClockInfo`.
- `providers.{h,cpp}` — non-blocking pollers (state machines on loop(),
  HTTPClient + WiFiClientSecure with `setCACertBundle` (arduino cert bundle);
  document and fall back to `setInsecure()` per-endpoint flag if bundle
  fails): `FlightProvider` (adsb.lol `/v2/point/{lat}/{lon}/{radius}`,
  optional OpenSky alt impl behind an interface), `MetarProvider`
  (aviationweather.gov `/api/data/metar?ids=X&format=json`), `AmgProvider`
  (bridge GET, Bearer token; detects submissions-cursor change → fires
  NotificationEvent). Poll intervals per config; exponential backoff on
  failure; all results pushed into scenes under the state mutex.
- `web_server.{h,cpp}` — API per spec below + embedded asset serving
  (`webui_assets.h`: gzip, correct content-type, cache headers, etag=build).
- `log_buffer.{h,cpp}` — ring buffer (256 lines) capturing our log macro +
  fanout to Serial and SSE.
- `ota_update.{h,cpp}` — POST /api/ota (multipart) → `Update` to inactive
  slot, sha reported, reboot on success; guarded by auth.
- Concurrency rule: Async server callbacks NEVER touch scenes/settings
  directly — they enqueue commands / copy snapshots under `state_mutex`.

## Config schema (NVS JSON `amgfw/config`, schema_version 2)

```json
{ "schema_version": 2,
  "display": { "brightness": 140, "night_brightness": 30,
    "night_start": "22:00", "night_end": "07:00", "off_when_idle": false,
    "geometry": {"panel_w":64, "panel_h":64, "chain":2, "driver":"FM6126A",
                 "clkphase":false, "latch_blanking":2, "min_refresh":60} },
  "time": { "tz": "EST5EDT,M3.2.0,M11.1.0", "ntp": "pool.ntp.org" },
  "location": { "lat": 0.0, "lon": 0.0, "radius_nm": 30 },
  "flights": { "provider": "adsblol", "poll_s": 15, "watchlist": ["N123AM"] },
  "metar": { "station": "KTEB", "poll_s": 600 },
  "amg": { "base_url": "", "poll_s": 45, "notify_on_submission": true,
           "notify_on_request": true },
  "playlist": [ {"id":"clock","enabled":true,"duration_s":10},
    {"id":"flights","enabled":true,"duration_s":20},
    {"id":"metar","enabled":true,"duration_s":10},
    {"id":"amg_ops","enabled":true,"duration_s":15},
    {"id":"missions","enabled":false,"duration_s":15},
    {"id":"countdown","enabled":false,"duration_s":10},
    {"id":"message","enabled":false,"duration_s":10} ],
  "countdown": { "label": "NEXT DEP", "target_epoch": 0 },
  "message": { "text": "", "color": "#176CFF", "scroll": true } }
```
Secrets stored separately in NVS, write-only via API: wifi creds, amg bridge
token, admin password (stored as SHA-256 hex).

## Device HTTP API (all JSON; mutating routes require header
`X-Auth: <sha256(admin_password)>` unless in AP setup mode)

- `GET /api/status` → {version, uptime_s, heap, wifi:{state,ssid,rssi,ip},
  scene, health(DiagnosticsSnapshot mapped), time_synced}
- `GET /api/config` → config JSON (secrets omitted; amg.token_set:bool,
  wifi.configured:bool flags instead)
- `PUT /api/config` → validate, persist, apply live (brightness/playlist
  without reboot; geometry marks reboot_required:true in response)
- `POST /api/secrets` → {wifi_ssid?, wifi_pass?, amg_token?, admin_password?}
  any subset; applies immediately
- `GET /api/scenes` → catalog [{id, name, configurable fields}]
- `POST /api/scene/activate` {id, duration_s?}
- `POST /api/message` {text, color?, duration_s?, scroll?} → shows now (overlay)
- `POST /api/notify` {title, body?, level: info|success|warn|alert, duration_s?}
- `GET /api/logs` → last N lines text
- `GET /api/events` (SSE): `log` lines; `status` every 5 s; `frame` every
  ~500 ms — base64 of RGB565 128×64 (16 KB→~21 KB b64; acceptable on LAN,
  only while a client is connected)
- `POST /api/ota` multipart firmware.bin; `POST /api/reboot`
- Captive setup mode additionally: `GET /api/wifi/scan`, `POST /api/wifi`
  (then reboots into STA)

## Web UI (Agent WEBUI) — `firmware/amg-flightwall/webui/`

No frameworks, no CDN: hand-written ES modules + one CSS file, dark aviation
aesthetic (AMG blue #176CFF accent). Pages (hash-routed SPA): Dashboard
(live canvas preview from SSE frames, status cards, quick actions
message/notify test), Playlist (enable/reorder/duration per scene + per-scene
settings forms driven by /api/scenes), Display (brightness, night mode,
schedule, geometry advanced panel with reboot warning), Integrations
(location picker lat/lon/radius, METAR station, flights provider/watchlist,
AMG bridge url + token set/replace, poll intervals), System (status, logs
live tail, OTA upload with progress, reboot, auth/password change), Setup
(wifi scan/join — same bundle serves the captive portal, UI detects setup
mode via /api/status). First-run: if no admin password set, UI prompts to
create one (calls /api/secrets). Build: `tools/build_webui.py` minifies
(naive whitespace strip ok), gzips each asset, emits `webui_assets.h`
(PROGMEM byte arrays + manifest table). Total gzipped budget: < 120 KB.
Test in a plain browser against a mock: `webui/mock_server.py` (python
stdlib) implementing the API spec with fake data — committed, documented.

## amg1 bridge (Agent BRIDGE — amg1 repo, new branch `flightwall-bridge`)

`GET /api/flightwall/summary`, `Authorization: Bearer ${FLIGHTWALL_API_TOKEN}`
(new env var; add to .env.example with placeholder; constant-time compare;
401 without leaking). Response (device contract — map real schema into this;
omit/null anything the schema lacks; every field optional to the device):

```json
{ "generated_at": "ISO8601",
  "requests": {"new_count": 0, "latest": [{"label":"KTEB-KPBI","name":"J S","age_min":42}]},
  "missions": {"active_count": 0, "items": [{"label":"N123AM KTEB-KOPF","status":"enroute","eta_min":95}]},
  "submissions": {"cursor":"<opaque, changes when new arrives>",
                   "recent":[{"kind":"contact","name":"...","age_min":3}]},
  "revenue": {"today_cents":0, "mtd_cents":0, "currency":"usd"},
  "site": {"state":"ok"} }
```
Names/labels truncated ≤ 24 chars server-side (LED display; also privacy:
first name + last initial only). In-memory cache 30 s. Follow repo's existing
route/auth/error conventions; `npm run` typecheck/lint/guard scripts must
pass; commit on the branch; DO NOT push to main; do not touch existing
routes/behavior.

## Definition of done (orchestrator gate)

- `pio run` succeeds for all envs incl. `hd_wf2_live`; host `make test` green.
- Simulator renders every scene with fixtures.
- webui builds; mock-server manual smoke documented.
- Security review pass (token/password handling, OTA auth, portal exposure).
- Everything committed to `claude/flightwall-platform` (amg1: its branch).

## Amendments (orchestrator, post-recon)

- Host build: root `Makefile` wildcard-discovers `src/*.cpp` — new portable
  sources need NO Makefile edits. Tests: extend
  `firmware/amg-flightwall/tests/test_main.cpp` (hand-rolled CHECK/run
  harness, single binary) additively. Simulator: extend
  `simulator/main.cpp` keeping existing PPM outputs intact
  (`make simulator-smoke` asserts classic/operations/hardware-smoke.ppm).
- DO NOT modify `renderer.cpp` glyph tables, `hardware_smoke_scene.*`, or
  existing scene rendering — a golden FNV-1a framebuffer-hash regression test
  guards them. New drawing helpers go in NEW files.
- Coding rules from docs/development.md bind all firmware agents: injected
  clocks/storage/transport, no mutable globals (Arduino composition root
  excepted), no heap allocation inside per-frame render paths (pre-size
  buffers in setters), bounded containers, typed errors, no exceptions
  across module boundaries, provider payloads normalized in adapters.
- `hd_wf2_live` env exists in platformio.ini (orchestrator-owned): NO_GFX,
  MatrixPanel pinned, esp32async ESPAsyncWebServer/AsyncTCP, ArduinoJson 7.
  Agent PLATFORM does not edit platformio.ini; report needed changes instead.
- amg1 specifics for Agent BRIDGE (from recon): App Router Next 16,
  routes at app/api/**/route.ts; auth precedent for shared-secret endpoints =
  app/api/webhooks/email/status/route.ts pattern; use createServiceClient()
  from lib/supabase/server.ts AFTER token check; `export const dynamic =
  "force-dynamic"`. Data sources: missions (intake statuses = submitted,
  under_review, awaiting_client_info => "new requests"; active board =
  quoted/approved/crew_assigned/scheduled/in_progress), quotes,
  contact_form_submissions (status 'new', created_at cursor),
  public_support_requests, payments (dollars numeric; exclude
  failed/void/refunded) + subscription_billing_invoices (amount_paid,
  paid_at) for revenue — NO live Stripe calls. Convert dollars→cents in the
  response. database.types.ts is stale for some tables — follow the existing
  `as any` cast pattern where needed. Typecheck = `npm run typecheck`.
  New env var FLIGHTWALL_API_TOKEN (add to .env.example placeholder only).
