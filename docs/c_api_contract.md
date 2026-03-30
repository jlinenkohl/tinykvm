# tinykvm C API Contract

This document captures the stability and usage contract for the C ABI in `lib/tinykvm/c_api.h`.

## Versioning

Use compile-time macros to gate behavior:

- `TKVM_CAPI_VERSION_MAJOR`
- `TKVM_CAPI_VERSION_MINOR`
- `TKVM_CAPI_VERSION_PATCH`
- `TKVM_CAPI_VERSION_NUMBER`

Feature probes:

- `TKVM_CAPI_FEATURE_TYPED_ERRORS`
- `TKVM_CAPI_FEATURE_GUEST_COPY`
- `TKVM_CAPI_FEATURE_VMCALL_U64_ARRAY`
- `TKVM_CAPI_FEATURE_FORK_RESET`

## Error Model

General rule:

- `TKVM_OK` (`0`) means success.
- Negative values are errors.

Primary error classes:

- `TKVM_INVALID_ARGUMENT`: caller contract violation
- `TKVM_ERR_TIMEOUT`: timed operations exceeded timeout
- `TKVM_ERR_MEMORY`: allocation or memory subsystem failure
- `TKVM_ERR_SYMBOL_NOT_FOUND`: requested symbol did not resolve
- `TKVM_ERR_INVALID_STATE`: operation is not valid in the current machine state
- `TKVM_ERR_MACHINE`: other machine/runtime errors
- `TKVM_ERROR`: fallback for uncategorized failures

## Ownership and Lifetime

- `tkvm_machine_t*` is caller-owned after successful `tkvm_machine_create` and `tkvm_machine_fork`.
- Release handles with `tkvm_machine_destroy`.
- `tkvm_machine_create` copies the guest binary bytes; caller may free its input buffer after successful creation.
- `tkvm_last_error()` returns a thread-local string pointer. The pointer may be invalidated by a subsequent API call on the same thread.

## Out-Parameter Rules

- Out-parameters are only valid when a function returns `TKVM_OK`.
- Passing `NULL` for a required out-parameter returns `TKVM_INVALID_ARGUMENT`.

## Non-Interactive Test Guidance

- The smoke harness defaults to non-interactive execution.
- To permit debug-oriented environment behavior, set `TKVM_ALLOW_DEBUG=1` when invoking `tests/evolution_smoke.sh`.

## API Expansion Policy (Phase 9C)

- New C API entry points are added only to unblock a concrete C consumer migration.
- Existing C++ internals are not exposed directly when equivalent behavior can be composed from current C API calls.
- Every newly added entry point must be exercised in `tests/c_api_smoke.c` before adoption in higher-level tools.
- If an API is specific to one temporary migration path, prefer keeping it out of the public header until a second consumer needs it.

## Consumer Coverage (Phase 10)

Current production-like C consumers:

- `src/simple_c.c`
- `src/tinytest_c.c`

Current smoke lanes:

- Full lane: `tests/evolution_smoke.sh`
- C-only lane: `tests/evolution_smoke_c_only.sh`

Quick c-api lane command:

- `tests/run_c_api_lanes.sh`

Quick contract lane command:

- `tests/run_contract_lanes.sh`

Baseline capture and trend comparison:

- `tests/capture_baseline.sh <phase-label>`
- `tests/compare_baselines.sh`

Baseline history now includes contract-lane status/seconds in addition to c_api and full lanes.

## Contract Lock Checks (Phase 12A)

To prevent accidental contract drift in public macros/enums, the smoke lanes now execute:

- `build/capi_contract_smoke`

This check verifies:

- version-number composition macro consistency
- feature-flag presence and expected values
- stable values for public error and snapshot enums

## Behavioral Drift Checks (Phase 12B)

`tests/c_api_smoke.c` now also locks key runtime contract behavior by asserting:

- `tkvm_last_error()` is non-empty after representative failures
- symbol lookup/vmcall missing-symbol failures surface expected error text
- invalid-state transitions remain enforced for fork-before-CoW and fork-from-forked
