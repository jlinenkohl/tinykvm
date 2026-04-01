# TinyKVM Rewrite Program Status (2026-03-30)

## Executive Summary

The program has completed the C ABI boundary, contract hardening, and baseline gating foundation.
Core C++ VM internals have not yet been broadly reimplemented in C by design.
This was intentional risk reduction and enables evidence-driven component decisions instead of rewrite-by-default.

## Working Document Policy

This file is the single living working document for current state, active task, and forward path.
To reduce documentation drift, updates should be made here first, with supporting details linked to canonical companion docs.

Canonical companion docs:

- docs/subsystem_rewrite_plan.md (phase scope and milestone boundaries)
- docs/deviation_provenance_log.md (mismatch/failure provenance)
- phase-specific audit docs only when explicitly required by a phase checkpoint

## Iteration Tracker (Living)

| Date | Iteration/Phase | What Changed | Validation Evidence | Baseline Checkpoint | Provenance Update | Docs Updated | Next Action |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 2026-03-30 | Phase14B2 | Closed Catch2 gap in harness and portability fix in unit ELF test | Memory-focused unit subset run: 4 pass, 1 fail (`test_mmap`) | `phase14b2` gate snapshot captured | DV-001, DV-002, DV-003 recorded as preexisting-vanilla | status, plan, provenance log | Begin Phase14C high-risk path audit |
| 2026-03-30 | Vanilla Full-Suite Local Validation | Recreated vanilla worktree at `origin/master` (`a6a044c`) and executed CI-equivalent matrix + unit harness + integration tinytest | Matrix: `g++/clang++ x Debug/Release` configure/build/ctest all exit 0; unit harness: 8 total, 6 pass, 2 fail (`test_elf`, `test_mmap`); integration tinytest (`glibc_test`) pass on both vanilla and current branch | Evidence captured in terminal logs and `/tmp/vanilla_matrix_20260330T135641.log` | Confirms DV-002 remains preexisting-vanilla and reproducible | working status | Add dedicated vanilla parity lane wrapper and run it at each phase checkpoint |
| 2026-03-31 | ELF Portability Test Split | Split ELF unit coverage into explicit no-relocation path (`[ELF][no-reloc]`) and relocation support gate (`[ELF][reloc]`) | `test_elf` now reports 3 cases: 2 pass, 1 fail (SIGSEGV in dynamic relocation support case) | not captured yet | Relocation-gate failure remains preexisting-vanilla candidate pending full relocation support matrix | working status | Implement missing dynamic-loader relocation support and re-run gate |
| 2026-03-31 | RELR Bootstrap Relocation Support | Implemented `.relr.dyn` decoding/application in loader bootstrap path and re-ran relocation gate | `test_elf` still fails in `[ELF][reloc]` (3 cases: 2 pass, 1 fail) after RELR addition | not captured yet | Remaining blocker likely includes `R_X86_64_IRELATIVE` handling during loader self-relocation | working status | Implement/handle IRELATIVE semantics (or explicit unsupported-path classification) and re-run gate |
| 2026-03-31 | ELF Relocation Gate Unblocked | Added targeted bootstrap handling and fixed `readlinkat` guest-copy bug uncovered by GDB backtrace | `test_elf` passes (4 assertions / 4 test cases); combined `test_elf|test_mmap` passes; full unit harness now 8/8 pass | not captured yet | Previously preexisting `test_elf` and `test_mmap` failures now resolved in this branch | working status | Re-run vanilla parity comparison and classify as intentional branch improvement |

## Vanilla Full-Suite Parity Tracker

| Lane | Vanilla Command | Current Branch Command | Vanilla Result | Current Result | Delta | Classification | Evidence | Owner/Next |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Top-level CTest discovery | `ctest --test-dir build --output-on-failure` | `ctest --test-dir build --output-on-failure` | no registered tests | labeled smoke/contract lanes present | topology differs | preexisting-vanilla | `metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline` | keep branch lane topology documented |
| Unit harness | `cd tests && bash run_unit_tests.sh` | `cd tests && bash run_unit_tests.sh` | 8 total, 6 pass, 2 fail (`test_elf`, `test_mmap`) | 8 total, 8 pass, 0 fail | +2 tests passing (`test_elf`, `test_mmap`) | intentional branch improvement (correctness gain) | `metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline`, `docs/deviation_provenance_log.md` | keep as mandatory parity lane per phase checkpoint |
| Integration tinytest | `build/tinytest guest/tests/glibc_test` | `build/tinytest guest/tests/glibc_test` | pass | pass (at last parity capture) | none observed | parity-observed | `metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline` | expand with additional integration lanes |

## Validation Gaps Found (Vanilla vs Current Routine)

1. Vanilla CI matrix validates build across `g++/clang++ x Debug/Release`, but routine phase gating in this branch has focused on smoke/contract and baseline timing; matrix parity should be run at explicit checkpoints.
2. Vanilla unit harness invocation is path-sensitive (`cd tests && bash run_unit_tests.sh`); running it from repository root fails due relative path assumptions.
3. Top-level vanilla CTest discovery has no registered tests (`No tests were found!!!` in all matrix build directories), so correctness evidence depends on unit harness and integration execution.
4. Integration tinytest (`glibc_test`) is an important correctness lane but is not part of vanilla CI workflow files; it should remain part of parity checkpoints because it validates real guest execution behavior.

## Dynamic ELF Portability Matrix (Bootstrap Path)

Goal: run unmodified dynamic ELF binaries across host environments by making loader bootstrap behavior feature-complete.

| Capability | Current Status | Evidence | Impact | Priority |
| --- | --- | --- | --- | --- |
| `.rela.dyn` relocation pass wired | partial | `Machine::dynamic_linking()` applies `.rela.dyn` only | bootstrap works for some binaries | high |
| `R_X86_64_RELATIVE` relocation write semantics | implemented (fixed) | relocation now writes `base + addend` | required baseline correctness | high |
| `.rela.plt` relocation pass | not enabled | `.rela.plt` call currently commented in loader path | may break lazy/PLT relocation cases | high |
| `R_X86_64_IRELATIVE` support | missing | host `/lib64/ld-linux-x86-64.so.2` includes IRELATIVE relocation | loader self-relocation may fail early | high |
| `.relr.dyn` / RELR support | missing | host `/lib64/ld-linux-x86-64.so.2` includes RELR section | modern glibc loader bootstrap can fail | high |
| Loader-critical mmap/brk/open/fstat/read/syscall surface | present | linux syscall handlers include mmap/munmap/mprotect/brk/openat/newfstatat/read/pread64/arch_prctl/rt_sigaction/etc | runtime dependency loading path is viable once bootstrap relocations are complete | medium |

### Portability Work Queue (Ordered)

1. Add RELR decoding/application in loader bootstrap relocation path.
2. Add `R_X86_64_IRELATIVE` handling in relocation pass.
3. Enable/evaluate `.rela.plt` relocation pass where required for bootstrap correctness.
4. Re-run `[ELF][reloc]` gate and record pass/fail plus relocation-type evidence.
5. Keep `[ELF][no-reloc]` as a stable control test for non-dynamic loader regressions.

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
