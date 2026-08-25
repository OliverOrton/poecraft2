# Exact-Goal Carrier Ladder Result

**Status: accepted (2026-08-23).**

Parent: [plan](plan.md) | [documentation map](../../README.md)

## Result

Solver success now means an exact explicit-affix item. Every occupied prefix
or suffix must satisfy a requested goal slot at the requested tier; empty
slots are allowed and implicits remain outside this test. Junk, blockers, and
metamods remain valid intermediate carrier state. The native state authority,
compiled success condition, reforge terminal handling, fixed-option exits,
constructive policies, and admissible clean-carrier models use the same
contract.

Focused and incremental work scheduling now stratifies frozen carrier epochs
by achieved goal subset and engine-derived carrier structure. It round-robins
subsets, preserves direct multi-slot jumps, prefers useful protected or
fractured carriers and less unrelated occupancy, and delays rather than
rejects dirty carriers. Cleanup remains successor-driven: protected reforges
may replace disposable opposite-side junk without a preliminary cleanup, while
cleanup remains available when it changes the useful continuation.

The incremental executable-upper path can now combine completed cooperative
rows with an already certified frontier policy. Missing frontier carriers are
fed back into the existing grow-in-place refinement scheduler. Once the joint
policy is proper, ordinary exact Howard improvement selects among all fully
materialized rows before the immutable candidate is retained. This authority
is upper-only; publication still independently compiles and exact-evaluates
the selected graph.

## Five-T1 witness

The accepted diagnostic is Oliver's clean item-level-86 Conquest Lamellar in
Allflame, requiring exactly the three natural T1 armour/evasion prefixes plus
natural T1 physical-damage reduction and spell suppression. Voluntary economic
Restart is disabled; automatic Imprint programs are enabled.

- Requested bounded solve: 60 seconds; native solve wall `71.8803405` seconds,
  then independent verification. Stop owner is
  `requested_bounded_finish`, not a resource cap.
- Published exact-evaluated upper: `87361.1690420501` Chaos. The prior
  executable Chaos fallback at the same frontier was about `470485191.44`.
- Certified global lower: `36.4286171890906`; absolute gap
  `87324.740424861`. The result is a useful executable strategy, not an
  optimality or exact-closure claim.
- Discovery: 14,372 states, 5,722 expanded, 8,650 frontier, 71,590 rows,
  360,660 retained transitions, and 1,243,936 reforge work.
- Carrier scheduling: 35 frozen epochs, 11,750 carrier candidates, and 615
  observed goal-subset buckets. Joint anytime proof ran 15 checkpoints and
  retained one improved candidate.
- Selected-allocation estimates: 209,362,905 live solver-owned bytes and
  387,387,874 total solver-owned bytes. The largest native cooperative solve
  step was 3.226 seconds.
- Compiled policy: 607 nodes, 1,460 edges, and 1,161,987 JSON bytes.
- Exact graph evaluation converged with success probability 1, zero
  off-policy mass, complete cost accounting, and exactly reconciled
  `87361.1690420501` expected cost.
- The required 10,000 simulations produced 10,000 successes, zero failures,
  zero off-policy failures, and no action, cost, or graph-step limit stops.
  Sampled mean cost was `86677.1523926521`, 0.783% below the exact mean.

The sampled strategy averages about 8,303 primitive actions. Its largest
components are 4,967.74 Chaos, 1,237.41 Eldritch Chaos, 751.33 Eldritch Annul,
517.26 Eldritch Exalt, 335.49 Exalt, and 243.98 temporary-blocker cleanup
cycles per success. It also selects small state-local amounts of Harvest
Augment Defences/Physical and protected Harvest Reforge Physical. Imprint is
available in the solve envelope but is not selected by this policy.

Evidence:

- [diagnostic case](zero-to-five-diagnostic.json)
- [benchmark manifest](diagnostic-manifest.json)
- [native solve, exact evaluation, and 10,000-run report](zero-to-five-native-60s-verified.json)
- [compiled accepted strategy](strategy-verified/conquest-lamellar-allflame-exact-zero-to-five.strategy.json)

## Acceptance

- Native build: pass.
- Solver Calc: 436,632 checks, zero failures.
- Solver Compile: 583 checks, zero failures.
- Solver S8.3 automatic options: 536 checks, zero failures.
- Solver policy refinement: 2,383 checks, zero failures.
- Solver Solve: 101,456 checks, zero failures.
- Release WASM rebuild: pass.
- Complete `npm test`, including 28/28 release-WASM smoke checks: pass.
- `npx tsc --noEmit`: pass.
- `git diff --check`: pass.
- Full repository pipeline: deliberately not run.
- Rendered/visual review: deliberately not run; it remains Oliver's.

## Remaining boundary

The important product failure is fixed: the bounded solve can now publish a
proper, exact-terminal five-goal policy that uses multiple mechanic families
instead of falling back to pure Chaos renewal. The remaining problem is
quality proof and search efficiency. The global lower remains far too weak,
the selected policy still spends thousands of rolls, and native work items can
still exceed the product responsiveness target. A successor should improve
rare-carrier successor-aware lower bounds and carrier/action prioritization
without granting pruning authority to an unproved subgoal ordering.
