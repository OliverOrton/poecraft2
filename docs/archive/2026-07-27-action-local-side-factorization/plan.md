# Action-Local Side Factorization Falsification

**Status: complete (negative at Gate 2).** Oliver selected this bounded
solver-research chunk on 2026-07-27 after observing that a fractured starting
suffix materially changes a three-prefix solve. The simple factorization was
falsified and its one richer boundary failed to compress even the synthetic
fixture.

Parent: [Milestone archive](README.md)

## Objective

Determine whether the exact destructive-reforge calculation can replace its
joint prefix/suffix roll frontier with an exact action-local side composition.
The prior protected-core census established only a large compression ceiling;
this milestone tests the probability contract that a side-factored evaluator
would actually require.

The initial hypothesis is deliberately narrow:

> For one destructive reforge, after conditioning on the complete preserved
> base, target total, prefix-pick count, and suffix-pick count, the prefix and
> suffix outcome identities can be represented by exact per-side factors and
> composed without retaining the full joint roll history.

A fractured affix is part of the complete preserved base. It is never erased
by the candidate projection. Its exact side occupancy, modifier/group
identity, slot flags, goal status, and pool-blocking effects must be applied
before either side frontier begins.

## Fixed interpretation

- This is an action-local reforge calculation study, not an outer solver-state
  quotient, cleanup route, Bellman pruning rule, or change to goal semantics.
- The current joint evaluator and ordinary solver behavior remain the
  authority until a shadow side evaluator proves exact outcome parity.
- Prefix/suffix factors may share explicit boundary variables. Compression is
  useful only if that boundary is materially smaller than the current joint
  frontier.
- Terminal status is evaluated on the composed complete successor. A goal
  modifier satisfies its slot whether fractured or not under the current goal
  contract.
- Ordinary starting junk and the same junk marked fractured form a required
  control pair: the ordinary mod is wiped by Chaos, while the fractured mod is
  preserved and must alter the factorization boundary.
- Cross-side exclusion groups, direct/forced mods, Harvest's guaranteed first
  pick, side locks, Eldritch side routing, Veiled final-side selection, global
  flags, and authored count conditions are not assumed independent.
- No mechanic ruling, cap increase, action-scope change, GPU work, ML,
  adaptive racing, compiled artifact change, binding change, WASM rebuild, or
  web work is in scope.
- Measurement-only source is restored if the hypothesis fails. A compact
  reusable native regression may remain only when it independently protects
  exactness.
- Commits remain local-only and end with
  `Co-authored-by: Codex <codex@openai.com>`.

## Gate 0 - Observation and coupling contract

**Complete.**

1. Map every variable read by the reforge path from preserved-base
   construction through pool building, target selection, roll transitions,
   outcome commitment, and terminal projection.
2. Separate variables into:
   - immutable complete preserved-base boundary;
   - prefix-local roll state;
   - suffix-local roll state; and
   - genuinely shared coupling state.
3. Record explicit counterexamples for:
   - ordinary versus fractured starting suffix;
   - fractured goal versus fractured junk;
   - side locks and fractured metamods;
   - cross-side exclusion-group overlap;
   - goal/block masks observed after composition;
   - affix-count and open-slot conditions; and
   - action variants whose final step chooses a side.
4. Freeze the first kill condition: within a fixed complete boundary and side
   pick-count cell, any non-zero 2x2 probability minor or support-rectangle
   hole disproves the simple independent per-side convolution.

## Gate 1 - Exact synthetic factorization probe

**Complete.**

1. Add a focused native, behavior-preserving probe over the existing exact
   synthetic reforge fixture.
2. For each complete outcome, construct collision-checked prefix and suffix
   identities from the existing `AbstractState` and `AbstractLayout`.
3. Partition the joint probability table by target/side counts and report:
   - prefix and suffix marginal cardinality;
   - observed joint support;
   - Cartesian support ceiling and density;
   - support-rectangle holes;
   - maximum absolute 2x2 minor; and
   - maximum conditional factorization error.
4. Run the probe for:
   - empty rare start;
   - one ordinary suffix, proving the destructive reforge control matches the
     empty preserved base;
   - the same suffix fractured; and
   - a fractured goal carrier where available.
5. Keep the ordinary evaluator's distribution, state identities, and work
   accounting unchanged.

## Gate 2 - Decision after the simple hypothesis

**Complete: simple count-conditioned convolution rejected.** Conditioning on
final remaining side weights restored rank one on the synthetic outcome table
but required 48 marginal identities for 41 joint outcomes. An online
evaluator would need that coupling at every roll step, so the richer boundary
recreated rather than reduced the joint representation.

If Gate 1 finds a support hole or non-zero minor above numerical tolerance:

1. reject simple count-conditioned convolution;
2. identify the first concrete coupling mechanism and smallest witness;
3. measure at most one richer interface suggested directly by that witness,
   such as interleaving/remaining-weight state; and
4. stop if the richer interface recreates the current joint Cartesian state.

If Gate 1 passes:

1. implement a shadow side-factored evaluator for ordinary Chaos only;
2. compare exact outcome support, probabilities, state hashes, goal
   probabilities, deterministic work, and peak frontier size against the
   current evaluator; and
3. expand action coverage only one mechanic variant at a time.

No production evaluator replacement occurs at this gate.

## Gate 3 - Representative real-data check

Only a surviving exact candidate reaches this gate.

**Not entered.** No exact candidate survived Gate 2.

1. Select one frozen three-prefix natural-T1 case.
2. Run empty, ordinary-junk-suffix, and identical-fractured-suffix starts
   under identical fixed caps and action scope.
3. Report deterministic reforge work and frontier/marginal/joint sizes as the
   portable comparison. Report wall time separately as machine/compiler-bound.
4. Reject the candidate if it changes any ordinary outcome, bound, policy,
   termination, state/row count, transition hash, or policy hash.

Cap-censored streams remain incomplete observations and are never described
as complete probability kernels.

## Gate 4 - Acceptance and handoff

**Complete.** The exploratory probe was restored. Only a narrow goal/fracture
regression, documentation, and versioned evidence remain.

1. Run the focused native calculation checks once after the retained path is
   known.
2. Run affected benchmark ingest checks only if a benchmark report/schema is
   retained.
3. Rebuild WASM or run downstream checks only if production engine behavior
   survives, which this research milestone does not presently authorize.
4. Store a versioned evidence summary for any measured result, including
   source, executable, artifact, case, generator, compiler, and machine
   identity where applicable.
5. Extract the durable result, restore rejected measurement source, archive
   this plan, clear the active boundary, update `HANDOFF.md`, and create one
   local completion commit.

## Completion criteria

This milestone completes with one of:

- a pinned counterexample rejecting simple exact side convolution and naming
  the coupling variable that caused it;
- a bounded richer-interface result showing that the remaining exact boundary
  is too large; or
- an outcome-identical shadow side evaluator with measured frontier and work
  reduction, ready for a separately selected production milestone.

It does not complete by treating marginal compression as probability
factorization, erasing fracture identity, extrapolating a censored stream, or
changing solver behavior without exact parity.
