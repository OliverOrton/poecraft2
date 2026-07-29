# High-Impact Executable Uppers - Final Rejection

**Status: rejected under the frozen Gate 0 qualification rule.**

## Decision

The implementation candidate is rejected. It produced neither strict
Fossil/Harvest upper-Q admission nor the required 10% deterministic reduction
of a completed qualifying row. Experimental production behavior and interface
changes were restored. This milestone is not successful.

The retained source change is observational provenance telemetry only. It
samples the states that dominate completed Fossil, Harvest, and
goal-relevant Essence upper Q, including carrier/progress state, parent-row
influence, selected continuation/fallback, materialized actions, contribution,
and the existing executable witness identity/properness status.

## Gate 1 Control

The behavior-disabled primary control matched the frozen `3a6d191` result:

| Field | Frozen and retained value |
| --- | ---: |
| Root lower / upper | `432.4068529534326` / `60341416.98784247` |
| Discovered / expanded | `200000` / `8908` |
| Rows / transitions | `56413` / `1188077` |
| Reforge work | `14246493` |
| Q rounds / states | `9` / `9216` |
| Rows reconsidered / recomputed | `75` / `0` |
| Kernels / reuse | `8` / `8907` |
| Admitted / non-improving / unresolved / unevaluated | `0 / 0 / 7 / 195969` |
| Cap | `max_discovered_states` |
| Transition / policy hash | `d3c8789915cd57b4` / `8b2a568f3c9cfd35` |

The channel considered `569970` deterministic provenance candidates, retained
the top `32`, explicitly omitted `569938`, and retained `42800` JSON bytes
under the `1048576`-byte telemetry cap. Finalization attaches the strings only
after scheduling, hashes, caps, and memory accounting are frozen. The
structural telemetry fields and strings are excluded from solver-owned
accounting.

## Candidate Experiments

The investigation reused the shared exact graph and the existing focused
upper Howard/SCC evaluator throughout. The sequence of bounded experiments
was:

1. invoke focused upper evaluation at the gated incremental cap handoff;
2. seed it from the existing gated primitive-renewal incumbent;
3. require a finite executable frontier for every ranked upper-policy exit;
4. evaluate completed delayed rows as one compatible temporary policy and
   promote only selected rows from a strictly cheaper proper witness;
5. switch from root-exhaustive carrier order to resumable operator-major
   state/action pairs;
6. take one bounded Q-directed partial-state checkpoint;
7. schedule Fracture successors directly rather than broadly expanding every
   action at each carrier; and
8. alternate bounded Harvest/Fracture waves to test deeper preserved-progress
   continuations.

These experiments exposed real architectural constraints but no qualifying
policy. Broad expansion spent discovery on unrelated anchors. Direct
state/action scheduling preserved headroom, but strict per-carrier rows still
covered too little expected policy occupancy before the frozen discovery cap.
Kernel reuse reduced distribution work but could not legally substitute one
selected row or scalar upper for another strict carrier.

## Final Primary Rejection

The definitive primary candidate used the frozen resource limits and
cooperative stepping:

| Field | Result |
| --- | ---: |
| Root lower / upper | `432.4068529534326` / `60341416.98784247` |
| Root upper change | `0` (`0%`) |
| `harvest_reforge:attack` lower / upper | `434.7225946216303` / `60341418.63180959` |
| Qualifying row upper change | `0` (`0%`) |
| Focused upper passes requested / proper / rejected | `5 / 5 / 0` |
| Admitted / unresolved / unevaluated | `0 / 1057 / 2815` |
| Q rounds / selected states | `2 / 144` |
| Rows / transitions | `2228 / 842726` |
| Reforge work | `16746695` |
| Kernel evaluations / carrier reuse | `14 / 1203` |
| Completed rows recomputed | `0` |
| Live / peak native owned bytes | `84002959 / 134297256` |
| Solve / total wall ms | `13379.0494 / 13950.6397` |
| Cap | `max_discovered_states` |
| Transition / policy hash | `d4346e90f923332c` / `8b2a568f3c9cfd35` |

The candidate result is diagnostic only. Its altered transition hash reflects
bounded work order, not semantic divergence of a completed solve. It did not
install a continuation or materially change a compiled policy, so compiled
strategy exact evaluation and the 10,000-run simulator gate were not
applicable.

## Secondary Snapshot

The one permitted bounded deep-four confirmation reached its pinned
`25000`-state cap during root expansion. It had no executable policy and no
upper-policy pass:

- root lower `0`, executable upper unavailable;
- expanded state count `1`;
- reforge work `1915409`;
- rows/transitions `0 / 0`;
- unresolved/unevaluated actions `1 / 21`;
- transition/policy hashes `14650fb0739d0383` /
  `f5142aa3d30d9ac3`;
- total wall `6625.0894` ms.

This confirms that deeper upper scheduling is not reachable under the
secondary case's smaller pinned cap; it does not qualify or disqualify the
primary decision.

## Retained Contract

The provenance channel is deterministically bounded by both sample count and a
quarter of the existing whole-telemetry byte cap, with explicit candidate,
retained, omitted, and retained-byte counts. Native tests cover valid JSON,
proper witness fields, deterministic repeated samples, and a one-sample cap.
Completed distributions remain retained and report
`completed_rows_recomputed: 0`.

No solver behavior, action filter, public ABI, WASM option, TypeScript
protocol, product default, mechanic, compiled strategy, or frontend authority
changed.

## Acceptance

- native build passed;
- native suite passed `502232` checks with `0` failures;
- standard and natural-T1 manifests validated `12` and `146` cases;
- WASM was rebuilt for the retained telemetry output;
- the complete web suite and `npx tsc --noEmit` passed; and
- rendered visual review was not run because it remains owner-owned.

The first complete web invocation stopped at a fallback-cancellation timing
assertion. The isolated 27-check smoke file and the subsequent complete suite
both passed without a source change, so the transient retry is disclosed but
does not alter the rejection or retained contract.
