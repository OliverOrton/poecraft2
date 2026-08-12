# Gate 1 action-family matrix

**Inspected:** 2026-08-09 on `codex/solver-goal-realignment` after the Gate 0
boundary. This is a source-and-evidence audit of the shared working tree; it is
not a new native or release-WASM qualification run.

Parent: [active plan](../plan.md)

## Reading this matrix

The milestone requires more than primitive support. For each family the audit
separates these claims:

1. the primitive exists and has native legality;
2. product filtering retains it as a candidate or a dependency, or records a
   deterministic exclusion;
3. a legal concrete carrier can materialize a state-local option when one is
   required;
4. the scheduler completes a row and Bellman has a Q value or a valid
   non-improvement certificate;
5. a selected policy can compile, exact-evaluate, and run in the Simulator;
6. the same public path exists through the C ABI and release WASM.

`Implemented` below means the source path exists. `Focused evidence` means a
family-specific checked test or retained milestone result exercises that
stage. `Gap` means the required product stage is absent. `Unqualified` means
the generic implementation exists but no current family-specific product
proof was found. These terms deliberately do not turn an explicit authored
strategy or an unrestricted registry test into proof of automatic product
coverage.

All ordinary primitives share the global refusal of null, corrupted, or
mirrored items. `restart` is the synthetic exception: it buys the fixture- or
user-priced base and restores the clean starting item.

## Engine ownership and shared paths

The inspected ownership boundaries are:

- primitive execution and legality: `engine/src/actions_basic.cpp` and
  `engine/src/actions_bestiary.cpp`;
- action descriptors, product roles, reason codes, and cost vectors:
  `engine/src/solver_registry.cpp`;
- carrier-local automatic programs: `engine/src/solver_options_helpers.hpp`,
  `solver_options_automatic.cpp`, and `solver_options_build.cpp`;
- exact rows: `engine/src/solver_calc.cpp` and `solver_reforge.cpp`;
- scheduling, row lifecycle, and Bellman work: `solver_solve_expand.cpp`,
  `solver_solve_incremental.cpp`, and `solver_solve_bellman.cpp`;
- selected-policy serialization: `solver_compile.cpp` and
  `solver_compile_serialization.hpp`;
- exact graph evaluation: `solver_eval*.cpp` over the same typed strategy
  descriptors parsed by `simulator.cpp`;
- public native surface: `engine/include/poecraft/solver.h`,
  `simulator.h`, and `bestiary.h`; and
- browser facade: `bindings/wasm/wasm_api.cpp` and
  `apps/web/src/app/engine-wasm.ts`.

The compiled strategy serializer has a case for every current ordinary
`ActionType`. The Simulator parser has the matching operation vocabulary plus
synthetic `restart` and the two Bestiary operations. This establishes a shared
implementation path; family-specific rows below still distinguish whether a
real product-selected policy has proved it.

## Admission, price, and carrier matrix

