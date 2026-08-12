# Veiled Crafting

**Status: current implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-07-30 on
`codex/cross-base-strategy-reliability`

Verification scope: native Veiled Chaos/Exalt/Unveil transitions, offer
generation, exact single-action calculation, solver compiler/evaluator, and
the Emulator, Calculator, and Strategy Builder controls.

## Scope

This family owns `veiled_chaos`, `veiled_exalt`, and `unveil`.

## Implemented Behavior

Only one veiled placeholder may exist on an item at a time.

- `veiled_chaos` requires a rare, non-corrupted, non-mirrored item with no
  existing veiled placeholder. It preserves fractured affixes and locked sides,
  removes other affixes, fills from the ordinary pool toward a random
  four-to-six-mod rare while reserving one slot, then adds one veiled prefix or
  suffix placeholder on an open side.
- `veiled_exalt` requires a rare, non-corrupted, non-mirrored item with no
  existing veiled placeholder and adds one placeholder on a randomly selected
  open side.
- The Simulator's placeholder creation attempts to generate up to three
  distinct persisted offers from the generic unveiled modifier mask. Each
  offer is weighted and checked for the placeholder side, item state, and
  modifier-group conflicts. Placeholder creation fails when it cannot generate
  an offer.
- `unveil` requires an existing placeholder and an explicitly selected
  `mod_key`. The selected modifier must be one of the stored offers, must match
  the placeholder side, and must not conflict with another live modifier group.
  On success it replaces the placeholder, clears the veiled flag, and records
  the chosen modifier ID.

Veiled Chaos is an ordinary metamod-respecting renewal: side locks and Cannot
Roll Attack/Caster apply to its preservation and random filler pool.

## Dated Oliver Rulings

- **2026-07-15:** selecting `unveil` is intentionally zero cost. Acquisition
  cost belongs to the preceding veiled-currency action and must not be charged
  again. This is recorded in the archived
  [economy plan](../archive/2026-07-15-economy/plan.md).

## Engine Coverage And Code Pointers

- `engine/src/session_builder.cpp` — veiled placeholder IDs and unveiled
  candidate mask.
- `engine/src/actions_basic.cpp` — `add_veiled_mod`,
  `generate_unveil_options`, `do_unveil`, and Veiled Chaos/Exalt dispatch.
- `engine/src/api.cpp` — `mod_key` parsing for explicit Unveil application.
- `engine/src/solver_reforge.cpp` — exact Veiled Chaos distribution and
  placeholder-side branching.
- `engine/src/solver_calc.cpp` — exact Veiled Exalt and Unveil distributions.
- `engine/src/solver_compile.cpp` and `engine/src/solver_eval.cpp` — strategy
  compilation and whole-graph evaluation boundary.
- `apps/web/src/app/components/pc-emulator.ts`, `pc-calculator.ts`, and
  `pc-strategy-editor.ts` — product controls.

## Emulator Support

The Veiled panel exposes Veiled Chaos and Veiled Exalt. When the live item has
stored unveil options, it shows each option and sends the chosen `mod_key` to
the native Unveil action.

## Solver Support

The registry exposes all three primitives and the single-action calculator has
exact support. Renewal fixed options may use Veiled Chaos as their renewal and
may append Unveil only immediately after that Veiled Chaos step.

The compiled strategy simulator can execute an explicit Unveil operation and
can test `has_unveil_option`. Whole-graph exact strategy evaluation carries
the sampled offer set in evaluator-pair identity. A
`has_unveil_option(mod_key)` router tests that offer context, and an authored
Unveil selection consumes the same context. A selected modifier absent from
the sampled offer is refused rather than resampled or approximated.

There is currently a cross-engine timing mismatch. The Simulator samples and
stores the offers when the placeholder is acquired. The exact solver's
`AbstractState` does not carry those stored IDs; `evaluate_unveil` instead
samples the offer set from the item state when Unveil is observed. These paths
agree for the existing immediate acquisition-to-Unveil program, but can
disagree if another action changes modifier conflicts between acquisition and
observation.

## Calculator Support

The Calculator Veiled panel exposes all three primitives. Single-action odds
for the registered actions use native exact calculation. A solver-generated
policy may also compile Unveil routing to ordinary strategy nodes.

## Explicitly Unsupported Behavior

- No additional veiled operation beyond the three named action IDs is present
  in the engine vocabulary.
- Unveil selection has no independent price key beyond its explicit zero-cost
  classification.

## Open Questions Requiring Oliver

The automatic goal-relevant program is intended to allow
`Veiled currency -> optional blocker -> observe offers`. Oliver must select one
offer-timing rule before that program is implemented:

- generate and persist offers at placeholder acquisition, matching the current
  Simulator; a blocker added afterward cannot improve those offers; or
- generate offers at observation/Unveil time, matching the current exact
  solver and allowing the requested post-acquisition blocker to change the
  distribution.

Oliver must also select the bounded continuation when none of the three offers
directly satisfies a goal: choose the best legal non-goal offer and clean up,
retry internally, or stop with the placeholder. Until both decisions are
recorded, automatic Veiled planning remains deferred and the authored
immediate acquisition-to-Unveil path is the supported solver boundary.
