# Verified subgoal/option system for executable upper bounds

**Repository:** `OliverOrton/poecraft2`
**Exact ref:** `769c3deb1a2a2913c228c4135c764271f662bef9`
**Review posture:** read-only; no files, commits, issues, or other public state were changed.

## Decision

The replacement should be a **verified executable graph-fragment system**, not a coarse option/semi-MDP planner.

An option generator may propose control flow and use estimated exit summaries to rank candidates. It must not author an authoritative stochastic transition row. The verifier must instead:

1. interpret the proposed fragment over exact executable state;
2. rebuild every primitive transition from engine mechanics;
3. prove complete probability mass and option absorption;
4. flatten all selected fragments into one ordinary strategy graph;
5. independently parse and exact-evaluate that flattened strategy; and
6. submit it through the existing `IncumbentPortfolio` promotion gate.

This directly preserves the current upper-bound contract: an upper is the exact expected cost of one proper executable strategy, exact success excludes junk, and compilation, route coverage, properness, complete pricing, and cost reconciliation are all publication requirements.

It also avoids the previous architecture’s defining failure. The removed carrier planner produced attractive estimates around 12.2–12.4k for two four-goal cases, but compiled evaluation rejected them because the coarse composition did not conserve probability mass. Its later clean-case policy was a side effect of growing the core graph, not a certified planner result.

The surviving `solver_executable_carrier_planner.hpp` should remain what it is now: diagnostic/search projection vocabulary with no conversion to proof or numeric authority. A carrier projection may index or rank option proposals, but it must never identify an option exit, a recurrent class, or an executable policy state.

---

## 1. Authority ladder

The option subsystem needs an explicit type ladder that makes accidental promotion difficult.

| Stage                       | Object                                     |                      May contain a scalar estimate? | Executable-upper authority |
| --------------------------- | ------------------------------------------ | --------------------------------------------------: | -------------------------: |
| Search                      | `OptionProposal`                           |                              Yes: `estimated_value` |                         No |
| Structural compilation      | `OptionFragmentIR`                         |                            Optional annotation only |                         No |
| Exact fragment verification | `CertifiedOptionFragment`                  | Exact resource/exit diagnostics, but no upper field |                         No |
| Meta search                 | `EstimatedMetaPolicyCandidate`             |                              Yes: `estimated_value` |                         No |
| Flattening                  | `FlattenedPolicyCandidate`                 |                           Candidate solver estimate |                         No |
| Final assertion             | existing `CompiledPolicyAssertion`         |                                Exact evaluated cost |         Not until complete |
| Portfolio                   | existing verified `BoundedPolicyIncumbent` |          `certified_upper_bound` and evaluated cost |                    **Yes** |

The following should be structural invariants:

```cpp
static_assert(!std::is_convertible_v<OptionProposal, double>);
static_assert(!std::is_convertible_v<CertifiedOptionFragment,
                                     ProofLowerValue>);
static_assert(!std::is_convertible_v<CertifiedOptionFragment,
                                     BoundedPolicyIncumbent>);
static_assert(!std::is_convertible_v<EstimatedMetaPolicyCandidate,
                                     BoundedPolicyIncumbent>);
```

Only the existing final policy assertion may set the incumbent fields corresponding to:

* independently certified;
* independently evaluated;
* proper;
* executable;
* cost complete;
* zero off-policy;
* cost reconciled.

The current portfolio already refuses candidates missing those properties and preserves the cheapest compatible verified upper monotonically.

### Required public telemetry separation

These must remain distinct fields throughout:

```text
option_candidate_estimate
option_structural_verification_status
option_internal_expected_resources
option_internal_priced_cost_diagnostic
meta_policy_candidate_estimate
flattened_policy_solver_estimate
flattened_policy_exact_evaluated_cost
verified_executable_upper
independent_global_lower
```

Only `verified_executable_upper` is an upper. None of the option fields may be displayed as a bound, used for a gap, or consumed by proof code.

---

## 2. Exact option identity and version contract

Identity should be split into three levels.

### 2.1 Semantic definition identity

```cpp
struct OptionDefinitionIdentityV1 {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::uint32_t schema_version = kSchemaVersion;
    std::string family_id;              // e.g. same_side_from_clean
    CanonicalBytes parameters;          // side, goal masks, action ids, etc.
    CanonicalBytes initiation_predicate;
    CanonicalBytes exit_partition;
    CanonicalBytes controller_graph;
    CanonicalBytes observed_choice_policy;
    CanonicalBytes recovery_contract;
};
```

Rules:

* Stable action IDs, goal-family/group identities, and semantic parameters are used—not context-local numeric indices.
* Display names and telemetry labels are excluded.
* Observed-choice ordering is part of the definition. Changing unveil/offer preferences creates a new option definition.
* A definition such as “choose the cheapest available action” is not stable enough. Synthesis may use prices to select a controller, but the resulting controller must capture the exact chosen actions and routes.

### 2.2 Structural certificate identity

