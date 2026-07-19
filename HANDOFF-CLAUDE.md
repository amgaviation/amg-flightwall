# AMG FlightWall — Claude Handoff

## Objective

Continue development and hardware bring-up of the AMG FlightWall platform using
this isolated `amg-flightwall` repository. Do not modify the separate `amg1`
website repository or the upstream AxisNimble firmware.

## Source of truth

- GitHub: `https://github.com/amgaviation/amg-flightwall`
- Current branch at handoff: `codex/hardware-smoke-screen`
- Pull request: `https://github.com/amgaviation/amg-flightwall/pull/5`

Start with `README.md`, then consult:

- `docs/hardware.md`
- `docs/hardware-smoke-test.md`
- `docs/recovery.md`
- `docs/architecture.md`
- `docs/decision-log.md`
- `firmware/amg-flightwall/README.md`

These artifacts contain the architecture, verified hardware profile, recovery
procedure, diagnostics, and distinctions between facts, assumptions,
recommendations, and open decisions. Do not duplicate them here.

## Verified current state

- Hardware is a FlightWall Mini using an HD-WF2 V7.2.0-2 ESP32-S3 controller.
- Flash size is 8 MB.
- Factory firmware was backed up before experiments and has been restored.
- Full post-restore comparison found immutable bootloader, partition table,
  applications, OTA selection, and filesystem content intact; expected runtime
  differences were confined to writable NVS state.
- The restored factory application boots, joins the network, advertises its
  FlightWall service, and answers its HTTPS identity endpoint.
- Sleep scheduling was disabled through the official app and saved.
- Live configuration showed nonzero brightness and the expected `mini-v1`
  profile/loading clock.
- Display remains completely blank under both restored factory firmware and an
  independent raw HUB75 diagnostic whose execution was confirmed over serial.
- The factory FlightWall application does not use the controller's S1 GPIO or
  RUN LED, so no reaction from those controls is non-diagnostic.
- The evidence currently narrows the fault to the physical HUB75 path: panel
  power/contact, 16-pin IDC ribbon/contact/orientation, or the HD-WF2 output
  buffer stage. No software buffer-enable GPIO was found.

## Hardware constraints and next diagnostic

- Controller ribbon must remain on `75EX1`.
- Panel ribbon must remain on `J1/IN`, not `J2/OUT`.
- Do not move to `75EX2`; its E-line is not mapped by the verified factory
  profile.
- Do not erase or overwrite the controller until `docs/recovery.md` and the
  backup manifest have been reviewed.
- The next substitution test is a known-good straight-through 16-pin HUB75 IDC
  cable, followed by a known-good compatible controller or panel if needed.
- The owner does not have a multimeter; do not repeatedly recommend one.

## Sensitive artifacts

`backups/local/` contains full device flash images required for recovery. Treat
them as confidential: flash/NVS images may contain Wi-Fi credentials, device
identity material, provisioning data, or private keys. Do not commit, publish,
upload, quote, or parse sensitive values into logs. Use the checksums and
manifests in `backups/manifests/` when validating them.

Hardware photographs are under `hardware/photos/diagnostic-session/`.

## Suggested skills

- `diagnosing-bugs` or `superpowers:systematic-debugging` for continued hardware
  isolation.
- `codebase-design` before expanding the embedded architecture.
- `implement` and `tdd` for production firmware work.
- `codex-security:security-scan` before introducing provisioning, OTA, secrets,
  or external APIs.
- `handoff` when transferring the work again.

## Handoff discipline

Preserve the factory image and recovery path. Separate every claim into
Verified Current Fact, Planning Assumption, Recommendation, or Open Decision.
Do not claim display operation, performance, flashing success, or hardware
compatibility without direct evidence.
