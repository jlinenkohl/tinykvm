# TinyKVM Component Audit Phase14A (API Boundary + Machine Lifecycle)

Date: 2026-03-30
Branch: phase14_audit

## Scope

Audited components:

1. C API boundary
   - lib/tinykvm/c_api.h
   - lib/tinykvm/c_api.cpp
2. Machine lifecycle and environment setup
   - lib/tinykvm/machine.hpp
   - lib/tinykvm/machine.cpp
   - lib/tinykvm/machine_env.cpp
   - lib/tinykvm/machine_utils.cpp

Excluded this phase (higher risk):

- vCPU run loop internals
- paging core internals
- exception/trap dispatch core

## Rubric

Each component is scored (1-5, higher is better) on:

- Correctness confidence
- Quality/maintainability
- Change risk
- Rewrite value

Decision outcomes:

- Keep as-is
- Refactor in C++
- Rewrite candidate

## Evidence Used

- Public ABI and wrapper implementation inspection.
- Behavior/contract tests in tests/c_api_smoke.c and tests/c_api_contract_smoke.c.
- Current lane/gate results via tests/run_baseline_gate.sh.
- Baseline trend snapshots up through phase13d.
- Vanilla upstream reference baseline (origin/master a6a044c) from:
  - metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline

## Findings

### Component A: C API Boundary

Files:

- lib/tinykvm/c_api.h (107 lines)
- lib/tinykvm/c_api.cpp (453 lines)

Observed strengths:

- Stable macro-versioned ABI with feature probes.
- Strong argument validation and typed error mapping.
- C-visible machine ownership/lifetime rules are explicit and consistently applied.
- Dedicated behavior locks exist for negative paths and error semantics.

Observed weaknesses:

- Error classification for invalid state uses message substring heuristics in c_api.cpp.
  - This is stable enough now, but brittle if exception text changes.
- Some API behavior still depends on downstream exception text shape.

Score:

- Correctness confidence: 4.5/5
- Quality/maintainability: 4.0/5
- Change risk: 2.0/5 (low-medium)
- Rewrite value: 1.5/5 (low)

Decision:

- Keep as-is for architecture.
- Targeted refactor in C++ only (no C rewrite) for explicit exception-to-error mapping APIs where feasible.

### Component B: Machine Lifecycle + Setup Helpers

Files:

- lib/tinykvm/machine.hpp (395 lines)
- lib/tinykvm/machine.cpp (431 lines)
- lib/tinykvm/machine_env.cpp (197 lines)
- lib/tinykvm/machine_utils.cpp (665 lines)

Observed strengths:

- Lifecycle operations (create, fork, reset, setup, run) are already operationally validated through full/c_api/contract lanes.
- Fork/reset behavior has dedicated smoke coverage through C API tests.
- mmap/file-backed flow and memory copy helpers are feature-rich and production-practical.

Observed weaknesses:

- Broad surface area and high coupling in Machine type make full rewrite expensive and risky.
- Several TODO/XXX comments indicate unresolved optimization and precision points:
  - machine_env.cpp: base_address marked as guesstimate.
  - machine_utils.cpp: TODO for madvise path after mmap-backed remap.
- Heavy reliance on assertions in lifecycle paths can be problematic if assumptions are violated in future refactors.

Score:

- Correctness confidence: 4.0/5
- Quality/maintainability: 3.2/5
- Change risk: 4.5/5 (high)
- Rewrite value: 2.5/5 (medium-low for full rewrite, medium for selective helper refactors)

Decision:

- Keep architecture in C++.
- Pursue selective C++ refactors to reduce coupling and clarify invariants.
- Defer C rewrite consideration to narrow leaf helpers only after measured trigger.

## Decision Matrix

1. C API boundary
- Decision: Keep + targeted C++ refactor
- Trigger for rewrite currently unmet

2. Machine lifecycle/setup
- Decision: Keep + selective C++ refactor
- Full rewrite not justified by current evidence

## Risks and Unknowns

Known risks:

- Exception-message-coupled error classification drift.
- Large monolithic lifecycle surface may hide edge-case regressions under future changes.

Unknowns:

- Whether performance bottlenecks attributable to C++ boundary exist under representative production loads.
- Whether memory-mapped helper TODO paths materially affect long-running workloads.

## Recommended Next Steps (Phase14B)

1. Add explicit non-string-based invalid-state error mapping path in C API layer where possible.
2. Produce a focused lifecycle refactor map (no behavior change) for:
   - setup_linux stack construction responsibilities
   - mmap_backed_area internal sub-steps
3. Extend audit to memory subsystem internals next:
   - lib/tinykvm/memory.cpp
   - lib/tinykvm/memory_bank.cpp
   - lib/tinykvm/page_streaming.cpp

## Rewrite Readiness Verdict

As of Phase14A:

- Broad core C rewrite: Not recommended.
- Narrow helper rewrites: Potentially acceptable only with explicit trigger and measurable benefit.