```cpp
struct OptionStructuralIdentityV1 {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::uint32_t schema_version = kSchemaVersion;
    OptionDefinitionIdentityV1 definition;

    MechanicsArtifactIdentity mechanics;
    std::uint64_t state_key_schema_version;
    std::uint64_t refinement_contract_version;
    std::uint64_t predicate_semantics_version;
    std::uint64_t condition_compiler_version;
    std::uint64_t option_verifier_version;
    std::uint64_t properness_proof_version;

    GoalIdentity goal_context;
    ActionVocabularyIdentity action_vocabulary;
    CallerScopeIdentity caller_scope;

    EntryDomainIdentity exact_entry_domain;
    ExactKernelVocabularyIdentity kernel_vocabulary;
    ControllerMemorySchemaIdentity memory_schema;

    ProbabilityToleranceBits probability_tolerance;
    ResidualToleranceBits residual_tolerance;
};
```

`caller_scope` must include all behavior-bearing controls already covered by the incumbent contract, including:

* explicit action envelope;
* disabled action families;
* economic Restart permission;
* automatic Imprint permission;
* goal-progress reforge rules;
* exact candidate action vocabulary.

The current retained-incumbent contract already binds goal, economy, action vocabulary, caller scope, artifact generation, graph prefix, source/target generation, and arena counts. Option reuse must be at least this strict.

### 2.3 Price-scoped cost identity

```cpp
struct OptionCostCertificateIdentityV1 {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::uint32_t schema_version = kSchemaVersion;
    OptionStructuralIdentityV1 structural;
    EconomyIdentity economy;
    PriceTableBitsIdentity price_table;
    std::uint64_t resource_reward_schema_version;
    std::uint64_t reward_solver_version;
};
```

Structural properness and exit probability may be reusable after prices change. The prior expected cost is not.

### 2.4 Identity equality rules

A digest is only a lookup accelerator. The cache must retain canonical identity bytes and perform full equality after a hash match.

Any change to one of the following requires re-verification:

* option or predicate schema;
* action or mechanics semantics;
* exact state/refinement vocabulary;
* observed-choice policy;
* initiation or termination semantics;
* action scope;
* recovery behavior;
* controller memory;
* probability or residual tolerance;
* properness algorithm;
* exact entry domain.

No “compatible minor version” rule should exist in v1. Compatibility requires an explicit migration followed by fresh verification.

---

## 3. Initiation and termination predicates

An option predicate needs two representations.

```cpp
struct VerifiedOptionPredicateV1 {
    SemanticStatePredicate exact_predicate;
    ConditionExpr executable_predicate;
    refinement::ObservationRequirement required_observations;
    PredicateEquivalenceCertificate equivalence;
};
```

### Exact semantic representation

The semantic predicate evaluates against exact verifier state:

```text
exact item/strict stable key
goal-slot status and exact member class
prefix/suffix occupancy
junk and blocker identities
crafted/fractured identity
protection flags
influence and Eldritch state
Veiled/offer observations
checkpoint and live-item identity
finite controller/resource memory
```

It must not evaluate from `ExecutableCarrierProjection`.

### Executable representation

The executable predicate uses the existing typed condition composition and strategy condition vocabulary. Current compiler-side conditions already support canonical `all`, `any`, `not`, and `at_least` composition, while exact policy compilation fails closed when the necessary state distinction cannot be represented.

For every exact reachable state in the certificate domain, verification must prove:

```text
semantic_predicate(state)
    == evaluation_of_compiled_condition(state)
```

A semantic predicate that cannot be represented by the ordinary strategy condition vocabulary is a compilation refusal, not permission to route on a coarser carrier.

### Initiation predicate

The initiation predicate is a deterministic guard over the complete option entry state.

Calling an option outside its initiation set is not a stochastic option outcome. It is a malformed meta-policy and must route to certification failure.

### Termination as an exit partition

Termination is not one Boolean. It is a mutually exclusive labeled partition:

```cpp
enum class OptionExitKind : std::uint8_t {
    FinalSuccess,
    SubgoalReached,
    RecoverableFailure,
    RecoveryCompleted,
    HardFailure
};
```

Each label includes typed parameters such as a subgoal ID or failure reason.

At every reachable option boundary, exactly one of these must hold:

1. continue inside the option;
2. take exactly one declared exit.

Overlapping exits are rejected unless priority is explicitly represented in the option definition and executable graph. V1 should require disjoint exits.

`FinalSuccess` must call the exact goal predicate. It cannot use “enough goal masks” while ignoring unrelated affixes. The current terminal contract requires every occupied explicit affix to be a satisfied requested slot.

A `SubgoalReached` exit may legitimately retain junk or blockers. Its exact exit state records them; it is not final success and it is not itself an upper bound.

---

## 4. Internal compiled-policy representation

The key design choice is that the authored option fragment contains **control flow, not transition probabilities**.

### 4.1 Executable fragment IR

```cpp
struct OptionFragmentIRV1 {
    OptionDefinitionIdentityV1 identity;
    OptionEntryPort entry;
    std::vector<OptionNode> nodes;
    std::vector<OptionControlEdge> edges;
    std::vector<OptionExitPort> exits;
};

enum class OptionNodeKind : std::uint8_t {
    Route,
    PrimitiveOperation,
    ObservedChoice,
    Exit
};
```

A primitive node contains:

* stable primitive or fixed-program action identity;
* exact legality contract;
* required observations;
* deterministic next controller node after application;
* any choice policy for observed outcomes.

A route node contains typed executable conditions and a mandatory fail-closed default.

A leaf option contains no nested option calls in v1. Composition belongs to the finite meta-controller. This keeps individual option verification and cache identity local.

