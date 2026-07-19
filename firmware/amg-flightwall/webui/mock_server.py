#!/usr/bin/env python3
"""AMG FlightWall — mock device server (python3 stdlib only).

Implements the full device HTTP API from docs/platform-design.md with
believable fake data, including an animated 128x64 RGB565 frame stream over
SSE so the dashboard preview visibly works, and serves the SPA from this
directory.

Usage:
    python3 webui/mock_server.py [--port 8377] [--setup] [--password PW]

  --setup      simulate captive-portal AP setup mode (auth disabled,
               wifi state 'setup')
  --password   preset the admin password (default: none -> first-run flow)
"""

import argparse
import base64
import hashlib
import json
import math
import os
import random
import re
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
FRAME_W, FRAME_H = 128, 64

CONTENT_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".json": "application/json",
    ".svg": "image/svg+xml",
    ".png": "image/png",
    ".ico": "image/x-icon",
}

DEFAULT_CONFIG = {
    "schema_version": 2,
    "display": {
        "brightness": 140, "night_brightness": 30,
        "night_start": "22:00", "night_end": "07:00", "off_when_idle": False,
        "geometry": {"panel_w": 64, "panel_h": 64, "chain": 2, "driver": "FM6126A",
                     "clkphase": False, "latch_blanking": 2, "min_refresh": 60},
    },
    "time": {"tz": "EST5EDT,M3.2.0,M11.1.0", "ntp": "pool.ntp.org"},
    "location": {"lat": 40.85, "lon": -74.06, "radius_nm": 30},
    "flights": {"provider": "adsblol", "poll_s": 15, "watchlist": ["N123AM"]},
    "metar": {"station": "KTEB", "poll_s": 600},
    "amg": {"base_url": "https://amgaviationgroup.com", "poll_s": 45,
            "notify_on_submission": True, "notify_on_request": True},
    "playlist": [
        {"id": "clock", "enabled": True, "duration_s": 10},
        {"id": "flights", "enabled": True, "duration_s": 20},
        {"id": "metar", "enabled": True, "duration_s": 10},
        {"id": "amg_ops", "enabled": True, "duration_s": 15},
        {"id": "missions", "enabled": False, "duration_s": 15},
        {"id": "countdown", "enabled": False, "duration_s": 10},
        {"id": "message", "enabled": False, "duration_s": 10},
    ],
    "countdown": {"label": "NEXT DEP", "target_epoch": 0},
    "message": {"text": "", "color": "#176CFF", "scroll": True},
}

SCENE_CATALOG = [
    {"id": "clock", "name": "Clock", "fields": []},
    {"id": "flights", "name": "Flight Radar", "fields": [
        {"key": "flights.provider", "label": "Provider", "type": "select",
         "options": ["adsblol", "opensky"]},
        {"key": "flights.watchlist", "label": "Watchlist", "type": "csv"},
        {"key": "location.radius_nm", "label": "Radius (nm)", "type": "number",
         "min": 5, "max": 250},
    ]},
    {"id": "metar", "name": "METAR", "fields": [
        {"key": "metar.station", "label": "Station", "type": "text", "max_len": 8},
    ]},
    {"id": "amg_ops", "name": "AMG Operations", "fields": []},
    {"id": "missions", "name": "Mission Board", "fields": []},
    {"id": "countdown", "name": "Countdown", "fields": [
        {"key": "countdown.label", "label": "Label", "type": "text", "max_len": 16},
        {"key": "countdown.target_epoch", "label": "Target", "type": "datetime"},
    ]},
    {"id": "message", "name": "Message", "fields": [
        {"key": "message.text", "label": "Text", "type": "text", "max_len": 120},
        {"key": "message.color", "label": "Color", "type": "color"},
        {"key": "message.scroll", "label": "Scroll", "type": "bool"},
    ]},
]

FAKE_NETWORKS = [
    {"ssid": "AMG-Hangar", "rssi": -52, "secure": True, "channel": 6},
    {"ssid": "AMG-Hangar-5G", "rssi": -60, "secure": True, "channel": 44},
    {"ssid": "TetLine-Guest", "rssi": -71, "secure": True, "channel": 11},
    {"ssid": "FBO-Public", "rssi": -78, "secure": False, "channel": 1},
    {"ssid": "N123AM-Cabin", "rssi": -84, "secure": True, "channel": 3},
]

