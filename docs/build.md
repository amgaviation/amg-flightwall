# Build Guide

## Current status

**Verified Current Fact:** This foundation contains no AMG firmware source and
therefore intentionally has no firmware build command yet.

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

## Planned commands

These names are recommendations, not currently implemented commands:

```sh
./scripts/bootstrap.sh
./scripts/check.sh
./scripts/build.sh simulator
./scripts/build.sh <approved-hardware-id>
./scripts/package-release.sh <approved-hardware-id>
```

The scripts should be non-interactive, safe to rerun, and refuse production
signing without an authorized isolated signer.

## Flashing gate

No generic flash command is documented because the controller and recovery path
are unverified. Add it only after an authorized backup and a successful restore
on a sacrificial or dedicated development unit.
