# Development Guide

## Scope

This directory is a separate project boundary inside the context repository.
Changes must not couple firmware or tooling to the parent Next.js application.

## Workflow

1. Start from the approved source-of-truth revision and create a task branch.
2. Read this documentation and the nearest `AGENTS.md` before editing.
3. Add or update an engineering record when new evidence changes an assumption.
4. Keep changes small; never edit a captured upstream firmware snapshot.
5. Add host tests for portable logic and target tests for hardware adapters.
6. Run formatting, static analysis, unit tests, build, and secret scanning.
7. Record exact commands and distinguish simulated from hardware verification.
8. Require review for security, storage layout, OTA, boot, or power changes.

## Coding rules

- Use modern C++ with RAII, strong domain types, explicit ownership, and minimal
  dynamic allocation after startup on constrained targets.
- Inject clocks, storage, transport, and display ports into services.
- Avoid mutable globals, blocking network operations on render paths, exceptions
  across firmware boundaries, and heap allocation in frame loops.
- Use bounded containers where a hardware budget requires them.
- Return typed errors; logs supplement errors but do not replace them.
- Never log credentials, tokens, precise private locations, client records, or
  raw mission/crew data.
- Put provider-specific payloads in adapters and normalize before domain use.

## Test pyramid

| Level | Runs where | Required coverage |
| --- | --- | --- |
| Unit | host | domain rules, mode transitions, config migration, layout math |
| Contract | host | plugin/theme manifests and provider fixtures |
| Integration | simulator/target | storage, network adapters, renderer snapshots |
| Hardware-in-loop | dedicated rig | panel mapping, brightness, watchdog, Wi-Fi recovery |
| Release | staging devices | signed OTA, interrupted update, rollback, soak test |

No test tier may be described as passing until the named command or procedure
has actually run and its evidence is retained.

## Sensitive configuration

Use ignored local files or injected CI secrets once a build system is approved.
Commit only redacted examples. Production API credentials must be provisioned
per environment/device and must not be compiled into distributable firmware.

## Definition of done

A change is done only when its acceptance criteria, tests, documentation,
security impact, migration/rollback, and verified target are recorded. Hardware
claims require evidence from the named hardware revision.
