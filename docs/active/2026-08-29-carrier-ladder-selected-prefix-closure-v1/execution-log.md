# Carrier-Ladder Selected-Prefix Closure v1 — Execution Log

Parent: [Plan](plan.md)

## 2026-08-29 — Activation

- Selected by Oliver after the diagnosis-only exact-boundary closeout.
- Starting branch: `main`.
- Starting HEAD: `e5c5ad6b0bfed7287a3f7892330755e77efc35ba`.
- Starting parent: `22c00f50d8d0dd6ef8269cc0dc73a0eab0887bab`.
- Upstream relation: `main...origin/main [ahead 11]`.
- Worktree: clean except protected untracked file `0`.
- Protected file policy: not read, altered, staged, cleaned, renamed, or
  committed.

### Current-source provenance

- `capture_failed_prefix` closes captured reachability over the selected
  ordinary cached sparse row and selected observed-choice routing.
- `capture_incumbent_policy` materializes actions only for states marked in
  that captured reachability set.
- `ExactBoundaryRecoveryWork` replays selected actions through
  `ProductionPolicyOracle::boundary_selected_compact_row_cooperatively`.
- Strict successor interning coarsens every native exact outcome, calls
  `ensure_coarse_policy_parent`, and requires the captured policy entry for
  that parent.
- Therefore the observed state-213 refusal is consistent with a strict exact
  outcome whose coarse parent was absent from ordinary sparse-row support. It
  is not yet evidence that state 213 lacks a completed row in the captured
  graph.

No source or test mutation preceded activation. Routine suites are deferred
until a substantial implementation milestone, per Oliver's instruction.

## Gates 0–2 — First Strict Support Boundary

The source and retained prior report resolve the original ambiguity:

- coarse state 213 was already one of the captured prefix's 485 typed
  `unresolved_missing` stops;
- the capture walk reached it from positive selected-row support, called the
  existing progress-aware `select_initial_row(213)`, and found no completed
  valid row;
- it therefore was not an already-completed row accidentally omitted by the
  `policy_reachable` filter; and
- recover mode incorrectly excluded unresolved stops from its observation
  boundary set, so coarse-policy discovery tried to execute state 213 and
  produced the misleading `no selected policy action` invalid-prefix error.

Implemented the narrow private correction:

- unresolved captured stops are now non-goal absorbing observations for
  selected-prefix replay, never successful requested entries;
- recovery retains one bounded first exact predecessor/action/outcome edge,
  plus a streaming reached-stop count and deterministic identity on every
  terminal/refusal path;
- the public projection binds collision-checked exact-key identities and word
  counts, complete coarse structural keys, selected coarse/strict operators,
  selected-action semantic identity, exact probability bits, stop kind,
  captured policy row, coarse reachability, and completed-row authority;
- full exact keys remain internal; public member samples likewise use bounded
  identities plus complete exported item state so telemetry cannot be
  dominated by large exact keys; and
- no ordinary row, value, policy, scheduler, bound, incumbent, compilation,
  or product authority reads the new result.

The first implementation accidentally retained every reached-stop event when
recovery ended at a cap, producing a 146,997,742-byte private diagnostic.
That defect was found with temporary stderr byte instrumentation, then fixed
by streaming the count/identity while retaining only the first edge. The
instrumentation was removed. Terminal/refusal finalization now owns the same
bounded path.

### Focused checkpoint

- Native build: passed.
- Focused `--solver-refinement-only`: 379 checks, zero failures.
- The new structural control proves a non-goal unresolved observation stop is
  complete absorbing support but does not become the requested entry.

### Exploratory CLI and direct witness ledger

The unchanged deterministic recover revision remains
`case-rev-03e9346553b8b7367f6397406fc733ae` / SHA-256
`03e9346553b8b7367f6397406fc733aea87f09a8274aa69b2c85c0289c944f73`.

| Role | Job | Attempt | Result |
| --- | --- | --- | --- |
| pre-bounded projection v1 | `job-13aec74e-6213-477f-85c0-d2bf0ae098cf` | `attempt-e59361bc-a5f3-44fb-aae9-f13d0c992480` | ordinary result unchanged; telemetry exceeded 2 MiB |
| one-sample projection v2 | `job-98775b38-abdd-4350-9d04-a3a38861f78b` | `attempt-522ae46c-4d65-4a01-a3e5-5b4f29115808` | ordinary result unchanged; telemetry still exceeded 2 MiB |
| hashed exact-key projection v3 | `job-cda47a97-3b1f-4ace-ac88-c6680ae6c2a8` | `attempt-95a4759d-437f-4595-b910-051e2eb3e05d` | ordinary result unchanged; unfinalized event vector still exceeded 2 MiB |
| hashed member projection v4 | `job-3efe8d07-3b37-4c83-8cb9-2bebfe37ba1d` | `attempt-36fc0413-51c8-43ff-91b6-1f7d878dde73` | ordinary result unchanged; unfinalized event vector still exceeded 2 MiB |

Diagnostic-only telemetry-cap revisions were
`case-rev-359f92e25e091577ef10576017963be4` (8 MiB) and
`case-rev-3d6570053fe4f40194993062da01a811` (64 MiB). They were used only to
prove the retained-vector defect; one transient Windows partial-file replace
failure and long generated strategy path are non-solver harness evidence.

After bounded streaming was implemented, direct execution through the same
native corpus case completed with telemetry under the original 2 MiB cap.
Ignored evidence path:
`build/solver-lab/debug-selected-prefix-report.json`.

The exact result is:

- recovery: `resource_cap` / `max_transitions`;
- exact states / rows / transitions: 26,927 / 892 / 9,995,835;
- private work / peak owned bytes / wall: 3,933 / 176,454,520 / 226,189 ms;
- requested members: zero;
- reached unresolved-stop edges: 4,710,858;
- reached-stop identity: `e7070f4026fa7ca4`;
- first predecessor exact identity / words: `8d56e91d0520b518` / 81;
- predecessor coarse state: 0;
- selected coarse/strict operator: 5 / 5 (`chaos` in the pinned product action
  list);
- selected semantic identity / words: `e13ae2cea3ca477b` / 28;
- first stopped exact identity / words: `833a6a9002e3c045` / 85;
- stopped coarse state: 213;
- exact probability bits: `3ee95f1f576b7265`; and
- captured policy row: none; captured coarse reachable: false; completed
  selected row: false; disposition:
  `unresolved_no_completed_selected_row`.

This closes Gate 2 diagnosis-only. No selected-prefix support extension is
authorized, no requested exact boundary was recovered, and no policy/cost
improvement was found. The first owner is incomplete completed-row/service
coverage after the selected root Chaos row, not a missing action-catalogue
entry and not fragment composition. The private transition cap has been
observed; Gate 3 still requires deterministic replay and genuine cancellation
controls on the retained final executable.