Existing `FixedOptionSpec` and `PlannerOperator` programs can be lowered into this IR, but they are not considered certified merely because they are finite programs. The current fixed-option structure does not contain a complete exit kernel, controller-memory model, or standalone properness certificate.

### 4.2 Exact verifier product graph

The verifier interprets the fragment and constructs:

```cpp
struct OptionProductStateV1 {
    refinement::StableKey exact_item;
    std::uint32_t controller_node;
    ExactCheckpointState checkpoint;
    FiniteResourceState hard_resources;
    FiniteControllerMemory local_memory;
};

struct OptionVerifiedRowV1 {
    OptionProductStateV1 source;
    ExactActionIdentity selected_action;
    std::vector<OptionVerifiedTransitionV1> transitions;
};

struct OptionVerifiedTransitionV1 {
    OptionProductStateV1 successor;
    double probability;
    ResourceQuantityVector resource_delta;
};
```

Important rules:

* Probabilities are rebuilt from the authoritative action evaluator.
* The option generator cannot supply or modify them.
* The full stored-double probability bits are retained for identity and regression checks.
* States may be quotient-merged only after an exact replay-backed lumpability proof.
* Two states having the same goal mask or carrier projection is not a merge proof.
* Choice outcomes and early exits of fixed programs are explicit transitions.
* Checkpoint state and item identity are part of the product state where relevant.

The verifier may use the existing refinement machinery to derive exact behavioral classes. It must not start from a carrier summary and assume that the summary is Markov.

---

## 5. Exact cumulative cost and resource model

The exact model should be transition-reward based.

For resource key \(k\), let

$$
q_k(x,a,y)
$$

be the quantity consumed when action \(a\) moves executable product state \(x\) to \(y\).

This supports:

* deterministic per-action currency quantities;
* outcome-dependent consumption;
* base replacement on explicit recovery;
* fixed programs that consume several resources;
* actions that do not consume resources when not applied;
* checkpoint create/restore operations.

Non-consumable finite execution state—checkpoint presence, bound item identity, or a bounded attempt counter—is part of `OptionProductStateV1`, not an averaged resource quantity.

### Expected resources

For an option policy:

$$
R_k(x)
=
\sum_y P(y\mid x)
\left(q_k(x,a,y)+R_k(y)\right).
$$

These equations are solved over the same verified transient SCCs as expected cost.

### Price-scoped cost

For economy prices \(c_k\),

$$
J(x)=\sum_k c_k R_k(x).
$$

The structural certificate should store expected resource quantities, not bake in economy prices. The price-scoped cost certificate computes the dot product afterward.

The current solver prices operators from exact `resource_quantities`, and the exact strategy evaluator separately reports expected material consumption, material prices, cost contributions, cost-dot-product difference, and occupancy-reward difference. Those are the appropriate reconciliation surfaces for option-derived policies.

### Three-way cost reconciliation

Before publication, require agreement among:

1. the option/meta reward equations;
2. exact state-operation occupancy multiplied by immediate rewards;
3. the independently rebuilt flattened-strategy material totals.

The option/meta result is diagnostic. The flattened evaluator remains authority.

Missing or nonfinite prices mean:

* structural option verification may still succeed;
* no price certificate is produced;
* no executable upper can be promoted.

### Computation limits are not stochastic exits

A state cap, byte cap, cancellation, or verifier timeout is not `RecoverableFailure`. It means no certificate was completed. Treating a computation refusal as option probability mass would be another form of silent mass loss.

---

## 6. Complete exit distribution

The exit kernel is derived only after the complete option product graph is built and proved proper.

```cpp
struct OptionExitKeyV1 {
    OptionExitKind kind;
    CanonicalExitParameters parameters;
    refinement::StableKey exact_exit_item;
    FiniteResourceState hard_resources;
    FiniteControllerMemory outcome_memory;
};

struct OptionExitAtomV1 {
    OptionExitKeyV1 key;
    double probability;

    // Joint reward mass, not conditional mean.
    ResourceQuantityVector joint_resource_mass;
    double joint_action_count_mass;
};

struct OptionExitKernelV1 {
    OptionStructuralIdentityV1 identity;
    ExactEntryState entry;
    std::vector<OptionExitAtomV1> exits;
};
```

For exit \(e\) and resource \(k\), store

$$
M_{e,k}
=
\mathbb E\left[
  \left(\sum_t q_{k,t}\right)\mathbf 1\{E=e\}
\right].
$$

Do not store only a conditional expected cost. The authoritative totals are:

$$
\sum_e P(E=e)=1
$$

and

$$
R_k=\sum_e M_{e,k}.
$$

A candidate-search continuation estimate can then be calculated without losing mass:

$$
\widehat J_o(x)
=
\sum_e
\left[
  \sum_k c_k M_{e,k}
  +
  P(E=e)\widehat V(e)
\right].
$$

That scalar is still only a candidate estimate.

### Exit completeness rules

* Every positive-probability physical outcome appears exactly once.
* Merging is allowed only when the entire `OptionExitKeyV1` matches.
* Exact exit states with the same carrier projection are not mergeable by default.
* No renormalization is permitted.
* Zero-probability entries may be omitted only after the original authoritative row sum has been checked.
* Recovery consumption is counted before the recovery-complete exit.
* Mechanical misses, destroyed progress, blocked continuations, and wrong Fracture carriers are explicit.
* `action_not_applied`, `no_matching_edge`, `unresolved`, and fail-closed defaults must be zero in a certified final policy.

