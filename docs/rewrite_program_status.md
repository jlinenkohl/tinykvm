# TinyKVM Rewrite Program Status (2026-03-30)

## Executive Summary

The program has completed the C ABI boundary, contract hardening, and baseline gating foundation.
Core C++ VM internals have not yet been broadly reimplemented in C by design.
This was intentional risk reduction and enables evidence-driven component decisions instead of rewrite-by-default.

## Starting Point

Initial question: should TinyKVM be converted from C++ to C, and if so, how?

Initial assessment outcome:

- Full immediate rewrite was high risk.
- Safer strategy was C API first, then selective subsystem decisions.

## What Has Been Completed

### C API and Consumer Enablement

- Stable C ABI exposed via lib/tinykvm/c_api.h and implemented in lib/tinykvm/c_api.cpp.
- Two production-like C consumers integrated:
  - src/simple_c.c
  - src/tinytest_c.c
- C API smoke and negative-path coverage expanded in tests/c_api_smoke.c.

### Contract and Behavior Locks

- Contract doc established and expanded: docs/c_api_contract.md.
- Contract macro and enum lock test added: tests/c_api_contract_smoke.c.
- Runtime behavior drift checks added for error and invalid-state semantics.

### Lane and Gating Infrastructure

- Full smoke lane and C-only lane operationalized.
- Contract lane became first-class CTest label.
- Baseline capture/compare system added and expanded:
  - tests/capture_baseline.sh
  - tests/compare_baselines.sh
  - metrics/baselines/history.tsv
- Threshold-based warning/failure gates added.
- Single-command gate wrapper with policy profiles added:
  - tests/run_baseline_gate.sh
  - policies: strict, balanced, lenient.

### Stability Checkpoints

- Phase11a through Phase13a committed checkpoints completed.
- Additional repeatability checkpoints captured:
  - phase13b
  - phase13c

## Vanilla Upstream Baseline (Original Remote Code)

Reference commit:

- origin/master a6a044c

Method:

- Created isolated worktree at /home/jlinenkohl/github/tinykvm-vanilla.
- Built top-level CMake targets.
- Ran vanilla unit harness at tests/run_unit_tests.sh.
- Ran integration-style tinytest with guest/tests/glibc_test.

Results summary:

- Top-level CTest in vanilla build: no registered tests.
- Unit harness: 8 tests total, 6 passed, 2 failed.
  - Failed: test_elf (segfault), test_mmap (assertion failures).
- Integration tinytest run: passed.

Vanilla executable sizes (bytes):

- build/simplekvm: 9187568
- build/tinytest: 9235928
- build/storagekvm: 9121888
- build/pipekvm: 8901104

Machine-readable vanilla snapshot artifact:

- metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline

## What We Know

- C API boundary is stable and continuously exercised.
- Contract and behavior drift checks are active.
- Baseline and threshold gates are functional and producing useful trend signals.
- Repeatability over recent runs is acceptable with expected timing jitter.
- Upstream vanilla code is functional for major integration path but has known unit-test failures in this environment.

## What We Do Not Yet Know

- Which core subsystems are objectively best kept as C++ versus rewritten in C.
- Coverage adequacy for all memory and paging edge paths under stress.
- Performance impact of any future selective core rewrites on real workloads.
- Cross-toolchain reproducibility for baseline deltas without normalization.

## Current Risks

- Overfitting rewrites without measurable benefit.
- Timing noise causing false-positive perf alerts if thresholds are too strict.
- High-risk areas (vCPU run loop, paging core, exception flow) remain sensitive to regressions.
- Unit-test instability in vanilla upstream limits direct apples-to-apples confidence for some paths.

## Decision Status

Current recommendation remains selective and evidence-driven:

- Keep core C++ by default.
- Rewrite only where a trigger exists and measured value is clear.

Triggers are tracked in docs/core_rewrite_decision.md.

## Deviation Provenance Policy

Notable failures/gaps are now tracked with explicit provenance classification to avoid chasing preexisting upstream behavior as if it were newly introduced.

Reference log:

- docs/deviation_provenance_log.md

## Forward Plan (Phase 14)

### Goal

Evaluate each major core component for correctness, quality, and rewrite value before implementation changes.

### Planned Work

1. Build a component audit rubric with scoring and decision outcomes.
2. Audit API boundary and machine lifecycle first.
3. Audit memory subsystem next.
4. Audit vCPU/run path last due risk.
5. For each component produce Keep, Refactor-in-C++, or Rewrite-Candidate recommendation with evidence.

### Exit Criteria

- Evidence-backed decision matrix for major components.
- No rewrite starts without explicit trigger and rollback plan.
- Baseline gate remains green during audit-related changes.
