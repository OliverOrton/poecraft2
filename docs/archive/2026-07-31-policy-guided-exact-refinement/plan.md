# Policy-Guided Exact State Refinement

**Status: archived implementation plan; qualification stopped at the 1 GiB
decision gate.**

Owner: Oliver

Branch: `codex/policy-guided-exact-refinement`

Starting source:
`71e1ad05e07949aafe6312fe0e50f49fb685dba3`
(`Complete cross-base strategy reliability`)

Parent: [Policy-Guided Exact Refinement qualification report](README.md)

## Objective

Convert policy-reachable states from the product solver's deliberately coarse
ordinary/reforge parent into exact executable strategy regions. Broad solver
discovery remains coarse. Exact modifier and exclusion identity is restored
only when an admitted action, a selected continuation, or a compiled router
can observe it.

The finished path is:

```text
coarse discovery and policy
  -> policy-reachable exact refinements
  -> contract-derived observation classes
  -> fixed-point lumpability proof
  -> witness-local Bellman re-optimization when needed
  -> ordinary strategy graph
  -> exact absorbing-policy evaluation and cost reconciliation
```

Every returned policy must compile, exact-evaluate, and reconcile with the
solver-owned value. Compatibility refusal remains a final assertion and
diagnostic, not the ordinary outcome for feasible one-goal crafts.

This milestone does not manufacture a Chaos or Restart fallback, prescribe a
crafting recipe, restore global strict identity, change mechanics or prices,
change the objective or action filter, add frontend crafting logic, add a new
strategy execution format, or begin the executable-anchor library.

## Starting Evidence

The qualified 49-case reliability portfolio currently has:

| Outcome | Count |
| --- | ---: |
| Cases | `49` |
| Published policies | `4` |
| No policy | `45` |
| `refused_unsupported_action` | `36` |
| Exact-exclusion refusals selecting Regal | `28` |
| Exact-exclusion refusals selecting Exalt | `7` |
| Exact-exclusion refusals selecting Restart | `1` |

Gate 0 will reproduce and preserve these two witnesses before behavior
changes:

- Regal: `reliability-class-belt`, state `8`, action `regal`,
  `coarse_parent_requires_exact_exclusion_identity`;
- Exalt: `natural-t1-breadth-two-4e65dda9c53b`, state `18`, action `exalt`,
  `coarse_parent_requires_exact_exclusion_identity`.

The durable corpus is
[`fixtures/solver-reliability/v1`](../../../fixtures/solver-reliability/v1/README.md).
Run-local evidence belongs under
`build/acceptance/policy-guided-exact-refinement/`; the final report will
record exact artifact paths and hashes.

## Shared Refinement Contract

One engine-owned, parameter-aware semantic contract is added to admitted
action descriptors. It composes:

- source features the action observes;
- features and sides the action preserves;
- features and sides it destroys;
- features it creates or changes; and
- whether a transition returns to a coarse parent because no observed
  identity survives.

The observation vocabulary covers only features actually used by admitted
mechanics or compiled routing: side; exact group/exclusion signature; goal
membership and tier-status class; crafted, fractured, Veiled and metamod
identity; prefix/suffix locks; influence; Eldritch tiers and dominance;
rarity and side occupancy; and any other engine-owned feature found during
the Gate 1 audit.

Equivalent exclusion signatures remain one class even when modifier IDs
differ. Features an action cannot observe are absent from its refinement key.
The refinement algorithm consumes this contract and contains no action-name
switch. Adding an ordinary action requires its semantic contract and tests,
not edits to the refinement algorithm. An admitted action with an incomplete
contract is rejected before solving.

Compiler routing, solver refinement, transition construction, compatibility
assertion, and exact evaluation consume the same contract and signature
authority. Existing strategy conditions are reused where they can spell the
result; no Regal- or Exalt-specific compiler path is allowed.

## Shared Refinement Engine

The existing collision-checked exact quotient machinery is generalized into
one deterministic refinement engine rather than duplicated compiler, solver,
or per-action partition systems.

For a policy-reachable coarse state the engine:

1. starts from the concrete solve input and lazily discovers only exact
   refinements reached by the selected policy;
2. resolves operators by semantic identity, never by cross-context numeric
   index or representative modifier;
3. builds transitions through the strict `CalcContext` and primitive engine
   authority;
4. partitions policy/state pairs by the minimal contract-derived observation
   signature;
