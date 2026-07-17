# B1 Bestiary And S8 Solver Capability Plan

**Status: active execution plan.** [HANDOFF](../../HANDOFF.md) names the exact
phase boundary.

Active execution plan for the next product chunk after S7. Bestiary breadth
lands first. A focused one-item solver capability and reviewability pass follows
before recombinators. Stable solver architecture remains in
[crafting-solver-plan.md](../solver/crafting-solver-plan.md); completed S7 depth and
performance history is archived under
[archive/2026-07-solver-s7](../archive/2026-07-solver-s7/).

Status 2026-07-17: Oliver closed S7 and directed work to move forward. The
recorded endgame sample remains `0.9942` against the former `0.995` target; the
number is retained as a disclosed miss, not rewritten as a passing numeric
gate. The next implementation boundary is **B1.0 only**: pin the selected
Bestiary recipe contract with Oliver before implementing mechanic behavior.

## Outcome And Sequence

This plan has two sequential milestones:

```text
B1  owner-selected Bestiary mechanics, engine first and exact end to end
S8  minimum-expected-cost solver capability, accounting, review, and trimming
```

B1 is complete when every selected recipe has an owner-approved rule fixture,
a native engine action, exact Calculator support where stochastic outcomes
exist, ordinary strategy execution, price identity, Python/WASM exposure, and
the requested workspace surface.

S8 is complete when the one-item solver still minimizes expected cost while it:

- controls destructive rolling actions from the actual preserved carrier,
  rather than offering the same broad action families in every state;
- automatically considers relevant Fracture routes, temporary bench blockers,
  and already-supported protected metamod combinations;
- reports exact expected action and material quantities for the compiled
  strategy, including useful review groupings;
- emits a compact review projection without replacing the ordinary editable
  execution graph; and
- can derive an optional empirically trimmed strategy with a clearly reported
  effect on cost, success, and graph size.

Recombinator foundations and automatic pyramid planning begin only after Oliver
accepts the B1/S8 one-item result. Trade leaves, Hinekora's Lock, corruption,
tainted currency, and terminal finishing-cost expansion are not prerequisites
for this plan and remain independently parked.

## Recorded Owner Direction

The following decisions were recorded in the roadmap discussion on 2026-07-17:

1. Bestiary expansion is the next mechanic work.
2. A further one-item solver capability pass follows Bestiary; recombinators
   wait until that solver pass is accepted.
3. The solver continues to return the single cheapest policy by expected cost.
   Alternative-policy generation and risk-adjusted objectives are not planned.
4. Action-space work should reflect real crafting structure: raw Fossil and
   reforge loops usually belong on disposable or reset-equivalent carriers;
   later transformations should preserve acquired value or use relevant bench
   and metamod setup. This is a direction for certified action control, not a
   license for depth-based heuristic deletion.
5. Fracture should enter the ordinary candidate space automatically when it can
   improve the cheapest route.
6. Bench crafts have distinct useful roles: permanent finish, temporary group
   or slot blocker, metamod setup, Multimod finish, and cleanup/replacement.
7. Exact expected actions and materials are a primary review output.
8. The exact strategy remains available, while a user may optionally focus or
   trim it using simulator visitation and a disclosed impact report.
9. Generic top-k strategy comparison, a new general macro language, risk
   optimization, policy linting, interactive forced re-solving, and a new
   owner-maintained strategy-quality corpus are outside this chunk.

## Invariants

1. **Oliver owns mechanic rules.** Do not research or infer Bestiary behavior.
   B1 implementation starts only from the approved B1.0 contract.
2. **The native engine remains the rule authority.** TypeScript displays engine
   results and sends user choices; it does not recreate legality, blocking,
   preservation, or transition rules.
3. **Minimum expected cost remains the objective.** S8 does not add top-k,
   variance, percentile, or risk-budget objectives.
4. **Action control stays honest.** An action is pruned only by a certified
   dominance/equivalence result. An uncertain action is retained or deferred
   behind a valid bound and keeps a diagnostic reason.
5. **Review stages are not solver rules.** They are derived labels over the
   chosen policy. They may explain action control but never independently make
   an action illegal or irrelevant.
6. **Reuse the S7 option layer.** Targeted Fracture, bench-blocking, and
   protected metamod candidates expand into ordinary primitive strategy
   subgraphs. Do not introduce an opaque simulator action or a new generic
   program language.
7. **The exact graph is the source of truth.** Focus mode changes presentation
   only. Trimming creates a separate derived document and never overwrites the
   exact policy.