| Product family | Primitive legality and supported carriers | Product role and deterministic reason | Required price keys and provenance | Carrier-local automatic generation |
| --- | --- | --- | --- | --- |
| Basic currency and Restart | `transmute` normal; `augment`/`alteration`/`regal` magic; `alchemy` normal; `chaos`/`exalt` rare; `annul` and `scour` magic/rare in the registry; all ordinary engine sessions. `restart` is always legal. | Ordinary actions are candidates with `candidate_general_currency`; Restart is `candidate_structural_restart`. | Direct currency IDs are `quote`; Restart uses manual-only `base`. | Primitives are global candidates. They can also be steps in constructive renewal, temporary blocker, protected-side, Imprint, and cleanup programs. |
| Crafted-mod cleanup | `remove_crafted_modifiers` requires a magic/rare carrier with a non-fractured crafted affix. | Dependency-only as `automatic_crafted_cleanup_dependency` when a materializable automatic family needs it; otherwise `filtered_cleanup_without_materializable_family`. | One `scour`, provenance `quote`. | Added only before/after the relevant bounded bench/metamod program, never as a universal product action. |
| Essence | Any rarity; session must resolve the Essence's guaranteed modifier and item-level restriction. Corruption-only Essences are unsupported product primitives. | Candidate only for an exact guaranteed goal modifier: `candidate_exact_essence_goal`. Otherwise `filtered_essence_without_exact_goal_mod`; corruption-only rows use `filtered_corruption_only_essence`. | `essence:<metadata-key>`, market `quote`. | No compound automatic family and deliberately excluded from protected-side programs because Essence ignores metamods. |
| Harvest | Reforge: rare. Augment: magic/rare, open affix, no generic influence or Eldritch implicit. Resist conversion: magic/rare with a convertible unlocked source. Only the owner allowlist and session-nonempty tags exist. | Candidate when the requested target tag intersects a goal: `candidate_goal_tag_harvest`; otherwise `filtered_harvest_without_goal_tag`. | `harvest_reforge:<tag>`, `harvest_augment:<tag>`, or target-specific `harvest_resist:<target>`; derived `recipe` provenance from live lifeforce quotes. | Reforge can be the follow-up in protected-side programs. Augment can be a temporary-blocker/Cannot-Roll follow-up. The primitive remains independently selectable when goal-relevant. |
| Bounded Fossil | One to four distinct real Fossils, any rarity, session-resolvable loadout. The action ignores metamods and preserves fractures. | Bounded positive-relevance beam: `candidate_bounded_goal_relevant_fossil`. Non-emitted loadouts are counted as deferred rather than silently filtered. | One `fossil:<key>` per component plus `resonator:<1..4>`; both market `quote`. | Global candidate only. It is deliberately excluded from protected-side programs. |
| Permanent bench goal | Magic/rare, session bench-craftable modifier, open side, crafted-count and group legality. Crafted and natural family identity remain distinct in goal resolution. | Direct goal craft is `candidate_direct_goal_bench`; unrelated bench rows are filtered or retained for a separate automatic role. | `bench:<mod-key>`, derived `recipe` provenance. | Deterministic legal goal finishes are tagged `PermanentBench`; the state-local pass admits the finish only on a legal carrier. |
| Temporary bench blocker and cleanup | Ordinary non-metamod bench craft plus an eligible add/upgrade follow-up; blocker must change conflicts or side capacity without blocking the goal. | Blocker is `automatic_temporary_bench_dependency`; cleanup is the dependency described above. No standalone blocker candidate. | Blocker `bench:<mod-key>` (`recipe`), follow-up's own keys, and `scour` (`quote`) when cleanup is needed. | Exact effect classes are precompiled, then carrier legality, price completeness, target reachability, and blocked-pool benefit are checked. Equivalent blocker-price variants collapse. |
| Prefixes/Suffixes Cannot Be Changed | Bench legality above. Automatic route additionally needs useful satisfied progress on the protected side and a supported follow-up that respects the lock. | `automatic_protected_side_dependency` (or `authored_metamod_dependency` in an explicit option). | Lock `bench:<mod-key>` (`recipe`) plus follow-up and cleanup keys. | Carrier-local `ProtectedSide` or `ProtectedRepeat`; the exact protected kernel is compared with its unprotected baseline before admission. |
| Cannot Roll Attack/Caster | Bench legality above. A supported add follow-up must have removable blocked-tag competitors and a rollable non-blocked goal. | `automatic_cannot_roll_dependency`. | Cannot-Roll `bench:<mod-key>` (`recipe`) plus follow-up and optional `scour`. | Carrier-local temporary-repeat variant marked `CannotRoll`; admitted only when the exact blocked pool changes beneficially. |
| Multimod finish | Bench legality above; Multimod and two mutually compatible, legal goal bench crafts must fit crafted and side capacity. | Multimod is `automatic_multimod_dependency`; goal benches retain their direct-goal role. | Three `bench:<mod-key>` recipe-derived keys. | Carrier-local deterministic `MultimodFinish`, generated for legal goal-craft pairs only. |
| Eldritch Ember/Ichor setup | Tier 1-4 implicit pools on helmets, body armours, gloves, or boots; no generic influence. | `automatic_eldritch_side_dependency` in product mode; never a standalone goal-relevant candidate. | `eldritch_ember:<tier>` and `eldritch_ichor:<tier>`, direct market `quote`. | Carrier-local side intent reads actual tiers and chooses the cheapest priced legal dominance setup. Existing dominance needs no setup. |
| Eldritch Chaos | Rare Eldritch-eligible carrier. The target side is chosen by real implicit dominance; generic influence is illegal. | `automatic_eldritch_side_dependency`. | `eldritch_chaos` plus any setup keys, all `quote`. | Generated for a side only when missing goal progress or useful opposite-side preservation makes that side relevant. |
| Eldritch Annul | Eldritch-eligible carrier; product automatic use is rare and dominance-targeted. Generic influence is illegal. | `automatic_eldritch_side_dependency`. | `eldritch_annul` plus setup keys, all `quote`. | Same bounded side-intent generator as Eldritch Chaos. |
| Eldritch Exalt | Rare Eldritch-eligible carrier, open target-side capacity, no generic influence. | `automatic_eldritch_side_dependency`; dependency-only, never standalone. | `eldritch_exalt` plus setup keys, all `quote`. | Generated only when an unmet target-side goal is rollable on the exact carrier and the strict opposite side already contains useful satisfied progress. |
| Fracture | Rare, at least four explicits, no generic influence, synthesis, or existing fracture. | Product candidate `candidate_fracture`. | `fracture` market `quote`; recovery needs fixture/user `base`. | Primitive carrier row, tagged `PrimitiveFracture` for automatic telemetry. Product mode does not use the authored `fracture_prepare` closure. |
| Imprint | Dedicated Bestiary create requires magic and no checkpoint; restore requires the same live item and a checkpoint. Both refuse corrupted/mirrored state. | Not a flat registry primitive. Automatic-only `ImprintRetry`; user-authored options are rejected. | Create consumes `beast:craicic-croaker` (`quote`) plus three `beast:rare` (`owner_default`, overridable). Restore is zero cost. | Bounded carrier-local program discovery tries priced, exact ordinary candidate programs from a legal magic checkpoint and retains useful goal exits. |
| Influence Exalt | Four currency-backed mappings only; rare, open affix, no generic influence, Eldritch implicit, or fractured affix. All ordinary base classes with a nonempty matching influence pool can expose a row. | Candidate only for a goal in the exact influence pool: `candidate_goal_influence`; otherwise `filtered_influence_without_goal_mod`. | `influence_exalt:{crusader,hunter,redeemer,warlord}`, direct market `quote`. | No compound automatic route. It is a global primitive candidate when goal-relevant and carrier-legal. Elder and Shaper do not produce actions. |
| Veiled Chaos acquisition | Rare, no existing veiled placeholder. Sampled native execution persists offers at acquisition, but the abstract exact solver does not carry that offer set. | **Gap:** `filtered_veiled_option_deferred`. | `veiled_chaos`, direct market `quote`. | **None in product mode.** Explicit authored renewal support does not satisfy Gate 3. |
| Veiled Exalt acquisition | Rare, open side, no existing veiled placeholder. Sampled native execution persists offers at acquisition, but the abstract exact solver does not carry that offer set. | **Gap:** `filtered_veiled_option_deferred`. | `veiled_exalt`, direct market `quote`. | **None in product mode.** It cannot currently enter the temporary-blocker path because filtering removes it before automatic synthesis. |
| Observed Unveil selection | Sampled execution requires one of the placeholder's persisted offered modifier IDs; choice is zero cost. The current exact solver instead recomputes offers from its observation state. | **Gap:** `filtered_veiled_option_deferred`. | `unveil`, provenance `zero`. | **None in product mode.** No automatic acquisition -> observe -> choose -> cleanup program exists, and the sampled/exact offer-identity contradiction requires Oliver's ruling. |

