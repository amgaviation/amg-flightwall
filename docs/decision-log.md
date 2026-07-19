# Engineering Decision Log

## Accepted for this foundation

### D-001 — Greenfield project boundary

- **Status:** Accepted
- **Decision:** AMG FlightWall is isolated under `amg-flightwall/` pending a
  standalone repository. The parent website is context only.
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

- **Status:** Recommended; implementation approval pending
- **Decision:** Begin with statically linked, manifest-registered plugins rather
  than arbitrary dynamically downloaded firmware modules.
- **Reason:** Provides modularity without introducing an unbounded code-loading,
  compatibility, memory, signing, and recovery surface on constrained hardware.

## Open decisions

| ID | Decision | Evidence/owner needed |
| --- | --- | --- |
| O-001 | Standalone repo location and visibility | Project owner/GitHub admin |
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
