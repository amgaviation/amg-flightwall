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

## AMG FlightWall Mini inspection — 2026-07-17

**Verified Current Fact:** Owner-supplied photographs identify the installed
controller as an HD-WF2 with a `V7.2.0-2` board marking. The owner states this
FlightWall Mini is factory-unmodified.

**Verified Current Fact:** The factory power path powered the display and the
display rendered coherent content. This verifies basic power-up only; current,
voltage stability, thermal behavior, pixel mapping, and long-run reliability
were not measured.

**Verified Current Fact:** A direct USB-A-to-USB-C attempt did not enumerate a
new USB device on the connected Mac. No firmware read, write, erase, flash, or
backup occurred.

**Planning Assumption:** A 128×64 host profile is being used for layout work
while the exact active pixel geometry, scan mode, orientation, HUB75 mapping,
controller MCU, and flash organization remain pending measurement.

**Recommendation:** Keep software work on the host simulator until the factory
firmware is captured, hashed, and its restore path is validated. Do not connect
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
- Whether the existing controller can safely support A/B OTA partitions.
- Required brightness/current limiting and thermal derating policy.
- Whether a companion computer is required for Operations Mode or web UI.
- Regulatory, installation, and serviceability requirements by sales region.
