# Carrier-Ladder Selected-Prefix Closure v1

**Status: active; Gates 0–2 reached a diagnosis-only implementation
checkpoint on 2026-08-29.** Oliver selected this boundary from clean source
checkpoint `e5c5ad6b0bfed7287a3f7892330755e77efc35ba`.

Parent: [Active work](../README.md)

Progress and exact evidence belong in the [execution log](execution-log.md).

## Objective

Determine why strict exact replay of the retained carrier-ladder selected
prefix reaches coarse state 213 without a selected action, then make that
prefix support-complete only where existing completed ordinary selected-row
authority already permits it.

The boundary may improve the benchmark-private exact-boundary observation
seam. It may not change ordinary solver search, row selection, scheduling,
bounds, incumbent publication, compiled strategy behavior, or product
defaults. It may never invent, reoptimize, or substitute an action.

## Starting Evidence

The completed
[Carrier-Ladder Exact Boundary Contract v1](../../../archive/2026-08-29-carrier-ladder-exact-boundary-contract-v1/result.md)
proved matched `off` / `record` / `recover` ordinary behavior and retained a
bounded private exact replay seam. The cumulative-10 witness stopped after
214 private work items at non-goal coarse state 213, before the requested
parent 1780, with no recovered members.

Current-source tracing establishes the concrete ownership mismatch:

- failed-prefix capture marks support by traversing the selected ordinary
  coarse `SparseRow` and its selected observed-choice successors;
- exact recovery replays the same selected action through strict native
  kernels from the authored exact item;
- every strict successor is coarsened and interned against the captured
  `SolveResult`; and
- a strict successor can therefore expose a coarse parent that was not in the
  ordinary captured reachability set, leaving its captured policy entry empty.

This is a candidate support-closure defect in the observational prefix, not
permission to broaden ordinary policy authority.

## Locked Scope And Authority

1. The existing joint-assembly candidate, selected rows, and completed-row
   facts remain the only action authority.
2. Strict replay uses existing native runtime semantics and exact kernels.
   No second mechanic or transition implementation is authorized.
3. A newly discovered strict-support parent may receive an action only when
   the same captured graph already owns a completed, valid selected row for
   that parent under the complete identity set.
4. If no such row exists, recovery must stop at the first typed support hole.
   It must report provenance and must not claim the downstream requested
   boundary was reached.
5. Private closure work remains observational. It cannot affect ordinary
   reachability, values, bounds, work order, incumbent choice, compiled
   strategy, or exact evaluation.
6. Product defaults remain off. No public C ABI, strategy vocabulary,
   mechanic, canonical data, GUI, or action-catalogue change is authorized.
7. New retained provenance and closure work must be canonical, bounded,
   cooperative, cancellable, identity-bound, and included in private memory
   and work accounting.
8. The preserved untracked file `0` is user state. Do not read, alter, stage,
   clean, rename, or commit it. Stop if any other unexpected dirty path
   appears.

## Gate 0 — Activation And Provenance Audit

Trace the first missing strict-support parent through exact predecessor,
selected operator/runtime semantics, strict outcome, coarse structural
projection, captured ordinary row, completed-row ownership, reachability,
frontier, and stop facts.

Pass when the first hole is reproducibly and structurally attributed without
a mechanic ruling or behavior change. Stop diagnosis-only if the necessary
provenance cannot be retained within the existing private contract.

## Gate 1 — Typed Strict-Support-Hole Witness

Add a versioned bounded internal/public diagnostic for the first strict-only
support hole. It must bind:

- predecessor exact stable key and coarse structural identity;
- selected coarse operator and strict runtime semantic identity;
- exact successor stable key, coarse parent and structural identity;
- captured policy-row index, row owner/operator, completion and reachability;
- goal, requested-entry, frontier, and captured-stop classification;
- complete source/graph/scope/vocabulary/economy/artifact/executable
  identities; and
- consumed work, retained/peak bytes, status, and first refusal.

Require deterministic identity and a focused synthetic control distinguishing
an absent row from a completed row excluded only by coarse support traversal.

## Gate 2 — Selected-Prefix Support Closure

If Gate 1 proves the first hole has a valid completed selected row in the same
captured graph, extend private prefix closure deterministically with that row
and continue strict replay. Repeat to a fixed point subject to the existing
private caps.

The closure must reject stale ownership, incomplete rows, invalid operators,
identity drift, unsupported kernels, or unbounded work. It must retain every
strict-discovered parent that influenced closure and distinguish:

- already coarse-reachable support;
- strict-discovered support closed from an existing completed row; and
- an unresolved strict-support boundary with no authorized row.

No alternative row selection or local reoptimization is allowed.

## Gate 3 — Cooperative And Identity Controls

Prove closure yields cooperatively, honors cancellation, releases retained
state/reservation at terminal cleanup, charges all private work and memory,
and has deterministic closure/witness/recovery identities. Cap refusal must
leave the ordinary result untouched.

## Gate 4 — Behavior-Neutral Qualification

At the substantial implementation milestone, run focused native and CLI
qualification with matched immutable cumulative-10 `off`, `record`, and
`recover` inputs. Require equal core solve identity and bit-identical ordinary
work, bounds, stop, policy, strategy, exact evaluation, and result. Only the
private request setting/budget and isolated diagnostics may differ.

If closure reaches the named parent, require deterministic exact member and
exit-contract identities. If it reaches another unsupported parent, retain a
diagnosis-only result naming that first truthful boundary.

## Gate 5 — Fresh CLI Evidence

Use the repository structured JSON CLI and saved artifacts for cumulative-10
matched controls, deterministic recovery replay, partial four-to-five, PDR,
same-side exact controls, a real private cap, a genuine cancellation, and
stable investigation-bundle replay. Record every revision, job, attempt,
report, artifact, strategy, exact-evaluation, bundle, prefix, closure,
support-hole, boundary, member, cap, work, memory, stop, hash, and comparison
identity. Do not use MCP, GUI, direct SQLite, catalog edits, or an alternate
runner.

## Gate 6 — Final Acceptance And Closeout

After retained source and focused evidence are coherent, run the complete
affected native/refinement/compile/evaluator/benchmark and CLI checks once,
release-WASM/web parity if common native telemetry changed, then
`powershell -File scripts/test.ps1` once. Finish with `git diff --check`, scope
and documentation audits, archive the boundary with a result, return
`HANDOFF.md` and `docs/active` to no active boundary, and make coherent local
commits ending with:

`Co-authored-by: Codex codex@openai.com`

Do not push or perform rendered visual review. If executable strategy behavior
changes unexpectedly, stop and replan before any simulator qualification.

## Valid Completion Outcomes

- **Diagnosis-only:** the first strict-support hole is fully attributed but
  has no already-completed selected row that can close it soundly.
- **Observational closure:** strict-only support is closed exclusively from
  existing completed rows, with ordinary behavior unchanged; the named exact
  boundary is either recovered or the next truthful unsupported boundary is
  identified.

Fragment composition remains parked. A usable exact boundary would enable a
later separately selected fragment/continuation boundary; it does not itself
authorize one.
