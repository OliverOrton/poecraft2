# Learned-Guidance Research Program for `poecraft2`

**Repository:** `OliverOrton/poecraft2`
**Pinned ref:** `769c3deb1a2a2913c228c4135c764271f662bef9`
**Scope:** research and design only; no model training and no repository modification performed.

## 1. Recommended program

The first learned component should be a **resource-aware work-order ranker over existing exact carrier, action, and proof obligations**, paired with run-time and memory prediction.

It should not begin as:

* a value network;
* an end-to-end policy generator;
* an action filter;
* a neural abstraction or state merger;
* a replacement for Bellman evaluation;
* a second implementation of crafting mechanics.

The proposed architecture is:

```text
authoritative exact solver state
        |
        v
exact candidate and obligation enumeration
        |
        v
versioned feature projection
        |
        v
untrusted learned ranking / resource prediction / proposal
        |
        v
hard fairness and completeness wrapper
        |
        v
existing exact expansion, proof, compilation, properness,
probability accounting, and publication machinery
```

This ordering follows the current evidence. The same-side three-prefix and three-suffix problems already close exactly and provide strong semantic regression anchors. The five-goal carrier ladder can publish a useful exact-evaluated proper upper, but its lower is still extremely weak. The four-mod PDR witness has a verified upper and certified lower but stalls with hundreds of thousands of proof obligations and almost 847 MB in proof-store-plus-quotient memory before its 1 GiB stop.

The staged priority should therefore be:

1. **Carrier, action, and obligation scheduling.**
2. **Run-time, memory, and cap-risk prediction.**
3. **Experiment and curriculum selection.**
4. **Verified option/subgoal proposal for executable uppers.**
5. **By-construction admissible pattern proposal.**
6. **Prediction inside an exact cost-partition constructor.**
7. **Set or graph neural models only after non-neural baselines plateau.**

---

# 2. Non-negotiable safety contract

The learned system is an advisory plane. It is not proof authority.

The repository already enforces the relevant distinction: ordering scores and restricted-envelope values are separate from public admissible lower values, and an upper is authoritative only after it denotes one executable proper strategy with exact cost accounting.

## 2.1 Permitted learned outputs

A model may return only:

| Output               | Meaning                                                                                      | Permitted consumer             |
| -------------------- | -------------------------------------------------------------------------------------------- | ------------------------------ |
| `GuidanceScore`      | Finite relative priority among already enumerated candidates                                 | Scheduler only                 |
| `ResourcePrediction` | Quantiles or risks for work, wall time, incremental bytes, peak bytes, or cap failure        | Scheduler and experiment queue |
| `ProposalId`         | Index into an engine-owned closed grammar of options, subgoals, patterns, or partition seeds | Exact proposal constructor     |
| `ExperimentPriority` | Which development case/configuration should run next                                         | External experiment queue      |
| `Uncertainty`        | Model uncertainty for exploration or fallback                                                | Scheduler, never proof         |

A learned score may use exact bound information as an **input**, but its output remains scheduling-only.

## 2.2 Prohibited learned outputs

A model must never directly or indirectly supply:

* `ProofLowerValue`;
* a public admissible lower or a component added to one;
* an incumbent-dominance or noncompetitive decision;
* an action-envelope lifecycle transition;
* state equivalence, quotient membership, or merge authority;
* terminal classification;
* legality;
* transition probability or probability mass;
* exact expected cost;
* properness;
* exact-policy publication;
* a declaration that an open frontier is closed;
* a declaration that a run is exact or complete.

The current proof-pattern type boundary should remain the model: only `ProofLowerValue` may cross a proof/pruning boundary, while executable and ordering projections have no conversion to it. The pattern manager independently validates nonnegativity, zero goal values, and the Bellman subsolution condition before a value can contribute, and it combines existing independent patterns by maximum with a zero fallback.

A useful implementation-level rule is:

```text
No model artifact, model score, embedding, prediction, or proposal object
may be implicitly or explicitly convertible to ProofLowerValue,
CertifiedRowIdentity, executable incumbent authority, or a closure flag.
```

---

# 3. Precise safe integration points

## 3.1 Carrier scheduling

**Current source:** `engine/src/solver_solve_priority.cpp`, called by focused and incremental scheduling.

Current carrier scoring already extracts:

* complete satisfied-goal subset;
* fractured goals;
* protection;
* useful protection;
* side-capacity obstructions;
* blocked missing goals;
* unrelated occupancy;
* focused path-mass/gap priority.

It then builds goal-subset buckets and applies deterministic within-bucket ordering. This is explicitly scheduling-only.

**Safe hook:**

```text
carrier_ordering_score(state)
    -> exact structural score
    -> learned_carrier_score(snapshot, carrier_projection)
    -> stable within-bucket ordering
```

The model may reorder carriers **within the exact frozen candidate set**. It must not:

* omit a carrier;
* change the carrier’s goal-subset bucket;
* change the per-class member cap;
* cause a dirty carrier to disappear;
* suppress the existing round-robin or closure fallback.

The outer goal-subset round-robin should remain exact authority. A learned model may rank only within each bucket during the first qualifying stage.

## 3.2 Focused-fringe scheduling

**Current source:** `SolveWork::Impl::schedule_next_focused_expansion` in `solver_solve_focused.cpp`.

Current focused scheduling computes path-mass and gap-derived priority, admits a diversified carrier ladder, and, critically, falls back to every discovered unexpanded state when the selected fringe yields no work. Only a genuinely exhausted strict graph can close the focused fallback.

**Safe hook:** use learned scores in the existing stable sort and within goal-subset buckets.

**Invariant:** the final “enqueue every discovered unexpanded state” fallback remains unchanged and bypasses the model.

## 3.3 Carrier/action ordering

**Current source:** `carrier_action_ordering_score` and `prioritize_carrier_actions` in `solver_solve_priority.cpp`.

Current exact projections measure:

* obstruction removal;
* preservation of satisfied goals;
* preservation of useful protection;
* immediately reachable missing goals;
* all reachable missing goals;
* stable operator identity.

The existing sort is deterministic and changes only work order.

**Safe hook:**

```text
exact candidate operator set
    -> one batched model request for the entire action set
    -> score per operator
    -> hard age/fairness key
    -> learned score
    -> current stable operator tie-break
```

The model cannot remove an operator. `operator_indices` must have identical membership before and after ranking.

## 3.4 Incremental carrier/action pair scheduling

**Current source:** `schedule_next_incremental_alternative` in `solver_solve_incremental.cpp`.

The incremental solver maintains separate automatic, legacy-fairness, high-progress, and exact-closure services. `schedule_pair` first records the exact state/operator pair in the action-envelope ledger; only existing exact proof can retire a pair without materializing it. The closure service eventually scans the Cartesian carrier/operator obligation.

**Safe hook:** rank available pairs within a lane or choose among non-mandatory lane tickets.

**Invariants:**

* ledger transitions remain engine-owned;
* exact inapplicability and operator proof remain the only non-expansion retirement routes;
* the exact-closure scan remains enabled;
* a model timeout or invalid output uses the current ordering.