An option may expose a `HardFailure` exit as a fragment result, but the final meta-policy must route it to explicit recovery. Leaving it as a non-goal terminal prevents final properness.

---

## 7. Finite-state option properness proof

The option verifier should reuse the properness definition already present in the exact refined-policy evaluator.

### Algorithm

For each exact entry state:

1. Enumerate the positive-probability reachable product graph.
2. Validate every nonterminal row:

   * one selected executable action;
   * at least one transition;
   * finite, nonnegative probabilities;
   * sorted unique successor identities;
   * probability sum equal to one within the named tolerance.
3. Validate each exit:

   * no selected action;
   * no outgoing transitions;
   * zero immediate action cost.
4. Run Tarjan SCC decomposition.
5. Reject a terminal in a non-singleton SCC.
6. Reject any reachable SCC with:

   * no exit terminal; and
   * no positive-probability edge leaving the SCC.
7. Return the lexicographically canonical improper SCC as a witness.
8. Solve expected action and resource equations only after properness succeeds.
9. Check residuals and finite values.

This matches the repository’s existing exact policy evaluator, which explicitly rejects a reachable closed nonterminal component as `improper_closed_component`.  The focused refinement tests already assert canonical improper-component reporting.

### Why this proves option absorption

In a finite Markov chain, every infinite path eventually remains in a recurrent class. If no reachable nonterminal SCC is closed, the only recurrent classes are declared option exits. Therefore the option reaches an exit with probability one.

Because the transient state set is finite and every immediate reward is finite and nonnegative, expected option duration and expected cumulative resource consumption are finite.

This proves **properness to an option exit**, not properness to the final crafting goal.

---

## 8. Meta-policy properness proof

The meta-policy is a finite graph of option invocations:

```cpp
struct MetaPolicyStateV1 {
    refinement::StableKey exact_item;
    std::uint32_t meta_node;
    FiniteResourceState hard_resources;
    FiniteMetaMemory memory;
};
```

A meta node contains:

* an exact initiation guard;
* one certified leaf option instance;
* a continuation target for every declared exit key or exit class.

### Embedded exit chain

Each certified option invocation induces a transition in an embedded chain between option-boundary states.

Meta-policy verification requires:

1. every invoked option is structurally certified for its exact entry domain;
2. every reachable option exit has an explicit continuation;
3. continuation selection is deterministic from the full exit key;
4. exact final success is the only allowed absorbing final class;
5. the finite embedded chain has no reachable closed non-goal SCC;
6. every invoked option has finite expected duration and reward.

If these hold, the embedded chain reaches exact final success almost surely. The expected number of option invocations is finite, and each invocation has finite expected primitive duration, so expected total primitive actions and cost are finite.

### Cyclic recovery is permitted

A finite meta graph may contain:

```text
attempt → recover → attempt
```

It is valid when the exact embedded chain has a positive-probability path out to final success and no closed non-goal recurrent class.

V1 flattening should instantiate one fragment per meta invocation node. It should not recursively inline by call-stack path, so cyclic meta graphs remain finite.

### Mandatory second proof

The compositional meta proof is a screening certificate only. After flattening, the complete primitive graph must undergo the same SCC/recurrent-class proof again. Agreement between the two proofs is a regression invariant.

---

## 9. Graph-fragment flattening

Flattening is graph substitution, not probability composition.

### Inputs

* one finite meta-controller;
* structurally certified leaf option fragments;
* exact option-entry and exit mappings;
* exact start item;
* exact final goal;
* caller action scope.

### Algorithm

1. Create one namespace for every meta invocation node.
2. Copy that option’s route, operation, observed-choice, and exit nodes.
3. Replace each fragment exit port with an edge to the configured next meta node.
4. Connect final-success exits only to the exact shared success terminal.
5. Emit exact route conditions using the current condition compiler.
6. Add a `CertificationFailClosed` default to every router.
7. Canonically order nodes and edges.
8. Prune only nodes proved unreachable from the exact start.
9. Merge operation or routing nodes only when complete executable behavior is equal:

   * same action;
   * same observation requirements;
   * same choice policy;
   * same continuation targets;
   * same route defaults.
10. Serialize as ordinary strategy JSON.

The ordinary runtime already has operation, router, and terminal nodes with prioritized conditional/default edges. No new simulator VM is required.

### Critical invariant

**Flattening never reads `OptionExitAtomV1::probability`.**

It copies executable operations and routing. The final evaluator applies each primitive action and reconstructs its exact outcomes from mechanics.

This makes the historical failure mode structurally unavailable: a flattening pass cannot drop 20% of an option outcome row because it is not multiplying or rewriting that row in the first place.

### Default behavior

Certification uses fail-closed routing. It must not hide a missing option branch by using the product-safe Restart default. Explicit Restart or recovery appears as a normal operation in the graph.

The current compiler already supports a certification fail-closed mode and treats unmatched policy states as verification failure.

---

## 10. Independent exact-evaluation path

The final publication path should remain separate from option verification.

### Path A: fragment verifier

* Consumes `OptionFragmentIRV1`.
* Uses a private exact mechanics context.
* Builds the option product graph.
* Produces properness and exit/resource certificates.

