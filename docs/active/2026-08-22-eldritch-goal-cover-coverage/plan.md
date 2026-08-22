# Eldritch Goal-Cover Coverage

**Status: completed and accepted (2026-08-22).**

Result: [Eldritch Goal-Cover Coverage Result](result.md)

Owner: Oliver

Starting checkpoint: `d2341dd`

## Objective

Restore a useful admissible completion-cost seed on Eldritch-eligible Helmet,
Body Armour, Gloves, and Boots solves. Replace the session-wide zero guard
with an Eldritch-aware optimistic relaxation while preserving zero as the
local fallback whenever coverage or finiteness cannot be proved.

This boundary changes solver guidance and proof strength only. It does not
change Path of Exile mechanics, real action admission, the minimum-expected-
cost objective, compiled-strategy vocabulary, prices, caps, or the separate
early executable-upper scheduling problem for Essence, Harvest, and Fossil.

## Retained evidence and constraints

- Commit `3a6d191` added automatic Eldritch side-intent options and the current
  `session.eldritch_eligible -> 0` completion-cost guard together. The guard
  was a deliberate correctness containment because the clean-carrier MDP did
  not model the new one-side-preserving compound actions.
- Later automatic-action work already prices each admitted carrier-local
  Eldritch program from its exact `OptionKernel` resource vector. Reuse that
  authority; do not reimplement real mechanics in the heuristic.
- The clean goal-progress MDP tracks rarity, satisfied goal mask, and explicit
  prefix/suffix counts. Its stochastic envelopes are optimistic by contract.
- The probability-free cover remains the universal fallback used by
  operator-price pruning and structure-changing carriers. It must include a
  no-more-expensive, no-less-capable relaxation for every possible automatic
  Eldritch final action before it can replace zero safely.
- Automatic Eldritch options currently include side-intent Annul, Chaos, and
  Exalt programs. Setup may be direct or may use priced Ember/Ichor steps.
- A restricted-action value is never a global lower bound. Removing the guard
  without covering every executable Eldritch compound is forbidden.

## Gate 0 — Baseline and proof contract

Record the current zero seed on the checked Allflame four-natural-T1 Conquest
Lamellar primary and a positive non-Eldritch control. Identify every automatic
Eldritch final-action family and every state fact that affects the optimistic
projection.

Write the proof obligation in Bellman form: for every exact executable action
or compound option `a`, the heuristic must satisfy

`h(s) <= immediate(a) + E[h(S') | s, a]`.

The heuristic may grant free setup, favorable side selection, favorable junk
removal, progress preservation, and probability upper bounds. It may never
charge more, preserve less, or give lower goal probability than the real
option.

## Gate 1 — Eldritch-aware optimistic projection

Extend the goal-cover preparation with explicit optimistic side macros derived
from existing action descriptors and goal-slot sides:

- Eldritch Chaos may retry a chosen target-side goal subset while preserving
  all prior progress and receiving the most favorable legal explicit shape.
- Eldritch Exalt may retry one target-side goal while preserving all prior
  progress and capacity; the relaxation may grant dominance/setup for free.
- Eldritch Annul may remove target-side junk favorably while preserving goals.
- Setup Ember/Ichor work may be omitted from the lower cost, but the final
  primitive's real nonnegative active-economy cost must be retained.

Use the existing optimistic goal-draw probability authority. Do not duplicate
mod pools or weights. Include a deliberately coarser Eldritch relaxation in
the universal probability-free cover for carriers outside the clean domain.

Do not use the strict clean-state refinement as a stronger maximum on an
Eldritch session until its action coverage is also proved. Unknown, unpriced,
nonfinite, or uncovered cases fall back locally to zero.

## Gate 2 — Focused soundness controls

Add native controls proving:

1. eligible clean carriers receive a finite positive seed when priced;
2. ineligible controls are numerically unchanged;
3. missing final-action prices do not manufacture a positive unsupported
   bound;
4. direct and setup-bearing prefix/suffix Annul, Chaos, and Exalt option rows
   satisfy the Bellman lower inequality;
5. partial progress, full target sides, changed implicit dominance, and both
   goal-side distributions remain admissible;
6. adding an Eldritch option cannot raise the relaxed value; and
7. repeated construction is deterministic.

If a small fixture can close exactly under the complete automatic envelope,
also require `heuristic(start) <= exact optimal value(start)`.

## Gate 3 — Native qualification and measurement

Run the complete affected native solver acceptance once after implementation.
Measure the checked four-T1 Conquest Lamellar primary against its stored zero-
lower baseline and run the non-Eldritch Bow control. Record lower trajectory,
first finite/verified upper, expanded/discovered states, open alternatives,
wall, memory, and termination boundary.

The boundary succeeds on sound positive coverage even if the five-minute
primary still does not close. It must not claim that a stronger lower alone
fixes delayed upper-action scheduling or exact-refinement cost.

## Gate 4 — Release WASM and handoff

Rebuild release WASM because browser-visible native solver behavior changes.
Run non-visual TypeScript/web-WASM acceptance, the existing 10,000-run
automatic Eldritch compiled-strategy control if a strategy is produced or
changed, and the Warlord non-Eldritch control. Do not perform rendered review.

Update stable solver documentation, active indexes, measured evidence, and
`HANDOFF.md` with the actual result. Commit locally with:

`Co-authored-by: Codex <codex@openai.com>`

Do not push.

## Stop conditions

Stop precisely if:

- the existing native option semantics are insufficient to prove an
  optimistic side projection without a new Path of Exile mechanic ruling;
- a proposed positive bound violates any exact-row Bellman inequality;
- the only sound projection measures indistinguishably from zero; or
- completing coverage requires changing action admission, mechanics, caps, or
  the separate executable-upper scheduler.