5. repeatedly refines until selected cost, choice vocabulary, and probability
   mass into successor refinement classes are equal for every merged member;
6. collapses identity after a destructive transition when the contract proves
   it unobservable, while retaining only the preserved side for side-preserving
   actions; and
7. produces deterministic exact policy regions for the existing compiler.

Cycles use monotone partition refinement to a deterministic fixed point.
Lumpability requires equal immediate resource cost and equal probability into
every successor refinement class. Values and witnesses from incompatible
refinements are never combined.

If a selected coarse row is non-lumpable or exact subclasses have different
action values, its feature-level witness schedules only that affected
state/action region for exact rows and existing policy-improvement/SCC
machinery. Every admitted alternative remains the existing filtered action
vocabulary; refinement does not prescribe the action. Value changes propagate
through existing predecessors until the refined policy is stable and proper.

The coarse `CalcContext`, hashes, and discovery graph remain authoritative for
broad-search evidence. Exact policy state IDs live in their own compatible
context and are never mixed with coarse IDs.

### Qualification checkpoint: exact publication phase ordering

The current production adapter reaches the shared contract-derived partition
in this exact order:

1. `ProductionPolicyOracle` derives the strict carrier layout and backward
   coarse-policy requirements from the admitted action contracts, imports the
   existing planner operators by semantic identity, and interns the concrete
   start item in the strict `CalcContext`.
2. `discover_policy_graph` enumerates only the requested exact roots, asks the
   oracle for each selected semantic action, and materializes that action's
   strict kernel.
3. Successor generation interns every strict successor in the adapter's
   collision-free carrier table and then in the refinement graph before graph
   discovery continues. The existing `policy_collapse_key` fast path may reuse
   the shared canonical operation/state signature for a fixed policy, but it
   is disabled for witness-local re-optimization; that path therefore retains
   every strict successor carrier.
4. Only after exact successor closure does `canonicalize_graph` order carrier
   identities and remap every absolute edge.
5. `refine_selected_action_routing` derives class-local routing predicates on
   the completed graph, and `propagate_policy_observations` carries the same
   action-contract requirements backward to a fixed point.
6. `initial_behavior_key` forms the contract-derived observation classes, then
   `refine_closed_probabilistic_partition` repeatedly splits them by immediate
   cost and probability mass into successor classes until the existing
   lumpability proof closes.
7. The engine retains counterexamples and final classes, runs the shared exact
   SCC evaluator, and only then constructs and independently verifies the
   ordinary compiled strategy.

Consequently, witness-local publication currently materializes strict
carriers before the canonical contract-derived partition can operate. Any
early aggregation considered during qualification must call the same shared
observation and partition authorities; it must not introduce a second
signature, contract, or refinement system.

The audit rejects lossless early aggregation on an open successor graph. The
shared `refine_closed_probabilistic_partition` contract requires complete
stochastic rows and is split-only: a class may separate after any successor
class separates. The minimal live-carrier witness is:

```text
x and y: same coarse parent, observation key, selected semantic action,
         and immediate cost
x -> 0.5 y + 0.5 success
y -> 0.5 x + 0.5 failure
success and failure: distinct terminal observation classes
```

Before both exits and the live cycle are closed, `x` and `y` have the same
available local key. The completed shared partition must split them because
their probability mass into the terminal classes differs. Discarding either
strict carrier during successor generation would make that later split
impossible. Using unresolved collision-free successor keys as absolute arc
labels is conservative but also prevents valid quotient-cycle merges and
therefore does not provide the required aggregation.

The existing fixed-policy `policy_collapse_key` remains the only approved
early reuse path: it uses the shared canonical operation/state authority after
the selected action and its backward router requirement are known, plus the
collision-checked reforge-kernel signature where required. Witness-local
re-optimization cannot extend that collapse to a class merely because its
current action matches: two carriers may have the same incumbent row while an
admitted alternative's contract or value separates them. The shared candidate
partition can prove that separation only after retaining the candidate
carriers and their complete rows. The closed partition may be scheduled for a
closed component while member identities remain retained, but that does not
remove the strict-carrier materialization boundary. No new early partition or
signature is retained by this milestone.

## Named Product Limits And Telemetry

Refinement is bounded by the existing solve-owned state, state/action-row,
transition, reforge-work, memory, compiled-node, compiled-edge, and output
limits unless implementation proves that a separately named limit is needed.
Any new limit must be public in diagnostics, deterministic, and distinguish a
bounded executable result from a no-policy stop.

