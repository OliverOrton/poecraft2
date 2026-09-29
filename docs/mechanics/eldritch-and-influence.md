# Eldritch And Influence

**Status: current implemented mechanic reference with explicit dominance rulings.**

Parent: [Mechanics](README.md)

Verified against code: 2026-08-09 @ automatic Eldritch Exalt milestone

Verification scope: native Eldritch implicit and explicit-currency actions,
influence-exalt application, exact calculator, solver registry/options, and
product controls and catalog.

## Scope

This family owns `eldritch_ember`, `eldritch_ichor`, `eldritch_exalt`,
`eldritch_chaos`, `eldritch_annul`, and `influence_exalt`.

The compiled modifier pools recognize the generic influence names
`adjudicator`, `basilisk`, `crusader`, `elder`, `eyrie`, and `shaper` in
addition to the no-influence state. Influence Exalt currency now exists for all six pool identities. Oliver approved
the Shaper/Elder expansion on 2026-09-28:

- Warlord's Exalted Orb -> `adjudicator`;
- Hunter's Exalted Orb -> `basilisk`;
- Crusader's Exalted Orb -> `crusader`; and
- Redeemer's Exalted Orb -> `eyrie`;
- Elder's Exalted Orb -> `elder`; and
- Shaper's Exalted Orb -> `shaper`.

Canonical item-class influence selector tags replace guessed normalized class
names. This fixes missing sword, axe and mace influence pools. Above-level
transferred tiers and elevated modifiers are retained separately from natural
roll eligibility.

Registry IDs, compiled strategy operations, and economy keys use the public
currency names `crusader`, `hunter`, `redeemer`, `warlord`, `elder`, and `shaper`. The strategy
and primitive-action parsers retain the old internal names for those four as
input aliases, while emitting and charging only the public names.

## Implemented Behavior

`eldritch_ember:<tier>` and `eldritch_ichor:<tier>` require an
Eldritch-eligible session, tier 1 through 4, no generic influence, and a
non-corrupted, non-mirrored item. The action chooses a weighted implicit from
the requested side/tier pool, removes an existing Eldritch implicit of that
same generation type, keeps the opposing implicit, adds the chosen implicit,
and records the requested tier on item state.

Dominance is computed by comparing the two stored tier numbers:

- Searing tier greater than Eater tier targets prefixes;
- Eater tier greater than Searing tier targets suffixes; and
- equal tiers, including both absent, mean no dominant side.

`eldritch_exalt` requires an Eldritch-eligible rare item. With dominance it
attempts to add one ordinary-pool modifier only on the targeted side. Without
dominance it uses ordinary open-side selection.

`eldritch_chaos` requires an Eldritch-eligible rare item. With dominance it
clears only the targeted side, restores its fractured affix if present, and
rolls two or three modifiers on that side; the opposing side is unchanged.
The target-side clear does not consult a target-side metamod lock. Without
dominance it performs the ordinary full Chaos reforge, including fractured and
locked-side preservation.

`eldritch_annul` requires an Eldritch-eligible item. With dominance it removes
one uniformly sampled non-fractured affix from the targeted side; it does not
consult a target-side metamod lock. Without dominance it performs the ordinary
lock-aware Annul transition. The raw dispatcher does not add a rarity guard.

Each of the six currency-backed `influence_exalt:<currency-influence>` actions
requires a rare item with no existing
generic influence, no Eldritch tiers, no synthesised state, and no fractured affix. It adds the
requested influence bit, then attempts to add one modifier from only that
influence’s pool. If no modifier can be added, it rolls the influence bit back
and reports the action unapplied.

All actions in this family also share the global corrupted/mirrored refusal.

## Dated Oliver Rulings

- **2026-07-15:** tied or absent Eldritch dominance uses the ordinary
  Exalt/Chaos/Annul behavior. Prefix/suffix intent is a separate explicit
  solver option that establishes dominance and pays for every setup currency.
  This is recorded in the archived
  [S7 plan](../archive/2026-07-solver-s7/plan.md).
- **2026-08-09 (superseded 2026-09-28):** the original release exposed four
  currencies. Oliver now explicitly authorizes Elder/Shaper, including the
  Eldritch exclusion despite conflicting external prose.

## Engine Coverage And Code Pointers

- `engine/src/session_builder.cpp` — Eldritch eligibility, tier pools, and
  influence-specific session masks.
- `engine/src/actions_basic.cpp` — `add_eldritch_implicit`,
  `dominant_eldritch`, `do_eldritch_chaos`, `do_eldritch_annul`, and influence
  exalt dispatch.
- `engine/src/solver_registry.cpp` — tiered Ember/Ichor IDs, explicit-currency
  descriptors, and `influence_exalt:<name>` rows.
- `engine/src/solver_reforge.cpp` and `engine/src/solver_calc.cpp` — exact
  dominance-sensitive transitions.
