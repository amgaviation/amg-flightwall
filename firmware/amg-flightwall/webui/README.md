# FlightWall Web UI

Hand-written, dependency-free SPA embedded in the `hd_wf2_live` firmware.
No frameworks, no CDN, no build-time npm: plain ES modules + one CSS file,
served gzipped out of `targets/hd-wf2-live/webui_assets.h`.

## Layout

```
webui/
  index.html            SPA shell (hash router mounts into #page)
  app.css               the single stylesheet (dark aviation, AMG blue #176CFF)
  js/
    app.js              bootstrap, hash router, auth modals, shared state
    api.js              fetch wrapper (X-Auth + 401 retry), SHA-256, SSE bus
    ui.js               DOM helpers, toasts, modals
    frame.js            base64 RGB565 -> canvas decoder
    pages/{dashboard,playlist,display,integrations,system,setup}.js
  mock_server.py        stdlib mock of the entire device API (dev only)
  README.md             this file (dev only)
```

`mock_server.py` and `README.md` are never embedded — `tools/build_webui.py`
only picks up `.html .css .js .svg .json .png .ico`.

## Dev workflow

```sh
# 1. serve the SPA + full fake API on http://127.0.0.1:8377
python3 webui/mock_server.py

# variants:
python3 webui/mock_server.py --setup              # captive-portal AP mode
python3 webui/mock_server.py --password hunter22  # skip first-run, test login
python3 webui/mock_server.py --port 9000 --host 0.0.0.0

# 2. edit sources, reload browser (mock serves files straight from webui/)

# 3. rebuild the embedded header (commit the result)
python3 tools/build_webui.py          # writes targets/hd-wf2-live/webui_assets.h
python3 tools/build_webui.py --check  # sizes only, no write; fails if >120 KB
```

The mock streams an animated fake panel frame over SSE (radar sweep, traffic
blips, ticking clock), generates rolling logs, enforces the real auth rules,
and accepts OTA multipart uploads (echoes sha256/size back), so every page is
exercisable end to end without hardware.

## Auth model (mirrors the device contract)

- Mutating routes require header `X-Auth: <hex sha256(admin password)>`
  except in AP setup mode, or before any password exists (first run).
- The hash is computed client-side (`crypto.subtle` when the origin allows it,
  bundled pure-JS SHA-256 fallback for plain-http LAN origins) and cached in
  `localStorage` under `amgfw.auth`. Plaintext passwords are never stored.
- On any 401 the cached hash is dropped and a sign-in modal appears; the
  failed request is retried after sign-in.
- First run: `GET /api/status` reporting `password_set:false` (outside setup
  mode) triggers the create-password modal, which calls
  `POST /api/secrets {admin_password}`.

## Status fields the UI depends on

Beyond the contract's `/api/status` list, the UI reads two flags the firmware
must supply:

- `setup_mode: bool` — true in captive AP mode; shows the banner, routes to
  Setup, and suppresses the first-run password prompt.
- `password_set: bool` — false until an admin password exists; gates the
  first-run modal and the System-page wording.

## SSE `/api/events`

- `frame` — base64 of 128x64 RGB565, **big-endian byte order per pixel**
  (byte0 = high byte: RRRRRGGG, byte1 = low byte: GGGBBBBB), row-major from
  top-left. 16384 raw bytes -> ~21.8 KB base64, emitted ~every 500 ms.
  `js/frame.js` and `mock_server.py` both implement this order; keep the
  firmware `web_server.cpp` frame encoder consistent with it.
- `status` — the `/api/status` JSON, every 5 s (plus once on connect).
- `log` — one log line per event.

## Serving the embedded bundle (notes for `web_server.cpp`)

- Every manifest entry is gzipped: always send `Content-Encoding: gzip`.
- Map `/` to `/index.html`; fall back to `/index.html` for unknown non-`/api/`
  paths (hash router + captive portal UX).
- Use `kWebuiBuildTag` as the `ETag` and honor `If-None-Match` with 304.
- `GET /api/events` carries no auth header (EventSource cannot set one) —
  it must stay a read-only route.

## Budget

Contract limit: total gzipped payload < 120 KB. Current build: ~19 KB
(`python3 tools/build_webui.py --check` prints the per-asset table).
