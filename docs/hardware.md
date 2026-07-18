# Hardware Baseline

## Verified current facts

The following facts are verified only from the public upstream repository at
commit `e22aec2ed83056898fadddd178a617b99694da9d`; they do not prove the
configuration of any AMG-owned or commercial FlightWall unit.

- The documented open-source example uses twenty 16×16 LED panels arranged
  ten wide by two high, for a configured 160×32 pixel matrix.
- The guide names an ESP32 R32 D1 development board, while saying other ESP32
  boards may work. That statement has not been validated for AMG hardware.
- The guide calls for a 5 V supply rated above 20 A and a 3.3 V-to-5 V level
  shifter. The repository does not provide certification, thermal, fuse,
  conductor, connector, or power-injection validation.
- The firmware configures a single display data pin (GPIO 25) and a serial
  monitor rate of 115200 baud.
- The repository includes bracket STL files, a wiring image, and an assembly
  photograph. Images are reference documentation, not an electrical schematic.

## Planning assumptions

- A 160×32 reference target may be useful for initial renderer simulation.
- An ESP32-class controller may remain a target, but no exact board, flash
  layout, secure-boot capability, or performance envelope is approved.
- The display may be a serial/addressable LED topology with a low practical
  refresh rate; this needs measurement on the actual product.

## AMG FlightWall Mini inspection — 2026-07-18

**Verified Current Fact:** Owner-supplied photographs identify the installed
controller as an HD-WF2 with a `V7.2.0-2` board marking. The owner states this
FlightWall Mini is factory-unmodified.

**Verified Current Fact:** The factory power path powered the display and the
display rendered coherent content. This verifies basic power-up only; current,
voltage stability, thermal behavior, pixel mapping, and long-run reliability
were not measured.

**Verified Current Fact:** The controller enumerates on macOS as Espressif's
native USB JTAG/serial interface (`VID 303A`, `PID 1001`) at
`/dev/cu.usbmodem101`. It requires no CH340, CP210x, or FTDI driver.

**Verified Current Fact:** Read-only chip identification reports an
ESP32-S3 (QFN56), revision 0.2, with Wi-Fi, Bluetooth LE 5, dual main cores,
an LP core, a 40 MHz crystal, native USB-Serial/JTAG mode, and 8 MB of quad
SPI flash. The controller reports no PSRAM capacity fuse.

**Verified Current Fact:** Read-only eFuse inspection reports Secure Boot and
flash encryption disabled. USB Serial/JTAG and USB download mode are enabled.
These settings describe the inspected factory controller only; they are not a
production security approval.

**Verified Current Fact:** A complete 8,388,608-byte read-only flash snapshot
was captured on 2026-07-18. Its SHA-256 is
`1160af2e1da2ea57baf6a7833cf0121812a576ea2cef29440f5f5ab612299f6a`.
The binary is deliberately local and ignored by Git. No erase, write, or flash
operation was issued.

**Verified Current Fact:** The snapshot's partition table defines `nvs`
(`0x9000`, 20 KB), `otadata` (`0xE000`, 8 KB), two application slots—`app0`
(`0x10000`, 2.75 MB) and `app1` (`0x2D0000`, 2.75 MB)—`factoryprov`
(`0x590000`, 4 KB), and `spiffs` (`0x591000`, approximately 2.43 MB). This is
an A/B-capable partition layout, but it does not prove the vendor's OTA
implementation or rollback behavior.

**Verified Current Fact:** The display panels are marked as 128×64,
1/32-scan hardware and visibly use FM6124E driver ICs. The exact pixel order,
orientation, color order, HUB75 pin mapping, and refresh behavior remain
unmeasured.

**Planning Assumption:** A 128×64 host profile is appropriate for Mini layout
work. Orientation, color order, HUB75 mapping, and refresh behavior remain
pending measurement.

**Planning Assumption:** The build-only HD-WF2 adapter uses the pin order
published by WLED at commit `a962116f54bc3b62db46013849f0bf5b3ebabb73`:
R1/G1/B1 `2/6/10`, R2/G2/B2 `3/7/11`, A/B/C/D/E
`39/38/37/36/21`, LAT/OE/CLK `33/35/34`. This profile compiles and agrees with
the controller family, but it has not been electrically verified on this Mini.
See [WLED's HD-WF2 definition](https://github.com/wled/WLED/blob/a962116f54bc3b62db46013849f0bf5b3ebabb73/wled00/bus_manager.cpp#L870-L876).

**Planning Assumption:** The experimental target disables PSRAM because the
eFuse report exposes no PSRAM capacity and the maintained WLED HD-WF2 profile
also disables it. This is a conservative build configuration, not proof that no
external PSRAM package exists on every HD-WF2 revision.

**Recommendation:** Continue host simulation and compile-only adapter work, but
limit controller interaction to read-only inspection until restoration has
succeeded on a sacrificial or dedicated development controller. Do not connect
wall power and computer USB simultaneously during controller investigation.

## Required inspection record

Before connecting, flashing, or powering development hardware, record:

| Area | Required evidence |
| --- | --- |
| Controller | board photos, markings, chip/flash IDs, bootloader and pin map |
| Display | panel part number, LED/driver chipset, tile mapping, data levels |
| Power | PSU label, measured rails, injection points, fuse/wire/connector ratings |
| Interfaces | USB/UART/JTAG/boot pins and safe voltage levels |
| Firmware | authorized backup hash, partition table, boot logs, restore test |
| Mechanical | dimensions, airflow, enclosure materials, service access |
| EMC/thermal | operating temperature measurements and applicable requirements |

## Safety boundary

**Recommendation:** Use a current-limited bench supply and an electrically
reviewed test fixture for first power-on. Do not infer safe power wiring from
the reference illustration. Do not flash the only known-good controller. A
qualified engineer must approve mains wiring, grounding, overcurrent protection,
thermal limits, and installation requirements.

## Open decisions

- Exact production hardware revision and supported variants.
- Whether the existing controller's observed A/B partition layout can safely
  support AMG-signed OTA updates and rollback.
- Required brightness/current limiting and thermal derating policy.
- Whether a companion computer is required for Operations Mode or web UI.
- Regulatory, installation, and serviceability requirements by sales region.
