# Session Handoff - simple Calculator Goal Item P3b next

Written 2026-07-14 after Oliver approved pre-S6 polish P3a Variant A. Read
[AGENTS.md](AGENTS.md), then [docs/direction.md](docs/direction.md), then this
file. The next task is revised Phase P3b only. P2 remains skipped; do not start
S6 Phase 1 until P3b is complete.

## Settled scope

Calculator stays deliberately simple:

```text
one concrete input item
+ one authored v1 goal item
+ one selected registry action
-> exact engine-returned odds for that action
```

Preserve the existing v1 goal contract and behavior: finished rarity,
modifier-family slots with tier-or-better thresholds, and
`min_satisfied_slots` (`All N` / `At least N of N`). Do not add named goals, OR
branches, predicate trees, multiple selected actions, or a new engine schema.

The only P3b product change is making Goal use the same item-frame UI as Input
item.

## Approved design and implementation authority

Oliver selected:

- `design/mockups/calculator-goal-item/variant-a-literal-twin.png`

The load-bearing implementation contract is:

- `design/specs/calculator-goal-item.md`

The brief, alternate mock, prompts, and known image-model fiction remain under
`design/`. The mock controls hierarchy only. Runtime labels, counts, tier
choices, and odds come from the catalog/native engine.

## Backend/component reuse ruling

There are not two crafting-rule backends to merge:

1. one-step Calculator odds use `pc_calc_action_outcomes`;
2. Strategy Builder whole-graph odds use `pc_strategy_evaluate`;
3. both already use the same native action registry, `CalcContext`, legality,
   and exact action distributions.

Keep those task-shaped entry points separate. P3b is frontend component reuse:

- extend `pc-mod-list` with an explicit concrete-versus-target model;
- keep `buildModifierOptions` as the shared family/tier source;
- keep Calculator's one `pc-mod-pool` as the input/goal authoring surface;
- do not touch Strategy Builder's recursive `pc-condition-editor`.

## P3b implementation slice

1. In `apps/web/src/app/components/pc-mod-list.ts`, add the explicit target
   model from the approved spec. Share the outer frame/header/ledger/slot
   structure with concrete mode. Preserve concrete right-click fracture events.
2. Target rows use stable `familyModKey` identity, catalog-derived selected-tier
   text, `Tn or better` / `Any tier`, optional engine-returned marginal odds,
   and tier/remove events. Empty rows say `No prefix requirement` or
   `No suffix requirement`.
3. In `pc-calculator.ts`, keep the current `Finished rarity` and
   `Success means` controls above a new target-mode `pc-mod-list`. Adapt the
   existing `CalculatorGoalSlot[]` through `ModifierFamilyOption`; do not change
   persistence or `SolverGoal` JSON.
4. Keep recovered legacy group slots readable/removable in target mode without
   inventing a P/S side or re-enabling group authoring.
5. Remove the superseded Calculator-only goal-list markup and CSS after the
   target component owns every state.

## Important implementation gotchas

- Calculator will contain two `pc-mod-list` elements. Replace the current broad
  `querySelector("pc-mod-list")` getter/listener with explicit input and goal
  roles. Concrete fracture events must bind only to Input item.
- `itemMaxPrefix` / `itemMaxSuffix` are the engine/session capacity already fed
  to Input item; reuse those values for stable Goal positions. Do not hard-code
  rarity capacity rules in TypeScript.
- Preserve `slots` array order within each side. Stable visual placement uses
  `familyModKey`; never sort by name, tier, or probability.
- Keep the all-slots-follow behavior when adding/removing requirements and the
  current clamp behavior for partial thresholds.
- A target is not a rolled item. Header copy is `N requirements`, not
  `N explicit`; do not infer influence badges or exact rolls not represented by
  v1 goal state.
- Variant A contains illustrative errors: its Input `S3` label and target
  section counts are wrong. The approved spec records the correct behavior.
- The frontend remains non-authoritative for legality and odds.

## Acceptance gate

- Extend `apps/web/test/item-display.test.ts` for unchanged concrete behavior
  and empty/populated/dense target modes, stable placement, any-tier, marginal
  odds, legacy groups, and target events.
- Add focused Calculator component/model coverage for rarity, all/partial
  thresholds, tier/remove edits, v1 recovery, one selected action, and verbatim
  engine results.
- `npx tsc --noEmit`, `npm test`, and `npm run build` in `apps/web`.
- `powershell -File scripts/test.ps1`.
- Separate-process headless Chrome smoke: edit Input and Goal through the shared
  pool, change/remove a target, select one action, observe odds, and finish with
  no application console errors or uncaught exceptions. Do not use Codex's
  in-app browser.
- Capture the implemented populated state beside the approved mock and append
  verification/deviations to `design/specs/calculator-goal-item.md`.

Native/WASM rebuilds are not expected because P3b changes no native or ABI
contract.

After the gate, commit locally, rewrite this handoff, and resume
[docs/s6-plan.md](docs/s6-plan.md) Phase 1. Do not begin S6 Phase 2.

Commits remain local-only unless Oliver explicitly says to push.
