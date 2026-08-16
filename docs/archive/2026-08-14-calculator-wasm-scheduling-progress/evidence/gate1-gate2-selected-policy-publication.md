# Gates 1-2 Selected Policy Publication Evidence

**Status: complete natively; release-WASM parity is reserved for focused
qualification.** Captured on 2026-08-16 from Gate 1 commit `ab80856` plus the
Gate 2 exact-attribution diff.

Parent: [Selected Policy Publication And Cooperative Exact Refinement](../selected-policy-refinement-plan.md)

## Candidate Separation

The five-natural-T1 numerical stop now captures an
`UnverifiedSelectedPolicyCandidate` before the certified Chaos incumbent is
restored. The wrapper retains the selected values, row ids and prices,
observed-choice sources, reachable-state mask, quotient representatives, exact
start item, immutable context identities, numerical-stop provenance, and
selected-policy hash.

The candidate is accounted by both audited and fast solver-owned-byte ledgers.
It cannot enter the certified fallback portfolio: conversion requires a
proper, executable, cost-complete, zero-off-policy compiled graph with a finite
independently evaluated cost. Identity, graph-prefix, start-item, materializing,
or memory failure releases the unverified snapshot and leaves the existing
certified incumbent unchanged.

On the frozen native witness:

- capture completed in `4.479` ms;
- retained candidate ownership was `109,930` bytes after materialization;
- capture and certification identities were distinct and valid; and
- the independently certified Chaos incumbent remained available throughout.

## Direct Evaluator Boundary And Repair

The first release-WASM attempt compiled the selected policy to 184 nodes and
666 edges but expanded exact attribution to 7,154 raw pairs. Its expanded
transpose SCC exhausted `max_sweeps=100000` after `91.928` seconds, so the
candidate correctly remained unverified and the old 3-node Chaos renewal was
published unchanged.

The evaluator already had an exact shared-row attribution solve, including
wide-arithmetic row equations, reconstructed raw-pair equations, and an
independent behavioral-quotient flow check. It was used only as a memory
fallback. Gate 2 now prefers that authority whenever shared exact rows
strictly contract the raw attribution graph. This does not weaken proof or
raise a cap: the returned raw occupancy must still satisfy all three equation
families within the existing tolerances.

Focused exact-evaluator tests pass with 16,786 checks and zero failures.

## Five-T1 Native Result

The native product witness now completes in `5.760` seconds and publishes the
selected policy:

| Field | Result |
| --- | ---: |
| policy / termination | `bounded_feasible` / `numerical_stability` |
| lower | `0` |
| selected coarse estimate | `690872.2205617285` |
| independently evaluated upper | `624800.9519118543` |
| prior Chaos upper | `37279857.73995944` |
| improvement versus prior upper | `59.667x` cheaper |
| compiled graph | 184 nodes / 666 edges |
| strategy JSON | 482,233 bytes |
| strategy SHA-256 | `f12a2cb13137e69d7b107015da9d417026a4b01accf5cb7206da18d315b2ee62` |

The published value comes only from the exact compiled-strategy evaluator;
the `~690872` coarse estimate is not used as a certified upper. The result
retains lower zero and 20,175 open incremental action-envelope obligations, so
it makes no global-optimality claim.

Raw native report:
[five-T1 selected publication](gate2-selected-policy-five-t1-native.json).

## Remaining Cooperative Boundary

The retained three-prefix exact control keeps its exact value
`2186.6911143146394` and 42-node/121-edge graph. Shared-row attribution reduces
total native time from `108.832` seconds to `31.883` seconds, but strict lift
still owns a synchronous `28.950` seconds: `5.803` seconds of carrier discovery
and `18.473` seconds of partition refinement. The five-minute monolithic
selected strict-lift viability run was stopped at the outer boundary before
the evaluator repair; no partial candidate was published.

Gate 2 therefore passes for the five-T1 product objective. Gate 3 remains
required for exact controls and truthful WASM progress/cancellation; faster
synchronous finalization is not cooperative finalization.