### Path B: final strategy evaluator

* Consumes only flattened ordinary strategy JSON and the exact start item.
* Parses the strategy afresh.
* Rebuilds route observations and primitive action kernels.
* Does not consume:

  * option transition rows;
  * option exit probabilities;
  * meta transition probabilities;
  * option expected costs;
  * candidate continuation values.

The existing assertion path calculates off-policy mass as the sum of failure, stop, action-not-applied, no-matching-edge, and unresolved mass. It requires convergence, success probability one within tolerance, complete pricing, and exact cost reconciliation before marking a candidate complete and executable.

Promotion should be equivalent to:

```cpp
const bool promotable =
    assertion.status == CompiledPolicyAssertionStatus::Complete &&
    assertion.proper &&
    assertion.executable &&
    assertion.zero_off_policy &&
    assertion.evaluation.cost_complete &&
    assertion.cost_reconciled &&
    std::isfinite(assertion.exact_cost) &&
    assertion.exact_cost >= 0.0;
```

Only after that result should a complete `BoundedPolicyIncumbent` be passed to `IncumbentPortfolio::observe_verified`.

### Additional independent test authority

For focused tests:

* compare the production exact evaluator with the existing forward-reference evaluator;
* check exact expected resource quantities;
* check `max_mass_conservation_error`;
* run the Simulator for exactly 10,000 trials where the strategy horizon makes sampling applicable.

The current evaluator tests already compare exact evaluation with a forward reference and assert tight mass-conservation error on deterministic, cyclic, and larger routed strategies.

---

## 11. Isolation from certification, scheduling, and proof behavior

V1 should run as a **shadow incumbent generator**, not as part of the existing search scheduler.

### Private context

An `OptionLabContext` should own:

* a private state interner;
* private exact transition caches;
* private option and meta work budgets;
* immutable references to mechanics/data;
* a copied exact start, goal, action scope, and economy;
* no reference capable of modifying the core action ledger, scheduler queues, proof store, or sparse graph.

### Invocation boundary

Run option synthesis only at explicit immutable checkpoints:

* after an existing verified incumbent exists; or
* during finalization as an optional additional candidate source.

Do not interleave it with core graph generation in the first qualification version.

This prevents a repeat of the historical clean-case side effect in which attempted carrier certification enlarged the main graph and improved a different policy.

### Failure behavior

Any of these discards the candidate and leaves the incumbent unchanged:

* option state cap;
* transition cap;
* byte cap;
* predicate expressibility failure;
* incomplete entry closure;
* row-sum mismatch;
* improper option SCC;
* improper meta SCC;
* compiler failure;
* exact-evaluator cap;
* missing price;
* off-policy probability;
* cost mismatch.

### Proof firewall

The option subsystem should not include or return `ProofLowerValue`.

It may not:

* prune a state or action;
* retire an action-envelope obligation;
* close an exactness proof;
* alter a Bellman lower;
* affect `ProofPatternManager`;
* change scheduler lane scores;
* change an action tie-break.

The repository already treats ordering/carrier projections as non-convertible to proof values and composes public lowers only from independently admissible proof components. The option subsystem should preserve the same compile-time boundary.

### PDR replay restriction

The current coarse checkpoint is not a truthful test boundary for the PDR witness because delayed carrier/action generation remains live after the last prepared graph. Option qualification on PDR must use the ordinary solve path or a future scheduler-aware checkpoint that has first proved ordinary/save/replay parity.

---

## 12. Cache and reuse rules

Use separate caches for structural verification and pricing.

### Structural option cache

Keyed by the complete `OptionStructuralIdentityV1`.

May retain:

* exact entry-domain closure;
* verified product graph;
* exit kernel;
* SCC decomposition;
* properness certificate;
* expected resource vectors;
* predicate-equivalence certificate.

### Price-scoped cache

Keyed by:

```text
structural certificate identity
+ exact economy identity
+ exact price-table bits
+ resource solver version
```

May retain internal priced diagnostics. It still does not confer upper authority.

### Flattened artifact cache

Keyed by:

```text
meta-policy canonical identity
+ all option certificate identities
+ exact start
+ final goal
+ caller scope
+ compiler version
+ certification default mode
```

A cached flattened graph can be reused as a candidate after prices change, but its prior evaluated cost cannot. It must be independently re-evaluated under the new economy.

### Reuse matrix

| Change                          |                                                          Structural option certificate |  Prior cost |                     Flattened graph |
| ------------------------------- | -------------------------------------------------------------------------------------: | ----------: | ----------------------------------: |
| Prices only                     |                                                                               Reusable |     Invalid |            Candidate graph reusable |
| Goal or exact start             |                                                                        Usually invalid |     Invalid |                             Invalid |
| Caller action scope             |                                                                                Invalid |     Invalid |                             Invalid |
| Restart/Imprint controls        |                                                                                Invalid |     Invalid |                             Invalid |
| Selected action or choice order |                                                                                Invalid |     Invalid |                             Invalid |
| Predicate/compiler semantics    |                                                                                Invalid |     Invalid |                             Invalid |
| Mechanics/data artifact         |                                                                                Invalid |     Invalid |                             Invalid |
| Verification tolerance/version  |                                                                                Invalid |     Invalid |                             Invalid |
| Append-only main graph growth   | Irrelevant for privately rebuilt artifact; otherwise only exact immutable-prefix reuse | Re-evaluate | Only with unchanged captured prefix |

