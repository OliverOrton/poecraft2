# Streamed Closure Gates 1-2 - Exact Routing And Stop

Date: 2026-08-17

## Result

Gate 1 is complete. The exact evaluator now contracts arbitrary
non-modifier-offer deterministic router chains during discovery. It retains a
collision-safe interned trace containing every skipped router, selected edge,
and resolved or no-matching-edge endpoint. Solved flow replays those traces
for node occupancy, edge traversals, top classes, terminals, and failures.
Deterministic router cycles remain raw.

Gate 2 reached its stop condition. Online routing removes the router-pair
frontier, but raw operation-row transitions remain open far beyond the point
that the existing one-GiB representation can hold. Gate 3 did not start
because no closed raw graph or initial partition exists.

## Focused Proof

The retained controls cover:

- a 65-router chain with exact node/edge flow, direct no-router value and
  operation-accounting parity, forward-reference parity, and single-step
  parity;
- two independently reached deterministic route roots;
- a 385-trace boundary that forces the collision-safe chained trace index to
  rehash while retaining full-key equality;
- modifier-offer routing, no-matching-edge, terminal, shared-row, and
  destructive-cycle controls already in the evaluator suite; and
- deterministic router cycles, which remain raw and unresolved.

The transition record remains 24 bytes. The trace index stores one exact trace
copy plus compact bucket heads and links. Replacing the initial ordered index
did not materially improve the real 10-million runtime, but it reduced the
real evaluator peak from 414,538,020 to 362,706,844 bytes by removing the
duplicate trace-key copy.

Checks on the final source:

- native build: passed
- focused evaluator suite: 18,004 checks, zero failures
- focused solver suite: 96,113 checks, zero failures
- `git diff --check`: passed

Source checkpoints:

- `88cc69e` - exact online deterministic routing and flow replay
- `7432410` - collision-safe compact exact-trace index

## Witness A

The refreshed control remains independently exact:

- exact cost: `624800.9519118543`
- success: `1`
- off-policy mass: `0`
- exact evaluation status: `matched`
- total wall time: `7.502` seconds
- solve wall time: `4.534` seconds
- largest native step: `325.482` ms

Its public solver status remains the existing fail-closed
`refused_unsupported_action`; online route contraction does not change its
value, actions, or publication authority.

Artifact:
`build/qualification/streamed-closure-gate1-indexed-witness-a.json`

## Checked Witness B At 10 Million

The checked case still stops at `max_transitions = 10,000,000`, but the
frontier changes from router pairs to operation rows:

| Measure | Gate 0 | Gate 1 indexed |
|---|---:|---:|
| Raw pairs | 9,998,209 | 35,828 |
| Router pairs | 9,987,873 | 0 |
| Operation pairs | 10,335 | 35,827 |
| Expanded operation pairs | 3,965 | 3,965 |
| Pending pairs | 9,994,243 | 31,862 |
| Retained rows | 728 | 728 |
| Retained transitions | 9,974,258 | 9,974,258 |
| Exact route traces | n/a | 239,614 |
| Skipped router-node/edge traversals during construction | n/a | 62,223,496 |
| Pair plus link carrier | 280,313,856 bytes | 1,376,320 bytes |
| Row payload | 239,382,200 bytes | 239,404,440 bytes |
| Trace payload | n/a | 40,097,520 bytes |
| Evaluator peak | 600,881,884 bytes | 362,706,844 bytes |

All 9,971,477 retained transitions with a target still carry the exact source
edge and route authority, and every retained route state equals its target
pair state. No deterministic route cycle was encountered.

This checked run took 103.17 seconds, including 102.23 seconds in Solve, with
a 3,697.21 ms largest step. The 62.2 million exact router-edge selections,
not trace-index lookup, now dominate construction time.

Artifact:
`build/qualification/streamed-closure-gate1-indexed-witness-b-10m.json`

## Single 20-Million Probe

Gate 1 left 688 MB below the evaluator budget at 10 million. The only higher
probe therefore used 20 million transitions: its measured linear memory and
time projections fit the one-GiB and five-minute case bounds. The checked case
was restored to 10 million immediately afterward.

The probe still stopped during discovery:

| Measure | 10M | 20M |
|---|---:|---:|
| Raw pairs | 35,828 | 35,828 |
| Expanded operation pairs | 3,965 | 8,239 |
| Pending operation pairs | 31,862 | 27,588 |
| Retained rows | 728 | 1,007 |
| Shared-row reuses | 3,238 | 7,233 |
| Retained transitions | 9,974,258 | 19,972,223 |
| Exact route traces | 239,614 | 479,280 |
| Row payload | 239,404,440 bytes | 479,377,920 bytes |
| Trace payload | 40,097,520 bytes | 80,202,800 bytes |
| Evaluator peak | 362,706,844 bytes | 650,793,188 bytes |
| Total wall time | 103.17 s | 196.62 s |
| Largest step | 3,697.21 ms | 3,592.26 ms |

The added ten million transitions expanded only 4,274 more operation pairs
and added 279 unique rows; 3,995 additional pairs reused an existing row. At
that measured slope, the remaining 27,588 pairs imply roughly 64.5 million
additional transitions before closure if sharing remains comparable. By
contrast, the measured byte slope reaches the one-GiB evaluator budget near
33.9 million total transitions, with no allowance for a subsequent
partition. This is a projection, not a claimed closure count, but the gap is
large enough to reject another cap-only run.

Artifact:
`build/qualification/streamed-closure-gate2-witness-b-20m.json`

## Publication And Next Boundary

The priced five-T1 candidate was never materialized. The independent exact
evaluation in both reports applies to the published six-node Chaos fallback,
which remains:

- cost: `37279857.73995944`
- success: `1`
- off-policy mass: `0`
- termination: `numerical_stability`

The next implementation, if Oliver selects it, must remove full routed
operation transitions as a retained pre-closure authority. A likely scope is
an exact replayable/shared operation-row kernel that feeds the existing
collision-safe replay partition and component solver without materializing
every 24-byte routed transition. Merely deriving the transition policy state,
raising a count cap, compacting pairs again, or optimizing the later replay
partition cannot close this boundary.

No release-WASM build, web suite, action-semantics work, Gate 3 partition
compaction, parent successor Gates 4-8, or full acceptance pipeline was run.