- `engine/src/solver_options.cpp` and `engine/src/solver_compile.cpp` — explicit
  side-intent setup and primitive compilation.
- `apps/web/src/app/components/pc-emulator.ts` and `pc-calculator.ts` — product
  panels and selectors.

## Emulator Support

The Eldritch panel exposes tier 1 through 4 Ember and Ichor controls plus
Eldritch Exalt, Chaos, and Annul. The Influenced panel exposes the four
currency-backed Influence Exalt choices. All outcome rules remain native.

## Solver Support

The registry has tiered implicit actions, the three explicit Eldritch actions,
and one Influence Exalt row for each of the four currency-backed mappings. All
have exact single-action support.

The user-authored `eldritch_side_intent` option can apply real implicit setup
actions and then one Eldritch Exalt, Chaos, or Annul. It retains the resulting
implicit tiers and charges setup resources. No hidden dominance flag or silent
implicit restoration is used.

When solver automatic candidates are enabled, rare carriers in
engine-certified eligible sessions may receive up to six goal-relevant
high-level candidates: Eldritch Annul Prefix/Suffix, Eldritch Chaos
Prefix/Suffix, and Eldritch Exalt Prefix/Suffix. Session eligibility remains
owned by `session_builder.cpp` and is true only for helmets, body armour,
gloves, and boots.

In product `goal_relevant` scope, the required Ember tiers, Ichor tiers,
Eldritch Exalt, Eldritch Chaos, and Eldritch Annul descriptors are retained as
automatic-option dependencies. They are not returned as standalone candidates
and do not enter the parent state layout merely because they were retained.
The native carrier-local builder resolves and admits only the dependencies of
a useful materialized side option.

Each automatic candidate reads the carrier's real implicit tiers. Existing
requested-side dominance avoids setup. Missing dominance uses the cheapest
priced legal real Ember/Ichor sequence supported by the same side-intent
machinery, then performs the real Annul, Chaos, or Exalt. Every setup resource is
charged, resulting implicit tiers remain in item state, and compilation emits
the real setup operations followed by the real final currency.

The candidates are filtered by whether their targeted or preserved explicit
side can matter to the remaining goal. Eldritch Exalt is stricter: the target
side must have capacity and an unmet goal intersecting the exact carrier's
ordinary roll pool, while the opposite side must already contain useful
satisfied goal progress. This is only action admission; Bellman chooses the
route. Automatic standalone Ember, Ichor, arbitrary implicit rolling, Veiled
crafting, and Influence Exalt are not added. Eldritch Exalt remains
dependency-only and is reachable only through the admitted compound.
Eldritch Chaos is excluded from the gated zero-progress retry basin because
its preserved side observes the discarded carrier.

Generic influence or any other carrier state rejected by the existing
Eldritch legality checks prevents automatic admission. Missing setup or final
currency prices reject the option with an explicit missing-price reason; they
do not make a dependency free.

## Calculator Support

The Calculator exposes the same Eldritch and Influenced panels as the Emulator
and resolves each visible choice to its parameterized solver registry ID for
exact calculation.

## Explicitly Unsupported Behavior

- Ember/Ichor and Influence Exalt do not combine generic and Eldritch
  influence states.
- Influence Exalt is refused when any fractured affix exists.
- An influence registry entry supplies no new automatic search grammar.
- Side intent is not an opaque primitive mechanic; it is a compound solver
  option made from real tier setup and explicit-currency actions.
- `influence_exalt` is the single-item influence primitive. Awakener uses the
  separate multi-item transaction described below.

## Open Questions Requiring Oliver

- When the dominant Eldritch target side itself carries a side-lock metamod,
  should Eldritch Chaos/Annul ignore that lock as currently implemented, or
  should the target-side operation refuse or preserve it?
- Should raw `eldritch_annul` retain its current no-rarity-guard behavior, or
  should native legality be narrowed to the rarities exposed by the solver and
  product?

## Awakener and Dominance (2026-09-28 expansion)

Awakener uses the native role-based multi-item transaction, resolves stable
modifier keys across sessions, consumes exactly one donor and changes its receiver.
The sampled subset requires same-class, distinct single influences and structural
modifiers without numerical rolls; every possible selected pair must be compatible
under all modifier groups. Group collision cases refuse atomically rather than
resampling. Elevated and above-level influence tiers can be retained. The receiver
keeps its base, level, sockets, links, quality, implicits and enchantments. Selection
and 4–6 affix refill use the existing structural engine. Possible additional
non-group exclusions remain an evidence limitation, not a verified game claim.

Dominance is recognized but unavailable: canonical data lacks explicit ordinary-T1
to elevated relationships; multi-candidate pair probabilities are unresolved.
Name and zero-weight selector metadata permit elevated retention only. They do
not establish upgrade mappings. The [execution record](../active/2026-09-28-currency-expansion/README.md)
owns the pinned sources, scope qualification and Pro handoff.