LOG_ROTATION = [
    "providers: adsb.lol poll ok — {n} contacts in 30 nm",
    "providers: metar KTEB fetch ok (VFR 10SM FEW250)",
    "wifi: rssi {rssi} dBm ch 6",
    "amg: bridge poll ok — 2 active missions, 1 new request",
    "scene: rotate -> {scene}",
    "heap: {heap} KB free, min 181 KB",
    "amg: submissions cursor unchanged",
    "providers: metar refresh scheduled in 600 s",
    "amg: WARN bridge latency 1240 ms, backoff ok",
    "ntp: drift +18 ms, resync ok",
]

# ---------------------------------------------------------------------------


class State:
    def __init__(self, args):
        self.lock = threading.RLock()
        self.config = json.loads(json.dumps(DEFAULT_CONFIG))
        self.setup_mode = args.setup
        self.password_hash = (
            hashlib.sha256(args.password.encode()).hexdigest() if args.password else None
        )
        self.wifi_configured = not args.setup
        self.token_set = False
        self.boot_time = time.time()
        self.logs = []
        self.log_seq = 0
        self.overlay = None  # {"kind","color","until"}
        self.forced_scene = None  # {"id","until"}
        self.log("boot: AMG FlightWall mock v1.0.0 (hd_wf2_live profile)")
        self.log("display: HUB75 64x64 chain 2 @ 8 MHz, FM6126A init ok")
        self.log("wifi: " + ("AP FlightWall-Setup up (setup mode)"
                             if args.setup else "STA connected AMG-Hangar 192.168.1.87"))

    def log(self, line):
        with self.lock:
            stamp = time.strftime("[%H:%M:%S] ")
            self.logs.append(stamp + line)
            if len(self.logs) > 256:
                self.logs = self.logs[-256:]
            self.log_seq += 1

    def uptime_s(self):
        return int(time.time() - self.boot_time)

    def active_scene(self):
        now = time.time()
        with self.lock:
            if self.overlay and self.overlay["until"] > now:
                return self.overlay["kind"]
            if self.forced_scene and self.forced_scene["until"] > now:
                return self.forced_scene["id"]
            slots = [s for s in self.config["playlist"] if s.get("enabled")]
        if not slots:
            return "idle"
        total = sum(max(3, int(s.get("duration_s", 10))) for s in slots)
        t = int(now - self.boot_time) % max(total, 1)
        for s in slots:
            d = max(3, int(s.get("duration_s", 10)))
            if t < d:
                return s["id"]
            t -= d
        return slots[0]["id"]

    def status(self):
        heap = 205000 + int(18000 * math.sin(time.time() / 37.0))
        with self.lock:
            wifi = ({"state": "setup", "ssid": "FlightWall-Setup", "rssi": 0,
                     "ip": "192.168.4.1"} if self.setup_mode else
                    {"state": "online", "ssid": "AMG-Hangar",
                     "rssi": -52 - int(6 * abs(math.sin(time.time() / 11))),
                     "ip": "192.168.1.87"})
            return {
                "version": "1.0.0-mock+hdwf2",
                "uptime_s": self.uptime_s(),
                "heap": heap,
                "wifi": wifi,
                "scene": self.active_scene(),
                "time_synced": not self.setup_mode,
                "setup_mode": self.setup_mode,
                "password_set": self.password_hash is not None,
                "health": {
                    "overall": "healthy",
                    "subsystems": {
                        "display": "healthy",
                        "configuration": "healthy",
                        "network": "degraded" if self.setup_mode else "healthy",
                        "storage": "healthy",
                        "application": "healthy",
                    },
                },
            }

    def public_config(self):
        with self.lock:
            cfg = json.loads(json.dumps(self.config))
            cfg["amg"]["token_set"] = self.token_set
            cfg["wifi"] = {"configured": self.wifi_configured}
            return cfg


# --------------------------- frame synthesis -------------------------------

