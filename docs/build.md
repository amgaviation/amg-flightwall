# Build Guide

## Current status

**Verified Current Fact:** The repository contains a portable C++17 core, host
framebuffer, renderer, application/mode controller, two prototype scenes,
plugin registry, configuration validation, Wi-Fi supervision policy, typed
health registry, simulator, and dependency-free tests.

**Verified Current Fact:** `./scripts/check.sh` builds with Apple Clang 17 using
strict warnings, runs 25 deterministic tests, runs the simulator, and checks
that Classic, Operations, and hardware-smoke PPM frames were produced.

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

**Verified Current Fact:** The experimental HD-WF2 profile pins PlatformIO
6.1.19, Espressif32 6.12.0, and the HUB75 driver to commit
`f17fb7fe9d487e9643f919eb5aeedea8d9d1f8d7`. Transitive tool packages and the CI
runner image are not yet locked by content digest, so this is not the approved
production/offline-reproducible toolchain described below.

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
./scripts/build.sh hd-wf2
```

The scripts are non-interactive and safe to rerun. Host artifacts are written
under `build/host/`; embedded build artifacts are written under PlatformIO's
ignored `.pio/` directory.

The `hd-wf2` command compiles only. Its PlatformIO safety gate rejects `upload`,
`uploadfs`, `program`, and `erase` targets. The Espressif platform and HUB75
driver are pinned, and the partition CSV mirrors the read-only factory
snapshot.

**Verified Current Fact:** The current smoke-screen target compiles to 287,045
bytes of application flash usage and 43,528 bytes of RAM usage under the
PlatformIO size report. The generated flash image is 287,408 bytes and contains
flash-mapped DROM/IROM segments, so it is not a RAM-loadable image.

## Planned target commands

No flash, package, or signing command exists yet. Those commands require a
validated restoration procedure and isolated signing design.

## Flashing gate

No generic flash command is documented. The factory controller is now backed
up, but restoration has not been validated. Add packaging, signing, hardware
activation, or flash commands only after a successful restoration test on a
sacrificial or dedicated development unit.