8. **Impact labels are explicit.** Exact evaluation, sampled visitation, and
   sampled validation are reported separately. An unseen branch is never
   called impossible merely because it received zero visits.
9. **SQLite is canonical and compiled data is derived.** Bestiary data follows
   the existing ingest/artifact contracts; neither database nor compiled output
   is hand-edited.
10. **Testing follows milestone cadence.** Use narrow tests only to diagnose
    implementation breakage during intermediate phases. Run each milestone's
    complete relevant acceptance suite once at its final phase. Compiled
    strategy verification uses exactly 10,000 simulator runs whenever it is a
    required gate. Oliver owns rendered visual review.

## Existing Substrate To Reuse

This is an extension pass, not a replacement architecture:

- S7 already provides exact Fracture calculation, carrier-specific abstract
  state, bounded Fracture preparation/retry options, lazy Fossil generation,
  exact-kernel action collapse, action inclusion diagnostics, protected-side
  options, deterministic Multimod finishes, policy compression, and bounded
  native/WASM work.
- The strategy evaluator already reports exact terminal mass, expected total
  actions, expected consumption by price key, node visits, edge traversals,
  and incoming abstract-state mixtures. S8 extends and groups this result; it
  does not create a second accounting engine.
- Strategy Builder Calculator mode already renders exact expected consumption
  and per-node/per-edge flow. The Strategy Editor design specifies an aggregate
  Simulator overlay, but the native Simulator currently aggregates operation
  counts rather than all node visits and edge traversals. S8 adds those native
  counters before simulator-guided trimming.
- The economy provider already recognizes the `Beast` stash category, but no
  Bestiary recipe/action contract or solver price identities exist yet.
- Item state reserves split and companion-state extension points. They do not
  authorize any particular Bestiary behavior before B1.0 is approved.

## B1 - Owner-Selected Bestiary Expansion

### B1.0 - Authoritative Recipe Contract

Status: immediate next task. No mechanic implementation belongs in this phase.

Oliver supplies the supported recipe list and resolves behavior through a
versioned fixture/manifest. Each selected recipe records:

```text
stable recipe/action id and display name
beast inputs and stable price keys
eligible item classes, rarities, influence and item-state restrictions
exact deterministic mutation or random selection law
crafted, fractured, influenced, split, corrupted, mirrored and implicit effects
no-op, refusal, failure and consumption behavior
number and identity of outputs
whether a saved copy, second live item, or restore operation exists
Emulator, Calculator, Strategy Builder and solver availability
```

Classify every requested recipe as one of:

- ordinary one-item deterministic;
- ordinary one-item stochastic;
- checkpoint/restore;
- multi-output or multi-item; or
- explicitly unsupported in B1.

Do not force checkpoint or multi-output recipes through the ordinary one-item
shape. The contract either selects an exact representation for them or parks
them explicitly.

Checkpoint: Oliver approves the recipe matrix and focused expected-outcome
fixtures. Every selected behavior is answerable from the matrix without online
research or agent inference. Stop at this boundary and rewrite `HANDOFF.md`.

### B1.1 - Data, Price, And Registry Substrate

- Add versioned manifest-backed recipe identities and validation.
- Map each consumed beast or recipe bundle to stable economy price keys; reuse
  the existing Beast provider surface rather than deriving fake recipes from
  tags or display text.
- Add native action descriptors with legality, cost vectors, transition kind,
  and state effects from B1.0.
- Refuse unsupported recipe classes explicitly and keep them out of product
  action envelopes.

Checkpoint: canonical source data produces deterministic artifact and registry
identities, selected prices resolve through the ordinary economy path, and
unsupported rows are named rather than silently approximated.

### B1.2 - Native Actions And Exact Calculation

- Implement selected item mutations in the native action engine.
- Implement exact calculation evaluators for stochastic selected recipes.
- Add only the item-state or companion-state distinctions required by the
  approved contract.
- Cross-check stochastic evaluators against engine Monte Carlo on the approved
  fixtures; deterministic recipes use direct before/after fixtures.
- Keep simulator execution primitive. Any recipe-specific solver option must
  compile to the same native operations visible in a strategy document.

Checkpoint: action application and calculation agree on legality, state
effects, outcome support, and probabilities for every selected B1 recipe.

### B1.3 - Solver And Strategy Integration

- Register solver descriptors only for recipes with complete engine,
  calculation, legality, and pricing support.
- Add goal-relevance and preservation metadata needed by the S8 action-space
  pass.
- If an approved checkpoint/restore technique needs a fixed solver option,
  implement that specific exact option through the existing S7 operator
  contract; do not add a generic macro language.
