# TinyKVM Deviation Provenance Log

Purpose: Track whether noteworthy behaviors/failures are pre-existing in upstream vanilla or introduced on current branch, to avoid misattribution during rewrite and optimization work.

## Classification Rules

- `preexisting-vanilla`: Reproduced on upstream reference baseline.
- `introduced-here`: Not seen on vanilla baseline, appears on current branch.
- `undetermined`: Not yet reproduced in both contexts.

Each entry should include:

1. Reproduction command(s)
2. Evidence artifact(s)
3. Decision impact (comparison-only, fix candidate, blocker)

## Entries

### DV-001 Catch2 Harness Assumption

- Classification: preexisting-vanilla
- Summary: Unit harness assumes `tests/Catch2` is present and does not self-initialize submodule.
- Vanilla evidence:
  - Initial vanilla unit-harness configure failed until manual submodule init.
  - Captured during baseline workflow leading to [metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline](metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline).
- Current branch handling:
  - [tests/run_unit_tests.sh](tests/run_unit_tests.sh) now auto-initializes `tests/Catch2` when missing.
- Decision impact:
  - Comparison integrity improvement.
  - Not a rewrite trigger by itself.

### DV-002 `test_mmap` Unit Failure

- Classification: preexisting-vanilla
- Summary: `test_mmap` fails in unit harness (memory exception/assertion mismatch patterns).
- Vanilla evidence:
  - Recorded as one of two failed unit tests in [metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline](metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline).
- Current branch evidence:
  - Reproduced with memory-focused run filter in Phase14B gap-closure execution.
- Decision impact:
  - Important correctness signal for memory/path auditing.
  - Treat as baseline-known issue unless behavior diverges materially.

### DV-003 Top-Level CTest Registration Gap (Vanilla)

- Classification: preexisting-vanilla
- Summary: Top-level CTest in vanilla build reported no registered tests.
- Vanilla evidence:
  - Captured in [metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline](metrics/baselines/vanilla_master_a6a044c_20260330T083320Z.baseline).
- Current branch status:
  - Branch has explicit labeled smoke lanes and contract lane.
- Decision impact:
  - Comparison caveat for upstream vs branch test topology.
  - Motivates preserving lane-gate workflow for forward work.

### DV-004 `test_elf` Unit Failure (Dynamic Relocation Path)

- Classification: preexisting-vanilla (resolved on current branch)
- Summary: `test_elf` segfaulted in vanilla and initial branch state during dynamic-loader bootstrap.
- Vanilla evidence:
  - Reproduced by `cd tests && bash run_unit_tests.sh` on vanilla worktree (failed `test_elf`).
- Current branch evidence:
  - Initially reproduced; now resolved after relocation/bootstrap fixes and syscall fixups.
  - Current branch unit harness now passes all tests.
- Decision impact:
  - Validates portability roadmap and control/gate test split.
  - Treated as an intentional correctness improvement relative to vanilla baseline.

### DV-005 `readlinkat` Guest Copy Length Bug

- Classification: preexisting-vanilla (resolved on current branch)
- Summary: `readlinkat` emulation path used unsigned return handling and unsafe copy semantics, which could trigger oversized guest copies after host syscall errors.
- Vanilla evidence:
  - Same syscall handler logic present in vanilla lineage.
  - Failure became visible while progressing dynamic-loader relocation support.
- Current branch handling:
  - Updated `readlinkat` path to use signed syscall return (`ssize_t`) and bounded host-buffer copy before guest write.
  - Eliminated observed crash path during `test_elf` relocation gate execution.
- Decision impact:
  - Correctness fix in syscall emulation path.
  - Improves robustness for dynamic-loader startup and path-resolution calls.