Current fallback properness reuse already follows a versioned “proof over exact immutable policy and graph prefix” pattern. Option caching should extend that pattern rather than use carrier hashes.

Additional rules:

* Hash match always requires canonical payload equality.
* A cancelled or capped verification produces no reusable positive certificate.
* Negative cache entries may record a deterministic refusal for diagnostics, but have no authority.
* The first production version should re-run final flattened exact evaluation for every publication, even on a structural cache hit.
* A controller selected using one continuation estimate remains executable under another estimate, but not necessarily competitive. Reuse is safe only as a fixed candidate, never as an optimal option claim.

---

## 13. Initial option library

Each entry below is a parameterized template. Every concrete instance must be verified for its exact domain.

### 13.1 `same_side_from_clean_v1`

Parameters:

```text
side
requested goal-slot mask
exact clean entry shape
allowed action scope
```

Initiation:

* exact supported clean seed;
* appropriate rarity/item state;
* target slots lie on the selected side;
* no unsupported live checkpoint or carrier feature.

Exits:

* `SubgoalReached(mask, exact_state)`;
* `FinalSuccess` when the subgoal is the complete goal;
* explicit recoverable exits if the retained policy has them.

The accepted clean three-prefix and three-suffix policies are the initial oracle instances:

* prefixes: `1618.2138946963837`, 154 nodes / 432 edges;
* suffixes: `1101.15648683309`, 78 nodes / 219 edges;
* both exact-evaluated with success probability one, zero off-policy, complete pricing, and 10,000/10,000 Simulator success.

Those two witnesses justify seeding the library. They do not prove the template for every base, goal family, or arbitrary dirty entry state.

### 13.2 `preserve_side_then_progress_v1`

Parameters:

```text
preserved goal mask
protected side
setup/protection action
progress action or fixed program
optional protection removal
```

Initiation must prove:

* the preserved affixes and protection setup are exactly observable;
* the operation is legal;
* all relevant blocker/capacity state is represented.

Exits distinguish:

* progress with preservation retained;
* no progress with preservation retained;
* destroyed requested progress;
* protection/setup loss;
* capacity obstruction;
* recoverable carrier.

No “preservation probability” may be inferred from a carrier effect summary. The exact action kernel determines every branch.

### 13.3 `cleanup_for_continuation_v1`

This should not be a generic “junk exists” option.

Parameters include the next intended continuation contract:

```text
next-option initiation predicate
target terminal predicate, if any
permitted cleanup actions
preservation requirements
```

Initiation requires that cleanup changes one of:

* legality of the next action;
* side capacity needed by the next option;
* a blocker relevant to the next option;
* exact final-success eligibility.

Exits include:

* continuation enabled;
* still obstructed;
* useful progress destroyed;
* alternate recoverable carrier.

This preserves the established successor-driven cleanup rule: dirty carriers are not cleaned merely because they are dirty, and direct multi-goal jumps remain available.

### 13.4 `fracture_prepare_v1`

Builds the exact legal carrier required for a named Fracture attempt.

The exit must retain:

* exact affix identities;
* raw affix count;
* acceptable Fracture targets;
* current fractured/crafted state;
* all miss-recovery-relevant state.

A coarse “carrier goal slot reached” exit is insufficient.

### 13.5 `fracture_attempt_v1`

Applies the exact Fracture primitive.

Exits include every physical branch:

* desired goal affix fractured;
* other acceptable requested affix fractured;
* wrong affix fractured;
* mechanic miss/replacement path where applicable;
* inapplicable/refusal, which prevents certification unless handled.

It must not consume the product-parent coarse `ProductFractureKernel` as its certificate. Verification should rebuild the strict exact mechanics outcomes.

### 13.6 `fracture_miss_recovery_v1`

This is a separate, explicitly named mechanic-owned recovery control.

It:

* consumes the priced replacement base;
* resets the exact item state and checkpoint state correctly;
* returns to a declared anchor;
* exists only where the Fracture mechanic contract authorizes replacement recovery.

The current product solver distinguishes this from voluntary economic Restart and requires a priced base when the automatic Fracture route is present.

### 13.7 `economic_restart_v1`

Available only when `allow_economic_restart` is present in caller scope.

It must not appear as:

* an unmatched-route default;
* a generic option failure handler;
* an implicit way to prove properness.

Changing Restart permission invalidates the option and meta identities.

### 13.8 `renewal_to_anchor_v1`

A proper exact renewal controller from a named entry domain to a verified anchor or final goal.

It should reuse the current primitive-renewal proof shape when applicable:

* identical exact renewal kernel across working states;
* row probability sum one;
* positive success probability;
* fixed captured action;
* exact resource cost;
* no unrepresented choice outcomes.

Automatic Imprint programs should not be in the initial library because the current Calculator product scope disables them. They can be added later with checkpoint identity explicitly represented.

---

## 14. Multiple subgoal-order handling

A meta-policy should be a state-conditioned graph, not a list such as:

```text
goal A → goal B → goal C
```

### Meta state

The exact item state remains authoritative. An achieved-goal mask may be retained as an index, but it is not assumed monotone: cleanup, protection loss, and destructive actions may reduce progress.

### Candidate generation

The search layer should propose several meta graphs:

