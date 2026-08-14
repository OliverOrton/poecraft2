# Strategy And Solver Vocabulary

**Status: current implemented cross-mechanic vocabulary reference.**

Parent: [Mechanics](README.md)

Verified against code through the final Solver Goal Realignment native/release-
WASM acceptance: 2026-08-13.

Verification scope: strategy compiler and simulator parsers, condition
compiler/evaluator, exact whole-graph evaluator, solver registry/request
parsers/options/compiler, WASM facade/worker, and the Calculator and Strategy
Builder source surfaces.

## Scope

This file owns the implemented vocabulary that crosses mechanic families:
synthetic `restart`, mutation-free `condition_check_only`, parameterized solver
IDs, strategy conditions, and compound solver option kinds. The 26 primitive
actions and Bestiary transitions remain owned by their family files.

## Implemented Behavior

### Strategy operations

The strategy compiler/simulator accepts all 26 `pc_action_type` names listed in
[Mechanics](README.md), plus:

- `restart`: no native craft action runs; the run loop resets the item to a
  fresh normal base in the selected session and charges the `base` price key;
- `condition_check_only`: the operation node compiles as a router and does not
  mutate the item or consume a mechanic price; and
- `bestiary:imprint` and `bestiary:restore_imprint`: operate on the simulator’s
  live item plus Bestiary companion state.

Parameterized primitive operation fields are:

- Essence: `essence_key`;
- Fossil: `fossils`, with one through four keys;
- Bench and Unveil: `mod_key`;
- Harvest reforge/augment: `target_tag`;
- Harvest resistance: `source_tag` and `target_tag`;
- Ember/Ichor: `tier` from 1 through 4; and
- Influence Exalt: `influence`.

### Conditions

The compiler and simulator accept these leaf conditions:

- `always`;
- `has_mod_group` and `has_mod_family`;
- `mod_count` and `mod_family_count`;
- `item_flag`;
- `influence_bits`;
- `eldritch_tier`;
- `has_unveil_option`;
- `observation_signature` (versioned, engine-authored exact-policy routing);
- `rarity_is`;
- `open_prefix_count` and `open_suffix_count`; and
- `prefix_count_range` and `suffix_count_range`.

Composite condition vocabulary is `all`/`all_of`, `any`/`any_of`, `not`, and
`at_least`.

The `item_flag` values are `corrupted`, `mirrored`, `split`, `synthesised`,
`fractured`, `crafted`, `veiled`, `veiled_prefix`, `veiled_suffix`, `multimod`,
`no_attack`, `no_caster`, `prefixes_locked`, `suffixes_locked`, `influenced`,
and `eldritch_implicit`.

The visual condition editor directly authors only `has_mod_family`,
`item_flag`, `eldritch_tier`, `rarity_is`, `open_prefix_count`,
`open_suffix_count`, `prefix_count_range`, `suffix_count_range`, and `always`.
The other supported compiler/simulator forms require imported or advanced
JSON. `observation_signature` is not a user-authored mechanic condition: the
web model accepts and round-trips it opaquely, while the native compiler and
evaluator own its version, shape, and meaning.

### Action refinement contracts

Every admitted solver action has a versioned engine-owned observation,
preservation, and destruction contract. Its vocabulary covers only mechanic
facts that the action actually reads or can carry forward: item rarity and
side occupancy; modifier side and exclusion-effect signature; goal/tier
status; crafted, fractured, Veiled, and metamod state; locks; influence; and
Eldritch state. Equivalent modifier IDs remain mergeable when all observed
effects match.

The same admitted contract is authoritative for policy refinement, strategy
compilation, and exact graph evaluation. Those layers do not maintain
independent action-name preservation tables. Missing, contradictory, or
incomplete contracts are registry-admission errors before solving begins.

Compound planner operators expose every engine-authorized primitive execution
path. The shared refinement engine reverse-composes downstream observations
through those paths, retaining only features that may survive. Full
destructive rerolls and Restart collapse discarded identity; side-preserving
actions retain only their declared survivor scope.

Registry admission rejects an action whose contract is missing, internally
contradictory, or incomplete for a reachable source-affix trait class. Adding
an ordinary future action therefore requires its mechanic descriptor,
semantic contract, and focused tests; it does not require a Regal/Exalt-style
branch in refinement, compilation, or exact evaluation.

### Solver action IDs

The complete parameterized registry grammar is:

- fixed primitive IDs and synthetic `restart`;
- `essence:<metadata-key>`;
- `fossil:<sorted-unique-key-list>` for one through four Fossils, joined with
  `+`;
- `bench:<mod-key>`;
- `harvest_reforge:<allowlisted-tag>`;
- `harvest_augment:<allowlisted-tag>`;
- `harvest_resist:<source>:<target>`;
- `eldritch_ember:<1..4>` and `eldritch_ichor:<1..4>`;
- `influence_exalt:<currency-influence>` for `crusader`, `hunter`, `redeemer`,
  and `warlord`; and
- fixed `fracture` and `remove_crafted_modifiers` IDs.

Fossil cost vectors contain each Fossil key and a resonator-size key. Harvest
resistance IDs include source and target but use a target-only cost key. Bench
uses canonical bench-option currency quantities. Restart uses `base`, which is
manual-only and never silently zero.

Influence Exalt strategy operations and price keys use those four public
currency names. The parser accepts the previously emitted internal aliases
`adjudicator`, `basilisk`, and `eyrie` for persisted strategy compatibility,
but normalizes their accounting to `warlord`, `hunter`, and `redeemer`.
`elder` and `shaper` remain generic modifier-pool identities and are not
Influence Exalt operations.

