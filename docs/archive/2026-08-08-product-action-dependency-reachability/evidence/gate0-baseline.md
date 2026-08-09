# Gate 0 Baseline

**Frozen:** 2026-08-08 at
`c95359663088d515982ba33e83fe2d15f89438ee`.

Parent: [milestone entry](../README.md)

## Repository boundary

- Branch: `codex/verified-best-policy-publication`.
- Worktree before the boundary documents: clean.
- HEAD subject: `Publish cheapest verified executable strategy`.
- The completed 49-case native and release-WASM portfolios remain preserved in
  the preceding milestone. They were not rerun at this boundary.

## Focused executable controls

The existing native release binary was run against
`data/compiled/current` before source changes:

```text
poecraft_engine_tests.exe --solver-api-only data/compiled/current
solver API gate: V=5.4351 empirical=5.4229 (24 states, policy exact-router)
solver API tests: 549 checks, 0 failures

poecraft_engine_tests.exe --solver-automatic-eldritch-only
solver automatic Eldritch compiled policy:
  exact=10.473684 empirical=10.571800 runs=10000
solver automatic Eldritch delta:
  closed=1 outside=186 states=337 expanded=337
  admitted=1216 rejected=8 unresolved=0
solver automatic Eldritch tests: 85 checks, 0 failures
```

The first command exercises public C ABI session creation, public goal JSON,
registry and candidate queries, exact calculations, solve, compile, evaluation,
and Simulator execution. The second proves the unrestricted internal registry
can materialize, select, compile, exactly evaluate, and simulate automatic
Eldritch. Its registry begins with unrestricted `build_action_registry()` and
therefore is a control, not product-path coverage.

## Before matrix

The current product path is Calculator `solverGoal(..., true)` ->
`pc_solver_create` -> `registry_build_options` ->
`build_action_registry` -> `retain_goal_relevant_actions`. Calculator then
enumerates the surviving registry, removes missing-price rows, and passes every
priced surviving id back as an explicit candidate list. The current action
information contract does not expose candidate versus dependency-only roles.

| Probe | Retained/selectable before | Dependency-only before | Automatic result before | Layout / price / reason evidence |
| --- | --- | --- | --- | --- |
| Eligible Eldritch armour | Ordinary relevant candidates; no Ember, Ichor, Eldritch Chaos, or Eldritch Annul | none | no Eldritch specs can resolve | Product filter returns `false` for every Eldritch primitive; failure is `dependency_absent_before_option_construction`. |
| Ineligible Eldritch base | Ordinary relevant candidates | none | correctly none | Unrestricted registry itself omits final Eldritch primitives when `session.eldritch_eligible=false`; stable reason to add: `ineligible_equipment_class`. |
| Influenced / otherwise illegal Eldritch base | Product filter removes all Eldritch primitives before carrier legality is observable | none | none | Current public reason is indistinguishable from the eligible false negative; native builder already owns carrier legality/usefulness once dependencies exist. |
| Ordinary temporary bench blocker | All session bench crafts survive registry filtering when automatic mode is enabled; priced rows are passed back as explicit candidates | non-goal bench rows are marked internally only | existing bounded blocker synthesis can generate after filtering | The broad `automatic_candidates => all Bench` rule over-retains; `automatic_dependency_only` is not transported to Calculator. Cleanup must be priced. |
| Prefixes Cannot Be Changed | all matching and unrelated bench rows survive | lock is internally dependency-only unless it is a goal craft | bounded protected-side option may materialize on a satisfied prefix carrier | Existing protected builder checks legality, supported follow-up, goal reachability, and price, but product candidate/dependency separation is incomplete. |
| Suffixes Cannot Be Changed | same as prefix lock | same | bounded protected-side option may materialize on a satisfied suffix carrier | Same broad bench retention and missing public role. |
| Cannot Roll Attack | survives only because all automatic bench rows survive | internally dependency-only | no automatic builder | False candidate exposure plus no bounded route; audit required against follow-ups whose preservation facts respect the metamod. |
| Cannot Roll Caster | survives only because all automatic bench rows survive | internally dependency-only | no automatic builder | Same. |
| Multimod finishing | goal crafts and Multimod survive; every unrelated bench also survives | Multimod internally dependency-only | existing pairwise goal-craft option can materialize | Price, legality, conflict, crafted-slot, and goal-progress checks are native; cleanup-before-finish is not composed. |
| Crafted-modifier cleanup | `remove_crafted_modifiers` survives in automatic mode | internally dependency-only | used only by current temporary blocker option | Existing temporary option refuses cleanup that would remove a pre-existing crafted carrier. Required cleanup-before-relevant-continuation is absent. |
| Permanent goal bench | every bench survives, goal bench tagged `PermanentBench` | unrelated bench only internally dependency-only | legal state-local permanent goal craft is considered | Correct feature exists, but the registry is broader than its goal-family contract. |
| Ordinary Essence | only exact guaranteed goal-mod rows survive | none | ordinary candidate | Existing exact guaranteed-mod test passes; stable include reason to add: `essence_guarantees_goal_mod`. |
| Corruption-only Essence | treated like an ordinary Essence if its guaranteed mod matches | none | can incorrectly reach ordinary Solve | Compiled `is_corruption_only` exists but native `DataImpl` and loader do not retain it; explicit native rejection is required. |
| Harvest | only supported target tags present on a goal mod survive | none | ordinary candidate | Existing target-tag rule is already narrow; include reason to add: `harvest_target_tag_matches_goal`. |
| Fossil | bounded goal-relevant positive-score beam survives | none | ordinary candidate | Existing beam widths, scoring, and emitted limits remain authority; unrelated combinations are deferred rather than enumerated. |
| Fracture | survives automatic product filtering and is selectable | Imprint actions are separate Bestiary dependencies | existing primitive and preparation controls remain | Existing product API Fracture exact-outcome checks passed in the 549-check suite. |
| Imprint | not an ordinary registry row | Bestiary create/restore are dedicated dependencies | existing bounded automatic Imprint discovery remains | Dedicated Bestiary dependency contract is already non-primitive and must remain unchanged. |

## Frozen failure and control

The failure is deterministic and precedes pricing, layout construction, and
automatic admission:

```text
goal-relevant product request
  -> action_is_goal_relevant(Eldritch*) == false
  -> primitive absent from registry/index_by_id
  -> synthesize_automatic_options cannot resolve setup/final ids
  -> zero automatic Eldritch candidates
```

The unrestricted control retains the same primitives and passes automatic
admission, Bellman selection, compilation, exact evaluation, and 10,000
Simulator runs. This isolates the defect to product registry dependency
reachability, not crafting mechanics or the existing automatic Eldritch
kernel/compiler/evaluator implementation.