## 3.5 Scheduler lanes

**Current source:** `solver_anytime_scheduler.hpp`.

The current scheduler has lanes for legacy fairness, executable upper, high progress, exact closure, and proof-directed work. It records wait and starvation telemetry, but reaching `starvation_dispatches` presently increments a counter; it does not itself force the waiting lane.

**Safe hook:** a model may rank optional lane service only after a hard forced-service wrapper has selected any overdue lane.

The learned model must not control the existence of a lane or set its availability bit.

## 3.6 Strict alternative obligations

**Current source:** `ProofStore::ordered_pending_alternative_obligations()` and `UnresolvedAlternativeObligationIdentity`.

This is the most delicate source-level integration point.

`UnresolvedAlternativeObligationIdentity` currently contains:

```cpp
CarrierWideOptimisticLowerQ optimistic_lower;
double scheduling_priority;
SharedStableKey resumable_work_identity;
```

and its equality and semantic hash include the bit pattern of `scheduling_priority`. The current policy-refinement path initializes that value to `0.0` before interning obligations.

**Do not place a learned score in this field at the pinned ref.** Doing so would make model version, floating-point output, or batch-dependent inference part of semantic obligation identity and proof-store reuse.

The safe design is a separate sidecar:

```text
GuidanceOverlayKey:
    obligation_id
    source_generation
    target_generation
    partition_generation
    action_generation
    admission_generation
    price_generation
    vocabulary_generation
    model_sha256
    feature_schema_id

GuidanceOverlayValue:
    finite_score
    predicted_work_quantiles
    predicted_memory_quantiles
    inference_batch_id
```

`ordered_pending_alternative_obligations()` would consult the sidecar only at ordering time. The canonical identity stays unchanged. A later implementation could instead refactor `scheduling_priority` out of the canonical identity, but that refactor itself requires proof-store regression testing.

## 3.7 Proof-pattern selection and generation

**Current source:** `solver_proof_pattern_manager.hpp`, `solver_solve_carrier_pattern.cpp`, and operator/envelope proof owners.

There are two safe levels:

1. **Selection among already validated patterns:** rank which pattern construction or refresh to attempt first.
2. **Proposal of a finite pattern specification:** the model emits a grammar object; the engine constructs the abstract SSP and validates the resulting lower.

The model must never emit the pattern’s lower value.

Existing validated patterns remain eligible for `select_maximum` regardless of the model’s ranking. A failed, capped, or invalid generated pattern is simply unavailable and leaves the public lower unchanged.

## 3.8 Options and subgoals

**Current source boundary:** `solver_executable_carrier_planner.hpp` plus existing automatic operator preparation and upper-policy verification.

The surviving carrier planner header is explicitly a reusable exact projection boundary after the previous product planner failed its gate. It has no conversion to proof, pruning, or terminal authority.

A model may propose:

* a goal-subset milestone;
* properties to preserve;
* an exact side-capacity or blocker predicate;
* a registered option family;
* a planning horizon or option budget.

The engine must then:

1. instantiate the proposal from registered exact actions;
2. enumerate its exact outcomes;
3. maintain complete probability mass;
4. attach any open continuation to a real executable fallback;
5. test properness;
6. compile the policy;
7. independently evaluate and reconcile its expected cost.

This must not recreate the discarded coarse carrier composition architecture under a new name.

## 3.9 Run-time, memory, and experiment selection

Resource prediction observes solver state but does not participate in proof:

* at native work-step boundaries;
* before a carrier/action/obligation batch;
* before a strict partition or pattern construction;
* at corpus-runner scheduling time.

Predictions may alter which work is attempted first or which development experiment runs next. They may not terminate a solve as exact, close an action envelope, or publish an upper.

---

# 4. Feature schema

Use a versioned schema whose primary state and action fields are built directly from the existing exact carrier projections.

## 4.1 Carrier fields

`ExecutableCarrierProjection` currently exposes the following exact fields:

```text
state                               identity/tie-break only
satisfied_goal_mask
missing_goal_mask
blocked_mask
crafted_goal_mask
fractured_goal_mask
fractured_metamod_flags
protection_flags
other_flags
junk_count
crafted_junk_count
fractured_junk_count
fractured_crafted_junk_count
debt_flags
prefix_count
suffix_count
missing_prefix_goals
missing_suffix_goals
prefix_capacity
suffix_capacity
rarity
influence_bits
veiled_side
searing_exarch_tier
eater_of_worlds_tier
```

`debt_flags` should be expanded into separate booleans:

```text
blocked_goal_debt
prefix_capacity_debt
suffix_capacity_debt
terminal_junk_debt
fractured_junk_debt
```

Raw masks should have two representations:

* fixed-width mask/popcount features for cheap baselines;
* canonical per-goal-slot set tokens carrying slot index, side, satisfied, missing, blocked, crafted, and fractured state.

`state` is a request key and stable tie-break. It should not be treated as a meaningful ordinal input.

## 4.2 Action fields

`ExecutableCarrierActionProjection` exposes:

```text
operator_index                       identity/tie-break only
preserved_goal_mask
destroyed_goal_mask
created_goal_mask
preserved_fractured_goal_mask
destroyed_fractured_goal_mask
preserved_protection
destroyed_protection
preserved_properties
destroyed_properties
created_properties
```

Add the existing exact `CarrierEffectSummary` and operator descriptors:

```text
min_prefix_count
max_prefix_count
min_suffix_count
max_suffix_count
preserved_satisfied_goal_count
destroyed_satisfied_goal_count
created_satisfied_goal_count
preserved_useful_protection_count
obstruction_removal_count
immediately_reachable_missing_goal_count
reachable_missing_goal_count

stable_operator_family
primitive_program_length
choice_recipe_kind
routing_observation_count
authoritative_immediate_cost
automatic_vs_static
action_envelope_lane
```

Stable operator IDs should be retained for provenance and ties, but evaluations should include an ablation without them to detect memorization.

## 4.3 Obligation fields

For a strict alternative obligation:

```text
status
work_completed
obligation_age_in_dispatches
age_within_source_cell
source_cell_coverage_count
source_probability_mass
observation_requirement_size
action_recipe_size
source_generation
target_generation
partition_generation
action_generation
admission_generation
price_generation
vocabulary_generation
resumable_work_kind
certified_rows_for_source
unresolved_alternatives_for_source
reverse_dependency_count
current_proof_memory_category_bytes
```

The exact carrier-wide lower and any valid conditional upper may be supplied as **context inputs**, along with their provenance kinds and generations. They remain exact inputs; the model cannot modify or replace them.

Raw hashes should be logged but not normally embedded as semantic model features.

## 4.4 Global search-state fields

At a decision snapshot:

```text
phase
focused_round
scheduler_lane
lane_services[]
lane_waits[]
lane_max_wait[]
lane_starvation_events[]

public_lower
verified_upper_present
verified_upper
absolute_gap
normalized_gap

states_discovered
states_expanded
frontier_states
rows
transitions
logical_reforge_work

incremental_carriers
unresolved_action_pairs
strict_cells
strict_carriers
strict_obligations
certified_obligations
stale_obligations

solver_live_bytes
solver_peak_bytes
proof_store_bytes
quotient_bytes
live_replay_slice_bytes
transition_cache_bytes
compiled_policy_bytes

work_since_last_lower_improvement
work_since_last_verified_upper
work_since_last_frontier_change
```

