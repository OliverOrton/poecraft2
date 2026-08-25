# Recovery-Scoped Restart And Successor-Aware Bounds

**Status: complete (2026-08-22).**

Owner: Oliver

Starting checkpoint: `40eeb87`

## Objective

Separate mechanic-owned replacement recovery from optional economic
abandonment, then strengthen operator lower bounds with the maximum goal
progress and most optimistic carrier information that every real successor
contract permits.

Calculator solves default to no voluntary discard-and-buy-new-base action.
Product-local Fracture miss retains its existing exact priced replacement
route. Explicit/native callers retain the historical economic-Restart default,
and Calculator exposes an explicit opt-in.

This boundary changes the Calculator optimization scope when the new default
is active. It does not change Path of Exile currency mechanics, Fracture hit or
miss probabilities, base prices, goal semantics, or the unrestricted engine
default.

## Retained facts

- The synthetic `restart` registry action is currently always legal and is an
  unconditional product candidate. It charges `base` and reaches a fresh
  Normal carrier.
- Product-local Fracture does not materialize wrong-fracture miss states. Its
  exact local kernel already composes miss probability with priced replacement
  and the compiler emits a dedicated retry route.
- Bounded compilation currently inserts Restart as the unmatched policy-tree
  default even when no selected policy region uses it.
- Focused fallback construction frequently assumes Restart supplies a proper
  upper from any carrier. That authority is invalid when economic abandonment
  is disabled and must be refused or replaced, never silently retained.
- `destroyed_affixes` means an affix may be destroyed. It is not must-destroy
  authority. `preserved_affixes`, sequential execution paths, and exact
  synthetic reset semantics are safe may-survive inputs.
- The strict clean cover does publish aggregate rare-carrier values, but most
  rare sources do not own exact action rows. A source audit cleared the
  reported concrete-rare ordering defect: current code correctly compares
  prefix plus suffix counts on both carriers.

## Gate 0 — Baseline and contracts

Record the current Calculator envelope/solve Restart lifecycle, the primary's
Restart row and policy counts, Fracture's local replacement route, bounded
compiler defaults, and every focused/constructive use of Restart as upper
authority.

Define two distinct contracts:

1. `economic_restart`: a selectable Bellman action that may abandon any
   carrier and buy a fresh base; historical engine default `true`.
2. `replacement_recovery`: an action-owned exact branch such as product-local
   Fracture miss; unaffected by `economic_restart`.

No state is guessed to be “bricked.” A replacement route exists only through
explicit mechanic/recovery authority or the economic opt-in.

## Gate 1 — Cross-layer economic-Restart control

Add an ABI-v2 solver flag and internal option that disable economic Restart
without changing struct layout or historical defaults. Thread it through
native parsing, WASM options, TypeScript protocol, Calculator solve options,
telemetry, cache compatibility, stable solver documentation, and tests.

Calculator defaults the option off and exposes an unchecked opt-in labelled as
abandoning the item and buying a fresh base. Odds and Emulator behavior remain
unchanged.

When disabled:

- no ordinary state receives a selectable Restart Bellman row;
- generic focused/constructive upper policies cannot use Restart;
- bounded policy routing fails closed on an unmatched state rather than
  inserting Restart;
- exact product-local Fracture miss still charges `base`, reaches the fresh
  recovery carrier, and compiles its dedicated retry route; and
- missing `base` still makes priced Fracture incomplete.

## Gate 2 — Successor-aware operator lower

Replace `satisfied(state) | reach(action)` with a proved optimistic successor
mask:

`may_survive(state, action) | may_reach(action)`.

For each currently satisfied goal slot, retain it whenever any admitted
execution path may preserve a compatible carrier. Uncertain side, traits,
tags, member class, or sequential behavior retains the slot. Exact Restart
retains none. Never subtract from the mask merely because a selector says the
slot may be destroyed.

Use the strongest admissible continuation among the universal cover and any
proved optimistic successor-shape relaxation. Exact deterministic successors
or already-authoritative option kernels may supply shape information; a
stochastic or incomplete contract falls back to the survivor mask plus
universal cover. Audit concrete-rare ordering and keep Eldritch strict coverage
excluded until its automatic rows are represented.

The [result](result.md) records the accepted implementation, soundness
controls, native measurements, release-WASM qualification, and remaining
Imprint-closure boundary.

## Gate 3 — Soundness and product controls

Add native controls proving:

1. historical/unrestricted solves still select economic Restart when it is
   genuinely cheapest;
2. disabled mode has no ordinary Restart row or selected policy region;
3. Fracture miss cost, exact value, compiled retry operation, and 10,000-run
   behavior are unchanged;
4. bounded unmatched routing is fail-closed without economic Restart;
5. every strengthened primitive/automatic lower satisfies
   `lower(s,a) <= immediate(a) + E[h(S')]` on focused exact kernels;
6. may-destroy actions such as Annul retain all source goals that may survive;
7. reset/renewal/cleanup actions remove only source progress absent from every
   possible successor; and
8. repeated lower construction is deterministic and never weaker than its
   retained universal component.

## Gate 4 — Qualification and measurement

Run the complete affected native solver suite once. Measure the checked
four-T1 Conquest Lamellar primary with Calculator-default Restart disabled and
compare states, rows, transitions, lower/upper trajectory, first upper,
refinement work, wall, memory, termination, and Restart telemetry against the
`40eeb87` result.

Run the Warlord control and a Fracture-selected control. Use 10,000 simulation
runs whenever a compiled strategy is requalified. Do not claim exact parity
between enabled and disabled economic-Restart modes: they intentionally solve
different policy scopes.

## Gate 5 — Release WASM and handoff

Rebuild release WASM, run `npx tsc --noEmit` and complete non-visual web tests,
update stable docs, active indexes, measured evidence, and `HANDOFF.md`, then
create coherent local commits ending with:

`Co-authored-by: Codex <codex@openai.com>`

Do not push. Do not run the full repository pipeline unless a focused failure
or cross-layer artifact change makes it necessary.

## Stop conditions

Stop precisely if disabling economic Restart cannot preserve product-local
Fracture recovery without changing its exact kernel, or if a proposed
successor-aware lower violates any exact Bellman row. A loss of the old
Restart-backed incumbent is an expected scope consequence, not permission to
reintroduce voluntary replacement under another name; record the new truthful
boundary instead.
