# Product-Local Fracture Execution Consolidation Result

**Status: accepted on 2026-08-20.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](../README.md)

Plan: [Product-Local Fracture Execution Consolidation](../product-fracture-execution-consolidation-plan.md)

## Result

The exact four-natural-T1 product strategy now emits one Fracture operation
and one post-Fracture result router. The compiler no longer serializes seven
equivalent executable regions or 41 Fracture-only refined-parent routers.

The solver model is unchanged. It still has one admitted Fracture operator,
291 evaluated state-local rows, 49 selected proper rows, 242 costlier rows,
49 tied rows, and zero unresolved Fracture Q-values. Those rows retain their
exact state-local `k/n` probabilities. Only the executable representation is
shared.

The shared result router recognizes a hit when any relevant goal-satisfying
slot is fractured, then returns to the exact root policy router. The root
observes which concrete goal was fractured and selects its state-specific
continuation. A non-goal hit still takes the owner-approved priced-Restart
route. No mechanics, solver objective, action envelope, strategy vocabulary,
or miss-recovery behavior changed.

## Four-T1 Measurements

| Metric | Accepted baseline | Consolidated | Change |
| --- | ---: | ---: | ---: |
| Nodes | 292 | 174 | -40.41% |
| Edges | 815 | 515 | -36.81% |
| Strategy JSON bytes | 4,670,987 | 546,057 | -88.31% |
| Condition bytes | 4,587,281 | 493,618 | -89.24% |
| Policy-route nodes | 262 | 156 | -106 |
| Primitive regions | 19 | 13 | -6 |
| Additional recipe nodes | 7 | 1 | -6 |
| Native compile wall time | 67.36 ms | 5.31 ms | -92.12% |
| Native exact-evaluation wall time | 3,721.33 ms | 2,006.37 ms | -46.09% |

The consolidated 174-node graph contains 158 routers, 13 operations, two
terminals, and one start node. It has exactly one `_fracture_route`, one
Fracture operation, no refined-parent routers, and the unchanged single local
gated router. Timing is machine-local supporting evidence; graph and byte
counts are the durable result.

## Invariants And Exact Evaluation

Native and release WASM both report:

- transition hash `1c5594f87917f760`;
- policy hash `2c96f9faf0479667`;
- exact lower, upper, solver, and independently evaluated cost
  `3745.7309340083884`;
- 1,207 expanded states, six policy sweeps, and zero residual;
- 291 Fracture rows, 1,399 raw outcomes, and 747 retained transitions;
- expected Fracture use `3.9969519837176852` and unchanged complete
  action/material accounting;
- exact success probability one, failure/off-policy mass zero, and zero cost
  reconciliation delta; and
- 174 nodes, 515 edges, 546,057 JSON bytes, and 493,618 condition bytes.

The native and WASM corpus-owned simulator checks each completed 10,000 runs
with 10,000 successes, no failures, no off-policy exits, no unmatched edges,
no unapplied actions, and no action or graph-step limit. Their seeded sampled
mean cost is identical at `3738.9538811199136`. The cross-runner comparison
passes all 96 checks with zero mismatches.

An additional native run with command-line verification overrides also
completed 10,000/10,000 successes. The native benchmark intentionally labels
such override-mode samples diagnostic and leaves `verification_passed` false
when no Monte Carlo mean tolerance exists; this is runner classification, not
a behavioral failure. The corpus-owned run is the parity and acceptance
authority.

## Acceptance

The following passed after the implementation was complete:

- native build;
- focused compiler suite: 834 checks;
- focused evaluator suite: 18,065 checks;
- focused refinement suite: 362 checks;
- focused solver suite: 96,120 checks;
- release WASM rebuild;
- native and WASM exact four-T1 solve, compilation, exact evaluation, and
  10,000-run verification;
- native/WASM comparison: 96 checks, zero mismatches;
- `npx tsc --noEmit`;
- the complete non-visual web test suite;
- `git diff --check`; and
- one final `powershell -File scripts/test.ps1` run, including 3,463,907
  native engine checks with zero failures and all downstream web checks.

No browser visual review was performed. Nothing was pushed.

## Deferred Boundaries

This result improves strategy size, legibility, transfer cost, compiler work,
and exact-evaluator observation storage. It does not recover an exact
five-natural-T1 proof, reconcile the bounded five-T1 stored/exact cost, map
strict carrier 5983, add junk-fractured salvage, or change the owner-approved
priced-Restart miss policy. Those solver-quality and proof boundaries remain
available for a separately selected plan.