Exact lower and upper values are permissible context, but their use must be ablated. A model that only memorizes “large gap means choose X” is unlikely to generalize across economies and goal shapes.

## 4.5 Goal, request, and immutable context

```text
goal_slot_count
required_goal_count
prefix_goal_slot_count
suffix_goal_slot_count
required_rarity
exact_terminal_contract_id

session_identity
start_identity
goal_identity
economy_identity
action_vocabulary_identity
artifact_identity
solver_options_identity

memory_cap
work_caps
wall_watchdog
requested_bounded_finish
exact_evaluation_setting
```

Identities are primarily grouping and provenance keys. Model features should use structural goal/action tokens instead of relying solely on opaque identity embeddings.

## 4.6 Pattern and option proposal fields

Pattern candidate:

```text
pattern_kind
selected_projection_coordinates
goal_slots_observed
capacity_fields_observed
junk/debt_fields_observed
estimated_abstract_states
estimated_abstract_rows
fallback_kind
covered_action_shape_count
cost_partition_slot
prior_build_status
```

Option candidate:

```text
option_library_id
subgoal_goal_mask
required_preservation_mask
allowed_destruction_mask
required_protection
capacity_predicate
debt_predicate
maximum_horizon
estimated exact outcome count
available fallback kind
```

## 4.7 Feature versioning

Each dataset row must carry:

```text
feature_schema_id
feature_schema_sha256
encoder_source_sha256
projection_abi_id
field_order
field_dtypes
mask_width
null/missing encoding
normalization_manifest_sha256
```

Changing mask semantics, feature order, normalization, or exact projection code creates a new schema version.

---

# 5. Labels and censoring

## 5.1 Dataset units

Use three related datasets rather than one overloaded table.

### Decision dataset

One row per:

```text
(run snapshot, scheduler decision, candidate)
```

Used for carrier, action, lane, obligation, option, and pattern ranking.

### Work-unit outcome dataset

One row per completed exact cooperative unit:

```text
candidate selected
work consumed
wall time
incremental and peak bytes
lifecycle result
bound or incumbent event
new frontier or dependency effects
```

### Run trajectory dataset

One row per run with a step-boundary trajectory and terminal runner/native status.

## 5.2 Run-status labels

| Class                 | Required evidence                                                                                               | Treatment                                                         |
| --------------------- | --------------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------------- |
| **Exact**             | Exact native status, exact closure, proper compiled policy, exact reconciliation, closed required envelopes     | Fully observed event times                                        |
| **Bounded**           | Valid completed report with open work represented explicitly                                                    | Milestones before stop are observed; no later outcome is invented |
| **Capped-completed**  | Native final report at a named work, memory, graph, evaluator, or product cap                                   | Separate terminal class; use observed trajectory only             |
| **Watchdog-censored** | Watchdog expiry plus atomic partial containing the selected case and at least one bound sample                  | Right-censored at the last valid step boundary                    |
| **Failed**            | Crash, OS OOM, invalid bound, cancellation, memory refusal, runner error, or watchdog with no usable trajectory | Competing failure label; not converted to a long runtime          |

This follows the repository’s current trajectory contract: only a watchdog with an atomic usable partial is considered right-censored; crashes, OOMs, invalid output, memory refusal, and a watchdog without trajectory remain failures. Completed exit code `0` or `2` with a final report remains a completed measurement.

A named native cap should not be imputed as exact completion and should not be relabelled as a crash. For time-to-exact-closure analysis, preserve it as a distinct administrative stop until the benchmark contract explicitly defines how such caps enter a survival estimator.

## 5.3 Scheduling labels

For each candidate serviced next, record:

```text
service_work
service_wall_ns
incremental_owned_bytes
peak_owned_bytes_during_service

result:
    certified_row
    exact_inapplicable
    operator_proved_noncompetitive
    frontier_returned
    partially_evaluated
    resource_interrupted
    stale_after_generation_change
    no_observable_progress

lower_improvement
verified_upper_improvement
proper_policy_found
frontier_reduced
new_carriers_discovered
new_obligations_created
obligations_retired
```

Primary rank labels should be **event-normalized**, not raw immediate “reward”:

```text
work to the next public lower threshold
work to the next verified upper threshold
work to a fixed normalized-gap threshold
work to exact closure, where observed
peak memory at the same threshold
probability of reaching the threshold under the cap
```

## 5.4 Counterfactual labels

The best scheduling label is produced by a paired experiment:

```text
from identical faithful snapshot:
    force candidate A for one exact work unit
    resume the deterministic baseline schedule
    measure work/memory to milestone M
```

This yields pairwise labels such as:

```text
A precedes B for milestone M
delta_work(A, B, M)
delta_peak_memory(A, B, M)
```

The current coarse PDR checkpoint must not be used for these labels. It does not serialize the live incremental scheduler and action-envelope state, and replay demonstrably follows a different solve with a different upper, stop reason, outstanding work, and wall time.

Until a faithful checkpoint exists, counterfactual labels should come from:

* clean full reruns;
* small exact microcases;
* deterministic prefix reruns whose entire live state is reconstructible;
* naturally occurring comparisons between identical grouped snapshots.

## 5.5 Censor-aware losses

Use separate heads and losses:

1. **Completed work-unit regression:** log work, log bytes, and wall-time quantiles.
2. **Run-level milestone survival:** discrete-time or AFT survival loss using only valid watchdog-censored records as censored observations.
3. **Competing failure head:** OOM, invalid result, crash, memory refusal, and other failures.
4. **Ordinal/listwise scheduling loss:** pairwise or listwise rank to the same milestone.
5. **Calibration loss:** probability of reaching a milestone before a named cap.

Right-censored algorithm-runtime data is a recognized problem in algorithm selection, and survival or censor-aware methods are preferable to assigning every timeout an arbitrary fixed runtime penalty. ([AAAI Publications][1])

Do not:

* treat cap time as the true eventual runtime;
* assign successful utility to a crash that happened after a promising partial;
* call failure “censored” merely because the final answer is unknown;
* train a completion-time regressor on uncensored runs only without reporting selection bias.

---

# 6. Train, validation, and frozen-test split

## 6.1 Grouping rule

Never randomly split states, candidate rows, obligations, or trajectory samples.

All observations sharing any of the following must remain in one role:

```text
case identity
goal identity
start identity
session/base/economy identity
action-envelope identity
generator stratum
source run
checkpoint lineage
semantic variant family
```

All snapshots and candidates from one run stay together. Near-duplicate cases generated from the same parameter tuple should also be grouped.

## 6.2 Roles

### Development

Used for:

* fitting;
* feature engineering;
* loss selection;
* curriculum experiments;
* threshold tuning;
* initial failure analysis.

### Validation

Used for:

