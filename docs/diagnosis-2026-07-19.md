# Blank-Panel Root Cause Analysis — 2026-07-19

Session: local Claude Code, controller attached at `/dev/cu.usbmodem101`.
Method: full review of repo evidence, live serial capture from the running
device, factory app0 string analysis, and a 28-photo forensic audit of
`hardware/photos/diagnostic-session/` (11 vision agents plus an adversarial
verification pass over the power question).

## Verified current facts (this session)

- The device currently boots the restored factory application from flash.
  Live serial capture over USB-CDC shows a clean `SPI_FAST_FLASH_BOOT`, the
  Arduino `Preferences` NVS read (`nets NOT_FOUND` at ~1.7 s), and no crash or
  reset over 60 s. Opening the CDC port resets the chip (`rst:0x15`).
- The factory app0 image embeds the same mrcodetastic
  `ESP32-HUB75-MatrixPanel-I2S-DMA` library used by our diagnostics (strings:
  `.pio/libdeps/mini-v1-secure/ESP32 HUB75 LED MATRIX PANEL DMA Display/...`),
  and it logs `MatrixPanel_I2S_DMA::begin() failed!` at error level on init
  failure. Error-level logging is demonstrably enabled (the Preferences error
  prints), and no such failure appears — the factory display driver
  initializes and runs.
- Photo `ABCAA2EF-...-1` (2026-07-17 19:34 EDT) shows the panel lit and
  rendering a clock: the complete chain (5 V, ribbon, panel, controller)
  worked that evening, immediately before the wrong-firmware flash.
- Photo `14411024-...-1` (2026-07-18 18:40 EDT, ~1 h before the smoke and raw
  observations): the ribbon's panel-end IDC socket on the `J1`/`HUB75-`
  input header is mated to only one of the two pin rows — a full row of 8
  bright unmated pins is exposed immediately beside the connector body.
  In that state no HUB75 signal reaches the panel regardless of power.
- Photos `14411024-...-2` and `70259BBE-...-1`: the panel's 4-pin power plug
  was unmated (dangling, socket empty) at multiple points; other frames
  (`E0EEE97E-...-6`, `14411024-...-6/7/10`) show it seated. The plug state
  changed repeatedly during handling.
- Adversarially verified across all 28 photos: **no external 5 V supply is
  connected in any photographed frame.** The supply-side leads end in bare
  stripped wire in `14411024-...-8`; the unit's black USB-A supply lead is
  held in-hand connected to nothing in `F93182EC-...-4`; the widest bench
  shot (`14411024-...-6`) contains no adapter or brick anywhere. In every
  frame where the controller is powered, the only source is the Mac's USB
  (AINOPE braided cable), which powers controller logic.
- No damage of any kind is visible on the controller, panel, ribbon,
  connectors, or wiring in any of the 28 photos (checked per-photo).
- Panel identification (photo-verified): `(2121)2L-128x64-7258-32S-V1.7`,
  FM6124E/FM6124EJ column drivers (lot `A012527`), 1/32 scan, `HUB75-E`
  input. Controller output stage near 75EX1 uses ~20-pin bus-driver ICs
  (245/16211-class; topmarks not fully legible).

## Conclusion

**The blank panel is explained by reassembly state, not by firmware and not
by damaged hardware.** Every blank observation on 2026-07-18/19 (smoke
artifact, DMA config variants, raw HUB75 diagnostic, restored factory app)
was made with at least one of the following true, and probably more than one:

1. no external 5 V feeding the screw terminals (verified absent in every
   photographed moment — USB powers only the controller);
2. the panel-end ribbon half-mated on `J1` (verified in the last photo taken
   before the test window);
3. the panel's 4-pin power plug unseated (verified at several moments).

The firmware side is exhausted and healthy: the factory image is restored
byte-exact, boots, and initializes its HUB75 driver without error. The
original post-flash darkness was expected behavior of the wrong-target
TheFlightWall_OSS image; everything after that is accounted for by the three
conditions above, which were introduced during teardown and handling.

## Recovery procedure (no tools, no flashing required)

1. Disconnect the Mac USB cable from the WF2.
2. Reseat the ribbon at the panel's `J1`/`HUB75-` input header so the IDC
   socket grips **both** pin rows, centered — afterwards no shiny pin row may
   be visible beside the connector. Reference of the wrong state:
   `hardware/photos/diagnostic-session/14411024-...-1-Photo-1.jpg`.
   Leave the other end on `75EX1` (verified correctly seated).
3. Seat the white 4-pin power plug in the panel `POWER` header
   (black/black/red/red = GND/GND/VCC/VCC) until the latch engages.
4. Reconnect the unit's original power lead(s) to the original wall
   adapter exactly as factory-wired — not to a computer.
5. Power on. The restored factory firmware should render the loading
   clock/FlightWall content within seconds, as it did on 2026-07-17 19:34.

## Contingency (only if still dark after the procedure above)

- `hd_wf2_raw_hub75_x2_diagnostic` (this branch,
  `targets/hd-wf2-raw-hub75-x2/`) is a build-verified variant of the raw
  diagnostic using the 75EX2 RGB pins (R1/R2 4/5, G1/G2 8/9, B1/B2 12/13;
  shared A–D/CLK/LAT/OE; X2's E line is unrouted per the community pin map,
  so a 1/32-scan panel shows row-duplicated output — any light at all is the
  pass signal). Moving the ribbon to 75EX2 and flashing it isolates the
  controller's 75EX1 output-buffer path from panel/ribbon/power.
- If X2 also shows nothing with verified power and seating, the remaining
  suspects are the ribbon cable itself (substitute a known-good straight
  16-pin IDC cable) and panel electronics.
