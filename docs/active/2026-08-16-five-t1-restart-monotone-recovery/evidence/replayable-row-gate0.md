# Replayable Operation Rows Gate 0 - Ownership And Replay Census

Date: 2026-08-17

## Result

Gate 0 is complete. The measured carrier selects a hybrid exact authority:

- keep immutable stable shared `OutcomeDistribution` kernels once under their
  existing collision-checked reforge observation authority;
- retain one 32-bit route-result token per broad-kernel outcome instead of one
  24-byte `EvalTransition`;
- keep state-local narrow rows materialized unless their measured payload
  becomes material;
- compact and intern exact route-result/trace authorities under full equality;
- replay tokens plus sorted kernel entries into the existing split-only
  partition; and
- aggregate exact attribution by target row before building its transpose.

A raw `(route root, state)` cache and the propagated-observation-key cache are
both rejected by measurement.

## Checked 10-Million Census

The behavior-neutral retained census stopped at the unchanged
`max_transitions = 10,000,000` boundary with the same 35,828 pairs, 3,965
expanded operation pairs, 728 rows, 9,974,258 retained entries, and
`exact_eval_pair_discovery_transition_cap` classification as the prior checked
run.

Operation-row ownership was:

| Action | Rows | Shared-row reuses | Stable | State-local | Exact outcomes | Routed transitions | Absorptions |
|---|---:|---:|---:|---:|---:|---:|---:|
| Chaos | 278 | 2,115 | 278 | 0 | 9,962,130 | 9,959,350 | 2,780 |
| Exalt | 448 | 0 | 0 | 448 | 2,632 | 2,632 | 0 |
| Dense Fossil | 1 | 1,123 | 1 | 0 | 9,495 | 9,494 | 1 |

Thus 99.88% of the exact outcomes belong to broad stable Chaos rows. The
state-local Exalt fringe is only 2,632 outcomes. `CalcContext` retained
68,408,968 bytes for states, actual shared kernels, and its other exact
authorities; the census's 291,830,784-byte per-routed-row outcome projection
deliberately counts the same immutable stable kernel once for every distinct
routing node and is not actual retained kernel memory.

At this prefix:

- routed row payload: `239,404,416` bytes;
- projected 32-bit route tokens: `39,897,028` bytes;
- existing exact route-trace payload: `40,097,520` bytes;
- pair carrier plus links: `1,376,320` bytes; and
- evaluator peak with the retained bounded sample index: `364,521,388` bytes,
  versus `362,706,844` before the census.

The selected token plus current trace representation is about 80.0 MB at the
checked prefix instead of 239.4 MB for rows. A linear 84.5-million-outcome
projection is about 676 MB before compacting trace tokens, leaving room for
the measured calculator/pair authorities and a streamed partition under one
GiB. This is a representation projection, not a closure claim.

Artifact:
`build/qualification/replayable-row-gate0-witness-b-10m.json`

## Runtime Owner

The retained 1/256 timing sample measured:

- source operation-edge selection: 39,214 calls / 2.18 ms, projecting about
  0.56 seconds over the checked ten million calls;
- deterministic local routing: 38,767 calls / 255.50 ms, projecting about
  65.8 seconds over 9,987,873 calls;
- exact-kernel lookup/build: 96.1 ms total;
- pair interning: 1.28 seconds total; and
- sampled row completion: 0.72 ms.

Pair discovery used 71.23 seconds, Solve used 79.92 seconds, and the complete
case used 80.62 seconds. The prior checked source used 102.23 seconds in Solve;
the census therefore does not establish a regression. Deterministic routing
is the clear runtime owner.

## Rejected Cache Keys

The exact sampled `(route root, state)` index observed 38,767 requests, 38,767
unique keys, and zero reuses. Retaining that cache would reproduce the broad
carrier without avoiding any route work.

A temporary second shadow pass tested the already-proved propagated
observation key. It also found zero reuse and zero conflicting traces across
38,767 sampled requests, but constructing and retaining those huge propagated
keys cost 4.077 seconds and 225,695,360 bytes for only the 1/256 sample. The
shadow evaluator peak rose to 589,080,804 bytes. That instrumentation was
removed immediately; it is evidence against the representation, not retained
production behavior.

Artifact:
`build/qualification/replayable-row-gate0-observation-witness-b-10m.json`

## Focused Checks

On the retained census source:

- final native build after removing the rejected shadow: passed;
- final focused evaluator suite: 18,021 checks, zero failures; and
- stable-shared, state-local, serialized JSON, and transition-cap diagnostic
  assertions are present.

No release-WASM build, web suite, parent successor gate, or full acceptance
pipeline was run.