* model family and hyperparameter choice;
* calibration;
* batching and overhead tuning;
* fixed go/no-go gates before test;
* choosing one final candidate per research stage.

### Frozen test

Used exactly once per declared candidate.

The repository already assigns whole generator strata to `development`, `validation`, or `frozen_test` and forbids splitting a stratum across roles. Preserve that contract.

The current archive witnesses are **known development/regression anchors**, not a legitimate unseen frozen test. A new frozen set should be generated and sealed before model or feature tuning begins.

## 6.3 Required frozen-test slices

The final frozen test should include whole held-out strata for:

* unseen bases or item classes;
* unseen goal-family combinations;
* unseen prefix/suffix balance;
* unseen required-goal counts;
* unseen blocker/protection/fracture combinations;
* held-out economy identities;
* held-out action-envelope combinations;
* hard resource-cap strata;
* exact-closing and non-closing instances;
* cases where useful direct subset jumps skip intermediate goal counts.

A later-commit test can be maintained as a separate distribution-shift suite, but its results should not be mixed with the pinned-ref frozen test.

---

# 7. Dataset and model provenance

## 7.1 Dataset manifest

Every dataset version should pin:

```text
dataset_id
dataset_semver
dataset_manifest_sha256

repository_ref
dirty_paths
executable_path
executable_sha256
build_type
compiler_identity

corpus_path
corpus_sha256
corpus_id
corpus_schema
generator_config_sha256
role_manifest_sha256
selected_roles

compiled_artifact_manifest_sha256
session/start/goal/economy identities
action-vocabulary identity
solver-options identity
case caps and watchdog

machine identity
OS
processor
logical CPU count
worker count
Python/runtime versions

feature_schema_id
encoder_sha256
label_schema_id
censoring_contract_id
split_manifest_sha256

source trace hashes
partial-report hashes
checkpoint hashes, where faithful
```

The current runner and benchmarking contract already records most execution, corpus, artifact, machine, and case identities; the learned-guidance manifest should extend rather than replace it.

## 7.2 Model manifest

```text
model_id
model_semver
model_sha256
model_family
architecture/config_sha256
feature_schema_id
normalization_manifest_sha256
training_code_sha256
training_container/runtime versions
training_seed
dataset_manifest_sha256
train-row-set sha256
validation-row-set sha256
split_manifest_sha256
loss definitions
calibration method and data
parent model, if fine-tuned
go/no-go report sha256
```

A suitable immutable identifier would be structurally similar to:

```text
guidance/obligation-ranker/1.2.0
+schema-guidance-v3
+data-4f91d2a1
+model-84c513...
```

Never load a mutable artifact called merely `latest`.

## 7.3 Inference provenance

Every request should log:

```text
model_sha256
feature_schema_id
request_kind
run/case identity
snapshot generation tuple
candidate-set sha256
batch_id
batch formation mode
scores and resource quantiles
fallback or stale-discard reason
inference latency
inference-owned bytes
```

A faithful checkpoint must either serialize outstanding inference results and the model SHA, or deterministically recompute them from the exact checkpoint state. It cannot silently resume under another model.

---

# 8. Model families and non-neural baselines

## 8.1 Mandatory baselines

### Scheduling

1. Current pinned solver ordering.
2. FIFO/oldest-first.
3. Existing goal-subset round-robin with current within-bucket order.
4. Stable random order with at least five fixed seeds.
5. Hand-authored linear score over current carrier/action fields.
6. Logistic pairwise ranker.
7. Elastic-net linear ranker.
8. Gradient-boosted trees.
9. LambdaMART or another listwise tree ranker.
10. Per-stratum empirical median work/resource rule.

### Resource prediction

1. Global median.
2. Median by case stratum.
3. Log-linear/AFT model.
4. Quantile regression.
5. Quantile gradient-boosted trees.
6. Survival forest.
7. Simple cap-risk logistic model.

### Options/subgoals

1. Primitive-only exact search.
2. Current automatic option generator.
3. Stable random verified proposal order.
4. Hand-authored structural proposal rules.
5. Frequency-mined proposal order from exact successful policies.

### Patterns

1. Current pattern portfolio and maximum.
2. Systematic small-pattern enumeration.
3. Smallest-first.
4. Coverage-first.
5. CEGAR-style pattern generation.
6. Hill-climbing pattern selection.
7. Exact online saturated cost-partition ordering where applicable.

Classical models must be taken seriously. Recent planning research found that carefully constructed graph/WL features with classical ML could use far fewer parameters and train much faster than deep alternatives while remaining competitive on several planning benchmarks. That does not establish the same result for `poecraft2`, but it makes “GBDT or WL-style features first” a strong experimental default. ([AAAI Publications][2])

## 8.2 First neural model

The first neural candidate should be a shallow permutation-invariant set model:

```text
global context encoder
carrier token encoder
candidate action/obligation token encoder
set aggregation or cross-attention
per-candidate score and resource heads
```

A DeepSets-style model is sufficient as the first neural test. A small set transformer is the next step only when interactions between candidate actions materially improve validation results.

## 8.3 Conditions justifying a graph neural model

A GNN qualifies only when all of the following hold:

1. The relevant signal demonstrably depends on typed relations that flat and set features cannot capture.
2. The graph has stable engine-defined node and edge types.
3. A graph ablation beats the best GBDT and set model on held-out strata.
4. Inference overhead remains within the systems gate.
5. An expressiveness audit finds no common collision where graph-indistinguishable inputs require materially different schedules.
6. Embeddings are never used as state-equivalence authority.

Potential nodes:

```text
global request
carrier
goal slot
prefix side
suffix side
action
obligation
proof pattern
```

Potential exact edges:

```text
carrier satisfies goal
carrier blocked by goal
action preserves/destroys/creates goal
action affects side
obligation sourced from cell
obligation depends on generation
pattern observes coordinate
```

Do not insert learned transition edges.

The GNN expressiveness literature is directly cautionary: symmetry can improve generalization, but common message-passing GNNs cannot distinguish some logically equivalent graph structures, and planning encodings are unsafe when such indistinguishable states require different actions. ([AAAI Publications][3])

**Proposed GNN gate:** at least another 10% reduction in deterministic work to the same milestone over the best non-neural or set model, with no safety regression and with inference below the declared overhead limits.

---

# 9. Batched inference architecture

## 9.1 Request boundary

Inference occurs only at exact scheduling boundaries:

```text
carrier epoch freeze
focused fringe formation
action-list formation for one carrier
pending-obligation chunk formation
pattern proposal round
option proposal round
experiment-queue refill
```

Do not call Python or a GPU once per action.

## 9.2 Request structure

```text
GuidanceRequest {
    request_kind;
    model_sha256;
    feature_schema_id;
    exact_snapshot_identity;
    generation_tuple;
    global_features;
    candidate_features[];
    candidate_stable_ids[];
}
```

One output score is required for every candidate. Missing, duplicated, nonfinite, or foreign candidate IDs invalidate the entire response and trigger deterministic fallback.

## 9.3 Batching

Batch by:

```text
model_sha256
feature_schema_id
request_kind
shape bucket
```

