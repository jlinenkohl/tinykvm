# TinyKVM Component Audit Phase14B (Memory Subsystem)

Date: 2026-03-30
Branch: phase14_audit

## Scope

Audited files:

1. lib/tinykvm/memory.cpp
2. lib/tinykvm/memory.hpp
3. lib/tinykvm/memory_bank.cpp
4. lib/tinykvm/memory_bank.hpp
5. lib/tinykvm/page_streaming.cpp
6. lib/tinykvm/page_streaming.hpp

Total footprint: 1187 lines across the above files.

## Rubric

Scored 1-5 (higher is better) for:

- Correctness confidence
- Quality/maintainability
- Change risk
- Rewrite value

Decision outcomes:

- Keep as-is
- Refactor in C++
- Rewrite candidate

## Evidence Used

- Static code inspection of memory subsystem files listed above.
- Existing behavior and contract gates from:
  - tests/run_baseline_gate.sh
  - tests/c_api_smoke.c
  - tests/c_api_contract_smoke.c
- Existing memory-focused unit tests source review:
  - tests/unit/mmap.cpp
  - tests/unit/reset.cpp
- Upstream vanilla baseline reference:
  - metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline

## Findings

### Component A: Core Memory Orchestrator (vMemory)

Files:

- lib/tinykvm/memory.cpp
- lib/tinykvm/memory.hpp

Observed strengths:

- Broad support for direct memory, CoW banks, remote memory fallback, and mmap-backed areas.
- Explicit failure paths throw MemoryException with address/size context.
- Fork/reset semantics include work-memory limits and fallback reset behavior.
- Safety-oriented helpers exist for user/kernel page access and expected flags.

Observed weaknesses:

- Large, multi-responsibility implementation increases coupling and reasoning cost.
- Several XXX markers indicate unresolved design debt:
  - security-check commentary placeholders in memory access/view paths
  - fallback and recovery TODO/XXX in mmap allocation path
- Some exception handling intentionally swallows errors and falls back, which is practical but can hide root causes during diagnosis.

Score:

- Correctness confidence: 3.9/5
- Quality/maintainability: 3.1/5
- Change risk: 4.7/5 (high)
- Rewrite value: 2.8/5 (medium for selective helper isolation, low for full rewrite)

Decision:

- Keep architecture in C++.
- Prioritize targeted C++ refactors to isolate responsibilities, not broad language rewrite.

### Component B: Memory Bank Allocator

Files:

- lib/tinykvm/memory_bank.cpp
- lib/tinykvm/memory_bank.hpp

Observed strengths:

- Clear high-level model for bank capacity, hugepage alignment, and reuse.
- Strong hard-fail behavior for impossible states (alignment/capacity).
- Reset path uses MADV_DONTNEED and bank reuse to control memory pressure.

Observed weaknesses:

- Bank index/memory slot lifecycle is sensitive and not yet covered by dedicated micro-tests at this layer.
- Single assert-based guard in get_next_page is correct but brittle under future refactor if invariant setup shifts.

Score:

- Correctness confidence: 4.1/5
- Quality/maintainability: 3.6/5
- Change risk: 3.8/5
- Rewrite value: 2.2/5

Decision:

- Keep as C++ implementation.
- Refactor only if needed to improve observability and invariant checks.

### Component C: Page Streaming Primitives

Files:

- lib/tinykvm/page_streaming.cpp
- lib/tinykvm/page_streaming.hpp

Observed strengths:

- Tight, bounded utility scope.
- Fast-path AVX2 specialization with fallback behavior.
- Low coupling to higher-level VM orchestration.

Observed weaknesses:

- Intrinsics path is architecture/compiler sensitive and should be kept where toolchain support is strongest.
- Small size but performance critical; regressions can be subtle.

Score:

- Correctness confidence: 4.2/5
- Quality/maintainability: 3.9/5
- Change risk: 3.0/5
- Rewrite value: 1.8/5

Decision:

- Keep as-is in C++ with intrinsics.
- Avoid rewrite unless a measured portability/performance trigger appears.

## Cross-Evidence Notes

1. Vanilla upstream unit harness showed memory-adjacent failures (mmap and elf) in this environment, while integration tinytest passed.
2. Current branch uses stronger contract and lane gating than upstream vanilla baseline.
3. Current audit branch baseline gate remains passing, supporting stability of current memory behavior under existing smoke workloads.

## Risk Register (Memory)

1. Large vMemory responsibility surface can amplify regression blast radius.
2. mmap-backed area path combines file I/O, paging, and region bookkeeping in one flow.
3. Performance-sensitive page duplication/zeroing code may regress if refactored without targeted benchmarking.

## Recommendations (Phase14C Candidate Actions)

1. No broad C rewrite of memory subsystem.
2. Perform C++ refactor-only decomposition in small slices:
   - isolate mmap_backed_area sub-operations
   - isolate fork_reset policy logic from page-copy mechanics
3. Add focused tests for memory bank index lifecycle and mmap range overlap/cleanup edge cases.
4. Keep page streaming primitive implementation language unchanged unless a trigger is proven.

## Verdict

Memory subsystem is functionally capable but structurally high-risk for broad rewrite.

- Broad memory rewrite: Not recommended.
- Selective C++ simplification/refactor: Recommended.
- Rewrite trigger status: Not met.
