# Telemetry

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

Telemetry describes work and evidence; it does not grant lower, upper, scheduling, or publication authority. This page is a lookup guide, not a checklist to dump into every research session.

## Ownership

Each phase records its typed counters. `solver_solve_telemetry.cpp` snapshots and accounts for them, while `solver_solve_telemetry_json.cpp` serializes the public JSON. Native phase owners remain responsible for the meaning of their fields.

Observational projections must not be read back as decisions about mechanics, admission, pruning, caps, or incumbent selection. A diagnostic process isolated from ordinary solving is still a different experiment whose additional cost and identity must be disclosed.

## Compact Triage Set

Start with the question, then request the smallest corresponding projection:

| Question | Evidence to inspect |
|---|---|
| Why did this solve stop? | Phase/phase owner, termination, first named cap, policy availability |
| Are these bounds comparable? | Target/start/goal/economy/action identities, lower provenance, verified incumbent identity |
| Is the action envelope complete? | Canonical action/family accounting, open/deferred/interrupted obligations |
| Is exact row construction expensive? | Row family, completed rows, logical work, evaluator-specific effort, active build and cursor costs |
| Did strict proof progress? | Selected versus alternative obligations, frontier insertions, splits/generations, certified rows/classes |
| Where is memory held? | Live and peak ownership by phase, active cursor payload, ProofStore, compiler/evaluator output |
| Did a candidate survive resumption? | Candidate identity, missing continuation, yield/resume/release state, reclaimed payload, subsequent ordinary work |
| Is the returned policy executable? | Emitted artifact identity, properness, success/off-policy mass, prices, exact evaluation, reconciliation |
| Did a lower help? | Preparation cost, compatible lookup/selection counts, complete-model value, actual retirement/gap/exact outcome |

The field vocabulary is defined by the existing native/progress/report structures. Missing fields must be reported as unavailable, not invented from an older run. Broad `phase` and narrower `phase_owner` are not interchangeable.

## Full Evidence

Full evidence adds bounded samples of carrier/action attribution, automatic candidates, proof patterns, row structure, observations, and compilation/evaluation structure. Sample limits and omitted counts remain visible. Samples locate witnesses; complete aggregates or full records support population statements.

Generated-operator lineage joins registry roles, fixed and automatic programs, priced support, ledger state, retained rows, joint-policy attempts, and consumption. A program may depend on more than one family, so per-family relations are not necessarily disjoint.

Capture-time and terminal-time observations answer different questions. A state without rows at capture may receive ordinary service later. A two-stage witness can establish that fact without implying the first snapshot was corrupted.

## Reading Rules

Compare logical work, evaluator-specific effort, wall time, and owned memory separately. Raw V3 effort is not the V1-equivalent cap and is not a hardware-independent runtime estimate.

An upper requires a compatible executable incumbent, not merely a finite field. Lower/upper equality needs provenance, action coverage, and the numerical/classification contract. Simulator rate and mean are sampled evidence, not graph-evaluated expectations.

Null or absent timing means uninstrumented or unavailable. It is not zero duration. Distinguish missing, non-finite, refused, incomplete, and valid zero values.

A lookup count is a call population, not distinct-state coverage. Cache hits and repeated maximum selections do not by themselves show newly covered semantic states.

Fewer rows before a common timeout can reflect preparation consuming the budget. Claim avoided work only against a shared target or an otherwise suitable matched comparison. A larger local lower may leave the complete model unchanged when another action/family remains limiting.

## Context-efficient use

Keep full reports and strategies in their existing artifact locations. Use targeted CLI/report projections for ordinary analysis. Expand to the original evidence only when the question depends on it; do not make every linked report part of startup.

Substantive research records its question, source identities, decisive observations, and limits in [research](research.md). General arguments belong in the mathematical chapters, not another telemetry addendum. Current experiment values belong in generated or original results, not this stable field guide.

## Adding a field

Define the counter in the owning phase, its unit/population, whether it is cumulative or retained, reset/snapshot behavior, and resource attribution. Then update collection, serialization, and actual consumers that expose it. Test only the affected contracts and outputs.

A field that never reaches its intended consumer is an observability defect. Repairing it is not evidence of improved crafting policy or exact closure. [Change impact](../foundation/change-impact.md) identifies the downstream path without imposing a full-pipeline ritual.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/telemetry.md) and [solver-internals.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md), [benchmarking.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md), [2026-09-05-native-applied-reforge-preparation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