Candidate lists can be padded with explicit masks or processed as ragged sets. Batching may combine concurrent corpus runs, but not merge their contexts.

Recommended progression:

1. Small in-process CPU GBDT or linear model.
2. Batched CPU set model.
3. GPU only after measured queue depth makes transfer and synchronization cheaper than CPU inference.

## 9.4 Staleness and caching

Cache key:

```text
model_sha256
feature_schema_id
request_kind
case/request identities
source and dependency generations
candidate-set sha256
```

Before applying a response, recheck:

* every generation;
* candidate-set hash;
* model/schema identity;
* finite outputs.

A stale response is discarded without changing scheduler state.

## 9.5 Deterministic replay

Two modes should exist:

* **deterministic benchmark mode:** fixed batch construction and stable request ordering;
* **throughput mode:** dynamic batching, but batch composition and returned scores are logged so a run can be replayed.

Arrival time or thread scheduling must not become undocumented search authority.

## 9.6 Failure behavior

On model load failure, timeout, OOM, schema mismatch, stale output, NaN, or queue overflow:

```text
use current deterministic solver ordering
record fallback reason
continue exact search
```

Inference memory and latency must be separately charged and reported.

---

# 10. Anti-starvation and completeness rule

The learned scheduler needs a stronger rule than the current starvation telemetry.

## 10.1 Hard rule

For every scheduling layer:

1. **No candidate removal.** The learned model receives and returns scores for the complete exact candidate set.
2. **Mandatory lane service.** Legacy-fairness and exact-closure lanes retain nonzero service when available.
3. **Forced overdue lane.** When a lane’s wait reaches the profile’s starvation threshold, it is selected before any learned preference.
4. **Goal-subset round-robin remains outer authority.** The model ranks within a bucket; it does not rank one goal-subset bucket out of existence.
5. **Oldest-first override within a bucket.** Once candidate age reaches the declared bound, age precedes learned score.
6. **Exact fallback remains.** Focused mode’s “all discovered unexpanded states” fallback and incremental mode’s full closure scan remain unchanged.
7. **Inference failure uses the baseline order.**

A useful ordering tuple is:

```text
(
    forced_overdue,               descending
    fair_epoch_ordinal,           ascending
    goal_subset_round_robin_slot, ascending
    model_score,                  descending
    current_exact_structural_key,
    stable_state_or_operator_id
)
```

For strict obligations:

```text
(
    forced_overdue,
    model_score,
    existing exact optimistic-lower scheduling key,
    source_cell_id,
    action_id,
    stable obligation identity
)
```

The model score must reside in the sidecar, not the obligation identity.

## 10.2 Completeness argument

For a finite frozen epoch:

* all exact candidates remain in append-only queues or ledgers;
* each nonempty lane has a finite maximum wait;
* each goal-subset bucket advances round-robin;
* every candidate eventually reaches the age override;
* exact closure and frontier rules are unchanged.

Therefore learned ranking does not remove work required by the existing completion argument.

This preserves the solver’s existing assumptions. It is **not** a new general theorem for indefinitely growing action generation, and it is not by itself a properness or cyclic-SSP proof.

A qualifying benchmark must report:

```text
maximum wait by lane
forced-service count
forced-service fraction
maximum candidate age
unserved candidates at termination
```

A high forced-service fraction is evidence that the model is attempting to starve difficult work even if the wrapper prevents unsoundness.

---

# 11. Research tracks

## 11.1 Track A — carrier/action/obligation scheduling

**Hypothesis:** structural carrier and action projections plus global resource state can predict which exact work most cheaply reaches the next certified bound or verified upper milestone.

Start with:

* pairwise GBDT ranker;
* separate work, memory, and certification heads;
* carrier/action ranking within current fairness structure;
* PDR obligation sidecar ranking only after the carrier/action stage qualifies.

Do not initially learn lane quotas. Fixed hard quotas reduce confounding.

## 11.2 Track B — run-time and memory prediction

Targets:

```text
work for next service
incremental bytes for next service
peak bytes before next milestone
probability of memory stop before milestone
work/time to first verified upper
work/time to lower threshold
work/time to exact closure
```

Use quantiles rather than only means. Scheduling can then prefer a candidate with slightly lower expected utility but substantially lower upper-tail memory risk.

A possible scheduling-only utility is:

```text
predicted probability of milestone before cap
------------------------------------------------
predicted work + λ * predicted memory pressure
```

This value must never be reported as a solver lower, upper, or gap.

The PDR memory categories and obligation counts make this track particularly relevant: its ordinary witness stopped at about 1.179 GB native peak with roughly 846.8 MB in proof store plus quotient.

## 11.3 Track C — experiment and curriculum selection

The experiment selector should optimize **information and semantic coverage**, not merely choose cases the current model is likely to solve.

Inputs:

```text
generator stratum
goal/action feature coverage
current model uncertainty
predicted runtime/memory distribution
prior failures
distance from already run cases
whether the case discriminates current candidates
```

Maintain a fixed fraction of:

* uniform anchor cases;
* boundary cases around current capability;
* deliberately hard cases;
* underrepresented semantic strata.

Autoscale’s useful transferable idea is to select instances of discriminating difficulty while reducing dependence on a single planner’s strengths. The `poecraft2` version should use whole semantic strata and preserve fixed anchor cases. ([Edoc][4])

Frozen-test cases are never selected adaptively.

## 11.4 Track D — verified options and subgoals

The model proposes from a closed grammar:

```text
SubgoalProposal {
    required_goal_mask;
    required_preservation_mask;
    allowed_destruction_mask;
    required_protection;
    capacity_constraints;
    debt_constraints;
    option_library_id;
    horizon_class;
}
```

The model can propose several horizons and diverse subgoals. The engine rejects proposals that are:

* inapplicable;
* unreachable under the exact option grammar;
* unable to conserve probability mass;
* dependent on missing prices;
* improper;
* not representable by the compiler.

The primitive action path and full exact scheduler remain present, so a bad subgoal cannot destroy completeness.

The learned-subgoal literature supports generating diverse, achievable, closer subgoals and using verification to discard unreachable proposals. kSubS learns diverse high-level subgoals; AdaSubS varies horizon and filters unreachable subgoals; Hybrid Search restores completeness in its discrete deterministic setting by retaining low-level actions. ([NeurIPS Proceedings][5])

For `poecraft2`, “verification” must mean exact engine materialization and proper executable-policy evaluation—not another learned reachability classifier.

## 11.5 Track E — by-construction admissible pattern generation

The model emits a **finite pattern grammar object**, never a value or arbitrary program:

```text
PatternSpec {
    pattern_kind;
    selected exact projection coordinates;
    covered action-shape family;
    exact fallback kind;
    maximum abstract-state budget;
    cost-partition slot, optional;
}
```

Construction pipeline:

1. Validate that every requested coordinate is part of an approved exact projection.
2. Build the abstract stochastic model from authoritative exact transitions and prices.
3. Assign zero or an existing proved fallback to unknown/open coverage.
4. Solve for a monotone subsolution or exact supported pattern.
5. Validate:

   * finite and nonnegative;
   * goal values zero;
   * exact probability accounting;
   * `v(s) ≤ min_a[c(a)+ΣP(s'|s,a)v(s')]` for represented authority;
   * complete provenance and coverage.