The current Allflame snapshot
`economy:allflame:de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`
contains complete direct keys for basic, Veiled, Eldritch, Fracture, the four
Influence Exalts, and Unveil; all 30 Harvest allowlist prices; all four
resonators; 101 priced/5 explicitly missing Essences; 25 priced/420 explicitly
missing Fossils; 673 priced/70 explicitly missing bench crafts; and both
Imprint inputs. Missing dynamic keys remain visible exclusions rather than
fallback prices. `base` remains a manual fixture/user input.

## Scheduler, Q, and selected-policy matrix

| Product family | Scheduler, completed row, and Bellman Q | Selected-policy and downstream evidence | C ABI and release-WASM evidence |
| --- | --- | --- | --- |
| Basic currency and Restart | Shared priced primitive scheduling; exact deterministic, single-slot, or reforge rows. `action_analysis.search_cost` reports built and interrupted rows by action ID. | Repeated native and release controls compile/evaluate/simulate ordinary renewal and Restart policies. | Generic action, Calculator, solve, compile, evaluator, and Simulator C surfaces exist. Release smoke covers common currency paths. |
| Crafted-mod cleanup | Only scheduled inside a materialized fixed option in product mode. The fixed option receives a normal Bellman row/Q when complete. | Compiler and Simulator use the real `remove_crafted_modifiers` operation and `scour` accounting. No fresh family-specific release selected-policy proof is pinned. | Public through generic strategy/solve surfaces; release facade source exists, current family proof unqualified. |
| Essence | Shared exact reforge row/Q for priced retained candidates. | Native calculation and Bellman-selection checks exist; compiler/Simulator vocabulary and WASM sampled transition smoke exist. No fresh current-snapshot forced product winner was found. | C and WASM generic paths implemented; release primitive smoke is not product selection proof. |
| Harvest | Shared exact reforge/special row/Q; protected/temporary uses a state-local fixed-option row. | Primitive compiler/evaluator support and an authored release exact-evaluation control exist. No fresh selected automatic Harvest product policy is pinned. | C and WASM generic paths implemented; current product-family end-to-end proof unqualified. |
| Bounded Fossil | Shared exact reforge row/Q for every emitted, priced loadout; deferred beam entries are counted before row construction. | Native exact calculations and many solver corpus rows exist; compiler/Simulator vocabulary and release primitive smoke exist. No fresh forced selected Fossil product policy is pinned. | C and WASM generic paths implemented; release primitive smoke is not selection proof. |
| Permanent bench goal | Shared deterministic candidate row; state-local legality records automatic eligibility. | Focused native and release-WASM automatic bench policies compile and simulate successfully. | Public generic C path and family-specific release smoke are present. |
| Temporary blocker and cleanup | Carrier-local fixed option is synthesized before admission; once admitted it uses the shared row lifecycle and Bellman Q. | Native temporary-repeat compilation/evaluation coverage and historical fixtures exist. A fresh current release selected product path is not pinned. | Exposed through solve telemetry and compiled strategy, not as a standalone candidate. Release family proof unqualified. |
| Prefix/Suffix locks | Carrier-local protected kernel; incomplete or neutral comparisons reject before scheduling, admitted comparisons receive a fixed-option row/Q. | Native protected-side/repeat compiler and evaluator tests exist. No fresh release product winner is pinned. | Public via ordinary bench operations plus generic solve/compile; release family proof unqualified. |
| Cannot Roll Attack/Caster | Carrier-local temporary-repeat kernel; admitted only after exact blocked-pool benefit. Shared row/Q after admission. | Primitive metamod behavior is tested. No retained proof was found of a product-selected Cannot-Roll strategy compiling, exact-evaluating, and simulating end to end. | C/WASM operation support exists; **selected product evidence gap**. |
| Multimod finish | Carrier-local deterministic fixed-option row/Q. | Native compile/evaluator/Simulator coverage exists for Multimod programs. No fresh release selected product proof is pinned. | C/WASM generic operation support exists; release family proof unqualified. |
| Eldritch setup + Chaos/Annul | Parent-carrier local side-intent rows use exact primitive kernels and shared Bellman selection. Automatic telemetry records candidate, eligible/rejected/deferred, selected/discarded rows under `eldritch_side`. | Focused native and release-WASM forced-winner evidence compiles, exact-evaluates, and completes 10,000 Simulator runs for the pre-Exalt side family. | Generic C/WASM surfaces plus family-specific release proof exist for Chaos/Annul side intent. |
| Eldritch Exalt | Same parent-carrier local row/Q path; current focused native tests cover both sides, rejection controls, selection, compilation, exact evaluation, and 10,000 simulation runs. | Fresh native Gate 4 evidence exists. The release module has not yet been rebuilt and requalified with the new dependency path. | C source path implemented; **release-WASM requalification pending**. |
| Fracture | Shared exact special row/Q. Miss recovery competes through priced Restart. | Native primitive, exact distribution, compiler, evaluator, and selected-policy evidence exist; release smoke proves primitive application. No fresh release product-selected Fracture result is pinned. | Generic C/WASM paths implemented; selected release proof unqualified. |
| Imprint | Carrier-local `ImprintRetry` row/Q after bounded discovery and exact checkpoint kernel construction. Automatic telemetry has a dedicated `imprint` kind. | Current Allflame public product gate selected Imprint, compiled both Bestiary operations, exact-evaluated with success probability one, and completed 10,000/10,000 native Simulator successes with complete material accounting. | Dedicated Bestiary C/WASM facade and binding tests exist. **Automatic selected-policy release-WASM proof remains pending.** |
| Influence Exalt | Shared exact single-slot row/Q when goal-relevant, priced, and legal. `influence-product-v2` admits one Influence Exalt candidate and completes the canonical Warlord root row (1 row, 5 outcomes). | Native primitive, registry, compiler, evaluator, and Simulator vocabulary tests exist. The v2 product run does **not** publish a policy: eager automatic admission hits a child `max_discovered_states` cap with only 698 retained states. Selected-policy proof remains pending; the cap/anytime obligation belongs to Gate 5. | Generic C/WASM paths exist; current release artifact predates the canonical identity repair. |
| Veiled acquisition + observed Unveil | **No product scheduler row or Q: all three primitives are filtered.** | Explicit authored paths compile `has_unveil_option` routers, but exact evaluation recomputes offers from the observation state while sampled Simulator execution consumes offers persisted at acquisition. This is not a coherent automatic selected-policy proof. | Generic C/WASM primitive and authored-strategy paths exist; **automatic product C/WASM coverage is absent**. |

