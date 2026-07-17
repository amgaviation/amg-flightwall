# Delivery Roadmap

Phases are evidence gates, not calendar promises. A later phase may prototype
behind interfaces, but it cannot be called complete while an earlier safety or
recovery gate is open.

## Phase 1 — Foundation (in progress)

Delivered here: project boundary, architecture, development/build/recovery
guides, hardware evidence record, roadmap, and decision log.

Exit evidence still required:

- approved standalone repository, owners, license, threat model, and CI policy;
- physical hardware inspection and validated original-firmware restore;
- approved target/toolchain decision and Classic Mode behavior capture;
- initial requirements traceability matrix.

## Phase 2 — Firmware platform

Build the hardware abstraction, renderer, scene manager, configuration store,
plugin registry, logging, health model, and simulator. Exit requires host tests,
target build, renderer snapshots, memory budget, watchdog/fault tests, and an
architecture review on an approved development device.

## Phase 3 — Classic Mode

Implement live aircraft, callsign/tail/altitude/speed/distance, airport filters,
arrivals/departures, airline styling, brightness, and approved reference timing.
Exit requires a signed comparison matrix against captured original behavior,
provider degradation tests, measured CPU/memory/frame timing, and soak results.

## Phase 4 — Operations Mode

Add only approved modules, beginning with read-only operational summaries and
clearly stale data. Exit requires module-level access/privacy review, failure
states, source attribution, data-retention decisions, and operational user tests.

## Phase 5 — Browser configuration

Deliver responsive authenticated configuration, live preview, theme/plugin
controls, diagnostics, safe log export, and reset/recovery controls. Exit
requires accessibility, session/CSRF, authorization, redaction, responsive,
concurrency, and interrupted-save tests.

## Phase 6 — Networking

Productionize provisioning, Wi-Fi recovery, time sync, certificate validation,
provider quotas/backoff, and optional local ADS-B behind ports. Exit requires a
threat model, captive/offline tests, certificate rotation, and long-loss soak.

## Phase 7 — AMG Connect integration

Do not implement until authenticated API contracts, authorization scopes,
privacy classification, rate limits, sandbox environment, and owner approval
are verified. Use versioned adapters and never embed portal credentials.

## Phase 8 — Polish

Profile and optimize measured bottlenecks only. Record before/after CPU, memory,
power, temperature, frame timing, and network behavior on named hardware.

## Phase 9 — Production readiness

Require reproducible signed releases, SBOM/vulnerability process, rollback and
disaster-recovery drills, hardware qualification, manufacturing/provisioning
records, support runbooks, telemetry/privacy policy, contributor onboarding,
and release approval.

## Cross-phase acceptance ledger

Each capability is tracked as `not started`, `prototype`, `verified`, or
`production approved`; “verified” includes the test target and evidence link.
No planning assumption is converted to a verified fact by implementation alone.
