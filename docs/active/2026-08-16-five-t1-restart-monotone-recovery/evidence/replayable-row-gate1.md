# Replayable Operation Rows Gate 1 - Compact Exact Tokens

Date: 2026-08-17

## Result

Gate 1 is complete. Broad stable operation rows now retain:

- the existing immutable `OutcomeDistribution` under CalcContext's exact
  collision-checked reforge authority;
- one 32-bit token per sorted outcome entry; and
- one collision-safe interned route result containing exact kind, endpoint,
  source edge, and policy/deterministic-route authority.

State-local rows remain materialized. Shared-row reuse still keys compiled
node, checkpoint state, and immutable distribution identity. Hashes select
route-result buckets only; full four-word equality is authoritative.

For bounded closed graphs, the tokens are rematerialized through the still-live
collision-safe pair index into legacy sorted `EvalRow` transitions before the
existing partition and solve. This is the Gate 1 reference bridge, not the
scalable endpoint. Gate 2 replaces that bridge with direct replay-partition
input.

## Exact Controls

The retained controls exercise stable shared Chaos, gated renewal, dense
Fossil, state-local Exalt, terminal and no-matching-edge absorptions, long and
multiple deterministic routes, the 385-trace rehash boundary, deterministic
cycles, forward reference, and one-work-item stepping.

Checks:

- native build: passed;
- focused evaluator suite: 18,027 checks, zero failures; and
- `git diff --check`: passed before checkpoint.

The suite explicitly proves that the compact path ran, retained route tokens
and result authorities, rematerialized to the existing values and flows, and
kept raw-reference parity.

## Checked Witness B

At the unchanged ten-million logical-entry cap, Witness B stops at the same
boundary and counts:

- classification: `exact_eval_pair_discovery_transition_cap`;
- raw pairs: `35,828`;
- expanded operation pairs: `3,965`;
- rows: `728`;
- logical entries: `9,974,258`;
- replayable rows: `279`;
- unique stable kernels: `2`;
- unique stable-kernel payload: `1,310,720` bytes;
- replay route tokens: `39,886,500` bytes;
- collision-checked route-result authorities: `240,257`;
- total row payload: `39,949,692` bytes;
- existing exact route-trace payload: `40,097,520` bytes; and
- evaluator owned/peak: `167,478,312` bytes against `1,050,980,991`.

The previous retained Gate 0 census used 239,404,440 row bytes and peaked at
364,521,388 bytes. Gate 1 therefore removes 199,454,748 row bytes and
197,043,076 peak bytes at the identical logical boundary.

Runtime also does not regress: Pair discovery used 66.60 seconds, Solve used
75.14 seconds, and total case time was 75.84 seconds. Exact kernel time was
188.1 ms and pair interning 856.1 ms. Deterministic routing remains the owner;
Gate 1 changes retention, not mechanics or route selection.

Artifact:
`build/qualification/replayable-row-gate1-token-witness-b-10m.json`

## Next Boundary

Do not raise the transition cap yet. The exact tokens now fit a projected raw
closure, but the temporary legacy rematerialization would recreate the full
24-byte graph at closure. Gate 2 must feed tokens directly into
`refine_closed_probabilistic_partition_replay`, retain the pair index through
that proof, and materialize only completed quotient rows. Gate 3 must then
remove the raw-edge transpose from exact attribution.

No release-WASM build, web suite, parent successor gate, higher real probe, or
full acceptance pipeline was run.