For every admitted primitive or fixed option, row construction feeds the same
sparse Bellman machinery. That architectural fact does not prove that a row
was scheduled or completed in a bounded run. Current `action_analysis` can
show per-action built/interrupted row counts, while automatic telemetry can
show generated, eligible, rejected, collapsed, deferred, selected, and
discarded candidates. Neither surface currently gives every family a durable
root-level tuple of `completed Q`, `certified non-improving`, or `unresolved`.

## Confirmed coverage failures and audit findings

### 1. Automatic Veiled planning is absent

This is the only confirmed family whose required product route has no
admission or scheduler path. Unrestricted-registry and authored-strategy tests
prove the primitive/evaluator vocabulary while product filtering records all
three primitives as `filtered_veiled_option_deferred`. This is exactly the
focused-test-versus-product-filter failure pattern Gate 1 asks us to find.

The pending mechanic ruling about when the three offers are generated blocks
the automatic acquisition/blocker/observation sequence. Sampled native
execution persists the offers at acquisition, while the abstract exact solver
does not retain that identity and `evaluate_unveil` recomputes from the
observation state. The audit records this contradiction and does not choose a
new rule.

### 2. The primitive family identity gap is repaired; Q disposition is not

The audited baseline reported Eldritch, Influence Exalt, and Veiled as
`other`. The Gate 1 contract now gives Eldritch setup, Chaos, Annul, and Exalt,
Influence Exalt, Veiled Chaos, Veiled Exalt, and Unveil distinct additive JSON
telemetry identities. `Other` remains only as a compatibility value; the
exhaustive native table rejects any current ordinary registry action that maps
to it.

