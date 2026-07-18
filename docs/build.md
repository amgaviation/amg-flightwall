# Build Guide

## Current status

**Verified Current Fact:** The repository contains a portable C++17 core, host
framebuffer, renderer, application/mode controller, two prototype scenes,
plugin registry, simulator, and dependency-free tests.

**Verified Current Fact:** `./scripts/check.sh` builds with Apple Clang 17 using
strict warnings, runs seven deterministic tests, runs the simulator, and checks
that Classic and Operations PPM frames were produced.

**Verified Current Fact:** The inspected Mini controller is an ESP32-S3 with
8 MB of quad SPI flash and no PSRAM capacity fuse. Its factory snapshot uses
two 2.75 MB application slots plus a SPIFFS partition. The source and hash of
that local recovery snapshot are recorded in `docs/hardware.md`; the binary is
not committed to this repository.

**Planning Assumption:** The simulator's 128×64 geometry is useful for Mini
layout development. It does not prove mapping, orientation, color order,
refresh behavior, or the compatibility of a new rendering driver.

**Verified Current Fact:** The inspected open-source reference uses PlatformIO
with the `espressif32` platform, `esp32dev` board, Arduino framework, and Unity
test framework. Its declared libraries include FastLED, Adafruit GFX,
FastLED NeoMatrix, and ArduinoJson.

**Planning Assumption:** PlatformIO may accelerate a reference proof of concept,
but it is not approved as the production toolchain until hardware, dependency
pinning, reproducibility, secure-boot, flash-layout, and OTA needs are evaluated.

## Proposed reproducibility requirements

Before adding application code, the approved build must:

1. Pin the compiler, platform, framework, libraries, and build container by
   immutable versions/digests.
2. Generate a version manifest containing source revision, toolchain versions,
   target hardware ID, configuration schema, and artifact hashes.
3. Separate debug, simulator, hardware-test, staging, and production profiles.
4. Inject secrets after compilation only where unavoidable; prefer device
   provisioning so release artifacts contain no service credentials.
5. Produce signed artifacts and a software bill of materials in CI.
6. Fail on warnings selected by the coding standard, static-analysis failures,
   test failures, oversized partitions, or secret-scan findings.
7. Support a clean offline rebuild from an approved dependency cache.

## Implemented commands

```sh
./scripts/check.sh
./scripts/build.sh simulator
```

The scripts are non-interactive and safe to rerun. They create only ignored host
artifacts under `build/host/`.

## Planned target commands

No embedded target, flash, package, or signing command exists yet. Those
commands require an approved controller profile, toolchain lock, partition
layout, recovery procedure, and isolated signing design.

## Flashing gate

No generic flash command is documented. The factory controller is now backed
up, but restoration has not been validated. Add a target build, package,
signing, or flash command only after a successful restoration test on a
sacrificial or dedicated development unit.
