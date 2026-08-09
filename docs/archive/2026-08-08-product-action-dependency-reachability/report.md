# Implementation Report

**Status: implementation complete; final acceptance evidence is recorded in
[final evidence](evidence/final-evidence.md).**

Parent: [milestone entry](README.md)

## Result

The Calculator product registry now has one explicit action-admission contract:

1. a **candidate** may compete independently;
2. an **automatic dependency** may be resolved only by an engine-owned fixed or
   carrier-local automatic program; and
3. a **filtered action** is excluded under a stable deterministic reason.

`pc_solver_candidates` returns only the first role. Retaining a possible
dependency does not add it to the parent state layout. A dynamic automatic
option builds an exact local carrier context and transports only the primitive
dependencies of the option that actually materialized. Telemetry reports role
and family totals, reason counts, parent layout width, fixed-option
dependencies, and materialized automatic dependencies.

## Implemented changes

### Registry and layout

- Added `ProductActionRole`, per-descriptor admission reasons, role/family
  counters, and filtered records in the native registry.
- Preserved candidate-only `pc_solver_candidates` behavior through both
  Calculator registry stages; explicit requests cannot turn a dependency-only
  descriptor into a primitive candidate.
- Removed the previous broad “all bench crafts survive when automatic mode is
  enabled” candidate rule. Goal bench crafts remain candidates; blockers,
  metamods, Multimod, Cannot Roll, and cleanup have bounded dependency reasons.
- Kept the parent layout to candidates plus dependencies of already-authored
  fixed programs. Carrier-local automatic dependencies stay in their exact
  child context and are counted only when materialized.

### Eldritch

- Ember tiers, Ichor tiers, Eldritch Chaos, and Eldritch Annul are retained
  under `automatic_eldritch_side_dependency` and remain hidden from the
  candidate API.
- Automatic synthesis now runs before delayed Harvest/Essence/Fossil rows so a
  bounded solve cannot finish without making its state-local dependencies
  reachable.
- Parent-side exact evaluation preserves the existing dominance, implicit,
  setup-price, final-price, targeted-side, and preserved-side rules.
- Generic influence and other illegal carriers are refused before admission;
  missing dependency prices remain explicit exclusions.

### Bench, metamods, and cleanup

- Added bounded Cannot Roll Attack and Cannot Roll Caster automatic routes.
  The native builder requires a supported follow-up whose pool distribution is
  changed by the corresponding tag block, positive remaining-goal relevance,
  a legal carrier, and complete setup/follow-up/cleanup pricing.
- Extended the existing temporary-bench program to compose required pre-cleanup
  and post-cleanup around blocker/metamod continuations. Cleanup remains
  dependency-only.
- Preserved permanent goal bench, side-lock protected routes, Multimod finish,
  Fracture, and Imprint behavior. Synthetic forced-price controls compile and
  execute Cannot Roll Attack/Caster at expected cost 13 and Multimod finishing
  at expected cost 6.

### Existing family filters

- Harvest still uses the existing target-tag match and filters unrelated tags.
- Ordinary Essence still requires an exact guaranteed goal modifier. The
  native loader now retains and validates `is_corruption_only`; all four
  corruption-only rows in the checked artifact are rejected before ordinary
  registry admission.
- Fossil discovery still uses the existing bounded positive-relevance beam.
  The public product probes explicitly use Calculator's
  `fossil_mode: "goal_relevant"`; a one-slot Energy Shield goal emits four of
  15,275 possible loadouts and defers 15,271.
- No Fossil special-effect analysis or full combination enumeration was added.

### Delayed-action publication repair

An ordinary Essence whose guaranteed modifier was the only route to its goal
exposed a pre-existing incremental-publication lifecycle gap. The row was
legally evaluated but had no unrelated anchor incumbent, so it remained
unpublished. A fully materialized delayed row whose only non-self exits are
goal terminals now publishes its already exact Q bound. When that admission
closes the last envelope during focused optimization, the solver runs its
normal focused direct-upper/closure proof before finalization. This changes no
crafting transition, Bellman comparison, state abstraction, or cap.

## Product disclosure

Calculator now retains telemetry for successful exact results and shows a
compact scope block beside policy quality:

- product goal-relevant action scope;
- the zero-progress destructive-reforge retry restriction;
- admitted priced counts grouped by family;
- missing-price exclusions;
- Veiled crafting as explicitly deferred; and
- unresolved action obligations after a resource stop.

The complete action identities remain in the existing collapsible detail.

## Boundaries preserved

No crafting mechanic, action price, Bellman comparison, state abstraction,
resource cap, simulator limit, strategy vocabulary, evaluator vocabulary, or
goal-progress-gated reforge rule changed. Ring/Amulet attribution and strict
partition work remain separate. No dependency-only primitive is independently
selectable. Rendered/visual review was not performed; Oliver owns it.

## Deferred

- Veiled automatic crafting remains explicitly deferred.
- A real product Multimod carrier was verified through role/dependency
  retention, while the forced-price winner remains in the exact synthetic
  automatic harness. Attempts to force the larger real carrier reached the
  existing resource/strict-materialization boundary; this milestone did not
  change abstraction or caps to manufacture a public winner.
- Ring/Amulet evaluator attribution and strict-partition repairs remain outside
  this milestone.

