# Relocation Loader Policy Foundation

## Why This Exists

Recent upstream ARM64 dynamic-interpreter changes make relocation ownership explicit for some startup paths, while current x86_64 work assumes host-side bootstrap relocation behavior. That mismatch creates review friction and merge conflicts in relocation-related PRs.

This document defines a small, explicit policy direction so relocation behavior is deterministic, architecture/path-aware, and testable.

## Goal

Introduce explicit loader relocation ownership policy with safe defaults, replacing implicit assumptions and tactical one-off behavior.

## Non-Goals

- No broad ELF loader rewrite in this step.
- No immediate expansion to full IFUNC support scope.
- No performance claims without before/after measurement.

## Proposed Policy Surface

1. Relocation ownership mode:
   - `Auto` (default)
   - `HostBootstrap`
   - `GuestOnly`
2. Keep existing IRELATIVE handling mode as a separate policy axis.
3. In `Auto`, resolve mode by runtime path (not only architecture macro).
4. Fail fast when selected policy conflicts with observed runtime path assumptions.

## Design Principles

- Prefer explicit policy over hidden behavior.
- Preserve correctness first; avoid silent fallback when uncertain.
- Keep architecture/path differences visible in code and tests.
- Keep changes small and reviewable.

## Immediate Engineering Plan

1. Add option plumbing and policy enum in loader options.
2. Centralize relocation ownership decision in one loader decision point.
3. Keep current upstream ARM64 safety behavior intact while introducing explicit policy semantics.
4. Add deterministic tests for both ownership lanes on supported paths.

## Pending PR Alignment

### PR #73 (relocation behavior + tests)

- Rebase onto current upstream.
- Preserve upstream ARM64 dynamic-loader safety behavior.
- Keep x86_64 relocation policy improvements.
- Update wording to clarify architecture/path ownership boundaries.

### PR #74 (guest toolchain fix)

- No policy coupling expected.
- Keep standalone.

### PR #75 (measurement tooling)

- Keep as infra lane.
- Use to validate no performance regression from policy clarification.

## Merge Conflict Strategy

When conflicts occur in loader relocation code:

1. Keep upstream ARM64 guard semantics.
2. Reapply x86_64 policy improvements in non-conflicting sections.
3. Add explicit TODO/reference to migrate to policy-based runtime selection.

## Validation Expectations

- Existing unit gates remain green.
- New policy tests must cover success and fail-fast lanes.
- No behavior change claims without matching tests.

## Next Step

Implement policy enum + selection skeleton in a narrow follow-up patch, with no broad behavioral expansion in that first code step.