1. different first subgoals;
2. different same-side-before-cross-side orders;
3. preservation-first and cleanup-first variants;
4. Fracture-before-finish and Fracture-after-partial-progress variants;
5. direct multi-goal jumps;
6. state-conditioned orders that differ after option failure exits;
7. recovery loops.

A transition such as 3-goal progress directly to 5-goal success must remain representable. The prior carrier-ladder contract explicitly rejected forcing a 3→4→5 order when one action can achieve 3→5.

### Search algorithm

A practical candidate generator can use:

* best-first search over finite meta boundary states;
* beam search with diversity by achieved subset and preserved carrier;
* exact option exit kernels for semi-Markov candidate evaluation;
* deterministic enumeration of the first option family before pruning;
* canonical deduplication of equivalent flattened controllers.

The semi-Markov solution, even when numerically exact for its verified option kernels, remains `EstimatedMetaPolicyCandidate`. It becomes upper authority only after graph flattening and independent full evaluation.

Missing an order does not affect lower bounds, pruning, or exact closure because the option system is an incumbent generator only.

---

## 15. Historical mass-loss regression

The production regression should include both a minimal failure-class fixture and the real four-goal integration case.

### 15.1 Minimal exact fixture

Authoritative primitive outcome row:

| Outcome               | Probability | Exact exit      |
| --------------------- | ----------: | --------------- |
| progress              |        0.50 | exact state `G` |
| recoverable miss      |        0.30 | exact state `R` |
| blocker/wrong carrier |        0.20 | exact state `B` |

`R` and `B` should deliberately share the same coarse carrier projection while differing in:

* next-option legality;
* required cleanup;
* or resource/checkpoint state.

The historical-style coarse composer is then made to emit only `G` and `R`, producing mass `0.80`.

Required tests:

#### `option_exit_kernel_missing_mass_rejected`

The verifier rejects the 0.80 row as `policy_transition_sum_mismatch` before any value is solved.

#### `option_exit_kernel_renormalization_rejected`

Renormalizing 0.50/0.30 to 0.625/0.375 still fails because the result does not match the authoritative exact outcome multiset and probability-bit signature.

#### `option_exit_kernel_duplicate_mass_rejected`

Routing one physical outcome to two exits produces mass greater than one and is rejected.

#### `option_projection_merge_without_lumpability_rejected`

Merging `R` and `B` solely because their carrier projections match fails exact replay/lumpability.

#### `option_explicit_recovery_preserves_mass`

Routing `B` to an explicit recovery fragment produces:

* total mass one;
* proper option absorption;
* proper meta-policy absorption;
* exact resource inclusion for recovery;
* a flattened strategy with zero off-policy mass.

#### `failed_option_candidate_does_not_replace_incumbent`

Begin with a verified 10-Chaos incumbent, attempt to promote the lower-looking malformed option candidate, and verify that the public upper and portfolio identity remain 10.

### 15.2 Historical integration fixture

Because the removed prototype should not return to production, retain a compact test-only fixture containing:

* exact source outcome rows;
* the old coarse exit mapping;
* the omitted/merged outcome witness;
* expected failure classification;
* expected pre-promotion candidate value metadata.

The test should reproduce the archived behavior:

* attractive coarse estimate;
* exact compiled-evaluation rejection;
* no incumbent replacement;
* no core graph growth credited to the option candidate.

The archive records the rejected 12,365.3929 and 12,197.2775 estimates and their mass-conservation failure, which should be the integration regression’s historical classification oracle.

---

## 16. Phased acceptance gates

## Gate 0 — Authority and baseline freeze

Before option behavior is enabled:

* add the type firewall;
* expose separate candidate/verified telemetry;
* verify no option type converts to proof or incumbent authority;
* preserve existing publication invariants;
* capture current PDR and five-T1 baselines at the exact ref.

Pass only if option-disabled runs are unchanged in:

* values;
* policy and graph identity;
* compiled artifact;
* action ledger;
* lower provenance;
* scheduling telemetry;
* termination classification.

## Gate 1 — Leaf fragment verifier

Focused finite fixtures must cover:

* deterministic success;
* stochastic success/recovery;
* zero-probability edges;
* observed choices;
* checkpoint state;
* missing transition mass;
* duplicate transition mass;
* improper closed SCC;
* proper cyclic retry;
* resource reward reconciliation;
* unexpressible executable predicate.

No full solver integration yet.

## Gate 2 — Initial option library

Verify concrete instances for:

* clean three-prefix closure;
* clean three-suffix closure;
* preservation;
* contextual cleanup;
* Fracture preparation and attempt;
* Fracture miss recovery;
* economic Restart on/off scope;
* renewal to an anchor.

Every accepted instance must have:

* exact entry closure;
* exact predicate equivalence;
* complete exit mass;
* proper option SCC proof;
* finite expected resources;
* deterministic identity on repeat.

## Gate 3 — Meta-policy and flattening

Required fixtures:

* two different subgoal orders;
* direct multi-goal jump;
* nonmonotone progress after destructive failure;
* attempt/recovery cycle;
* one improper closed meta cycle;
* two exits sharing a carrier projection but requiring different continuations.

Acceptance:

* compositional meta properness agrees with flattened-graph properness;
* flattening consumes no exit probabilities;
* strategy JSON is deterministic;
* compiler uses certification fail-closed defaults;
* exact evaluator and forward reference agree;
* internal resource diagnostics reconcile with flattened evaluation.