FONT_3X5 = {
    "0": ("111", "101", "101", "101", "111"),
    "1": ("010", "110", "010", "010", "111"),
    "2": ("111", "001", "111", "100", "111"),
    "3": ("111", "001", "111", "001", "111"),
    "4": ("101", "101", "111", "001", "001"),
    "5": ("111", "100", "111", "001", "111"),
    "6": ("111", "100", "111", "101", "111"),
    "7": ("111", "001", "010", "010", "010"),
    "8": ("111", "101", "111", "101", "111"),
    "9": ("111", "101", "111", "001", "111"),
    ":": ("0", "1", "0", "1", "0"),
    " ": ("0", "0", "0", "0", "0"),
}


def hex_to_rgb(value, fallback=(23, 108, 255)):
    m = re.fullmatch(r"#?([0-9a-fA-F]{6})", value or "")
    if not m:
        return fallback
    v = int(m.group(1), 16)
    return ((v >> 16) & 255, (v >> 8) & 255, v & 255)


def render_frame(state):
    """Synthesize a believable animated panel frame -> RGB565 big-endian bytes."""
    t = time.time() - state.boot_time
    buf = bytearray(FRAME_W * FRAME_H * 2)

    def px(x, y, r, g, b):
        x, y = int(x), int(y)
        if 0 <= x < FRAME_W and 0 <= y < FRAME_H:
            v = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            i = (y * FRAME_W + x) * 2
            buf[i] = v >> 8
            buf[i + 1] = v & 0xFF

    def text(s, ox, oy, r, g, b):
        cx = ox
        for ch in s:
            glyph = FONT_3X5.get(ch, FONT_3X5[" "])
            w = len(glyph[0])
            for gy, rowbits in enumerate(glyph):
                for gx, bit in enumerate(rowbits):
                    if bit == "1":
                        px(cx + gx, oy + gy, r, g, b)
            cx += w + 1

    # header: clock + separator
    text(time.strftime("%H:%M:%S"), 2, 2, 220, 230, 245)
    for x in range(FRAME_W):
        px(x, 9, 10, 40, 90)
    for y in range(FRAME_H):
        px(0, y, 23, 108, 255)  # AMG blue accent edge

    # radar plan view
    cx, cy = 88, 36
    for radius in (9, 18, 27):
        steps = radius * 8
        for k in range(steps):
            a = 2 * math.pi * k / steps
            px(cx + radius * math.cos(a), cy + radius * math.sin(a), 8, 30, 70)
    # sweep with trail
    sweep = t * 1.1
    for trail in range(4):
        a = sweep - trail * 0.09
        lvl = (36, 24, 14, 8)[trail]
        for d in range(27):
            px(cx + d * math.cos(a), cy + d * math.sin(a), 0, lvl * 3, lvl * 6)
    # own ship
    for dx in (0, 1):
        for dy in (0, 1):
            px(cx + dx, cy + dy, 23, 108, 255)
    # traffic blips (one watchlisted, amber)
    for i in range(3):
        a = t * (0.10 + 0.03 * i) + i * 2.1
        radius = 11 + i * 7
        bx, by = cx + radius * math.cos(a), cy + radius * math.sin(a)
        color = (255, 176, 46) if i == 0 else (110, 220, 140)
        for dx in (0, 1):
            for dy in (0, 1):
                px(bx + dx, by + dy, *color)

    # left column: fake data rows (flight strip bars)
    for row in range(3):
        y0 = 16 + row * 12
        width = 34 + int(8 * math.sin(t / 2 + row))
        for x in range(2, 2 + width):
            px(x, y0, 40, 70, 110)
            px(x, y0 + 1, 24, 42, 66)
        color = (255, 176, 46) if row == 0 else (90, 160, 230)
        for x in range(2, 8):
            px(x, y0, *color)
            px(x, y0 + 1, *color)

    # bottom ticker (METAR green)
    shift = int(t * 22)
    for x in range(FRAME_W):
        if ((x + shift) // 9) % 2 == 0:
            px(x, 60, 40, 190, 90)
            px(x, 61, 20, 110, 55)

    # overlay banner (message / notify): colored border + center band
    with state.lock:
        overlay = state.overlay if state.overlay and state.overlay["until"] > time.time() else None
    if overlay:
        r, g, b = overlay["color"]
        pulse = 0.6 + 0.4 * math.sin(t * 6)
        rr, gg, bb = int(r * pulse), int(g * pulse), int(b * pulse)
        for x in range(FRAME_W):
            for y in (0, 1, FRAME_H - 2, FRAME_H - 1):
                px(x, y, rr, gg, bb)
        for y in range(FRAME_H):
            for x in (0, 1, FRAME_W - 2, FRAME_W - 1):
                px(x, y, rr, gg, bb)
        for y in range(26, 38):
            for x in range(8, FRAME_W - 8):
                px(x, y, r // 5, g // 5, b // 5)
        # scrolling bright block inside the band
        bx = int((t * 40) % (FRAME_W + 24)) - 24
        for y in range(28, 36):
            for x in range(max(8, bx), min(FRAME_W - 8, bx + 24)):
                px(x, y, r, g, b)

    return bytes(buf)


# ------------------------------ HTTP handler -------------------------------


def log_generator(state):
    while True:
        time.sleep(random.uniform(2.0, 4.5))
        template = random.choice(LOG_ROTATION)
        state.log(template.format(
            n=random.randint(3, 9),
            rssi=-52 - random.randint(0, 9),
            heap=random.randint(195, 222),
            scene=state.active_scene(),
        ))


def make_handler(state):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"
        server_version = "FlightWallMock/1.0"

        # ---- helpers ----
        def send_json(self, obj, code=200):
            body = json.dumps(obj).encode()
            self.send_response(code)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(body)

        def send_text(self, text, code=200, ctype="text/plain; charset=utf-8"):
            body = text.encode()
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def read_body(self):
            # Cached: with HTTP/1.1 keep-alive the body must be drained exactly
            # once per request even on early 401/400 replies, otherwise the
            # leftover bytes corrupt the next request on the connection.
            if hasattr(self, "_body"):
                return self._body
            length = int(self.headers.get("Content-Length", "0") or 0)
            data = b""
            while len(data) < length:
                chunk = self.rfile.read(length - len(data))
                if not chunk:
                    break
                data += chunk
            self._body = data
            return data

        def read_json(self):
            try:
                return json.loads(self.read_body() or b"{}")
            except json.JSONDecodeError:
                return None

        def authorized(self):
            """Mutating routes: open in setup mode and before a password exists."""
            if state.setup_mode or state.password_hash is None:
                return True
            return self.headers.get("X-Auth", "") == state.password_hash

        def deny(self):
            self.read_body()  # drain so keep-alive connections stay in sync
            self.send_json({"error": "unauthorized"}, 401)

        def log_message(self, fmt, *args):  # quieter console
            print(f"  [{self.command}] {self.path} -> {fmt % args}")

        # ---- GET ----
        def do_GET(self):
            path = self.path.split("?", 1)[0]
            if path == "/api/status":
                return self.send_json(state.status())
            if path == "/api/config":
                return self.send_json(state.public_config())
            if path == "/api/scenes":
                return self.send_json(SCENE_CATALOG)
            if path == "/api/logs":
                with state.lock:
                    text = "\n".join(state.logs)
                return self.send_text(text)
            if path == "/api/events":
                return self.serve_events()
            if path == "/api/wifi/scan":
                nets = []
                for n in FAKE_NETWORKS:
                    item = dict(n)
                    item["rssi"] = n["rssi"] + random.randint(-3, 3)
                    nets.append(item)
                return self.send_json({"networks": nets})
            return self.serve_static(path)

        # ---- POST/PUT ----
        def do_PUT(self):
            if self.path.split("?", 1)[0] == "/api/config":
                if not self.authorized():
                    return self.deny()
                cfg = self.read_json()
                if not isinstance(cfg, dict):
                    return self.send_json({"error": "invalid json"}, 400)
                err = validate_config(cfg)
                if err:
                    return self.send_json({"error": err}, 400)
                with state.lock:
                    old_geo = state.config["display"]["geometry"]
                    new_geo = cfg.get("display", {}).get("geometry", old_geo)
                    reboot = new_geo != old_geo
                    for key in DEFAULT_CONFIG:
                        if key in cfg:
                            state.config[key] = cfg[key]
                    state.config.pop("wifi", None)
                    if isinstance(state.config.get("amg"), dict):
                        state.config["amg"].pop("token_set", None)
                state.log("config: applied via web ui"
                          + (" (geometry -> reboot required)" if reboot else ""))
                return self.send_json({"ok": True, "reboot_required": reboot})
            self.send_json({"error": "not found"}, 404)

        def do_POST(self):
            path = self.path.split("?", 1)[0]
            if path == "/api/secrets":
                return self.post_secrets()
            if not self.authorized():
                return self.deny()
            if path == "/api/scene/activate":
                body = self.read_json() or {}
                scene_id = body.get("id")
                if scene_id not in [s["id"] for s in SCENE_CATALOG]:
                    return self.send_json({"error": "unknown scene"}, 400)
                dur = int(body.get("duration_s", 30))
                with state.lock:
                    state.forced_scene = {"id": scene_id, "until": time.time() + dur}
                state.log(f"scene: activate {scene_id} for {dur} s (web)")
                return self.send_json({"ok": True})
            if path == "/api/message":
                body = self.read_json() or {}
                if not str(body.get("text", "")).strip():
                    return self.send_json({"error": "text required"}, 400)
                dur = int(body.get("duration_s", 15))
                with state.lock:
                    state.overlay = {"kind": "message",
                                     "color": hex_to_rgb(body.get("color")),
                                     "until": time.time() + dur}
                state.log(f"overlay: message \"{body['text'][:40]}\" {dur} s")
                return self.send_json({"ok": True})
            if path == "/api/notify":
                body = self.read_json() or {}
                if not str(body.get("title", "")).strip():
                    return self.send_json({"error": "title required"}, 400)
                level = body.get("level", "info")
                colors = {"info": (23, 108, 255), "success": (53, 201, 107),
                          "warn": (255, 176, 46), "alert": (255, 77, 90)}
                dur = int(body.get("duration_s", 15))
                with state.lock:
                    state.overlay = {"kind": "notify",
                                     "color": colors.get(level, colors["info"]),
                                     "until": time.time() + dur}
                state.log(f"overlay: notify [{level}] \"{body['title'][:40]}\"")
                return self.send_json({"ok": True})
            if path == "/api/ota":
                data = self.read_multipart_file()
                if data is None:
                    return self.send_json({"error": "no firmware part"}, 400)
                sha = hashlib.sha256(data).hexdigest()
                state.log(f"ota: received {len(data)} B sha256 {sha[:16]}… -> app1")
                state.log("ota: verify ok, marking app1 bootable, rebooting")
                return self.send_json({"ok": True, "sha256": sha,
                                       "size": len(data), "slot": "app1"})
            if path == "/api/reboot":
                state.log("system: reboot requested via web ui")
                state.boot_time = time.time()
                return self.send_json({"ok": True})
            if path == "/api/wifi":
                body = self.read_json() or {}
                ssid = str(body.get("ssid", "")).strip()
                if not ssid:
                    return self.send_json({"error": "ssid required"}, 400)
                with state.lock:
                    state.wifi_configured = True
                state.log(f"wifi: credentials stored for \"{ssid}\", rebooting to STA")
                return self.send_json({"ok": True, "reboot": True})
            self.send_json({"error": "not found"}, 404)

        def post_secrets(self):
            body = self.read_json()
            if body is None:
                return self.send_json({"error": "invalid json"}, 400)
            # First-run: setting the admin password is allowed unauthenticated.
            # Once set, all secret changes require X-Auth.
            if not self.authorized():
                return self.deny()
            changed = []
            with state.lock:
                if body.get("admin_password"):
                    state.password_hash = hashlib.sha256(
                        body["admin_password"].encode()).hexdigest()
                    changed.append("admin_password")
                if body.get("amg_token"):
                    state.token_set = True
                    changed.append("amg_token")
                if body.get("wifi_ssid"):
                    state.wifi_configured = True
                    changed.append("wifi")
            if not changed:
                return self.send_json({"error": "no recognized secret"}, 400)
            state.log(f"secrets: updated {', '.join(changed)} (values not logged)")
            return self.send_json({"ok": True, "updated": changed})

        # ---- SSE ----
        def serve_events(self):
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "close")
            self.end_headers()

            def emit(event, data):
                self.wfile.write(f"event: {event}\ndata: {data}\n\n".encode())
                self.wfile.flush()

            with state.lock:
                log_cursor = len(state.logs)
            tick = 0
            try:
                emit("status", json.dumps(state.status()))
                while True:
                    frame = render_frame(state)
                    emit("frame", base64.b64encode(frame).decode())
                    with state.lock:
                        new_lines = state.logs[log_cursor:]
                        log_cursor = len(state.logs)
                    for line in new_lines:
                        emit("log", line)
                    if tick % 10 == 0 and tick > 0:
                        emit("status", json.dumps(state.status()))
                    tick += 1
                    time.sleep(0.5)
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass

        # ---- multipart ----
        def read_multipart_file(self):
            ctype = self.headers.get("Content-Type", "")
            m = re.search(r"boundary=([^;]+)", ctype)
            if not m:
                return None
            boundary = ("--" + m.group(1).strip().strip('"')).encode()
            body = self.read_body()
            for part in body.split(boundary):
                part = part.strip(b"\r\n")
                if not part or part == b"--":
                    continue
                if b"\r\n\r\n" not in part:
                    continue
                head, data = part.split(b"\r\n\r\n", 1)
                if b"filename=" in head or b'name="firmware"' in head:
                    if data.endswith(b"\r\n"):
                        data = data[:-2]
                    return data
            return None

        # ---- static SPA ----
        def serve_static(self, path):
            if path == "/":
                path = "/index.html"
            rel = path.lstrip("/")
            full = os.path.realpath(os.path.join(BASE_DIR, rel))
            ext = os.path.splitext(full)[1].lower()
            if (not full.startswith(BASE_DIR + os.sep)
                    or ext not in CONTENT_TYPES
                    or not os.path.isfile(full)):
                # SPA fallback: unknown non-API path -> index (captive portal UX)
                if not path.startswith("/api/"):
                    index = os.path.join(BASE_DIR, "index.html")
                    with open(index, "rb") as fh:
                        body = fh.read()
                    self.send_response(200)
                    self.send_header("Content-Type", CONTENT_TYPES[".html"])
                    self.send_header("Content-Length", str(len(body)))
                    self.end_headers()
                    self.wfile.write(body)
                    return
                return self.send_json({"error": "not found"}, 404)
            with open(full, "rb") as fh:
                body = fh.read()
            self.send_response(200)
            self.send_header("Content-Type", CONTENT_TYPES[ext])
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Cache-Control", "no-cache")
            self.end_headers()
            self.wfile.write(body)

    return Handler


def validate_config(cfg):
    disp = cfg.get("display", {})
    if not isinstance(disp, dict):
        return "display must be an object"
    b = disp.get("brightness", 140)
    if not isinstance(b, (int, float)) or not 0 <= b <= 255:
        return "display.brightness out of range 0-255"
    playlist = cfg.get("playlist", [])
    if not isinstance(playlist, list):
        return "playlist must be a list"
    for slot in playlist:
        if not isinstance(slot, dict) or "id" not in slot:
            return "playlist slots need an id"
        if not isinstance(slot.get("duration_s", 10), (int, float)):
            return "playlist duration_s must be numeric"
    return None


def main():
    parser = argparse.ArgumentParser(description="FlightWall mock device server")
    parser.add_argument("--port", type=int, default=8377)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--setup", action="store_true",
                        help="simulate captive-portal setup mode")
    parser.add_argument("--password", default=None,
                        help="preset admin password (default: first-run flow)")
    args = parser.parse_args()

    state = State(args)
    threading.Thread(target=log_generator, args=(state,), daemon=True).start()

    server = ThreadingHTTPServer((args.host, args.port), make_handler(state))
    mode = "SETUP (captive portal)" if args.setup else "station"
    print(f"FlightWall mock on http://{args.host}:{args.port}  mode={mode}  "
          f"password={'set' if state.password_hash else 'FIRST-RUN (unset)'}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