Telemetry records, with bounded samples:

- refinement triggers and feature-level counterexamples;
- exact states, coarse parents, observation classes, and fixed-point rounds;
- exact transitions and strict-kernel work/reuse;
- locally scheduled and evaluated state/action rows;
- local policy changes and propagated value changes;
- collapse events by destroyed/preserved feature;
- live/peak owned bytes and cap proximity; and
- remaining final-assertion refusal causes.

## Implementation Gates

### Gate 0 — Freeze evidence

1. Reproduce the named Regal and Exalt cases from clean equivalent inputs.
2. Preserve state, action, reason, values, graph counts, and hashes.
3. Record the 49-case outcome distribution.
4. Freeze the qualified Fracture/coarse-parent invariants and their tracked
   evidence.

### Gate 1 — Observation and refinement contract

1. Audit every admitted primitive and automatic/fixed operator dependency.
2. Add the composable contract, deterministic signature, and admission check.
3. Replace compatibility and exact-evaluator action switches with the shared
   authority.
4. Add focused tests for minimal observation, equivalent-signature merging,
   goal-status/tier identity, flags/locks/influence/Eldritch state,
   preserved-side retention, destructive collapse, and incomplete admission.

### Gate 2 — Exact policy lifting

1. Lazily refine only policy-reachable coarse states from the exact start.
2. Build strict transitions and deterministic exact routers without choosing a
   representative identity.
3. Reuse completed coarse rows/kernels only after a lumpability proof.
4. Evaluate cycles as proper absorbing policies and reconcile exact cost.
5. Make the frozen Regal and Exalt repros compile and exact-reconcile.

### Gate 3 — Local solver re-optimization

1. Feed non-lumpability/value witnesses into the same refinement engine.
2. Schedule only affected exact state/action work.
3. Reuse existing Bellman, policy-improvement, SCC and reverse-propagation
   machinery.
4. Prove that distinct exact subclasses may select distinct existing actions.
5. Leave compatibility checking as a final assertion.

### Gate 4 — Focused product qualification

Run, before either portfolio:

- a formerly rejected one-goal prefix Regal case;
- a formerly rejected suffix-oriented Exalt case;
- the mixed-side rarity regression;
- Ring and Gloves reliability policies;
- the qualified Fracture-local full-four case; and
- one preserving-side and one destructive-reroll cycle.

Every feasible one-goal case with legal priced actions must publish an exact
or bounded executable policy. Exclusion-identity no-policy is a hard failure.

### Gate 5 — Portfolio and final acceptance

1. Run the 27-class one-goal smoke portfolio.
2. Require zero
   `coarse_parent_requires_exact_exclusion_identity` refusals and an executable
   policy for every structurally feasible case under the declared limits.
3. Run the remaining 49-case native portfolio.
4. Exact-compile/evaluate every published policy and reconcile its cost.
5. Run the selected 10,000-simulation verification cases.
6. Rebuild release WASM.
7. Run `powershell -File scripts/test.ps1` once after the complete
   implementation.

Codex performs no rendered visual review.

## Frozen Non-Regression Gates

The implementation must preserve:

| Invariant | Required value |
| --- | ---: |
| Coarse parent junk classes | `6` |
| Qualified root Chaos successors | `217` |
| Qualified carrier graph | `927` states |
| Fracture transition hash | `04a66ba6c6dfcabf` |
| Fracture policy hash | `3e5d7530e7aed5fb` |
| Compiled Fracture strategy SHA-256 | `e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e` |
| Completed-row recomputation | `0` |

Exact primitive, Calculator, Simulator mechanics, deterministic identities,
and completed-row reuse also remain unchanged.

Global strict identity, broad root-support growth, a changed qualified
transition hash, an action-specific hard-coded policy, weakened exact-cost
reconciliation, or a second independent refinement system is a hard stop.
Preserve evidence and report instead of forcing through.

## Completion

After qualification:

1. record before/after refusal and policy-availability counts;
2. explain where lifting alone sufficed and where local solver refinement was
   required;
3. document the future-action observation/refinement contract;
4. update stable solver/compiler/product documentation and evidence;
5. archive this plan with a full dated report and restore
   [`docs/active`](README.md) plus `HANDOFF.md` to a clean no-active boundary;
6. create one final local implementation/evidence/documentation commit ending
   with `Co-authored-by: Codex <codex@openai.com>`; and
7. leave the worktree clean without pushing.
