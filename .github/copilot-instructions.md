# TinyKVM Copilot Collaboration Contract (Draft)

## Purpose
This file defines how Copilot should collaborate in this repository so changes are incremental, testable, and evidence-driven.

## Primary Goals
1. Functional correctness parity with vanilla is mandatory.
2. Prefer targeted refactors over broad rewrites unless evidence supports rewrite.
3. Keep the C API contract stable while evaluating C++ internals.
4. Reach and maintain the ability to run the full vanilla correctness test suite in this branch.
5. Maintain reproducible baselines to detect regressions early.
6. Preserve TinyKVM performance intent: native-like machine performance with minimal overhead.

## Working Agreement
1. Work phase-by-phase with explicit checkpoints.
2. For each meaningful change, do all of the following:
   - implement the smallest viable change,
   - run relevant build/tests,
   - capture outcomes,
   - update docs/status,
   - commit with a phase-scoped message.
3. Stop-the-line rule: if behavior regresses versus vanilla, do not proceed with new feature/refactor work until the cause is classified and documented.
4. Do not assume a behavior is new without comparing against vanilla baseline evidence.
5. If uncertainty exists, record assumptions explicitly in docs before making high-risk changes.

## Decision Policy: Keep, Refactor, Rewrite
Use this order by default:
1. Keep when behavior is correct and maintainable.
2. Refactor when localized structure changes reduce risk or complexity.
3. Rewrite only when one or more trigger conditions are met and documented:
   - correctness cannot be reliably achieved by local refactor,
   - performance or memory goals cannot be met with incremental changes,
   - architecture blocks required capabilities,
   - operational constraints require fundamentally different structure.

## Test and Validation Policy
1. Prefer fast, scoped validation first, then broader lanes.
2. Treat smoke/contract coverage as required for C API-facing changes.
3. Treat local smoke/unit lanes as interim gates; they do not replace full-suite parity with vanilla.
4. Run baseline gate checkpoints for phase boundaries.
5. Any failing test must be classified as one of:
   - preexisting (reproducible in vanilla baseline),
   - introduced (caused by current branch changes),
   - unknown (insufficient evidence; investigate before proceeding).
6. Unknown or introduced failures block phase advancement until disposition is recorded and accepted.

## Vanilla Full-Suite Parity Plan
### Discovery and Execution Checklist
1. Identify all correctness lanes executed by vanilla (CI workflows, CTest labels, test scripts, and unit/integration harnesses).
2. Record exact vanilla invocation commands, required environment variables, and dependencies.
3. Run vanilla suite and capture baseline artifacts (pass/fail counts, durations, notable logs).
4. Reproduce the same suite in this branch with equivalent commands and environment.
5. Classify every mismatch as preexisting, introduced, or unknown and log disposition.
6. Open explicit follow-up items for unknown mismatches before proceeding with new refactor work.

### Parity Tracking Table Fields
1. Lane name and scope.
2. Vanilla command.
3. Current-branch command.
4. Vanilla result (pass/fail/skip + counts).
5. Current-branch result (pass/fail/skip + counts).
6. Delta summary.
7. Classification (preexisting/introduced/unknown).
8. Evidence links (logs, baseline artifacts, docs).
9. Owner and next action.

### Full-Suite Parity Exit Criteria
1. Every vanilla correctness lane is runnable in this branch with documented commands.
2. No introduced correctness failures remain open.
3. Every remaining failure is either preexisting or explicitly accepted with documented rationale.
4. Unknown classifications are zero.
5. Baseline comparison shows no unapproved drift for correctness and key performance indicators.

## Baseline and Metrics Discipline
1. Keep baseline snapshots append-only with clear phase labels.
2. Use compare thresholds/policies to flag drift.
3. Record key metric deltas in status documents when they affect decisions.
4. Never silently relax thresholds; document rationale and approval context.

## Documentation Requirements
For each completed phase, update relevant docs with:
1. What changed.
2. Why it changed.
3. What was validated.
4. Current risks and open questions.
5. Recommended next step.

## Iteration Tracking Requirements
For each meaningful implementation iteration (not just phase boundaries), update tracking artifacts before closing the checkpoint:
1. Update the working status document with current state, active task, and next task.
2. Record validation evidence (commands, pass/fail/skip counts, and key deltas).
3. Record baseline checkpoint outcomes when lane timing or behavior is relevant.
4. Record provenance classification for notable failures (preexisting, introduced, unknown).
5. If any item is intentionally deferred, log owner and explicit follow-up action.

## Documentation Topology (Avoiding Doc Sprawl)
Use a small, stable set of canonical docs and prefer updating them over creating new files.

Canonical docs:
1. `docs/rewrite_program_status.md`: single living working document (where we are, what we are doing now, and where we are going).
2. `docs/subsystem_rewrite_plan.md`: phase/milestone plan and scope boundaries.
3. `docs/deviation_provenance_log.md`: provenance and classification of notable mismatches/failures.
4. Component audit docs by phase (for example `docs/component_audit_phase14a.md`) only when a phase explicitly requires a separate audit artifact.

New documentation files require explicit justification in status updates, including:
1. Why existing canonical docs are insufficient.
2. Expected owner.
3. Sunset or merge-back plan to avoid stale/ignored docs.

## Provenance and Risk Tracking
1. Log notable deviations with source classification (vanilla vs current branch).
2. Do not conflate inherited defects with newly introduced regressions.
3. Keep a visible list of high-risk paths under active audit.

## Commit and Branch Conventions
1. Use focused commits aligned to a single phase checkpoint.
2. Commit messages should include phase tag and intent.
3. Avoid mixing unrelated refactors in audit commits.
4. Keep branch history readable for post-hoc forensic review.

## Communication Style Expectations
1. Be direct and evidence-based.
2. Prefer concrete findings over broad summaries.
3. When asked for review, lead with risks/findings ordered by severity.
4. Clearly separate facts, assumptions, and recommendations.

## Practical Defaults for This Repository
1. Preserve existing public APIs and behavior unless phase goals explicitly require change.
2. Prefer minimal diff patches.
3. Validate with relevant CMake/CTest lanes and unit subsets before broad runs.
4. Keep generated artifacts out of commits unless intentionally tracked.

## Draft Status
This is a draft for interactive review. Adjust wording and strictness before treating as final policy.
