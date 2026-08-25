# Proof-Gated Route And Operation-Graph Compaction Result

Date: 2026-08-17

Implementation checkpoint: `01035ea`

Selection checkpoint: `6e57783`

## Decision

Retain complete-behavior compiler sharing, graph/row/Fracture attribution,
and cooperative policy-observation propagation. Reject and remove the local-
gated compact-evaluator prototype. Stop Gate 5 at the remaining atomic exact-
reforge leaf; do not build release WASM or run the parent acceptance gates.

Witness B is now independently materializable as a bounded executable policy,
but it is not an exact optimality result. The solver's stored coarse value and
the compiled policy's exact value do not reconcile, the lower bound remains
zero, the alternative envelope remains open, and strict lifting stops on a
coarse mapping failure.

## Gate results

### Gate 0 - attribution complete

The retained report separates infrastructure, policy routes, local gated
routes, primitive regions, full versus gated exact rows, row payload, and
Fracture final-Q status. On final Witness B:

- 1,082 operation rows comprise one direct-repeat row, one structurally
  canonical local-gated row, and 1,080 other rows;
- only the direct-repeat row uses the existing goal-progress-gated kernel;
  1,081 rows retain full physical authority;
- the local-gated full row owns 35,835 outcomes, 35,825 routed transitions,
  1,048,576 outcome-payload bytes, and 143,340 routed-payload bytes; and
- the compiled graph census is four infrastructure nodes, 84 policy-route
  nodes, one local-gated router, three primitive operation regions, and no
  additional recipe nodes.

### Gate 1 - compact local kernel rejected

The structural prototype reached the intended local route, but it failed the
plan's exact-oracle invariant on both frozen witnesses:

- Witness A full physical: `624800.9519118543`; compact prototype:
  `624800.9519136797`;
- Witness B full physical: `16226566.773294946`; compact prototype:
  `16226566.773146724`.

The differences are small but real and arise from changing normalization and
accumulation authority. The prototype was default-off during the final runs
and was removed before checkpointing. Local route structure remains diagnostic
only and cannot select a compact kernel.

### Gate 2 - complete-behavior sharing retained

The compiler now partitions goal-gated primitive regions by a collision-free
planner semantic key and repeatedly refines that partition by the exact block
or external region selected for the canonical retry continuation. The process
stops only at a fixed point. Hashes do not grant equality, expected-cost
annotations do not split execution, and partial progress continues through
the common policy root.

This reduces the previously measured Witness B candidate from 2,015 nodes,
4,123 edges, and 757 operation regions to 92 nodes, 338 edges, and three
operation behaviors. The exact evaluator now closes 35,828 raw pairs to 1,094
refined pairs without a resource cap. It independently evaluates the compiled
policy at `16226566.773294946`, success one, and zero off-policy mass. The
executable action mix is Chaos, Exalt, and Dense Fossil plus its one-socket
resonator.

This is a real graph-size, memory, legibility, and downstream-evaluation win;
compilation itself remains negligible at 1.29 ms. The result is a bounded
upper, not an exact optimum: direct certification reports `cost_mismatch`
against the solver value `37279651.842345364`.

### Gate 3 - Fracture characterized, no selection repair

The canonical sparse-Q diagnostic sees 214 exact eligible Fracture rows,
1,022 raw physical outcomes, and 452 retained hit/miss entries. None is
selected. All 214 final Fracture Q values are unresolved because at least one
required successor value is nonfinite; there are zero evaluated cheaper,
tied, or costlier rows. This is not a cheaper-Q witness, so no Bellman,
revisit, price, or Fracture-preference change is authorized.

### Gate 4 - bounded native decision complete

Witness A remains exact at `624800.9519118543`, success one, zero off-policy
mass, 184 nodes / 666 edges, and paired defaults only. Its transition hash
remains `284ff325a96fe0d7` and its policy hash remains
`cee2bf6579b1857a`.

Final Witness B is `bounded_feasible` with termination
`numerical_stability`, lower bound zero, and upper/evaluated cost
`16226566.773294946`. Exact graph evaluation takes 3.961 seconds; the complete
case takes 16.835 seconds. Direct evaluation peaks at 121,334,760 bytes. Its
main exact-evaluation stages are:

- pair discovery: 434.584 ms;
- pair refinement: 3.248 s, including 173.539 ms partition and 73.454 ms
  quotient conversion;
- component solve: 59.064 ms; and
- finalization: 105.613 ms.

Strict lift remains open with `coarse_mapping_failure`: strict carrier 5983
maps outside the solved coarse graph.

### Gate 5 - cooperativity improved, then stopped

Backward policy-observation propagation now uses one resumable fixed-point
authority. The synchronous entry point drains the same coroutine, and a
focused cyclic control proves identical assignments, rounds, payload
telemetry, and more than one suspension. This removes the earlier roughly
2.7-second observation fixed-point work item.

The final native Witness B still has a 1,254.159 ms public work item in the
compiling/strict-lift phase. Temporary owner tracing, removed before the
checkpoint, isolated it to the already documented atomic
`CalcContext::outcomes()` exact-reforge leaf in cooperative attempt execution.
The direct evaluator's largest item is also 265.345 ms in pair discovery,
slightly above the 250 ms contract. Gate 5 therefore does not pass.

The next implementation scope, if Oliver selects it, is a cooperative exact-
reforge cursor beneath `CalcContext::outcomes()` that preserves exact entry
order, normalization, caching, resource accounting, and cheap cancellation.
It should also split the remaining 265 ms evaluator discovery item. The strict
coarse-mapping failure is a separate proof boundary and must not be hidden by
the scheduling work.

## Validation

Passed after removing the rejected prototype:

- `powershell -File scripts/build.ps1`;
- evaluator focused suite: 18,065 checks;
- compiler focused suite: 815 checks;
- refinement focused suite: 362 checks;
- solver focused suite: 96,120 checks; and
- `git diff --check`.

The final real commands used the frozen five-natural-T1 manifest, skipped
simulation verification, and enabled independent exact strategy evaluation.
The reports are local build artifacts:

- `build/qualification/gated-route-gate5-final-a.json`;
- `build/qualification/gated-route-gate5-final-b.json`.

No release-WASM build, web suite, Warlord/automatic matrix, priced primary,
full acceptance pipeline, or 10,000-run simulation verification was run.