6. Wrap the result in a typed proof contribution.
7. Combine with existing patterns using the proof manager.

Memory interruption, convergence failure, missing fallback, or incomplete coverage makes the pattern unavailable. It must not weaken or contaminate the existing public lower.

The June 2026 LLM-evolved-pattern preprint is relevant because it learns programs that construct abstractions rather than learning heuristic values directly, then obtains admissibility through exact pattern construction and saturated cost partitioning. It is, however, a deterministic classical-planning result, so only the by-construction architecture should be transferred. ([arXiv][6])

Probabilistic-planning research already supplies stronger domain-relevant foundations: SSP pattern databases, probabilistic pattern selection using hill climbing and CEGAR adaptations, and compositional SSP merge-and-shrink theory. ([AAAI Publications][7])

## 11.6 Track F — valid cost-partition prediction

The current proof manager combines independently admissible patterns by **maximum**, not by an arbitrary learned sum. A cost-partitioned sum therefore requires a new exact typed authority before it can enter the live lower.

### CP0 — ordering only

The model predicts:

* pattern construction order;
* abstraction order for saturated cost partitioning;
* likely useful columns or partitions.

The exact cost-partition algorithm computes all allocations.

### CP1 — constrained allocation proposal

The model emits logits or priorities. An exact constructor maps them to nonnegative allocations satisfying, for every authoritative cost key:

```text
allocated_cost(pattern_1, key)
+ ...
+ allocated_cost(pattern_n, key)
+ residual_cost(key)
= authoritative_cost(key)

allocated_cost >= 0
residual_cost >= 0
```

Every pattern is recomputed under its allocated costs. The model’s raw outputs never enter the lower.

### CP2 — exact optimizer warm start

The model proposes:

* an initial feasible partition;
* LP basis or column order;
* abstraction ordering;
* candidate columns.

An exact SSP cost-partition optimizer proves feasibility and computes or verifies the resulting partition. Failure falls back to the existing independent-pattern maximum.

A future `CostPartitionCertificate` should bind:

```text
price/economy identity
action-vocabulary identity
pattern IDs and versions
projection identities
allocation and residual hashes
per-cost-key feasibility witnesses
abstract-solution provenance
validation result
```

SSP-specific cost-partition theory exists and shows how multiple admissible abstraction heuristics can be combined while preserving admissibility in stochastic shortest-path problems. Classical online saturated and decomposition methods are useful algorithmic references, but the SSP formulation must own live proof authority. ([AAAI Publications][8])

---

# 12. Benchmark and ablation matrix

## 12.1 Common milestone definitions

Every comparison uses one of four predeclared events:

```text
E_L(L*)   public admissible lower first reaches L*
E_U(U*)   independently verified executable upper first reaches U* or better
E_G(g*)   certified normalized gap first reaches g*
E_X       exact closure with proper compiled policy
```

The same numerical target and same authority class must be used for both baseline and candidate.

## 12.2 Fixed anchor suite

| Suite                            | Anchor event                                                                      |
| -------------------------------- | --------------------------------------------------------------------------------- |
| Three-prefix exact same-side     | `E_X`, exact cost `1618.2138946963837`                                            |
| Three-suffix exact same-side     | `E_X`, exact cost `1101.15648683309`                                              |
| Five-T1 bounded carrier ladder   | `E_U(87361.1690420501)` and `E_L(36.4286171890906)`                               |
| PDR four-mod, 300 MiB diagnostic | Same verified-upper and certified-lower events as the ordinary witness            |
| PDR four-mod, 1 GiB              | `E_U(7866.432124027084)`, `E_L(21.772459401271156)`, memory-stop behavior         |
| PDR watchdog                     | 300-second and 900-second trajectories                                            |
| Development corpus               | Whole development strata                                                          |
| Validation corpus                | Whole validation strata                                                           |
| Frozen corpus                    | Sealed whole frozen-test strata                                                   |
| Negative controls                | Open envelope, improper cycle, missing price, stale generation, invalid inference |

The numeric anchors come from the pinned repository’s archive evidence and should be treated as benchmark milestones, not presumed optima for the bounded witnesses.

## 12.3 Matrix A — scheduling models

| ID |        Carrier |         Action |     Obligation |           Resource heads |
| -- | -------------: | -------------: | -------------: | -----------------------: |
| A0 |        Current |        Current |        Current |                       No |
| A1 |    FIFO/oldest |        Current |        Current |                       No |
| A2 |  Stable random |  Stable random |  Stable random |                       No |
| A3 |         Linear |         Linear |        Current |                       No |
| A4 |  GBDT/listwise |        Current |        Current |                       No |
| A5 |  GBDT/listwise |  GBDT/listwise |        Current |                       No |
| A6 |  GBDT/listwise |  GBDT/listwise |  GBDT/listwise |                       No |
| A7 |  GBDT/listwise |  GBDT/listwise |  GBDT/listwise |                      Yes |
| A8 | Best set model | Best set model | Best set model |                      Yes |
| A9 |            GNN |            GNN |            GNN | Yes; only after its gate |

Stable-random A2 uses at least five predeclared seeds. All learned configurations use the hard anti-starvation wrapper.

## 12.4 Matrix B — feature and label ablations

Starting from the best non-neural model:

1. Remove carrier structure.
2. Remove action effect projection.
3. Remove global search state.
4. Remove age and fairness state.
5. Remove resource telemetry.
6. Remove exact lower/upper context.
7. Remove opaque stable IDs.
8. Pointwise versus pairwise versus listwise rank loss.
9. Observational labels versus paired counterfactual labels.
10. Completed-only regression versus censor-aware survival heads.
11. Work-only versus work-plus-memory objective.
12. One shared model versus separate carrier/action/obligation models.

The “no exact lower/upper context” ablation is mandatory to determine whether the ranker learns useful structural guidance or merely keys off current gap magnitude.

## 12.5 Matrix C — batching and systems

```text
batch size:          1, 8, 32, 128 candidates
execution:           in-process CPU, batched CPU, GPU where justified
cache:               off/on
model precision:     full precision versus validated reduced precision
batch formation:     deterministic versus recorded dynamic
```

Report:

* requests per solver work unit;
* candidates per inference;
* median and p99 inference latency;
* inference fraction of wall time;
* inference-owned live and peak bytes;
* stale-response rate;
* fallback rate;
* cache hit rate.

## 12.6 Matrix D — options and subgoals

| ID | Proposal source                               | Verification                            | Primitive fallback   |
| -- | --------------------------------------------- | --------------------------------------- | -------------------- |
| O0 | Existing automatic system                     | Existing exact pipeline                 | Yes                  |
| O1 | Stable random grammar proposals               | Exact                                   | Yes                  |
| O2 | Hand-authored structural rules                | Exact                                   | Yes                  |
| O3 | Frequency-mined proposals                     | Exact                                   | Yes                  |
| O4 | Learned flat/set proposer                     | Exact                                   | Yes                  |
| O5 | Learned graph proposer                        | Exact                                   | Yes; only after gate |
| O6 | Learned proposals without exhaustive fallback | **Unsafe control only; cannot qualify** | No                   |

