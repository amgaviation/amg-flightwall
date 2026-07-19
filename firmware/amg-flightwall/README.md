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

The sample scenes use synthetic data. The 128×64 profile is a planning
assumption. No controller, panel, network provider, storage, web configuration,
OTA, or production security behavior is claimed.