- Extend strategy operation vocabulary and compiler conditions only where an
  approved selected recipe requires them.

Checkpoint: selected solver-visible recipes participate in a small exact solve,
compile into an ordinary strategy, and have no unsupported or unmatched route.

### B1.4 - Bindings And Workspace Surfaces

- Carry the registry/action/calculation additions through Python, C ABI, WASM,
  and the worker protocol.
- Add the selected Bestiary family to the shared action presentation used by
  Emulator and Calculator; Strategy Builder uses the ordinary operation model.
- Display engine-provided legality, odds, price identities, and refusal reasons.
- Rebuild release WASM after action, C ABI, or strategy-vocabulary changes.

Checkpoint: native and WASM expose the same selected recipe ids, outcomes, and
strategy behavior. No rendered browser review is performed by an agent.

### B1.5 - Bestiary Acceptance

- Run the complete relevant ingest, artifact, native, binding, WASM, and web
  acceptance once.
- Verify each selected recipe's engine/calculation parity on its approved
  fixture.
- For any selected solver-visible recipe, solve, compile, and run the required
  strategy verification exactly 10,000 times.
- Deliver the resulting strategies and Calculator outputs for Oliver's visual
  and mechanic review.

Stop after B1 acceptance. Record which recipes shipped, which were explicitly
parked, and any state-model constraints before starting S8.

## S8 - Practical One-Item Solver Capability And Reviewability

### S8.0 - Baseline And Review Contract

- Capture the current exact solver envelope, compiled graph, evaluator result,
  and Simulator aggregate result for the applicable existing S7 cases plus
  narrow fixtures for Fracture, bench blocking, and protected reforging.
- Version a review-projection schema that references raw strategy node/edge ids
  without changing execution semantics.
- Version action-accounting additions by action descriptor and review section.
- Version trimming provenance: parent strategy hash, discovery parameters,
  selection threshold, explicit fallback choice, exact impact result, and
  independent validation parameters.
- Pin whether a derived trim routes removed entries to Restart or an explicit
  `trimmed` stop/failure terminal. Both are executable and measurable; neither
  may be selected silently.

This is implementation evidence, not a new owner-maintained quality corpus.

Checkpoint: the before-state is reproducible and every later readability or
action-space claim can be compared with the same exact strategy/evaluator
contracts.

### S8.1 - Derived Review Sections

Build a display-only projection over the chosen policy. Group policy regions
using exact facts already present in the graph/evaluator, including:

- restart-equivalent or otherwise disposable carriers;
- satisfied goal carriers and their crafted/fractured status;
- the affix side or goal subset being preserved;
- active protection/setup intent;
- deterministic finishing readiness; and
- recovery paths back to an earlier carrier.

Retry SCCs remain inside one section. Restart and genuine carrier loss can move
back to an earlier section. Every section retains links to its raw nodes and
edges. Suggested role labels are rolling, temporary blocking, protection,
Fracture, finishing, and recovery; labels are descriptive metadata only.

Checkpoint: projected and unprojected documents compile and evaluate
identically because they are the same strategy. Deliver representative
projections to Oliver before investing in further presentation polish. If the
grouping is not useful, retain action accounting and trimming without making
sections solver authority.

### S8.2 - Preservation-Aware Action Control

- Add symbolic metadata for which exact goal/junk carriers an action can
  preserve, destroy, create, or make unreachable.
- Define restart-equivalent/disposable carrier certification from exact state
  and transition facts, not solve depth or a display-stage label.
- Keep raw Fossil/reforge renewal candidates on disposable carriers and after
  genuine resets. On progressed carriers, prune a destructive candidate only
  when its continuation is certified dominated by a reset/recovery route.
- Use admissible action bounds for expensive uncertain candidates. Retain them
  when no proof or valid bound exists.
- Preserve the existing inclusion/deferred/pruned/unpriced/unsupported reason
  for every action and add the preservation/dominance witness to diagnostics.
- Continue exact-kernel collapsing after context-sensitive generation; retain
  price ties and deterministic ordering.

Checkpoint: small exhaustive-oracle fixtures have the same cheapest value and
policy cost as the controlled envelope. The real cases show the intended action
reduction without being relabelled heuristic.

### S8.3 - Automatic Fracture, Bench, And Metamod Candidates

Promote existing targeted option machinery into ordinary candidate generation:

- synthesize the current carrier-exact Fracture preparation/retry option when a
  satisfying carrier, legal Fracture path, and complete recovery exits exist;
- consider permanent goal bench crafts directly;
- synthesize a temporary bench blocker only when applying it changes the exact
  kernel of a relevant following action through group conflict or slot use;