Metrics use work and memory to the same independently verified upper. Proposal acceptance rate alone is not a success metric.

## 12.7 Matrix E — proof patterns and cost partitioning

| ID | Pattern source                                   | Combination                         |
| -- | ------------------------------------------------ | ----------------------------------- |
| P0 | Current patterns                                 | Existing maximum                    |
| P1 | Systematic small patterns                        | Maximum                             |
| P2 | Hill climbing                                    | Maximum                             |
| P3 | CEGAR-style                                      | Maximum                             |
| P4 | Learned closed-grammar proposals                 | Maximum                             |
| P5 | Learned ordering                                 | Exact online saturated partition    |
| P6 | Learned constrained allocation seed              | Exact feasibility and recomputation |
| P7 | Learned LP/basis/column warm start               | Exact SSP optimizer                 |
| P8 | Raw learned pattern value or raw learned weights | **Rejected**                        |

Measure total construction cost as part of work-to-bound. A stronger lower that takes more work to build is not automatically a win.

## 12.8 Fairness ablation

The hard anti-starvation rule may be disabled only in a bounded, non-authoritative diagnostic to measure what it prevents. Results without the rule cannot qualify for exact or production use.

---

# 13. Success metrics

## 13.1 Primary paired metrics

At the same milestone:

```text
deterministic work ratio
expanded-state ratio
row ratio
transition ratio
logical-reforge-work ratio
strict-obligation work ratio

live bytes at event
peak bytes to event
proof-store-plus-quotient bytes at event

maximum cooperative-step latency
wall time on pinned hardware
probability of reaching event before cap
```

Do not collapse different work counters into one undocumented weighted score. The repository’s benchmarking contract already treats deterministic work as the first search-envelope comparison and wall time/memory as hardware- and implementation-sensitive evidence.

## 13.2 Safety metrics

Required:

```text
0 learned transition probabilities
0 learned state merges
0 model-driven candidate removals
0 invalid lower promotions
0 invalid incumbent prunes
0 improper upper publications
0 cost-reconciliation failures
0 exact-anchor value changes
0 action-envelope closure changes
0 nondeterministic replay mismatches in benchmark mode
```

Every exact anchor must still:

* close with the same exact cost;
* compile;
* exact-evaluate;
* pass its fixed simulator validation;
* have zero off-policy mass and complete pricing.

## 13.3 Initial pre-registered go thresholds

These are proposed project decisions, not literature-derived constants.

### Scheduling stage

Go only if, on validation:

* geometric-mean deterministic work ratio to the same milestone is at most `0.80`;
* paired 95% interval upper bound is below `0.95`;
* peak-memory ratio is at most `1.05`;
* inference consumes at most 5% of solve wall time;
* inference-owned memory is at most 2% of the active solver memory budget;
* no semantic stratum regresses by more than 25% without an explicitly accepted tradeoff;
* all safety metrics remain zero.

### Resource predictor

Go only if:

* selected upper memory quantiles have acceptable held-out calibration;
* cap-risk calibration error is at most 0.05;
* using the predictor reduces cap failures or work-to-milestone without worsening safety;
* it does not systematically avoid hard but necessary exact work.

### Options

Go only if:

* work to the same verified upper improves by at least 15% on the affected validation subset;
* the number of cases obtaining a verified upper does not decrease;
* no option reaches publication without exact materialization, properness, compilation, and reconciliation.

### Pattern/cost-partition stage

Go only if:

* public lowers never weaken at an equal solver snapshot;
* total work to a shared lower milestone improves;
* construction work is included;
* every cost partition has a complete exact certificate;
* failed construction falls back exactly to the existing lower portfolio.

### GNN

Go only if it provides at least another 10% deterministic-work improvement over the best GBDT or set model at the same safety and systems limits.

---

# 14. Explicitly rejected unsafe proposals

| Proposal                                                        | Rejection reason                                                                     |
| --------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| Direct neural estimate used as public lower                     | No admissibility proof                                                               |
| “Calibrated” or low-quantile model value used as lower          | Statistical conservatism is not pointwise admissibility                              |
| Learned value used for incumbent pruning                        | Can remove the optimal action or an unresolved obligation                            |
| Learned score marking an action noncompetitive                  | Lifecycle transitions require exact lower/upper evidence                             |
| Embedding similarity used for state equivalence                 | No exact behavioral-equivalence proof                                                |
| Learned transition model                                        | Duplicates and can disagree with authoritative mechanics and probability mass        |
| Model-generated probability normalization                       | Approximate normalization does not prove the exact kernel                            |
| Direct model publication of a strategy                          | Does not establish properness, exact cost, complete routing, or off-policy mass zero |
| Hard top-k action or subgoal filtering                          | Can starve or remove required exact work                                             |
| Learned option represented by a coarse transition kernel        | Repeats the probability-mass failure of the removed carrier planner                  |
| Learned carrier MDP used as an upper                            | A coarse optimistic composition is not an executable proper strategy                 |
| Unconstrained cost-partition weights                            | May allocate more cost than exists or double-count cost                              |
| Clipping invalid weights after inference                        | Clipping is not a complete cost-partition certificate                                |
| Learned pattern value with a post-hoc Bellman sample check      | Sampling rows is not universal subsolution validation                                |
| Arbitrary model-generated runtime code for patterns             | Escapes the finite reviewed grammar and proof boundary                               |
| Putting model score in current obligation `scheduling_priority` | Changes canonical obligation identity and proof-store hashing                        |
| Treating OOM/crash as censored success                          | Violates the benchmark failure contract                                              |
| Treating cap time as true eventual solve time                   | Produces biased runtime labels                                                       |
| Random row/state split                                          | Leaks the same run, case, and goal structure across roles                            |
| Using known archive witnesses as frozen test                    | They have already shaped the design                                                  |
| Counterfactual labels from the current PDR coarse checkpoint    | Replay is not behaviorally faithful                                                  |
| Model-only stopping or exactness declaration                    | Resource prediction does not close frontiers, envelopes, or Bellman obligations      |
| Dynamic inference arrival order without recorded replay         | Makes undocumented thread timing search authority                                    |

---

# 15. Staged go/no-go plan

## Stage 0 — contract and data audit

Deliver:

* feature and label schemas;
* provenance manifests;
* status/censoring mapping;
* exact list of forbidden data flows;
* candidate-membership and fail-open tests;
* faithful-snapshot requirements.

**Go:** schemas can be reproduced byte-for-byte from identical runs.
**No-go:** any output can flow into proof, prune, equivalence, probability, or publication authority.

## Stage 1 — non-learned baselines and offline replay

Run:

* current ordering;
* FIFO;
* round-robin;
* stable random;
* linear and GBDT baselines;
* basic runtime/memory baselines.

No live solver decisions yet.

