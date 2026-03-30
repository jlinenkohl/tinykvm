# TinyKVM Subsystem Rewrite Plan (Phase 10D)

This plan defines a low-risk path for selective C rewrites, with explicit rollback points.

## Scope

Out of scope for first-wave rewrites:

- vCPU run loop
- paging core
- exception dispatch and trap flow

In scope for first-wave rewrites:

1. Utility/stateless helpers
2. Isolated memory helpers
3. Peripheral tooling adapters

## Strategy

- Keep C API stable as the primary integration boundary.
- Rewrite one subsystem at a time behind existing behavior contracts.
- Require green smoke lanes after each rewrite slice:
  - `evolution_smoke`
  - `evolution_smoke_c_only`
- Prefer additive implementation with feature flag/toggle before replacing defaults.

## Milestones and Rollback Points

### M1: Baseline Capture

Goals:

- Record baseline latency and success rates from smoke lanes.
- Record binary size for `simplekvm_c` and `tinytest_c`.

Rollback point:

- Tag or checkpoint commit before any subsystem replacement.

### M2: Utility Helper Rewrite

Candidate targets:

- Small pure helper functions that do not own VM state.

Acceptance criteria:

- No C API signature changes.
- Smoke lanes remain green.
- No regression in typed error behavior.

Rollback point:

- If behavior differs, revert only this subsystem patch set.

### M3: Isolated Memory Helper Rewrite

Candidate targets:

- Localized allocation/copy helpers without paging-core semantics.

Progress:

- Phase 11C extracted guest copy roundtrip validation into shared C helper code and reused it across `capi_smoke` and `tinytest_c`.

Acceptance criteria:

- `capi_smoke` and `tinytest_c` pass unchanged.
- Fork/reset and guest-copy checks remain green.

Rollback point:

- Revert this milestone and retain prior C++ implementation.

### M4: Peripheral Tooling Adapter Rewrite

Candidate targets:

- Non-critical adapters around diagnostics, runners, or support tooling.

Progress:

- Phase 11D introduced a shared CTest label adapter script and switched both lane execution and baseline capture tooling to consume it.

Acceptance criteria:

- No new required C API entry points unless demanded by two consumers.
- Labeled CTest lanes pass in CI and local runs.

Rollback point:

- Revert adapter-only changes without touching VM internals.

## Release/Branching Guidance

- Use one branch per milestone (`rewrite-m1`, `rewrite-m2`, ...).
- Merge only after passing:
  - `tests/run_c_api_lanes.sh`
  - `ctest --test-dir build --output-on-failure`
- Keep each milestone in multiple small commits to simplify selective rollback.

## Metrics to Track

- Smoke runtime trend (full and c_api lanes)
- Pass/fail rate for timeout and symbol-not-found assertions
- Binary size trend for C consumers
- Any change in crash/exception incidence in `tinytest_c`

## Baseline Workflow

Use checkpointed, self-describing baseline snapshots:

- Capture: `tests/capture_baseline.sh <phase-label>`
- Compare latest two: `tests/compare_baselines.sh`

Produced files:

- Per-checkpoint snapshot: `metrics/baselines/<phase>_<commit>_<timestamp>.baseline`
- Append-only history table: `metrics/baselines/history.tsv`

This keeps each checkpoint named by phase and commit while allowing simple diff and scalar comparison without JSON tooling.

## Exit Criteria for First Wave

- M1-M4 completed with green smoke lanes.
- No unresolved behavior regressions.
- Decision made whether to proceed toward deeper VM-core rewrites or keep core in C++ with C API front.

## Phase 12 Scope (Post First Wave)

Focus shifts from first-wave rewrites to contract hardening and operational reliability.

### P12A: C API Contract Lock

Goal:

- Add explicit smoke validation for public C API macro/enum stability.

Status:

- Completed via `capi_contract_smoke`, integrated into full and C-only smoke lanes.

### P12B: Runtime Behavior Drift Lock

Goal:

- Add explicit checks for `tkvm_last_error` content and invalid-state transitions in C API smoke.

Status:

- Completed in `tests/c_api_smoke.c` with added assertions for error-message and fork-state behavior.

### P12C: Contract Lane Operationalization

Goal:

- Make contract checks a first-class CTest lane and track contract timing/status in baseline history.

Status:

- Completed with contract-labeled CTest entry, `tests/run_contract_lanes.sh`, and expanded baseline capture/compare schema.

### P12D: Baseline Drift Threshold Gates

Goal:

- Add warn/fail threshold policies for contract/c_api/full lane time regressions.

Status:

- Completed in `tests/compare_baselines.sh` with CLI and environment-variable threshold controls.

### P12E: Baseline Gate Wrapper

Goal:

- Provide a one-command baseline capture + thresholded compare workflow for local and CI use.

Status:

- Completed via `tests/run_baseline_gate.sh` with default thresholds and override flags.

## Phase 13 Scope

Focus shifts to policy-driven operational guardrails and CI repeatability.

### P13A: Baseline Policy Profiles

Goal:

- Add named threshold profiles to baseline gating for consistent strict/balanced/lenient workflows.

Status:

- Completed in `tests/run_baseline_gate.sh` with `--policy` and `--list-policies` support.

### P13B/P13C: Stability Re-runs

Goal:

- Capture additional baseline-gated checkpoints to characterize run-to-run variance.

Status:

- Completed with `phase13b` and `phase13c` snapshots and no gate failures.

## Phase 14 Scope

Focus shifts from tooling expansion to evidence-based component quality/correctness evaluation and keep-or-rewrite decisions.

Reference status and plan document:

- `docs/rewrite_program_status.md`