- include the blocker application, repeated use, cleanup/replacement, occupied
  slot, and every outer exit in its price-independent option kernel; and
- synthesize already-supported protected-side/metamod combinations only for
  legal goal-relevant follow-up actions, including reapplication and recovery
  costs already required by the S7 option contract.

Do not select candidates by raw hit-rate improvement alone. Each candidate
competes by complete expected downstream cost in the ordinary Bellman step.
Do not create a general action-program authoring system.

Checkpoint: price-flip fixtures select and reject automatic Fracture,
bench-blocking, and protected candidates at analytically checked boundaries;
every selected option compiles to primitive operations and simulator routes.

### S8.4 - Exact Action And Material Accounting

Extend the existing exact strategy evaluator result rather than duplicating its
occupancy solve:

- aggregate expected visits by action descriptor as well as raw node;
- retain price-key consumption and add cost contribution after applying the
  active price table;
- aggregate expected actions, materials, retries, restarts, temporary blocker
  applications, protection setup, cleanup, and finishing work by review
  section when sections are enabled;
- report totals per strategy invocation; when success probability is below one,
  optionally report `total / P(success)` as explicitly labelled
  success-normalized work for independent whole-strategy retries, not as a
  conditional-path expectation; and
- compare exact forecasts with Simulator run averages without merging the two
  evidence sources.

The review output must make temporary versus permanent bench crafts and setup
versus finishing costs explicit.

Checkpoint: analytic loops match hand-computed action/resource counts, exact
section totals sum to whole-graph totals, and simulator aggregates converge to
the exact forecast within the existing fixture tolerances.

### S8.5 - Compact Review And Optional Empirical Trim

Provide two separate controls:

1. **Focus view** hides or collapses low-visit regions using exact evaluator or
   Simulator visitation without mutating the strategy.
2. **Trim strategy** creates a new derived document. Candidate regions come
   from a user-selected discovery-run count and visit threshold. Removed entry
   routes use the explicit fallback pinned at S8.0—Restart or a `trimmed`
   stop/failure terminal—and never become unmatched.

For a derived trim:

- preserve the original strategy and record its hash;
- add native/C ABI/binding/WASM/TypeScript aggregate counters for every visited
  node and traversed edge, while retaining the existing action totals;
- record discovery run count, seed policy, threshold, removed nodes/edges, and
  the observed visitation mass;
- run the exact strategy evaluator on original and derived documents and report
  success, expected action/material, expected cost, and graph-size deltas;
- validate the derived document with a separate Simulator sample so the runs
  used to select rare regions are not reused as impact evidence;
- report confidence intervals for sampled deltas and the nonzero upper bound
  implied by an unvisited branch; and
- label the derived strategy `empirically_trimmed` and heuristic even when its
  measured impact is zero.

Checkpoint: a pinned rare-branch fixture can focus and trim without changing
the original, without producing an unmatched route, and with an impact report
that distinguishes exact evaluation from discovery and validation samples.

### S8.6 - Solver Acceptance

- Run the complete relevant native, binding, WASM, web, and repository
  acceptance once.
- Re-run the applicable existing real one-item cases and the narrow new feature
  fixtures; this is not a new subjective quality corpus.
- Compile every required exact policy and run required simulator verification
  exactly 10,000 times.
- Confirm native/WASM parity, worker responsiveness, resource caps, and no
  unmatched/off-policy routes for exact policies.
- Present action inclusion/defer/prune diagnostics, automatic Fracture/bench/
  metamod selections, exact action/material accounting, review projections,
  and optional trim impact reports for Oliver's evaluation.

## Testing And Commit Cadence

Each numbered phase is one implementation checkpoint, one local commit, and a
rewritten `HANDOFF.md`. Intermediate phases do not carry routine full native,
binding, WASM, web, simulator, or repository test runs. Use a narrow test only
when needed to diagnose or fix current work. Run the appropriate complete suite
once at B1.5 and once at S8.6. Do not perform screenshots, rendered browser
smoke, or visual acceptance unless Oliver explicitly asks.

## Immediate Owner Input

Before B1.1 can start, Oliver must provide the B1.0 Bestiary recipe list and
mechanic decisions. The repository currently contains no authoritative
Bestiary rule source. Agents must not fill this gap from web research or memory.

## Stop Boundary

Stop after S8 acceptance and Oliver's review. Do not begin recombinator outcome
rules, two-item Calculator work, feeder/recombination blocks, pyramid planning,
trade-leaf product expansion, Hinekora's Lock, corruption, publishing/accounts,
or ML as part of this plan.