Automatic telemetry still intentionally has one `eldritch_side` operator
bucket because that identity describes the compound carrier-local option, not
one primitive. There is no automatic Veiled bucket because product admission
remains explicitly deferred. The remaining acceptance gap is the absence of a
per-family terminal Q disposition, described next.

### 3. Row counts are not Q-completeness certificates

`action_analysis.search_cost` records rows, outcomes, transitions, work,
cache use, and the last interrupted row per action ID. It does not state, for
every relevant root family, one of:

- a completed finite Q;
- a valid non-improvement certificate; or
- an unresolved obligation with the stopping resource.

The Gate 0 primary result demonstrates the consequence: thousands of exact
rows existed while 33,990 incremental alternatives were still unevaluated.
The Chaos incumbent was executable, but its existence did not certify Chaos
optimality.

### 4. Price provenance stops at the runtime-envelope boundary

The immutable economy snapshot retains `sources` with `quote`, `recipe`,
`zero`, and `owner_default`. The native `pc_economy_load_json` currently stores
only key/value prices in `EconomyImpl`; solver and evaluator telemetry do not
carry provenance. This does not change cost arithmetic or expose provenance
through the C ABI. The focused Gate 1 native assertion now parses the pinned
snapshot directly: Croaker must be `quote`, the rare beast must be
`owner_default`, registry keys must match their `quote`/`recipe`/`zero`
category or appear in `missing_keys`, and the manual base price remains
explicitly outside the snapshot.

