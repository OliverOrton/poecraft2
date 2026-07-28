# Exact Quotient Audit Report

**Status: final.** This report records the completed 2026-07-27 audit.

Parent: [Milestone archive](README.md)

## Result

The implementation is not operating against a broken quotient baseline.
Completed exact refinement demonstrably merges, and the suspected raw Unveil
modifier identity is required for executable policy behavior.

The actual defect was evidentiary: several cap-stopped reports described equal
strict/working counts as if a completed exact quotient had proved zero merges.
Those runs took the explicitly incomplete shadow-only path. The stable docs and
concise evidence now say so.

## Completed positive witness

The existing synthetic alt-spam solve already contained a small exact quotient
oracle, but its regression allowed equality. The retained test now requires:

- completed and converged strict and quotient solves;
- 10 strict states reduced to 3 quotient classes;
- 7 applied exact behavioral merges and a non-identity representative map;
- equal strict/quotient start value and selected start action; and
- zero observation-signature mismatches.

This protects the live merge path without relying on the slower historical
57,722-to-3 Chaos corpus.

## Observation-choice identity

`row_observation_cache_key` projects the Bellman, observation, and actual state
IDs through the current partition but retains `choice.mod_id` literally. That
asymmetry is deliberate.

The completed observed Veiled Chaos plus Unveil witness contains eight literal
offered modifiers. Two projected successor classes each contain multiple
literal offers, and the largest contains seven. Successor value behavior can
therefore coincide while the concrete observed menu differs.

Extraction lifts one representative's literal preferences to its quotient
members. Compilation then emits each modifier twice:

1. `has_unveil_option(mod_id)` selects an actually offered option; and
2. `unveil(mod_id)` performs the selected operation.

Removing the literal modifier from state equivalence would allow states with
different offered menus to share one representative preference list. The
compiled policy could then request a modifier absent from a concrete member's
offer. Raw modifier identity remains part of exact observe-then-decide
behavior.

## Incomplete shadow semantics

Completed partition refinement is skipped whenever a resource cap was hit or
the expansion queue remains nonempty. With full evidence enabled, the solver
instead calculates `shadow_state_signature` over the observed strict payload:
expanded status, literal row slices, strict successor/choice IDs, operator
payloads, resources, and literal modifier IDs.

Unexpanded states have no completed action rows. The shadow grouping is
therefore neither a candidate exact quotient nor a proof of non-equivalence.
The retained cap-stopped test requires:

- `shadow_only=true`;
- strict working identities retained as `quotient_states`;
- a separately reported shadow-class count;
- no behavioral representative map; and
- zero applied `exact_behavioral_merges`.

The historical real three-T1 reports make the distinction concrete:

| Run | Strict / working | Expanded | Shadow classes | Exact quotient |
| --- | ---: | ---: | ---: | --- |
| First expansion | 74,563 / 74,563 | 1 | 2 | not run |
| Production caps | 200,000 / 200,000 | 55,088 | 55,090 | not run |

The production shadow count closely follows expanded states because expanded
rows carry observed payload while the unfinished frontier does not. It cannot
be interpreted as either 144,910 safe merges or zero possible completed-graph
merges.

The four hard natural-T1 reports likewise reached the 200,000-state cap. Their
199,981/199,967/199,983/199,976 witnessed split counters came from the
incomplete literal-shadow comparison, not completed partition refinement.

## Action-relative reforge reuse

The preserved-boundary reuse proposed after the factorization study was
already implemented:

- `exact_reforge_kernel_signature` keys by action plus the collision-checked
  complete preserved base;
- the reforge cache shares one immutable exact roll distribution for states
  differing only in wiped junk;
- sparse expansion reuses the row payload and successor envelope; and
- the shared distribution identity enqueues its fringe only once.

This is exact action-relative reuse, not an outer-state merge. It saves repeated
work after one identical kernel exists. It cannot avoid enumerating and
materializing the first unique carrier's large successor distribution, so it
does not repair the one-expansion hard cases.

## Identity classification

| Location | Retained identity | Classification |
| --- | --- | --- |
| Completed row observation | Planner/action and resource payload | admitted action and cost observation |
| Completed row observation | `choice.mod_id` | executable offered-choice label |
| Completed row observation | choice state IDs | projected behavioral identity |
| Completed kernel cache fast key | strict row slice offsets | implementation-only lookup; fallback behavior signature compares the projection |
| Incomplete shadow signature | row slices and strict state IDs | literal diagnostic payload, non-authoritative |
| Action-cardinality diagnostic | strict offsets/state IDs | strict observation inventory, not a quotient |

## Verification

- `powershell -File scripts/build.ps1`: passed using the GCC fallback; the
  existing MinGW optimizer warning in `solver_solve_heuristics.cpp` remained.
- `build/engine/poecraft_engine_tests.exe --solver-solve-only`:
  532 checks, 0 failures.
- `build/engine/poecraft_engine_tests.exe --solver-compile-only data/compiled/current`:
  359 checks, 0 failures.
- Measurement executable SHA-256:
  `772ca734833b3a16eb0f128b3b21fe05cc789908f58f5f7286da9c900681a937`.

No WASM rebuild or web test was required because production behavior and all
public contracts are unchanged.

The machine-readable record is the
[evidence summary](../../../fixtures/solver-scaling/v1/evidence/exact-quotient-audit-summary.json).
