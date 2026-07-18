# AMG FlightWall Portable Core

This directory contains the greenfield, host-buildable product core. It does not
contain a hardware driver or flash command.

## Implemented

- abstract display port and bounds-safe RGB framebuffer;
- pixel, line, rectangle, fill, and 5×7 text rendering;
- scene lifecycle and Classic/Operations sample scenes;
- deterministic Classic/Operations/Auto mode selection;
- manifest validation and duplicate/API checks for statically linked plugins;
- 128×64 simulator output and dependency-free tests.

## Boundaries

The sample scenes use synthetic data. The inspected FlightWall Mini has a
128×64 1/32-scan panel and an ESP32-S3 controller with 8 MB flash, but its
mapping, orientation, color order, refresh behavior, and driver integration
remain unmeasured. No network provider, storage, web configuration, OTA, or
production security behavior is claimed.
