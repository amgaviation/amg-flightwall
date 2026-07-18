# Engineering Decision Log

## Accepted for this foundation

### D-001 — Greenfield project boundary

- **Status:** Accepted
- **Decision:** AMG FlightWall uses the standalone `amgaviation/amg-flightwall`
  repository. The parent website is context only.
- **Reason:** Prevent accidental coupling and preserve unrelated production behavior.

### D-002 — Preserve upstream firmware

- **Status:** Accepted
- **Decision:** Do not modify or silently copy the original firmware. A future
  snapshot, if approved, goes only in `firmware/flightwall-original/` with
  provenance, revision, license, and checksums.
- **Reason:** Maintain an auditable reference and clean implementation boundary.

### D-003 — Evidence labels

- **Status:** Accepted
- **Decision:** Technical records explicitly separate verified facts, planning
  assumptions, recommendations, and open decisions.
- **Reason:** Avoid unverified hardware and integration claims.

### D-004 — Interface-based plugins first

- **Status:** Accepted for the portable prototype
- **Decision:** Begin with statically linked, manifest-registered plugins rather
  than arbitrary dynamically downloaded firmware modules.
- **Reason:** Provides modularity without introducing an unbounded code-loading,
  compatibility, memory, signing, and recovery surface on constrained hardware.

### D-005 — Dependency-free host core first

- **Status:** Accepted; embedded-framework restriction superseded by D-006
- **Decision:** Use C++17, GNU Make, and an in-memory display adapter for the
  first runnable slice. The original restriction on selecting an embedded
  framework applied until controller evidence and a safe compile-only boundary
  existed.
- **Reason:** Enables testable product progress without writing to unknown
  hardware or locking the platform to an unverified controller/toolchain.

### D-006 — Experimental embedded target remains compile-only

- **Status:** Accepted for the HD-WF2 prototype
- **Decision:** Pin the experimental PlatformIO target and HUB75 dependency,
  block PlatformIO `upload`, `uploadfs`, `program`, and `erase` targets, and
  treat compilation as evidence only—not hardware compatibility or
  production-toolchain approval.
- **Reason:** Exercises the target boundary without risking the sole factory
  controller or overstating runtime verification.

### D-007 — Configuration references secrets by opaque profile key/revision

- **Status:** Accepted for the portable prototype
- **Decision:** Ordinary configuration may contain a validated opaque numeric
  Wi-Fi profile key and revision but never a plaintext password or token. A
  future platform adapter must resolve that reference through a separate
  secret-storage interface.
- **Reason:** Reduces accidental exposure in configuration export, diagnostics,
  backup, and browser responses.

### D-008 — Diagnostics are typed and payload-free by default

- **Status:** Accepted for the portable prototype
- **Decision:** Core health reports contain only subsystem, severity, typed
  status code, and timestamp. They do not accept free-text messages or provider
  payloads.
- **Reason:** Makes the default diagnostic path bounded and secret-free by
  construction.

### D-009 — Visible hardware progress remains behind the write gate

- **Status:** Accepted for the HD-WF2 prototype
- **Decision:** Make the compile-only target boot into a dedicated smoke screen.
  Only facts observable by the running firmware may leave `WAIT`: the target
  build and render loop report `RUN`. Host tests, factory-backup hash, and
  source-gate evidence remain external preflight results and are not embedded
  as pass claims. Do not weaken the device-write safety gate.
- **Reason:** Provides an immediately recognizable hardware acceptance artifact
  without turning compilation into a compatibility claim or risking the sole
  factory controller.

## Open decisions

| ID | Decision | Evidence/owner needed |
| --- | --- | --- |
| O-002 | Product license and upstream reuse policy | Owner and legal review |
| O-003 | Supported hardware revisions | Physical inspection and engineering owner |
| O-004 | Production build framework/toolchain | Hardware evidence and reproducibility spike |
| O-005 | OTA key custody and release authority | Security/operations owners |
| O-006 | Local provisioning/authentication model | Threat model and product requirements |
| O-007 | Required Classic fidelity baseline | Original product capture and owner sign-off |
| O-008 | Flight/weather provider contracts | Cost, license, quota, privacy, API validation |
| O-009 | AMG Connect API scope/schema | Verified API contract and security review |
| O-010 | Target data classification/retention | Privacy, legal, and operations owners |
| O-011 | Named themes and trademark clearance | Brand/legal review |
| O-012 | Multi-target portability strategy | Prototype measurements and platform roadmap |
