# HD-WF2 Raw HUB75 Diagnostic

This is an isolated, standalone electrical/display diagnostic for the inspected
FlightWall Mini. It uses Arduino only and does not link the MatrixPanel library,
the portable AMG application core, or the upstream FlightWall firmware.

The target drives the factory-binary-derived X1 pin profile directly:

- RGB1: GPIO 2/6/10;
- RGB2: GPIO 3/7/11;
- row A/B/C/D/E: GPIO 39/38/37/36/21;
- LAT/OE/CLK: GPIO 33/35/34;
- 128 x 64 logical pixels, 1/32 scan.

At boot it is intended to program the FM6124E/FM6126-family control words, then
continuously scan solid red, green, and blue screens, a white checkerboard,
RGBW columns, a large `AMG` screen, and row-address bands. The output-enable
pulse is kept short to limit average current during diagnosis.

The inspected controller executed the scan loop and emitted continuing serial
heartbeats, but the owner observed a completely blank panel. That failed visual
result is recorded in `docs/hardware-smoke-test.md`; this target is diagnostic
evidence, not a claim of electrical compatibility.

Compile without writing a device:

```sh
pio run --project-dir firmware/amg-flightwall \
  --environment hd_wf2_raw_hub75_diagnostic
```

The existing safety gate rejects generic `upload`, `program`, and `erase`
targets. Building this environment does not alter the production adapter or
flash hardware.
