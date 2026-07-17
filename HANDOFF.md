# Session Handoff - B1.0 Bestiary Contract Is Next

Updated 2026-07-17 after Oliver closed S7 and selected the next roadmap:
owner-selected Bestiary expansion, then a focused one-item solver capability and
reviewability pass, then recombinators. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[docs/bestiary-and-solver-capability-plan.md](docs/bestiary-and-solver-capability-plan.md).

## Current State

S7.0-S7.6 and S7.2R are closed. Oliver explicitly directed work to move
forward. The final endgame sample remains recorded honestly at `0.9942` success
against the former `0.995` target; the numeric gate was not retroactively
called passing and no replacement sample was run. The completed plan and full
final handoff are preserved under
[docs/archive/2026-07-solver-s7](docs/archive/2026-07-solver-s7/).

The active plan is B1 followed by S8:

```text
B1  selected Bestiary mechanics, exact engine-to-product vertical slice
S8  cheapest-policy action control, accounting, review projection, trimming
```

The old M1-M5 ordering is retired. Trade leaves, Hinekora's Lock, corruption,
tainted currency, and finishing-cost work remain independently parked.
Recombinator foundations and pyramid planning wait until Oliver accepts S8.

## Exact Next Boundary

Implement **B1.0 only**. No Bestiary mechanic implementation begins until
Oliver supplies and approves the selected recipe contract. Do not research PoE
mechanics online or infer them from memory/data strings.

For every requested recipe, record:

- stable id/name and beast price inputs;
- item eligibility and legality;
- exact deterministic mutation or random outcome law;
- crafted/fractured/influenced/split/corrupted/mirrored/implicit behavior;
- no-op, refusal, failure, and beast-consumption behavior;
- output count and whether a saved copy or second live item is involved; and
- Emulator, Calculator, Strategy Builder, and solver availability.

Classify each row as ordinary deterministic one-item, ordinary stochastic
one-item, checkpoint/restore, multi-output/multi-item, or explicitly unsupported
for B1. Stop after the approved fixture/manifest, make the local checkpoint
commit, and rewrite this handoff for B1.1.

## Recorded S8 Product Direction

- Continue optimizing only minimum expected cost; do not add alternative-policy
  or risk objectives.
- Improve action control from exact carrier preservation/dominance facts. Do
  not hard-ban Fossils/reforges by graph depth or a display-stage label.
- Automatically consider relevant carrier-exact Fracture routes, temporary
  bench blockers, permanent bench finishes, and existing protected metamod
  options through the ordinary S7 option/compiler vocabulary.
- Extend the existing exact strategy evaluator for per-action and review-section
  action/material accounting; do not build a second occupancy engine.
- Derive compact review sections without changing execution semantics.
- Keep the exact graph, add a presentation-only focus mode, and create optional
  empirically trimmed copies with exact and independently simulated impact
  reports.

## Existing Substrate And Gotchas

- There is no Bestiary action in the engine, solver, bindings, strategy
  vocabulary, or product UI. The economy provider recognizes the `Beast` stash
  category but has no canonical Bestiary recipe/action price mapping.
- Fracture calculation, carrier distinctions, preparation/retry options,
  protected-side options, Multimod finishes, lazy Fossil generation, and action
  diagnostics already exist. Product solves currently create the special
  options only when explicitly requested; S8 makes selected ones automatic.
- `pc_strategy_evaluate` already returns exact success/failure mass, expected
  actions, price-key consumption, node visits, and edge traversals. Strategy
  Builder Calculator mode already renders much of it.
- The native Simulator currently aggregates operation counts, not every visited
  router/terminal and traversed edge. S8 trimming requires those counters across
  native/C ABI/Python/WASM/TypeScript before simulator visitation can drive a
  derived graph.
- Action-space reduction must retain the cheapest-policy guarantee: certify
  dominance/equivalence, use a valid defer bound, or keep the candidate.
- Rebuild release WASM after engine C ABI, action vocabulary, solver option, or
  strategy vocabulary changes.
- Oliver owns rendered and visual review. Agents do not run screenshots or
  browser visual smoke unless explicitly asked.

## Documentation State

The active roadmap, implementation plan, stable solver plan, future mechanic
notes, archive index, and README now point to B1/S8. The point-in-time
S7 simulator/solver improvement report remains historical evidence rather than
sequencing authority.

## Parked Scope

S6 Phase 3 ambient Emulator odds remains skipped. Economy E0-E7 is complete
except external production activation. Accounts, publishing/community,
trade-leaf expansion, Hinekora's Lock, corruption/tainted/finishing work,
recombinators, and ML remain outside the active boundary.
