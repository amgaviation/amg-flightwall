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

**Verified Current Fact:** The HD-WF2 compile-only profile boots into the same
smoke scene in its generated firmware image. Compilation does not verify panel
pin mapping, orientation, color order, FM6124E initialization, brightness,
refresh stability, or runtime compatibility.

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

## Hardware activation gate

**Recommendation:** Do not execute AMG firmware on the sole factory controller
while restoration remains untested. Hardware activation requires a compatible
spare HD-WF2/development controller on which full backup restoration has been
rehearsed, plus electrical/pin-profile approval. A future RAM-only image may be
compiled and reviewed before that point, but RAM execution does not bypass the
recovery and electrical gates.

After those prerequisites are verified, add a separate, narrowly scoped
activation change. It must require an explicit serial port and artifact hash,
preserve the generic write block, and provide a time-bounded restore procedure.

## Visual acceptance record

When hardware activation is approved, record photos/video and serial output for:

- all five rows readable and in the expected order;
- blue header, amber pulsing `BUILD RUN` and `HW TEST RUN`, conservative
  `WAIT` states for external evidence, and no red state;
- correct orientation and no mirrored, wrapped, missing, or duplicated regions;
- stable refresh for at least two minutes at conservative brightness;
- successful power cycle and successful restoration of the factory image;
- final factory behavior comparison after restoration.

Any blank panel, scrambled mapping, unexpected color, unstable refresh, reset,
heat, odor, or power anomaly is a failed test and requires immediate power-off.

## Open decision

**Open Decision:** Select and approve the spare compatible controller and the
electrical test fixture used for the first activation and restoration rehearsal.