### 5. Product-selected evidence remains uneven

Fresh end-to-end selected-policy evidence exists for permanent Bench,
Eldritch side intent, Eldritch Exalt, and Imprint. Primitive and authored graph
coverage exists for all other implemented families, but current product-path
winner evidence was not found for Cannot Roll, Influence Exalt, and automatic
Veiled; fresh current-snapshot forced winners are also absent for Essence,
Harvest, Fossil, temporary blockers, locks, Multimod, and Fracture. The matrix
records those as unqualified instead of inferring selection from vocabulary
coverage.

Influence now has stronger partial evidence at
`build/solver-goal-realignment/gate1/influence-product-v2`: product admission
reports `candidate.by_family.influence_exalt = 1`, and the canonical Warlord
root row completes with five outcomes. The run still cannot establish a
selected policy because eager automatic admission reaches a child
`max_discovered_states` boundary at 698 retained states. Gate 1 records the
completed root row and unresolved selected-policy obligation separately;
Gate 5 owns the cap/anytime repair.

### 6. Other requested failure-pattern searches

- Synthetic prices: the new Eldritch Exalt forced-winner fixture labels its
  synthetic dependency-only override and the benchmark validates that the
  live snapshot owns all primitive keys. The current Imprint product fixture
  uses the live Allflame Bestiary prices and discloses its two unrelated
  suppressing overrides. Older narrow unit tests still use synthetic prices
  and remain unit evidence only.
- Delayed rows and publication: the Gate 0 primary run confirmed an open
  alternative envelope. Scheduler and resource-accounting repairs are active
  work; this matrix does not mark the primary family set complete.
- Completed values without executable provenance: Gate 0 captured Ring and
  Amulet core values whose independent evaluation/publication failed. The
  current evaluator repair is not accepted until those focused cases publish.
- Compiler vocabulary: no missing serializer/parser case was found for a
  current primitive or Bestiary operation. Lack of a selected family policy
  remains an evidence gap, not proof that its compound graph will compile.
- Frontend presentation: the public result currently has aggregate admitted
  families, prices, bounds, and unresolved diagnostics but no engine-owned
  complete family/Q certificate. Presentation work cannot manufacture this
  missing authority.
- Resource counters: transient automatic-discovery rows/transitions are being
  separated from retained sparse-graph caps. The stable V1-equivalent reforge
  counter still aggregates evaluator work by design; per-action search costs
  are observational and not independent cap authorities.

## Implemented executable engine-owned contract

The Gate 1 repair adds no crafting rules and no public ABI. It consists of one
private native vocabulary table, registry-finalization validation, additive
telemetry identities, and one focused native snapshot assertion.

### Private family table

`engine/src/solver_action_family_contract.hpp` now owns an exhaustive
`ActionFamilyContract` entry for all 26 `ActionType` values. Each entry contains
only audit identity, never mechanic behavior:

