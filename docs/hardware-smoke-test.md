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
stability, or long-run compatibility. Visual acceptance remains pending the
owner's panel observation.

**Verified Current Fact:** The inspected controller is currently visible on the
development Mac as `/dev/cu.usbmodem101`. The path may change after reconnect or
reboot and must be re-identified before any approved hardware procedure.

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

Any blank panel, scrambled mapping, unexpected color, unstable refresh, reset,
heat, odor, or power anomaly is a failed test and requires immediate power-off.

## Open decision

**Open Decision:** Approve the electrical test fixture and schedule a controlled
factory-restoration rehearsal. The first activation no longer depends on a
spare controller, but production release and generic flashing remain gated.
