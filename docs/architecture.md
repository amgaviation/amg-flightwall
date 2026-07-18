# Architecture

## Status legend

- **Verified Current Fact** — supported by inspected source or documentation.
- **Planning Assumption** — useful for planning but requires validation.
- **Recommendation** — proposed engineering direction, not implemented fact.
- **Open Decision** — requires owner approval or additional evidence.

## Reference baseline

**Verified Current Fact:** At upstream commit
`e22aec2ed83056898fadddd178a617b99694da9d`, the open-source reference uses an
ESP32 Arduino PlatformIO environment, a FastLED NeoMatrix display adapter,
OpenSky state vectors, FlightAware AeroAPI enrichment, and compile-time headers
for Wi-Fi, API, location, display, and timing settings.

**Verified Current Fact:** The reference already has small interfaces for state
vector, flight detail, and display providers. Its entry point nevertheless owns
connectivity, scheduling, object construction, diagnostics, fetching, and
rendering, and it uses global/static lifetime objects.

**Verified Current Fact:** The reference configuration enables insecure TLS for
two services. That is unsuitable as a production security baseline and is not
carried forward here.

## Target boundaries

**Recommendation:** Use a ports-and-adapters architecture with dependencies
pointing inward:

```text
Hardware drivers -> Display port -> Renderer -> Scene manager
                                          ^          |
Data providers -> Domain services -> Plugin manager -+
       ^                  ^              ^
Network/time        Configuration   Application/mode controller
       ^                  ^              ^
Diagnostics       Local web config   OTA/recovery supervisor
```

| Layer | Owns | Must not own |
| --- | --- | --- |
| Hardware | GPIO, buses, storage partitions, watchdog | flight or UI policy |
| Display | framebuffer/pixel output, brightness capability | scenes or provider calls |
| Renderer | primitives, text measurement, clipping | network or persistent settings |
| Scene manager | scene lifecycle, transitions, layout scheduling | credentials or HTTP |
| Plugins | declared capabilities and bounded feature logic | direct global hardware access |
| Domain | normalized aircraft, mission, alert, weather models | provider JSON types |
| Network | Wi-Fi state, TLS transport, time sync | presentation decisions |
| Configuration | versioned schema, validation, atomic persistence | raw secret logging |
| OTA | signature verification, slots, health confirmation, rollback | feature behavior |

## Implemented portable slice

**Verified Current Fact:** The host-buildable C++17 slice implements `Display`,
`Renderer`, `Scene`, `SceneManager`, `ModeController`, `PluginRegistry`, and
`Application` interfaces without network, storage, or hardware dependencies.

**Verified Current Fact:** The simulator uses an in-memory RGB framebuffer and
exports PPM images. The Classic and Operations scenes contain synthetic data
only; they are layout prototypes, not a fidelity claim or live integration.

**Recommendation:** A compile-only adapter may reuse these inward-facing
contracts without touching a controller. Activating that adapter on hardware or
claiming compatibility requires memory, allocation, error, timing, panel
mapping, and toolchain behavior to be measured on an approved development
controller.

## Runtime modes

**Recommendation:** A mode controller owns exactly one active top-level mode:

- **Classic:** flight-centric scenes matching the approved reference behavior.
- **Operations:** a configured rotation of operational plugins.
- **Auto:** evaluates typed events and selects Classic or Operations according
  to deterministic, priority-ordered rules with cooldown and manual override.

Mode switching must not destroy provider state or bypass scene cleanup. Auto
triggers require source, timestamp, expiry, priority, and audit reason.

## Plugin contract

**Recommendation:** Plugins should be statically linked on constrained targets
until hardware measurements justify runtime-loaded modules. “Plugin” means
interface isolation and manifest-driven registration, not arbitrary downloaded
native code.

A plugin manifest should declare a stable ID, semantic version, required core
API range, capabilities, configuration schema version, memory budget, data
retention class, and supported deployment targets. The lifecycle is:

```text
register -> validate configuration -> initialize -> activate/deactivate -> stop
```

Core provides explicit services (clock, logger, event bus, storage namespace,
network client, renderer); plugins never discover globals.

## Theme contract

**Recommendation:** Themes are data packages validated against a versioned
schema. Tokens cover palette, typography, spacing, icons, motion, and widget
variants. Layout capability must be validated per display geometry. Brand-like
theme names require trademark/legal review before distribution.

## Configuration and secrets

**Recommendation:** Store user configuration as a versioned document with
defaults, validation, migration, atomic commit, and last-known-good recovery.
Secrets use a distinct storage interface and are redacted from logs, backups,
diagnostics, and browser responses. Browser configuration requires authenticated
sessions, CSRF protection, secure transport, rate limits, and explicit reset.

## Reliability model

**Recommendation:** The application uses bounded queues, timeouts, backoff with
jitter, stale-data indicators, watchdog heartbeats, structured error codes, and
no unbounded retry loops. Network loss must not blank the display or block local
configuration/recovery. Each subsystem reports health without deciding reboot
policy.

## Open decisions

1. Controller family and minimum flash, PSRAM/RAM, storage, and security features.
2. RTOS/task model versus a cooperative event loop.
3. Native framebuffer format and supported display geometries.
4. Device identity, provisioning, local-admin authentication, and fleet PKI.
5. OTA signing authority, key custody, release channels, and rollback threshold.
6. Whether multi-target support shares portable C++ core code or API contracts only.
