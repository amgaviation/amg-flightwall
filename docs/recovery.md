# Recovery and Rollback Guide

## Current status

**Verified Current Fact:** The initially factory-unmodified FlightWall Mini was
inspected and its ESP32-S3 controller was identified read-only. One complete
8 MB flash snapshot was captured with esptool 5.3.1. The redacted manifest is
stored at `backups/manifests/flightwall-mini-hd-wf2-20260718.json`; the binary
remains local and ignored by Git.

**Verified Current Fact:** Before the owner-authorized smoke activation, a
second complete 8 MB snapshot was captured. Immutable regions matched the first
snapshot; runtime NVS accounted for the full-image hash difference. A restore
slice for `app0` was extracted, and the untouched factory `app1` remains in
flash. These are stronger recovery inputs; the app0-only recovery was later
verified, while full-image restoration remains untested.

**Verified Current Fact:** The smoke artifact was written only to `app0` at
`0x10000`. The exact written range was read back and matched the compiled
artifact SHA-256. See the activation manifest at
`backups/manifests/flightwall-mini-hd-wf2-smoke-activation-20260718.json`.

**Verified Current Fact:** The complete saved factory `app0` slice was later
written back at `0x10000` and esptool verified all 2,883,584 bytes against
SHA-256
`ed3f4bc8d26c32ec674a5285882e34ec67c55ed89b88bcd3b0f3b84e1d9581b4`.
The preserved OTA metadata selected valid `app0` sequence 7 over `app1`
sequence 6. Esptool issued a hard reset after the restore, but no factory serial
marker was captured and the owner observed the panel as blank. App0 binary
restoration is therefore verified; factory application execution, full factory
display behavior, and full-image restoration are not independently verified.

**Verified Current Fact:** A subsequent standalone raw diagnostic was written
only to app0. Its reviewed 263,696-byte post-flash readback matched SHA-256
`732196319ebbfb488c7ecbe1ca0c32c1c44553ce506c9e6155782f3eddc20f66`.
Factory app1 and every non-app0 region remain preserved. See
`backups/manifests/flightwall-mini-hd-wf2-raw-hub75-diagnostic-20260719.json`.

**Recommendation:** Treat recovery as a prerequisite to firmware development,
not a final OTA feature.

## Pre-development backup procedure

The controller and flash layout are now identified, but full-image restoration
remains unvalidated. On a dedicated development unit:

1. Photograph labels, wiring, and connector orientation before disassembly.
2. Capture chip identity, security fuses, boot output, partition map, and tool
   versions without changing device state.
3. Read every permitted non-volatile region twice.
4. Hash both reads and require identical hashes before accepting the backup.
5. Store the encrypted image in approved access-controlled storage; commit only
   a redacted manifest with hashes, device alias, date, and procedure version.
6. Restore to a separate compatible controller where legally and technically
   permitted, then verify boot and Classic behavior.
7. Document physical recovery entry and how to return to the original image.

Backups may contain credentials or personal/location data and must never be
committed to this repository.

## Target OTA state machine

```text
idle -> downloading -> verified -> staged -> trial boot
                                      |          |
                                    abort     confirmed -> active
                                                 |
                                    timeout/crash -> rollback -> recovery
```

An update must be rejected unless its signature, hardware compatibility,
version policy, manifest, size, and hash pass. Power loss in downloading,
verification, or staging must leave a bootable image. Trial firmware confirms
health only after display, configuration, storage, and watchdog checks pass.

## Recovery modes

**Recommendation:** Provide, in priority order:

1. Automatic rollback to the last-known-good slot.
2. Local recovery UI with networking optional and secrets redacted.
3. Physical recovery gesture that cannot be triggered remotely.
4. Wired factory recovery using documented, version-pinned tooling.

Factory reset and firmware rollback are distinct actions. A rollback should not
erase user settings unless their schema is incompatible and an explicit,
auditable migration policy requires it.

## Release verification

Every hardware revision must pass interrupted-download, interrupted-write,
invalid-signature, wrong-hardware, downgrade, corrupted-config, boot-loop,
network-loss, and full-storage tests. Evidence must include artifact hashes,
device revision, procedure, result, and operator/reviewer.

## Open decisions

- Flash capacity/partition topology and immutable recovery mechanism.
- Signature algorithm, trusted key storage, revocation, and key rotation.
- Confirmation health checks, trial duration, and rollback count.
- Local versus managed update channels and offline update support.