The automatic Imprint option uses the Bestiary descriptor's four-entry cost
vector: one `beast:craicic-croaker` and three repeated `beast:rare` keys. The
current economy supplies Croaker as a required market quote and the rare key as
an overridable one-chaos-per-beast `owner_default`. Price completeness is
evaluated against those engine-authored keys; missing identities remain
incomplete rather than being silently zeroed.

### Compound solver options

Solver request JSON accepts these user-authored fixed-option kinds:

- `scour_alchemy`;
- `eldritch_side_intent`;
- `protected_side`;
- `multimod_finish`;
- `renewal`;
- `protected_repeat`; and
- `fracture_prepare`.

`imprint_retry` is rejected as user-authored and is discovered automatically.
The automatic candidate system can also synthesize permanent goal benches,
temporary bench blocker/repeat, protected metamod, Multimod finish, primitive
Fracture, and Imprint kernels. The internal temporary-bench repeat kind is not
a user request kind.

A `renewal` program is deliberately bounded. Its valid forms are one of
Alteration, Chaos, Essence, Fossil, Harvest reforge, or Veiled Chaos; Scour
followed by Alchemy; or Veiled Chaos followed by an observed Unveil. Invalid
sequences are rejected rather than treated as a generic macro language.

An observed-Unveil fixed program retains the exact pre-Unveil carrier identity
on every choice group. A preference or compiled branch for one observation
carrier never matches an offer from a different carrier solely because the
offered modifier ID or projected successor is equal.

Every selected option compiles to primitive strategy operations and routers.
The simulator never executes an opaque option action.

## Dated Oliver Rulings

- **2026-07-15:** `base` is manual-only, absent until supplied by a user
  override, and never silently zero; Unveil selection is explicitly zero cost.
- **2026-07-17:** automatic action-space work recognizes Fracture, permanent
  bench, temporary blocker, protected metamod, Multimod finish, and cleanup
  roles, but compound options must expand to primitives rather than become new
  crafting rules.
- **2026-07-18:** user-authored Imprint programs/exits were removed; Imprint is
  state-local automatic discovery, and checkpoint creation’s magic requirement
  is not a final-goal requirement.
- **2026-07-18:** product Fracture planning uses the ordinary primitive and a
  priced Restart miss route; `fracture_prepare` remains only for explicit
  authored envelopes.
- **2026-08-09:** automatic Imprint pricing uses Craicic Croaker plus three
  generic rare beasts; each generic rare has an explicit, user-overridable
  one-chaos owner default with non-market provenance.

Sources are the archived
[economy plan](../archive/2026-07-15-economy/plan.md),
[S7 plan](../archive/2026-07-solver-s7/plan.md),
and [S8/B1 plan](../archive/2026-07-19-bestiary-solver-s8/plan.md).

## Engine Coverage And Code Pointers

- `engine/src/simulator.cpp` — operation and condition parsing and sampled run
  execution.
- `engine/src/solver_registry.cpp` — complete action-ID grammar and descriptors.
- `engine/src/solver_api.cpp` — fixed-option request parsing and validation.
- `engine/src/solver_options.cpp` — exact compound kernels and automatic kinds.
- `engine/src/solver_compile.cpp` — primitive graph emission, including
  synthetic Restart.
- `engine/src/solver_eval.cpp` — whole-graph exact evaluator and explicit gaps.
- `engine/src/solver_calc.cpp` — exact single-action support, including all 26
  primitives and Restart.
- `bindings/wasm/wasm_api.cpp` — WASM solver/simulator facade.
- `apps/web/src/app/engine-protocol.ts`, `engine-wasm.ts`, and
  `engine-worker.ts` — TypeScript protocol, release facade, and worker routing.
- `apps/web/src/app/components/pc-strategy-editor.ts` and
  `pc-condition-editor.ts` — visual authoring subsets.

## Emulator Support

The Emulator exposes the primitive mechanic families and both Bestiary
operations, but not synthetic Restart or `condition_check_only`. Its “Use in
Strategy” command exports an item snapshot; it does not add new action
vocabulary.

## Solver Support

All 26 primitives and Restart have single-action exact-calculation support.
The registry is session-dependent and may omit parameterized actions whose
data/pool is unavailable. Relevance, legality, price, and automatic-candidate
filters can further narrow a solve without changing the vocabulary.

Whole-graph exact strategy evaluation resolves `mod_count`,
`mod_family_count` (including crafted/fractured requirements),
`has_unveil_option`, and authored Unveil selection. Offer identity is carried
from the sampled Veiled outcome through the selected Unveil operation.
Bestiary operations continue to use their separate stateful calculation path
rather than ordinary one-item evaluator actions.

## Calculator Support

The Calculator exposes Restart in its basic panel, uses the complete solver
registry for action selection and solves, and has a dedicated Bestiary
calculation path. It does not present `condition_check_only` as a user action.
Solver-generated strategies can contain Restart and automatic compound-option
expansions.

## Explicitly Unsupported Behavior

- There is no open-ended user macro/program language beyond the named bounded
  fixed-option schemas.
- User-authored `imprint_retry` is rejected.
- The visual Strategy Builder cannot directly author every condition accepted
  by advanced JSON.
- The visual operation dropdown omits `restart`, even though solver-generated
  Restart compiles and simulates.
- Whole-graph exact evaluation does not treat the two Bestiary operations as
  ordinary one-item calculator actions; sampled support must not be described
  as ordinary exact-evaluator support.

## Open Questions Requiring Oliver

- Should `restart` become a directly authorable Strategy Builder operation, or
  remain solver-generated/import-only?
- Should Bestiary operations be integrated into the ordinary whole-graph
  evaluator, or remain on their existing separate stateful calculation path?
