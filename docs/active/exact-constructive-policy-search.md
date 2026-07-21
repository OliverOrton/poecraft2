# Exact Constructive Policy Search

**Status: C0-C3 implemented; C4 exact rare-state refinement active.**

Parent: [Active work](README.md)

## Objective

Make broad real-item solves converge from empty starts by pairing the existing
admissible focused lower search with a generic, executable policy upper. The
first acceptance target is the item-level-86 empty rare Dire Pelt targeting T1
rarity, T1 life, and T1 crafted cold resistance under its complete 32-action
goal-relevant product envelope.

The constructive policy is an incumbent, never an action restriction. The
solver must continue to consider every admitted action that can beat or tie
the incumbent and may report exact optimality only when its global lower and
executable upper meet.

## Fixed Boundaries

- Derive policy chunks from goal slots, engine-owned legality/producibility,
  exact primitive kernels, and current prices. Do not hard-code the Dire Pelt,
  three named modifiers, or a preferred craft route.
- Compile chunks back to ordinary primitive strategy behavior. A feasible
  policy may be measured before optimal closure but must not be labelled exact
  until the optimality gap closes.
- Preserve the complete product action envelope, mechanic behavior, price
  ties, and all-actions exactness. Approximate compaction is forbidden.
- Keep production caps unchanged. A later cap decision requires new closure
  evidence after structural work.
- Price-bound partial graphs are not reusable reprice caches.
- Run routine acceptance once after the complete milestone. Use focused tests
  and bounded diagnostics only to develop or diagnose the selected changes.

## Phases

- **C0 — gates and witnesses:** retain the empty-start cap-stop as the before
  record; add native oracle cases for finite constructive policies, incumbent
  validity, all-action preservation, price ties, and repricing safety.
- **C1 — generic constructive upper:** build exact renewable/finish chunks over
  goal-progress and structural finishability predicates. Compose them into a
  real executable fallback policy and evaluate its expected cost exactly.
- **C2 — upper-guided focus:** install a feasible fallback for frontier states,
  maintain global lower/upper values, expand states by their contribution to
  the start gap, and apply only witnessed exact price-bound dominance.
- **C3 — action admission amortization:** memoize temporary-bench applicability
  and exact templates by collision-checked carrier signatures; reject kernels
  early only when exact legality, equivalence, or incumbent dominance proves
  they cannot matter.
- **C4 — measured state proof:** run the real case under unchanged caps. If the
  remaining gap requires it, strengthen the admissible lower over goal subset,
  affix capacity, finishability, and permanent reachability; do not add an
  unmeasured abstraction.
- **C5 — acceptance:** require real-case exact convergence, compilation, exact
  compiled-policy evaluation, 10,000-run verification, native regression
  gates, WASM rebuild, non-visual web checks, durable documentation, archive,
  and final handoff.

## Acceptance

The real empty-start case must obtain a finite executable upper, close its
global exact gap under the complete product envelope, compile within existing
compiler caps, and pass exact evaluation plus 10,000 simulator runs. The same
constructive machinery must pass a second goal/base oracle without case- or
modifier-specific code. No cap-only or approximate result satisfies this
plan.

## Current C4 evidence

The exact constructive policy now supplies a finite executable upper for the
real empty-start case. It uses engine-derived goal slots and ordinary primitive
kernels; no base name or modifier id is hard-coded. The bounded two-expanded-
state diagnostic currently brackets the start at approximately
`156.2205227766 <= V* <= 509.8475882024` under the complete 32-action envelope.

The lower audit found the remaining relaxation in rare-state continuation:
the compact goal/count summary can select Exalt after destructive or Regal
exits using an optimistic junk-blocker identity. Fully materializing that
fringe raised the lower only to `165.8638671428`, discovered 89,099 states, and
made one solve step exceed 100 seconds, so it is rejected as the production
representation. The next C4 chunk is an exact behavior-class recurrence for
rare Exalt/destructive continuation. It must preserve all actions and share
kernels by collision-checked exclusion behavior instead of enumerating every
strict carrier.

Production caps remain unchanged. No exact value, compiled strategy, or C5
acceptance is claimed yet.
