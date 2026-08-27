# Solver Stabilization And Action-Family Controls

**Status: completed 2026-08-27.** Selected by Oliver on 2026-08-27.

Parent: [Documentation archive](../README.md)

## Objective

Restore a truthful broad native-solver acceptance baseline, add engine-owned
controls for disabling coherent solver action families, expose those controls
as diagnostic Calculator toggles, and use one focused PDR ablation to determine
whether temporary bench programs materially own the current broad-row proof
cost.

This milestone is diagnostic infrastructure plus stabilization. It does not
promise an exact four-mod PDR solve, checkpoint/replay, resumable broad-row
certification, or a five-goal qualification.

## Fixed Boundaries

- Default Calculator and solver behavior must remain unchanged when no family
  is disabled.
- Family identity and generated-program filtering belong to the native engine;
  TypeScript may present the contract but must not infer action mechanics from
  action IDs.
- Disabling a family must remove both directly admitted primitives and
  automatically generated operators whose executable program depends on that
  family.
- A diagnostic solve under a restricted family set is exact only within that
  requested action envelope. It must not be described as globally exact for
  the unrestricted envelope.
- The retained PDR control is the current four-mod Conquest Lamellar witness.
  Run only the minimum control/ablation needed to attribute work; do not run a
  broad benchmark matrix.
- Do not begin checkpoint/replay, resumable destructive-row work, five-goal
  tuning, state-model removal, or mechanic changes in this boundary.

## Gate 0 — Truthful Native Baseline

1. Identify the current broad native-solve target and reproduce the historical
   13 goal-progress-gated expectation failures and empty-strategy parse cascade,
   or prove later source changes already removed them.
2. Repair stale expectations or the test harness at the true owner. Do not
   weaken exactness, policy executability, or strategy parsing contracts.
3. Leave the complete native solve target green before adding the family
   contract.

## Gate 1 — Engine-Owned Family Contract

1. Audit existing action, automatic-candidate, telemetry, and refinement
   taxonomies before defining another one.
2. Add the smallest stable family vocabulary that can name the useful solver
   mechanisms, including temporary bench programs and generated Imprint
   programs.
3. Parse disabled families in the native goal contract and carry the resolved
   filter through direct admission and automatic option synthesis.
4. Reject unknown family names with a precise request error.
5. Add focused native tests proving:
   - an omitted/empty filter preserves the admitted envelope;
   - direct members are excluded;
   - generated programs cannot reintroduce disabled dependencies; and
   - result telemetry states the requested restriction.

## Gate 2 — WASM And Calculator Diagnostics

1. Thread the typed request through the WASM facade and TypeScript goal model.
2. Replace Calculator-owned ID-prefix grouping for solver controls with native
   family metadata where it overlaps this contract.
3. Add advanced diagnostic toggles to the solve surface, defaulting to all
   families enabled and scoped to the current solve request.
4. Explain that a disabled-family run solves a restricted action envelope.
5. Update nonvisual web/WASM tests and rebuild the release WASM module.

## Gate 3 — Focused PDR Attribution

1. Run the retained PDR witness with current product defaults and a bounded
   diagnostic allowance only if no directly comparable current artifact
   exists.
2. Run one matched ablation with temporary bench programs disabled.
3. Compare termination owner, logical/V3 reforge work, completed/partial/
   certified obligations, states/frontier, policy improvements, lower/upper,
   and wall time.
4. Characterize the result honestly. A faster restricted solve is attribution,
   not evidence that temporary bench programs should be removed from product
   scope.

## Gate 4 — Acceptance And Handoff

Run one final acceptance pass after implementation:

- fresh native build;
- complete affected native solver/API/registry/compile/eval suites;
- release WASM rebuild;
- `npm test` and `npx tsc --noEmit` in `apps/web`;
- the full `powershell -File scripts/test.ps1` pipeline once;
- `git diff --check`.

If compiled-strategy verification is required by a changed publication result,
use 10,000 Simulator runs. No rendered browser review is included.

Record the actual result, archive the completed boundary, update stable solver
and product documentation, and leave `HANDOFF.md` naming the next owner. The
expected successor is either resumable broad destructive-row certification or
checkpoint/replay, informed by the family attribution from this milestone.
