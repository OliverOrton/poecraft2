# Transition Carrier Gate 0 - Compact Raw Records

**Status: passed.**

This native checkpoint splits raw exact-evaluator transition storage from
post-refinement deterministic-chain metadata. Release WASM, the frozen real
witnesses, and the full acceptance pipeline were not run at this checkpoint.

## Representation

`EvalTransition` now stores the exact double probability followed by target,
edge, compressed-policy route, and exact policy state. A compile-time assertion
fixes its size at 24 bytes, down from 32.

The removed `via` field never had discovery or partition authority: every raw
transition stored `kNoId`. Pass-through contraction now creates a parallel
`transition_via` row sidecar only for rows it rewrites. The sidecar remains
empty throughout discovery and pair refinement, and later flow, chain,
attribution, and forward-reference paths read it through one checked accessor.

Every owning ledger includes the sidecar:

- full and fast evaluator-owned estimates;
- retained row and attribution payload;
- refinement conversion projections;
- raw-graph scratch projections; and
- post-contraction payload refresh.

The `max_transitions` diagnostic now reports stored entries, raw pairs, row
payload, transition record size, sidecar bytes, and current evaluator-owned
bytes so the real witness can be compared without inference.

## Focused Proof

The destructive refinement cycle still discovers 76 raw pairs and refines to
57 classes. Pass-through contraction creates nonzero sidecar storage. Exact
terminal probabilities, expected action/material consumption, and edge
traversals match the independent raw forward evaluator. Single-item and
1,024-item step sizes remain identical.

Checks:

- native build - passed;
- evaluator suite - 16,823 checks, zero failures;
- solver suite - 96,113 checks, zero failures;
- `git diff --check` - passed before checkpoint.

Gate 1 may now run Witness A once and Witness B at the unchanged 10-million
transition cap before selecting any scoped higher-cap probe.
