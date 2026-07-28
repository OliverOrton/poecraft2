# Gated Root Renewal Incumbent — Final Report

Date: 2026-07-28

Plan: [Gated Root Renewal Incumbent](plan.md)

Evidence:
[tracked summary](../../../fixtures/solver-natural-t1/v1/evidence/gated-root-renewal-incumbent-summary.json)

## Decision

Retain the gated-only early fixed-renewal incumbent and compact compiler path.
It closes the frozen product's “no executable policy” failure without
claiming exact convergence or changing the default unrestricted solver.

## Exact Boundary

After a completed gated row owned by the solve start state, a candidate
qualifies only when:

1. the operator is one priced primitive destructive reforge;
2. the exact gated kernel has positive terminal probability `p`;
3. every positive-probability non-goal exit can legally repeat that action;
4. every such exit has the same complete engine-owned exact reforge-kernel
   signature as the root; and
5. immediate cost `c` and `c / p` are finite within the solver value domain.

The resulting fixed policy satisfies
`J = c + (1 - p)J`, hence `J = c / p`. Retry and partial states are not merged.
The witness says only that the selected action has the same kernel on its
reachable carriers. Other actions remain in discovery and the lower-bound
problem.

The compiler independently repeats the legality and signature checks over
every reachable non-goal carrier. Only then does it emit four nodes and four
edges: start, goal router, success, and the reforge loop.

## Frozen Acceptance

| Case | Terminal probability | Fixed-policy upper | Validated non-goal carriers | Policy / witness hash | Compiled |
| --- | ---: | ---: | ---: | --- | --- |
| full-four | `1.6572365216439631e-08` | `60,341,416.98784247` | 134,477 | `816d94db8a2d0cbd` / `1b6c5bec239f461a` | 4 nodes, 4 edges, 1,397 bytes |
| deep-four | `5.38535873115097e-09` | `185,688,651.38279814` | 123,697 | `364d755f6df87bed` / `3a7f727b64af1731` | 4 nodes, 4 edges, 1,418 bytes |

Both results are `bounded_feasible` within the
zero-progress-reroll restriction. Their lower bound remains zero, so no
exactness or global-optimality claim is made.

The first Chaos kernel and transition hashes remain unchanged:

| Case | Kernel hash | Transition hash | Final discovered / expanded | Reforge work |
| --- | --- | --- | --- | ---: |
| full-four | `54c479dfcd7d14f3` | `fe0ea011721ae996` | 134,508 / 1 | 3,000,000 |
| deep-four | `5cfd88b724a36b01` | `5ca6030024523dec` | 123,728 / 1 | 3,000,000 |

The first Chaos row consumes 2,807,580 reforge-work units. The next root
Fossil request consumes the remainder. Therefore this milestone provides an
executable policy but does not improve the exact-search boundary. Competing
root broad rows are the immediate measured wall; partial-state expansion is a
later, unmeasured wall.

Two repetitions per case preserved deterministic policy, witness, kernel,
transition, and compiled-strategy hashes. Unrestricted controls retained
their prior no-policy result with transition hash `566809fa56c7e2a4`, policy
hash `0cfb7e9c1000a4da`, and no renewal candidate.

## Verification

- Native release build passed; its only diagnostic was the pre-existing GCC
  warning in `prepare_goal_cover_cost`.
- Native solver acceptance passed 513,858 checks with zero failures.
- The exact toy oracle verifies native `c / p`, compiled evaluation, an
  illegal-retry rejection, and stale-signature compilation refusal.
- The toy compiled strategy succeeded in 10,000 seeded simulator runs.
- Frozen simulation was not run: the exact terminal probabilities imply
  roughly 60 million and 186 million actions per successful run.
- Release WASM was rebuilt. All non-visual web tests and TypeScript typecheck
  passed.
- No mechanics, economy, ingest, C ABI, strategy vocabulary, or product cap
  changed.

WASM SHA-256 after the rebuild:

- module: `fcee42df5a398e92533144ac58ec778d0f7ffcd439853fe55801b9efab103c26`;
- binary: `ce755f14f2a695975c5b54f53588d3fa6f0dbe619f8d214985e60903d5e019a9`.

## Identity

- Starting source: `c8109d1`.
- Plan commit: `e42ee25f102740eb5e1978ba3a38d177ca93ac92`.
- Benchmark executable SHA-256:
  `c8417dd0dab64d347f9ef71c068d11d3c17315d5d5161bcdb2c4eea9912c9648`.
- Product portfolio manifest SHA-256:
  `a4f080e680499b95d2b6b5e635c4bc9fd6678a82f4ce698ea8c6882b37fb3efd`.
- Artifact manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Natural-T1 generator config SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Compiler/machine: GCC 14.2.0, Windows build 26200, Intel i7-13700K,
  16 cores / 24 logical processors, 68,396,957,696 physical bytes.

Sequential solve wall times were 1000.16/1034.76 ms for full-four and
795.84/864.57 ms for deep-four. They are machine/compiler-bound and do not
survive a hardware or compiler change. Deterministic work, mass, values,
hashes, and compiled sizes are the portable evidence.

Raw reports remain under
`build/gated-root-renewal-incumbent/gate4/`; their hashes and portable fields
are pinned in the tracked summary.

## Consequence

Keep the feature. It supplies the first executable answer for these gated
hard cases with a narrow exact proof and no loss of solver discovery.

The next solver-improvement pass should investigate how to avoid or bound the
remaining competing broad rows on the root. Do not start by merging retained
partial states: current captures expand none of them.
