# Core Rewrite Decision Gate (Phase 9D)

This document defines when to keep the TinyKVM core in C++ and when to start selective C rewrites.

## Current Position

Current recommendation: keep the core in C++ and continue migrating consumers through the C API.

Rationale:

- Existing C API now covers lifecycle, setup, execution, vmcalls, symbol lookup, guest memory copy, fork, reset, and typed errors.
- Non-interactive smoke and CTest are stable.
- Two production-like C consumers are integrated and continuously exercised (`simplekvm_c`, `tinytest_c`).
- A dedicated c-api-only lane exists (`evolution_smoke_c_only`).

## Rewrite Triggers

Begin selective core rewrites only when at least one of the following is true:

- A required capability cannot be exposed safely through the C API without leaking unstable C++ internals.
- A measured performance regression is attributable to the C++ core boundary and cannot be resolved at the API layer.
- Toolchain or deployment requirements prohibit C++ runtime usage in target environments.

## Minimum Readiness Before Rewriting

- [x] At least two production-like C consumers are running exclusively through `c_api.h`.
- [x] C API contracts are versioned and documented.
- [x] Negative-path and timeout/error behavior are covered in smoke tests.
- [ ] A subsystem-by-subsystem replacement plan exists with rollback points.

## Suggested First Rewrite Candidates (if triggered)

Prioritize small, bounded components over VM execution core:

1. Utility/stateless helpers
2. Isolated memory helpers
3. Peripheral tooling adapters

Avoid first-wave rewrites of vCPU run loop, paging core, and exception handling paths.
