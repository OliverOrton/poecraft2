# Project Direction

**Status: current orientation, not an execution plan.** The complete knowledge
map is [Documentation](README.md). [HANDOFF](../HANDOFF.md) is intentionally
minimal until Oliver selects the next implementation boundary.

## Vision

poecraft2 is a fast, trustworthy Path of Exile 1 crafting simulator that runs
client-side, paired with an exact planner for minimum expected-cost crafting
strategies. One native engine owns crafting behavior across manual crafting,
simulation, exact calculation, strategy compilation, and solver search.

```text
Python ingest -> canonical SQLite -> compiled runtime data
              -> C++20 engine/C ABI -> Python and WASM bindings
              -> Vite + TypeScript + Web Components product
```

The solver plans over engine-owned transitions. Its policies compile to the
same editable strategy vocabulary that the simulator executes; the frontend
does not reimplement crafting rules.

## Current Posture

- Canonical RePoE ingest, SQLite validation, compiled engine data, the native
  C ABI, Python bindings, and a rebuildable WASM module are implemented.
- Ordinary non-cluster item sessions share the implemented action vocabulary
  documented in [Mechanics](mechanics/README.md), including the 26 serialized
  action kinds plus Bestiary Imprint checkpoint/restore behavior.
- The browser product provides Emulator, Calculator, Strategy Builder and
  Simulator, Stash, saved workspaces, and league-aware economy inputs.
- The exact solver, compiled simulation, accounting, automatic candidates,
  all-actions state scaling, shared policy compilation, exact compiled-policy
  evaluation, and non-visual product integration are implemented. Remaining
  delivery boundaries are preserved without scheduling them in the
  [solver roadmap](future/solver-roadmap.md).
- Economy ingest, immutable snapshots, league selection, overrides, and cost
  identities exist. Production activation remains external, and scheduled
  refresh does not currently fetch Beast prices; see [Economy](economy/README.md).
- Accounts and publishing, recombinator planning, additional mechanics, and ML
  planning remain deferred in [Future](future/README.md).

The [exact constructive policy search](active/exact-constructive-policy-search.md)
is currently selected. It has established generic destructive-renewal and
progressive-fracture incumbents for a genuine three-ordinary-pool-T1 target;
exact gap closure remains active. [Active work](active/README.md) and
[HANDOFF](../HANDOFF.md) record its scope.
Exact solver action/state pruning is complete and preserved in its
[dated archive](archive/2026-07-21-solver-action-state-pruning/README.md).
Exact solver state scaling is preserved in its
[dated archive](archive/2026-07-20-solver-state-scaling/README.md). Historical
target misses, scaling measurements, waivers, and final gates remain
discoverable through [Evidence](evidence.md) and the
[archive](archive/README.md); they are not silently converted into current
acceptance claims.

## Direction Of Travel

These are durable product directions, not a selected order:

- Preserve engine-owned mechanic correctness and auditable data authority.
- Finish practical solver/product delivery only through a newly selected plan;
  the known boundaries are summarized in the
  [solver roadmap](future/solver-roadmap.md).
- Add exact two-item recombinator outcomes before automatic recombinator
  planning.
- Continue workspace and economy fluency where it supports real crafting use.
- Keep accounts, publishing, and ML downstream of their explicit prerequisites.

## Non-Negotiable Boundaries

- SQLite is canonical; compiled runtime data is derived and never hand-edited.
- The native engine owns pools, weights, item transitions, and crafting rules.
- Ambiguous Path of Exile mechanics require Oliver's ruling; agents do not
  research or guess them.
- Minimum expected cost remains the solver objective unless Oliver explicitly
  changes it.
- Compiled-strategy verification uses 10,000 simulator runs when verification
  is required, unless Oliver specifies otherwise.
- Browser memory, responsiveness, and exactness limitations are disclosed, not
  hidden by raising caps or weakening claims.

## Read Next

- [Foundation](foundation/README.md) for system boundaries
- [Mechanics](mechanics/README.md) for implemented behavior
- [Engine](engine/README.md), [Solver](solver/README.md),
  [Product](product/README.md), and [Economy](economy/README.md) for subsystem
  references
- [Decisions](decisions.md), [evidence](evidence.md), and
  [glossary](glossary.md) for cross-cutting knowledge
- [Future](future/README.md) and [Archive](archive/README.md) for deferred and
  historical material
