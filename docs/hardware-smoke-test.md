# FlightWall Mini Hardware Smoke Test

## Current status

**Verified Current Fact:** The portable smoke-test scene renders a 128×64
progress screen in the host simulator. It has rows for host tests, embedded
build/runtime, local factory-backup hash, hardware-test state, and source-gate
evidence. A running state pulses every 500 ms so a hardware observer can
distinguish an active render loop from a frozen frame.

The generic simulator frame leaves every evidence row at `WAIT`. The embedded
composition changes only `BUILD` and `HW TEST` to pulsing `RUN`: executing that
code demonstrates an active target build and render loop. It deliberately does
not embed `CORE PASS`, `HASH PASS`, or source-gate `PASS`, because the device
cannot attest the host checks, ignored backup, or PlatformIO command policy.

**Verified Current Fact:** On 2026-07-18, after an explicit owner override of
the no-spare-controller gate, the reviewed smoke artifact was written to the
active `app0` partition at `0x10000`. Esptool verified the write, an exact
post-boot readback matched artifact SHA-256
`ab5530027f2ed54c628019e711de14a8702946c08171601c0dfa59ca0017fb92`,
and serial output reached `AMG FlightWall: hardware smoke screen initialized`.
The factory `app1` partition and every non-`app0` region were preserved. A
120-second serial watch recorded no unexpected reset after initialization.

**Verified Current Fact:** This proves boot and display-adapter initialization,
not correct panel mapping, orientation, color order, brightness, refresh
stability, or long-run compatibility. The initial visual acceptance later
failed as recorded below.

**Verified Current Fact:** The owner observed that smoke artifact as completely
blank. Multiple single-variable DMA configuration tests were also blank. The
complete saved factory `app0` slice was then restored at `0x10000`, verified
against SHA-256
`ed3f4bc8d26c32ec674a5285882e34ec67c55ed89b88bcd3b0f3b84e1d9581b4`,
and esptool issued a hard reset. The owner observed the panel as blank after a
full power cycle, but no factory serial marker was captured. This validates the
app0 restore/write path; factory application execution and display behavior
were not independently established.

**Verified Current Fact:** A standalone raw HUB75 diagnostic was built without
the MatrixPanel library or upstream FlightWall code. It directly programs the
FM6124E/FM6126-family control words and scans the factory-binary-derived X1 pin
profile at 128×64 and 1/32 scan. The reviewed 263,696-byte artifact has
SHA-256
`732196319ebbfb488c7ecbe1ca0c32c1c44553ce506c9e6155782f3eddc20f66`.
It was written only to `app0` at `0x10000`, independently read back with the
same hash, and produced continuing serial pattern and frame heartbeats. The
owner observed no light, color, line, or flicker. This is a failed visual
hardware test and is recorded in
`backups/manifests/flightwall-mini-hd-wf2-raw-hub75-diagnostic-20260719.json`.

**Verified Current Fact:** The independent raw scan eliminates the FlightWall
application, renderer, DMA library, network, and API code from the active test
path. It does not prove that 5 V reaches the panel or that HUB75 signals reach
the panel connector. Those electrical measurements are now the next required
evidence; additional firmware variants cannot distinguish the remaining power,
continuity, level-shifter, and panel-electronics hypotheses.

**Verified Current Fact:** The inspected controller most recently enumerated as
`/dev/cu.usbmodem11201`. The path has changed after reconnects and must always
be re-identified before an approved hardware procedure.

**Verified Current Fact:** `esptool load_ram` can execute a specially built RAM
image without writing flash, according to
[Espressif's ESP32-S3 esptool documentation](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/esptool/advanced-commands.html#load-a-binary-to-ram-load-ram).
That documentation requires IRAM/DRAM-only segments. The current Arduino/HUB75
firmware image is 287,408 bytes and `esptool image_info` reports DROM and IROM
segments in addition to DRAM/IRAM. It is therefore not eligible for that
command. No RAM-only AMG image exists yet.

## Compile-only preflight

With the isolated PlatformIO 6.1.19 environment active:

```sh
./scripts/hardware-smoke-preflight.sh /dev/cu.usbmodem101
```

The preflight verifies the ignored local factory-backup hash, runs all host
tests and conservative simulator outputs, compiles and hashes the HD-WF2 image,
and confirms that `upload`, `uploadfs`, `program`, and `erase` remain blocked.
It detects but does not open the optional serial path. Its terminal output is
the evidence record; the firmware screen does not inherit or claim those
external results. A successful preflight is not authorization to write the
controller.

## Hardware activation record and remaining gate

**Verified Current Fact:** The owner explicitly directed activation without a
spare controller after the untested-restore risk was reported. Before the
write, a second full 8 MB snapshot was captured and all immutable regions
matched the first snapshot. An app0-only restore image was extracted. The
activation manifest is
`backups/manifests/flightwall-mini-hd-wf2-smoke-activation-20260718.json`.

**Recommendation:** Do not treat this exception as the production activation
policy. Full restoration is still untested, and electrical/pin-profile review
is still incomplete. Do not add or enable a generic upload target until those
gates are closed.

Before adding any reusable activation workflow, require an explicit serial port
and artifact hash, preserve the generic write block, and provide a time-bounded
restore procedure.

## Visual acceptance record

Complete the visual record with photos/video for:

- all five rows readable and in the expected order;
- blue header, amber pulsing `BUILD RUN` and `HW TEST RUN`, conservative
  `WAIT` states for external evidence, and no red state;
- correct orientation and no mirrored, wrapped, missing, or duplicated regions;
- stable refresh for at least two minutes at conservative brightness;
- a successful power cycle;
- later restoration of the factory image and factory behavior comparison.

Any blank panel, scrambled mapping, unexpected color, unstable refresh, or
reset is a failed test. Heat, odor, or a power anomaly also requires immediate
power-off. The blank-panel failure above must remain unresolved until panel
voltage/current and HUB75 continuity are measured.

## Open decision

**Open Decision:** Approve a meter-equipped electrical test fixture and measure
panel-side 5 V under load before any more display firmware experiments. A full
factory-image restoration rehearsal, production release, and generic flashing
remain gated.
