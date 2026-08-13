# Veiled Crafting

**Status: current implemented mechanic reference.**

Parent: [Mechanics](README.md)

Verified against code: 2026-08-13 on
`codex/solver-goal-realignment`

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
- **2026-08-13:** the three offers are fixed when the veiled placeholder is
  acquired. An action after acquisition cannot improve those offers. When an
  observed set contains no goal modifier, choose the legal offered modifier
  with the best exact cleanup/retry continuation rather than resampling or
  stopping at the placeholder.

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
exact support. Renewal fixed options may use Veiled Chaos or Veiled Exalt as
their acquisition and may append Unveil only immediately after that acquisition
step.

The compiled strategy simulator can execute an explicit Unveil operation and
can test `has_unveil_option`. Whole-graph exact strategy evaluation carries
the sampled offer set in evaluator-pair identity. A
`has_unveil_option(mod_key)` router tests that offer context, and an authored
Unveil selection consumes the same context. A selected modifier absent from
the sampled offer is refused rather than resampled or approximated.

The automatic product path admits bounded immediate programs with optional
relevant crafted-mod cleanup before Veiled Chaos or Veiled Exalt, followed
immediately by observed Unveil. The Simulator samples and stores offers at
acquisition. The exact solver's `AbstractState` does not carry those stored
IDs and `evaluate_unveil` enumerates them at observation, but the two paths are
distribution-equivalent for this grammar because no state-changing action may
occur between acquisition and observation. A post-acquisition blocker remains
unsupported and would violate that proof boundary.

The observation group retains every legal offer. Bellman choice ordering picks
the least-cost continuation, so an offer set with no direct goal hit chooses
the best exact cleanup/retry continuation through its best legal non-goal
modifier. The selected policy compiler emits `has_unveil_option` routing, the
exact evaluator carries offer-set identity, and the Simulator consumes the
persisted acquisition-time offer set.

## Calculator Support

The Calculator Veiled panel exposes all three primitives. Single-action odds
for the registered actions use native exact calculation. A solver-generated
policy may also compile Unveil routing to ordinary strategy nodes.

## Explicitly Unsupported Behavior

- No additional veiled operation beyond the three named action IDs is present
  in the engine vocabulary.
- Unveil selection has no independent price key beyond its explicit zero-cost
  classification.
- Post-acquisition blockers are not admitted by the automatic grammar because
  offers are already fixed.