## Gate 4 — Isolation and cache qualification

With option synthesis enabled in shadow mode:

* main transition graph and scheduler trajectory remain unchanged under fixed core work;
* proof lower and action-envelope state are bit-identical;
* option caps do not alter the existing incumbent;
* a worse verified option candidate cannot replace a cheaper verified incumbent;
* an unverified cheaper estimate cannot replace anything;
* cache hits reproduce certificate identities;
* every identity mutation in the invalidation matrix causes a miss;
* a price-only change reuses structural verification but recomputes exact cost.

The behavior-changing scheduler integration remains disabled unless separately qualified.

## Gate 5 — Fixed PDR witness safety gate

Use the ordinary 1 GiB PDR path, not the current coarse checkpoint.

The archived authoritative reference is:

| Field                         |                Reference |
| ----------------------------- | -----------------------: |
| Independently evaluated upper |      `7866.432124027084` |
| Certified lower               |     `21.772459401271156` |
| Strict reforge work           |                `3507568` |
| Proof store plus quotient     |        `846846750` bytes |
| Native peak                   |       `1179431999` bytes |
| Stop                          | `max_solver_owned_bytes` |

Acceptance has two parts.

### PDR control parity

With options disabled, reproduce:

* request/action/caller identities;
* upper and lower;
* compiled policy identity;
* strict frontier/work;
* resource-stop classification.

### PDR shadow qualification

With options enabled:

* the core solve trajectory remains identical under fixed core work;
* the pre-existing verified upper can never be erased or increased;
* a capped, malformed, or improper option candidate leaves output unchanged;
* any promoted option-derived policy must independently prove:

  * exact success probability one;
  * zero off-policy probability;
  * complete prices;
  * properness;
  * finite cost;
  * cost reconciliation;
  * cost no greater than `7866.432124027084`.

Improvement is not required to pass the safety gate. It is required before enabling the option lane as a useful product default.

## Gate 6 — Five-T1 replay and current-source qualification

The five-T1 case needs separate historical-replay and current-source gates.

### 6A — Historical executable replay

Where the historical strategy artifact is available, current exact evaluation must reproduce:

* cost `87361.1690420501`;
* 607 nodes / 1,460 edges;
* exact-terminal success;
* success probability one;
* zero off-policy mass;
* complete pricing;
* exact cost reconciliation;
* 10,000/10,000 Simulator success.

The historical result was a genuine multi-mechanic exact-terminal upper, not lower-bound authority.

Documentation alone does not pass this gate. The executable artifact must be parsed and evaluated at the pinned mechanics ref.

### 6B — Fresh current baseline

Run the current five-T1 control afresh at the exact ref. The later archived control was `14454067.4260706`, but that is a comparison reference rather than a substitute for a fresh run.

### 6C — Option-derived functional pass

Require at least one option-derived flattened policy that:

* is strictly cheaper than the fresh current verified baseline;
* passes all independent exact-evaluation checks;
* does not alter the lower or core scheduler;
* preserves exact terminal semantics;
* passes deterministic native repeat;
* passes 10,000 Simulator runs when applicable.

### 6D — Competitive qualification

The stronger product qualification is:

```text
option-derived verified upper <= 87361.1690420501
```

or exact reproduction of the historical policy identity.

This is the appropriate threshold for claiming the option system has recovered the previously demonstrated five-T1 quality, rather than merely producing another valid but weak fallback.

## Gate 7 — Final publication acceptance

Only after PDR and five-T1 qualification:

* fresh release native build;
* focused option, refinement, solve, compiler, and evaluator suites;
* deterministic native repeats;
* exact graph evaluation;
* required 10,000-run simulations;
* release WASM rebuild and focused nonvisual parity;
* unchanged lower/proof telemetry;
* `git diff --check`;
* no enabled behavior-changing scheduler profile without its own measured qualification.

---

## 17. Recommended ownership layout

A clean implementation boundary would be:

```text
solver_option_contracts.hpp
    identities, predicates, fragments, exit labels, type firewall

solver_option_verify.cpp
    exact product construction, predicate equivalence, mass checks,
    SCC properness, reward/exit-kernel derivation

solver_option_meta.cpp
    candidate order generation, embedded meta chain, meta properness

solver_option_flatten.cpp
    graph substitution into ordinary strategy representation

solver_option_cache.cpp
    structural and price-scoped cache identities

solver_option_candidates.cpp
    heuristic ranking only; no proof or incumbent authority
```

Integration should be narrow:

* `FixedOptionSpec` gets an adapter into fragment IR.
* `solver_executable_carrier_planner.hpp` remains diagnostic/ranking-only.
* The existing compiler remains the ordinary strategy compiler.
* The existing exact strategy evaluator remains final evaluation authority.
* `IncumbentPortfolio` remains the sole upper-bound owner.
* `ProofPatternManager`, lower-bound composition, pruning, and scheduler ownership remain untouched.

## Final architectural invariant

> **An option summary may select what to compile, but it is never what executes and never what is published.**

The published object is always one flattened ordinary strategy whose complete primitive probability mass, recovery behavior, recurrent classes, resource use, and expected cost have been independently rebuilt and verified. That is the decisive difference between this system and the rejected carrier planner.
