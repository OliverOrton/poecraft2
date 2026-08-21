# Product-Local Fracture Execution Consolidation

**Status: selected for implementation.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](README.md)

## Objective

Compile every selected product-local primitive Fracture row through one shared
Fracture operation and one shared post-action goal-hit router, without changing
solver expansion, Bellman rows, prices, the deliberate priced-Restart miss
model, policy selection, public strategy vocabulary, or native mechanics.

## Source And Artifact Baseline

The accepted exact four-natural-T1 control has 292 nodes, 815 edges, and
4,670,987 JSON bytes. Its 270 routers include the root, 219 decision-DAG
routers, 42 refined-parent routers, seven product-local Fracture routers, and
one local gated router. Seven identical Fracture operation nodes are emitted.

Forty-one refined-parent routers lead only to those Fracture regions and own
3,933,985 serialized condition bytes. They distinguish seven source-local
acceptable-hit masks even though all regions execute the same primitive and
all successful concrete results return to the exact root policy router.

This is not solver action duplication. The control has one admitted Fracture
operator. It evaluates 291 state-local rows over exact `k/n` shapes, selects
49 proper rows, reports 242 costlier rows, 49 tied selected rows, and zero
unresolved Fracture Q-values. Those state-local kernels remain necessary and
unchanged.

The product solver deliberately maps every non-goal Fracture hit to priced
Restart. That owner-approved restricted behavior remains unchanged. This plan
does not add junk-fractured salvage or broaden the optimality claim.

## Proof

On every selected product-local Fracture carrier:

1. primitive legality proves the input has no fractured explicit affix;
2. Fracture changes only one existing explicit slot's fractured flag;
3. the solver accepts exactly currently satisfied, unfractured goal slots;
4. therefore the concrete result is a hit exactly when any goal-satisfying
   slot selected by the planner's relevant mask is fractured; and
5. after a hit, the existing root policy router observes which exact slot was
   fractured and selects its state-specific continuation. A miss takes the
   unchanged shared priced-Restart edge.

The compiler may consequently share the executable operation/result route
without merging solver rows, abstract states, successful continuation values,
or policy decisions.

## Implementation

1. Replace the source-state-specific product Fracture hit predicate with one
   planner/goal predicate covering every relevant satisfied fractured goal
   slot.
2. Use that complete emitted behavior in product-local Fracture policy-region
   sharing, producing one operation and one post-action router.
3. Permit a refined coarse parent whose classes all select that shared product
   Fracture behavior to route directly, as already allowed for other uniform
   primitive decisions. Unrestricted primitive Fracture retains its existing
   conservative routing.
4. Update the focused product-Fracture compiler control to require multiple
   solver hit masks but exactly one emitted operation/result route. Preserve
   exact evaluation, cost, consumption, properness, and zero off-policy mass.

## Acceptance

- The exact four-T1 transition hash `1c5594f87917f760`, policy hash
  `2c96f9faf0479667`, value `3745.73093400839`, Fracture row/Q census, and
  expected action/material accounting remain unchanged.
- Native and release WASM emit behaviorally identical graphs and independently
  evaluate to success one, failure/off-policy zero, and the same exact cost.
- The four-T1 graph emits exactly one product-local Fracture operation and one
  result router; its nodes, edges, condition bytes, JSON bytes, compile time,
  and evaluation time do not regress.
- Focused compiler/evaluator/refinement/solver controls, native build,
  TypeScript, non-visual web tests, `git diff --check`, the required 10,000-run
  native/WASM verification, and one final repository acceptance pipeline pass.
- Update durable evidence and `HANDOFF.md`, create coherent local commits with
  `Co-authored-by: Codex <codex@openai.com>`, then merge the completed branch
  into local `main`. Do not push.

## Stop Conditions

Stop with a precise handoff if consolidation changes any solver hash, selected
row, exact value, terminal mass, action/material accounting, miss recovery,
default authority, or requires a public strategy-schema change.

