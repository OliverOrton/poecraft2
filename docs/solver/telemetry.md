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

The native `carrier_bound_attribution.dirty_order_counterfactual` field compares
legacy and neutral-extra order on the same frozen incremental eligible set.
Its five bounded sample strata carry run-local state identity, old/new rank,
admission time and rows observed at finalization, with repeated and omitted
position counts. Rank reversal establishes an ordering effect; final row counts
alone do not attribute a useful continuation or root-cost gain to that reversal.
The treatment flag is a separate input identity. First finite proposal,
first independently verified policy and final original-root cost must be read
separately. [A5 diagnostic and pair](../active/2026-09-25-dirty-progress/README.md#j1--bounded-causal-observation).

The private `native-seed-progress-observation` records actual
`select_joint_policy_seed_row` calls and its complete first-policy gate:
high-impact uppers, incremental generation and absence of an output incumbent
object. Gate-on comparisons use the same completed priced rows and preserve
pending-route precedence; bounded witness samples have their own omission
count. Gate-off reason counts can overlap and are not exclusive strata. The
observer's computation is part of the instrumented run's wall time; it cannot
be read as uninstrumented Current performance. The aggregate
call/gate/eligible-row counters are the denominator, not
the sample. `measured_gate_inactive` means no gate-on comparison was possible;
it does not mean an active gate produced zero reversals. On the complete A5
observation, 19,667 calls were gate-off, all with an output incumbent object;
11,291 had no eligible row. The observer returned the original selection, and
its instrumented run identity and cost belong to the [K1 record](../active/2026-09-25-seed-retention/README.md#k1--s-selector-observation).

## Reading Rules

Compare logical work, evaluator-specific effort, wall time, and owned memory separately. Raw V3 effort is not the V1-equivalent cap and is not a hardware-independent runtime estimate.

An upper requires a compatible executable incumbent, not merely a finite field. Lower/upper equality needs provenance, action coverage, and the numerical/classification contract. Simulator rate and mean are sampled evidence, not graph-evaluated expectations.

Null or absent timing means uninstrumented or unavailable. It is not zero duration. Distinguish missing, non-finite, refused, incomplete, and valid zero values.

A lookup count is a call population, not distinct-state coverage. Cache hits and repeated maximum selections do not by themselves show newly covered semantic states.

<a id="strict-preparation-timer-scope"></a>
### Strict preparation timer scope

The current `strict_carrier_discovery` finalization field is assigned from a
persistent session's elapsed clock after selected-locator discovery and
partition-node preparation. On later passes it is another cumulative prefix,
not a fresh active-duration increment. Do not add those snapshots or subtract
one from a separately instrumented child timer as though they shared exclusive
scope. Direct-call and child-resume timing need their own owner and coverage;
an unfinished pass may leave this field at zero despite strict work before
Finish. The [S0 evidence](../active/2026-09-24-strict-preparation/README.md)
keeps those domains and the unresolved remainder distinct.

<a id="cost-only-entry-query-scope"></a>
### Entry-query populations

The existing `ordinary_entry_query` counters describe compiler-authored
decision requests and physical entries visited by a completed native query.
`unsupported_operation` is a declaration count; `visited`, hidden-context,
unavailable-tail and combined clean-guard refusals are entry populations. A
dirty nonempty selection sample is not a clean one-goal-missing shortlist.
`queries: 0` means the query was not called, not that it proved zero eligible
entries. The [L0 investigation](../active/2026-09-24-cost-only-continuation/README.md)
discarded an A4 zero from the wrong hook and used completed pre-Finish queries
for both scoped eligibility findings. Those diagnostic source hooks were
removed; ordinary telemetry remains passive under the default activation.

The later [N0 native boundary query](../active/2026-09-24-native-boundary-repair/README.md)
included all 55 compiler-authored A5 decisions and 53,851 certified reached
physical entries, beyond the old two-declaration selector. Its persistent
context, debt, extra-affix and below-tier flags overlap; below-tier is counted
as slots, while the other cited flags count entries. No retained ordered
first-failure census or immediate macro-spend attribution follows from them.
The case-level `refused_unsupported_action` reports a separate Chaos renewal
compatibility refusal; the selected policy remains `bounded_feasible`, and the
entry query and candidate root evaluations completed. Report each of these
statuses at its own owner rather than combining them into an unsupported-entry
or economic-failure count. The temporary query and candidate hooks were removed.

Fewer rows before a common timeout can reflect preparation consuming the budget. Claim avoided work only against a shared target or an otherwise suitable matched comparison. A larger local lower may leave the complete model unchanged when another action/family remains limiting.

## Context-efficient use

Keep full reports and strategies in their existing artifact locations. Use targeted CLI/report projections for ordinary analysis. Expand to the original evidence only when the question depends on it; do not make every linked report part of startup.

Substantive research records its question, source identities, decisive observations, and limits in [research](research.md). General arguments belong in the mathematical chapters, not another telemetry addendum. Current experiment values belong in generated or original results, not this stable field guide.

## Adding a field

The cheap progress cursor reports effective continuation/retention options,
configured limits, measured setup spans and fixed entry-query counters. The
[activation audit](../active/2026-09-15-verified-delivery/activation.md) defines their
populations and public/native distinction. Counters aggregate declarations and
physical entries already visited by the existing bounded query. Zero uncalled
queries do not prove that every possible entry was rejected. Availability observes
the retained artifact owner's completed flags and a scope-guarded projection of
complete evidence in the active strict owner. The latter is updated at work
boundaries and cleared when that owner exits. Full compatibility remains a
publication check, not a graph scan on every read.

Focused upper work sets both scheduler mode flags. Its observation gives upper
role/owner precedence while preserving both flags and numerical control. Worker
timing records begin, maximum stepped call with input/output owner, lifecycle
cursor and quantum, progress serialization, export and cleanup separately.
Source event time, worker observation, UI intent and usable readiness are distinct.
These fields extend JSON; the public `pc_solve_progress` remains 200 bytes.

Define the counter in the owning phase, its unit/population, whether it is cumulative or retained, reset/snapshot behavior, and resource attribution. Then update collection, serialization, and actual consumers that expose it. Test only the affected contracts and outputs.

A field that never reaches its intended consumer is an observability defect. Repairing it is not evidence of improved crafting policy or exact closure. [Change impact](../foundation/change-impact.md) identifies the downstream path without imposing a full-pipeline ritual.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/telemetry.md) and [solver-internals.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md), [benchmarking.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md), [2026-09-05-native-applied-reforge-preparation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
