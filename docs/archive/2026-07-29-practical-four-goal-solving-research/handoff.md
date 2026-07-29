# Fracture-Local Coarse-Parent Prototype Handoff

**Status: proposed next boundary; not active until Oliver selects it.**

Parent: [Practical Exact Four-Goal Solving Research](README.md)

## Objective

Prototype an exact product-solver boundary in which Fracture observes explicit
modifier identity locally while ordinary parent carriers use the coarsest
layout sufficient for the remaining admitted actions.

Do not change Calculator or Emulator Fracture semantics. Do not prescribe a
crafting sequence. Do not add the anchor library in this milestone.

## Frozen facts

- Starting point: commit `fc8787750445b89c7dadc37db7b4007bc7c74b87`.
- Owner case:
  `build/gate1-baseline-corpus/cases/natural-t1-full-four-47d8b909aa88.json`.
- Current parent: `36` layout actions, `105` junk classes, root Chaos support
  `134,477`.
- With the same core vocabulary but no global exact-group observer: `6` junk
  classes and projected root Chaos support `217`.
- Fracture alone restores `105` classes and `134,477` support.
- `pc_solver_create` currently passes the force-exact argument as `true`; this
  must be removed or narrowed for the prototype, or localizing Fracture cannot
  change the parent layout.
- Current Fracture work is `158` rows and `706` direct entries. Each source has
  one satisfied goal, so solver-local goal-hit plus aggregate-miss rows would
  contain `316` entries, saving `390`. None of the fractured successors was
  expanded in the measured run.
- All product-fallback non-goal Fracture hits are already priced as
  `Restart + anchor`; the exact primitive nevertheless interns the individual
  misses.
- `Remove Crafted Modifiers` is dependency-only but is not a parent
  `layout_action` in this case. Keep temporary blocker cleanup in its exact
  state-local child context.

## Required design

1. Stop unconditionally forcing complete group-exclusion identity in the
   parent product solver. Preserve force-exact construction for exact graph
   evaluation or any caller that truly needs it.
2. Exclude product Fracture from parent-layout observer derivation.
3. Add a solver-local Fracture operator over a coarse parent carrier:
   - read total live explicit modifier count `n`;
   - emit one probability `1/n` successor for every acceptable goal modifier
     hit;
   - aggregate the remaining probability `(n-k)/n`;
   - route the aggregate miss to priced Restart;
   - preserve distinct fractured-goal masks for the `k` acceptable hits.
4. Keep `CalcContext::outcomes(ActionType::Fracture)` unchanged for Calculator,
   Emulator, exact strategy evaluation, and mechanic parity.
5. Keep standalone `Remove Crafted Modifiers` outside the product envelope.
   Temporary bench programs may construct an exact child layout containing
   blocker, follow-up, and cleanup; project only their complete exits back to
   the parent.
6. Reuse completed reforge distributions. The prototype may reproject their
   retained successors, but must not re-enumerate an already completed kernel
   merely to change the parent identity.

## Exactness witnesses

For every coarse Fracture row publish:

- source coarse-state identity;
- `n`, acceptable goal-hit masks, and `k`;
- exact probability sum;
- each goal-hit successor;
- aggregate miss probability;
- priced Restart target and cost;
- proof that no non-goal miss remains live under Oliver's product rule.

Reject the local operator when any live non-goal continuation is possible.
Do not silently fall back to a coarse approximation.

## Deterministic gates

All gates are required.

1. **Layout gate:** the frozen case reports no more than `6` parent junk
   classes before action-local children. If attack/caster observation is
   deliberately retained in the parent, freeze and justify a replacement
   ceiling no greater than the measured `11`.
2. **Frontier gate:** projected root Chaos support is exactly `217` for the
   core configuration, with zero materialization/projection failures.
3. **Primitive parity gate:** existing exact Fracture Calculator/Emulator tests
   and uniform-probability fixtures remain unchanged.
4. **Local-row gate:** the measured `158` Fracture rows contain exactly `316`
   solver-local entries (`158` acceptable hits and `158` aggregate misses),
   probability-normalized within existing tolerance.
5. **Restart gate:** every aggregate miss has a complete priced executable
   Restart witness; no junk-miss state ID is interned in the parent.
6. **Discovery gate:** on the frozen 200,000-state request, transition and
   policy hashes are deterministic across two clean runs. The run must either
   close below the cap or show a materially later, explicitly identified
   boundary. Merely reaching the same cap with renamed states is a no-go.
7. **Work gate:** completed reforge-kernel recomputation remains zero. Report
   raw evaluator work separately from retained support and transitions.
8. **Policy gate:** any returned upper is a proper executable policy. If the
   compiled strategy materially changes, run the repository-standard 10,000
   simulator executions; otherwise state why compiled-policy verification is
   not applicable.
9. **Scope gate:** no C ABI, WASM protocol, strategy vocabulary, or web
   behavior change unless separately selected. No visual review is required
   for a native-only prototype.

## Stop conditions

Stop and report rather than broaden the milestone if:

- a non-goal Fracture hit is not always dead under Oliver's rule;
- parent coarse equivalence fails for an admitted non-Fracture action;
- exact child exits cannot be projected without retaining observer identity;
- completed reforge distributions must be recomputed;
- the root support does not reproduce `217`; or
- the next wall is still the same strict-identity frontier.

## Deferred follow-up

Only after this boundary passes should the solver prototype the 14-mask anchor
library in the order three-goal, two-goal, one-goal. A three-goal-only library
cannot by itself qualify the frozen root: three-goal outcomes are only
`0.014794%` of Chaos partial-progress mass. The final one-goal layer is where
the root benefit enters.