**Go:** offline features reproduce source candidates and exact stable ordering; labels have no split leakage.
**No-go:** observations cannot distinguish open/capped/failed cases or lack required provenance.

## Stage 2 — shadow inference

Run models during real solves but discard their ordering.

Measure:

* inference overhead;
* score determinism;
* cache behavior;
* schema failures;
* staleness;
* memory use;
* calibration.

**Go:** zero behavioral difference from baseline and overhead below the systems gate.
**No-go:** model execution changes solver state, candidate enumeration, or completion status.

## Stage 3 — carrier and action ranking

Enable learned ranking within existing goal-subset buckets and action lists. Keep lane quotas, closure service, and all proof ordering unchanged.

**Go:** validation scheduling threshold passes with zero safety failures.
**No-go:** improvement comes mainly from starving buckets or from unstable candidate IDs.

## Stage 4 — strict-obligation scheduling

Add the external `GuidanceOverlay` and hard age override. Do not alter canonical obligation identity.

Primary target: PDR work and proof memory to the shared lower and verified-upper milestones.

**Go:** same exact obligations and accounting, lower work/memory to milestone, bounded maximum wait.
**No-go:** score enters semantic hashing, stale generations are applied, or forced-service rate indicates persistent attempted starvation.

## Stage 5 — resource-aware lane and experiment selection

Use resource quantiles to choose among optional service and development experiments. Exact-closure and fairness tickets remain hard.

**Go:** fewer resource failures and lower work/memory without reduced milestone coverage.
**No-go:** selector avoids hard strata, overfits machine-specific wall time, or worsens frozen semantic coverage.

## Stage 6 — verified option/subgoal proposals

Introduce a finite engine-owned proposal grammar. Model ranks proposals; exact materialization and publication remain unchanged.

**Go:** measurable improvement to the same verified upper with complete probability mass.
**No-go:** any proposal needs an approximate option kernel, unverified reachability, or incomplete failure routing.

## Stage 7 — by-construction pattern proposal

Model proposes finite pattern specifications. Exact engine construction and subsolution validation own all lower values.

**Go:** work to the same public lower improves after including pattern construction cost.
**No-go:** generated pattern requires unknown fallback behavior or incomplete action coverage.

## Stage 8 — exact cost partitioning

Progress through CP0 ordering, CP1 constrained feasible seeds, and CP2 exact optimizer warm starts.

**Go:** typed certificate, exact per-cost-key feasibility, and validation under authoritative SSP costs.
**No-go:** raw model allocations enter lower arithmetic.

## Stage 9 — set and graph models

Compare DeepSets/set transformer and, only if justified, a typed GNN.

**Go:** incremental held-out gain beyond the best non-neural model and expressiveness audit passes.
**No-go:** gain disappears after stable-ID ablation, inference dominates work, or graph collisions demand different schedules.

## Stage 10 — sealed frozen test

Select one candidate before accessing frozen outcomes. Run:

* semantic invariance suite;
* anchor suite;
* frozen corpus;
* OOD slices;
* fixed resource caps;
* exact evaluation and simulator gates.

A frozen-test failure returns the program to development with a new model and a new future frozen test. The failed test is never reused as unseen evidence.

---

# 16. Literature interpretation and the cyclic-SSP caveat

Policy-guided heuristic search shows that policies can reduce search effort while a conventional search framework retains guarantees, but its stated guarantees concern single-agent deterministic search. ([AAAI Publications][9])

Learned subgoal work shows that high-level proposals can greatly reduce search, while Hybrid Search demonstrates the important architectural pattern of retaining low-level actions as a completeness fallback in discrete deterministic planning. ([NeurIPS Proceedings][5])

The 2026 learned pattern-generator preprint is especially aligned with the safe lower-bound direction because it learns abstraction constructors rather than heuristic values and obtains admissibility by construction. Its theorem and experiments are still for optimal classical planning. ([arXiv][6])

For stochastic shortest paths, the more directly relevant foundations are SSP pattern databases, probabilistic pattern selection, SSP cost partitioning, merge-and-shrink theory, and exact constraint generation that avoids unnecessary suboptimal-action work while retaining an exact SSP algorithm. ([AAAI Publications][7])

**Deterministic-search guarantees do not automatically transfer to `poecraft2`.** A cyclic SSP adds requirements absent from ordinary deterministic tree or graph search:

* exact probability mass;
* positive-probability cycles;
* properness;
* exclusion of closed non-goal recurrent classes;
* complete Bellman action coverage;
* open action-envelope and frontier accounting;
* source/target generation validity;
* exact compiled-policy evaluation.

The safe transfer from deterministic learned-search literature is therefore:

```text
learn proposals and work order
+ preserve exhaustive exact fallback
+ verify every authoritative result in the native solver
```

It is not:

```text
reuse an A* or deterministic completeness theorem
as proof that a learned cyclic stochastic policy is proper or exact
```

The repository’s own proper-policy, open-frontier, lower-authority, and upper-publication contracts remain stronger and controlling.

## Final decision

Proceed with a learned-guidance program, but define its first deliverable narrowly:

> **A deterministic, fail-open, resource-aware ranker for current exact carrier, action, and strict-obligation queues, with hard anti-starvation and event-normalized benchmarks to the same certified lower or independently verified upper.**

Options, generated proof patterns, and cost partitioning should remain later stages. A set or graph neural model is not justified until a GBDT/listwise baseline has been shown insufficient on leakage-safe held-out strata.

[1]: https://ojs.aaai.org/index.php/AAAI/article/view/21279 "https://ojs.aaai.org/index.php/AAAI/article/view/21279"
[2]: https://ojs.aaai.org/index.php/ICAPS/article/view/31462?utm_source=chatgpt.com "Return to Tradition: Learning Reliable Heuristics with Classical Machine Learning | Proceedings of the International Conference on Automated Planning and Scheduling"
[3]: https://ojs.aaai.org/index.php/ICAPS/article/view/31486 "https://ojs.aaai.org/index.php/ICAPS/article/view/31486"
[4]: https://edoc.unibas.ch/entities/publication/79b59cc3-62a0-4a0e-ba12-0c54dc45f6c3 "https://edoc.unibas.ch/entities/publication/79b59cc3-62a0-4a0e-ba12-0c54dc45f6c3"
[5]: https://proceedings.neurips.cc/paper_files/paper/2021/hash/05d8cccb5f47e5072f0a05b5f514941a-Abstract.html "https://proceedings.neurips.cc/paper_files/paper/2021/hash/05d8cccb5f47e5072f0a05b5f514941a-Abstract.html"
[6]: https://arxiv.org/abs/2606.02438 "https://arxiv.org/abs/2606.02438"
[7]: https://ojs.aaai.org/index.php/SOCS/article/view/18561 "https://ojs.aaai.org/index.php/SOCS/article/view/18561"
[8]: https://ojs.aaai.org/index.php/ICAPS/article/view/19802 "https://ojs.aaai.org/index.php/ICAPS/article/view/19802"
[9]: https://ojs.aaai.org/index.php/AAAI/article/view/17469 "https://ojs.aaai.org/index.php/AAAI/article/view/17469"
