# AMG FlightWall Portable Core

This directory contains the greenfield product core, host simulator, and an
experimental compile-only HD-WF2 display adapter. It contains no flash command.

## Implemented

- abstract display port and bounds-safe RGB framebuffer;
- pixel, line, rectangle, fill, and 5×7 text rendering;
- scene lifecycle and Classic/Operations sample scenes;
- deterministic Classic/Operations/Auto mode selection;
- manifest validation and duplicate/API checks for statically linked plugins;
- fixed-capacity, versioned configuration validation with opaque Wi-Fi profile
  key/revision references;
- bounded Wi-Fi retry/backoff, disconnect actions, and explicit reprovisioning
  policy;
- fixed-size, typed health diagnostics without free-text payloads;
- 128×64 hardware smoke-test scene with typed progress states and a pulsing
  runtime indicator;
- 128×64 simulator output and dependency-free tests;
- build-only ESP32-S3/HD-WF2 adapter with a fixed-size shadow framebuffer,
  source-derived HUB75 pin profile, FM6124 initialization, conservative
  brightness, and four-bit DMA color depth with PSRAM disabled.

## Boundaries

The sample scenes use synthetic data. The inspected FlightWall Mini has a
128×64 1/32-scan panel and an ESP32-S3 controller with 8 MB flash, but its
mapping, orientation, color order, refresh behavior, and driver integration
remain unmeasured. No network adapter, credential store, persistence layer,
provider, web configuration, OTA, or production security behavior is claimed.

The embedded environment intentionally blocks PlatformIO `upload`, `uploadfs`,
`program`, and `erase` targets. It is not authorized for the factory controller
until the recovery image has been restored successfully on a dedicated
development unit.