```text
primitive telemetry family
primitive ActionType and canonical registry identity shape
allowed product roles and stable reason codes
cost-key schema and expected provenance category
canonical operation name
carrier-local generation path
scheduler-admission path
row-completion and Bellman-Q paths
selected-policy evidence status
compiler and exact-evaluator paths
Simulator and C-API paths
release-WASM coverage/rebuild status
```

Registry finalization maps every retained or filtered descriptor to exactly
one table entry and recomputes the role/family/reason counters as an assertion.
Unmapped descriptor identities, stable reasons, operation names, cost-key
shapes, or counters throw before the registry is returned. A second exhaustive
table maps every non-`None` `AutomaticCandidateKind` to one distinct automatic
telemetry identity. Bestiary is represented explicitly by its canonical
`bestiary:imprint` and `bestiary:restore_imprint` operation contracts: create is
mixed provenance (Croaker
`quote` plus rare-beast `owner_default`) and restore is zero cost. Manual
fixture/base overrides remain a separate provenance category and cannot satisfy
the owner-default contract.
The table validates vocabulary and accounting; native actions, Bestiary
descriptors, and session data remain the mechanic and legality authorities.

The calculation and product-admission paths consume this shared mapping, so
Eldritch setup, Eldritch Chaos, Eldritch Annul, Eldritch Exalt, Influence
Exalt, Veiled Chaos, Veiled Exalt, and Unveil no longer collapse to `other`.
Their names are emitted through additive telemetry JSON; public C structs are
unchanged.

### Remaining focused corpus assertion

The identity contract does not claim family/Q completeness. A later acceptance
gate must extend the goal-realignment manifest with one evidence case or
deterministic exclusion probe per family. The native harness should fail
unless the corpus records exactly one terminal disposition for every required
family:

```text
selected with completed Q and verified executable policy
completed Q and certified non-improving
mechanically/carrier ineligible with stable reason
missing price with the exact missing key
unresolved with the exact stopping resource
```

For a selected family the harness must additionally require compiler success,
exact evaluator convergence, success probability one, complete pricing, zero
failure/off-policy/unresolved mass, and Simulator success when the fixture
requests 10,000 runs. The release-WASM runner should consume the same manifest
instead of maintaining a second family list.

The contract must fail when a required family has zero registry/admission
observations across the corpus without an explicit deterministic exclusion.
It must also fail when a family has generated/eligible automatic options but
no completed row, Q/certificate, or unresolved obligation. These two checks
close the largest blind spots without turning the full family matrix into a
routine solve suite.

### Provenance assertion

The focused calculation test parses pinned Allflame snapshot
`de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0.json`
and validates each emitted cost key against the table's
expected category; absent keys must be present in the snapshot's explicit
`missing_keys`. It also asserts the mixed Bestiary components and zero-cost
Unveil/Imprint restore identities. This keeps provenance at its economy
authority and avoids a C ABI change. If the frontend later needs provenance
on an individual solver action, carrying `sources` into `EconomyImpl` should
be treated as a separate additive product contract, not inferred from the
numeric price.

### Focused verification

The isolated native target was run against `data/compiled/current` after the
shared engine binary relinked:

```text
build/engine/poecraft_engine_tests.exe --solver-family-contract-only data/compiled/current
solver family-contract tests: 182656 checks, 0 failures
```

This target builds the full Vaal Regalia registry, requires every `ActionType`
to be observed, validates every retained/filtered descriptor, role/reason,
cost-key shape, support-path field, automatic-kind identity, canonical
Bestiary operation, and pinned-snapshot provenance category, and confirms all
three Veiled primitives retain the explicit deferred disposition. It does not
run a broad solver suite or claim the remaining family/Q acceptance evidence.

## Gate 1 disposition

The primitive-to-Simulator vocabulary is complete for the currently
implemented families. Product reachability is not: automatic Veiled is absent,
and family/Q audit telemetry cannot yet enforce the requested terminal
disposition matrix even though registry identity, admission reason, cost-key,
automatic-kind, and snapshot provenance mappings now fail closed. Several
families still have only primitive or authored-policy evidence. These are
assigned to the Veiled mechanic ruling/implementation, solver completeness,
and final native/release-WASM acceptance gates rather than being hidden behind
the shared generic code path.
