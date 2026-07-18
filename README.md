# AMG FlightWall

AMG FlightWall is a greenfield, commercial-quality display platform for classic
FlightWall functionality and future AMG Aviation operations workflows. This is
the standalone project repository; the AMG website remains context only and is
not a firmware dependency.

> **Status:** The portable Phase 2 core and 128×64 host simulator are prototypes.
> Host build/tests and generated preview frames are verified on Apple Clang 17.
> The experimental HD-WF2 target compiles but has not run on hardware. Hardware
> compatibility, flashing, OTA, external APIs, and target performance remain
> unverified.

## Project rules

- The upstream FlightWall firmware is reference material, not a codebase to edit.
- `firmware/flightwall-original/` must remain read-only if upstream source is
  vendored in a future, separately reviewed change.
- New product code belongs under `firmware/amg-flightwall/` and communicates
  through narrow interfaces.
- Credentials and device-specific configuration must never be committed.
- Every engineering record labels statements as **Verified Current Fact**,
  **Planning Assumption**, **Recommendation**, or **Open Decision**.

## Current evidence

The public [AxisNimble/TheFlightWall_OSS](https://github.com/AxisNimble/TheFlightWall_OSS)
repository was reviewed at commit `e22aec2ed83056898fadddd178a617b99694da9d`
on 2026-07-17. The observations captured in
[`docs/hardware.md`](docs/hardware.md) and
[`docs/architecture.md`](docs/architecture.md) are limited to that revision.
It is licensed under Apache License 2.0. No upstream source is copied here.

The AMG implementation now includes a dependency-free C++17 core with a display
port, framebuffer, renderer, scene lifecycle, Classic/Operations scenes,
deterministic Auto mode, plugin manifest registry, versioned configuration
validation, a portable Wi-Fi supervision policy, and typed health diagnostics.
The host simulator does not connect to or modify a FlightWall controller.

## Build the current prototype

Requirements: a C++17 compiler, POSIX shell, and GNU Make.

```sh
./scripts/check.sh
./scripts/build.sh simulator
```

Simulator frames are written to `build/host/artifacts/`. Build output is ignored
and must not be committed.

## Documentation map

| Document | Purpose |
| --- | --- |
| [`docs/architecture.md`](docs/architecture.md) | Target layers, boundaries, and runtime model |
| [`docs/development.md`](docs/development.md) | Contributor workflow and quality gates |
| [`docs/build.md`](docs/build.md) | Reproducible build plan and current limitations |
| [`docs/hardware.md`](docs/hardware.md) | Evidence-backed hardware baseline and unknowns |
| [`docs/recovery.md`](docs/recovery.md) | Recovery and rollback design requirements |
| [`docs/roadmap.md`](docs/roadmap.md) | Gated delivery phases and acceptance evidence |
| [`docs/decision-log.md`](docs/decision-log.md) | Decisions and unresolved choices |

## Repository layout

The directories are contracts for future work, not claims of implemented
functionality. Each subsystem contains a short scope file so contributors do
not mistake an empty directory for a completed component.

```text
amg-flightwall/
├── firmware/
│   ├── flightwall-original/  # immutable upstream snapshot location
│   └── amg-flightwall/       # greenfield device application
├── display/                  # display-driver interfaces and implementations
├── renderer/                 # device-independent drawing primitives
├── network/                  # connectivity and transport adapters
├── weather/                  # future weather plugin
├── flights/                  # flight-domain models and provider adapters
├── web-config/               # future local configuration client
├── ota/                      # signed update and rollback subsystem
├── plugins/                  # plugin contracts and first-party plugins
├── themes/                   # validated theme packages
├── assets/                   # licensed, optimized product assets
├── hardware/                 # schematics, BOMs, and hardware revisions
├── tools/                    # developer tools
├── scripts/                  # deterministic automation
├── tests/                    # host, integration, hardware-in-loop tests
└── backups/                  # manifests/instructions only; no secrets or dumps
```

## Before hardware activation

Compile-only adapter development may proceed without connecting to or writing
the controller. Uploading, programming, erasing, runtime compatibility claims,
and production target approval remain gated on the following:

1. Identify the exact production controller, panel chipset, wiring topology,
   power design, and recovery interface from physical inspection.
2. Approve the target hardware adapter, flash layout, and embedded toolchain.
3. Capture an authorized original-firmware backup and validate restoration on
   non-production hardware.
4. Record Classic Mode behavior with an explicit visual and timing test matrix.
5. Resolve the open security, OTA signing, licensing, and API-provider decisions.
