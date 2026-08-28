# Integrated conclusion from the five reports

The reports converge on a much sharper program than the original “native version + subgoals + maybe neural networks” framing:

> **Build a native research control plane, then improve bounded policies through verified executable graph fragments. Treat replay, proof-memory work, and retention-aware lower bounds as a parallel exactness track. Add learned guidance only as a fail-open ordering and proposal layer.**

That ordering differs from the archaeology report’s PDR-first ranking because your stated objective is now **generally better 4–5 modifier bounded policies**, with PDR serving as the main test rather than the sole product target.

The strongest consensus findings are:

* The native execution substrate already exists. The missing piece is an experiment-facing control plane, not another solver runner.
* A subgoal system is worth reopening, but only as an **exact executable graph-fragment system**. The previous coarse carrier planner failed because its attractive abstract estimates could not conserve probability mass when compiled.
* Retention-aware lower bounds should be a small, proof-only portfolio with action-local precision—not a giant exact carrier abstraction or another goal-mask table.
* The first learned system should rank existing exact work and predict resource usage. It should not be a value network, action filter, learned transition model, or end-to-end policy generator.
* PDR’s immediate exactness wall has moved. It is now retained strict proof/quotient memory, rather than the old Chaos graph, global Fracture observation, long atomic rows, or repeated strict-session reconstruction.

---

# Important reconciliation decisions

## 1. Replay should not block policy-quality work

The archaeology ranks scheduler-aware replay first because it is optimizing the path to understanding the current PDR proof-memory stop.

That is correct **for the exactness track**, but not for your whole project.

The following can proceed without faithful PDR replay:

* persistent experiment queue;
* GUI and LLM control surface;
* general 4–5 mod corpus;
* verified option-fragment infrastructure;
* exact fragment properness and mass checks;
* meta-policy flattening;
* ordinary fresh-run comparisons;
* non-neural scheduling baselines;
* shadow model inference.

Faithful replay becomes mandatory for:

* PDR proof-memory attribution;
* counterfactual scheduling labels from the exact same live snapshot;
* pause-and-resume of running hard solves;
* strict-obligation scheduling experiments that require identical prefixes.

So replay should be a **parallel enabling track**, not the gate before all upper-bound research.

## 2. The GUI belongs in the first usable Lab, but not as an authority layer

The Native Solver Lab report deliberately deferred a polished GUI and focused on CLI/JSON.

Your requirement changes that boundary. The correct compromise is:

* stable service/API and job model first;
* a thin practical desktop GUI immediately on top;
* no rich native Calculator replacement;
* no second mechanics implementation;
* no GUI-owned queue or solver state;
* no requirement for the LLM to click GUI controls.

The GUI, CLI, and LLM tools must all call the same typed control operations.

## 3. The lower-bound report contains a baseline inconsistency

Four reports identify the authoritative fixed PDR boundary as approximately:

```text
verified upper: 7866.432124027084
certified lower: 21.772459401271156
strict work: 3,507,568
proof store + quotient: 846,846,750 bytes
native peak: 1,179,431,999 bytes
stop: max_solver_owned_bytes
```

For example, the option report uses those values in its PDR safety gate.  The replay report gives the same boundary.

The RCASSP report’s production acceptance section instead lists an upper of `460678.970156889` and a slightly different lower.

The uploaded material does not explain whether that is:

* a different run/profile;
* an earlier incumbent;
* a stale copied baseline;
* or a transcription error.

Therefore, **none of the RCASSP numeric acceptance thresholds should be activated until one fresh baseline matrix pins the exact request, executable, profile, economy, and action scope**. The RCASSP architecture itself is still useful.

---

# Unified architecture

```text
                           ┌────────────────────┐
                           │ Practical desktop  │
                           │ GUI                │
                           └─────────┬──────────┘
                                     │
┌──────────────────┐       ┌────────▼─────────┐       ┌──────────────────┐
│ Typed LLM tools  ├──────►│ Solver Lab       │◄──────┤ JSON CLI         │
│ / MCP adapter    │       │ control service  │       │                  │
└──────────────────┘       └────────┬─────────┘       └──────────────────┘
                                    │
                         ┌──────────▼───────────┐
                         │ SQLite experiment    │
                         │ and attempt catalog  │
                         └──────────┬───────────┘
                                    │
                      one isolated process per solve
                                    │
                     ┌──────────────▼──────────────┐
                     │ Existing native benchmark   │
                     │ and production C++ engine   │
                     └──────────────┬──────────────┘
                                    │
             exact mechanics / solver / compilation / evaluation
```

The Lab should extend the current runner and benchmark. The existing architecture already supports isolated native subprocesses, memory reservations, watchdog cleanup, partial trajectories, provenance, and resumable completed-case ledgers.

The control service owns:

* queue state;
* experiment matrices;
* priorities and retries;
* process supervision;
* host-memory admission;
* artifact indexing;
* GUI/CLI/LLM operations;
* optional model inference.

The native benchmark and engine remain the owners of:

* case validation;
* exact mechanics;
* action legality;
* transition probabilities;
* Bellman values;
* proof bounds;
* strategy compilation;
* properness;
* independent exact evaluation.

That ownership separation is already laid out cleanly in the Lab report.

---

# Workstream A — Native Solver Lab and benchmark foundation

This is the first implementation boundary because every later research track benefits from it.

## Locked research profile

Create one explicit profile along the lines of:

```text
native_allflame_no_imprint_v1
```

It should bind:

```text
economy:
    fixed Allflame identity
    no automatic refresh

action scope:
    generated Imprint programs disabled
    voluntary economic Restart controlled explicitly
    other Calculator product scope inherited deliberately

execution:
    native only
    exact strategy evaluation enabled
    process isolation enabled
```

Imprint should be absent from:

* initial option generation;
* benchmark action scopes;
* training examples for initial models;
* learned candidate vocabularies;
* automatic dependency expansion.

Do not delete Imprint code. Preserve it as a disabled family whose later reintroduction changes the caller-scope identity and invalidates incompatible option/model caches.

## Persistent catalog

The Lab report’s proposed SQLite design is sound:

* immutable experiments;
* immutable jobs;
* append-only attempts;
* attempt-specific artifacts;
* persistent controls;
* supervisor leases and orphan recovery;
* no benchmark process writing directly to SQLite.

A retry must create a new attempt rather than overwrite the prior report or strategy.

## Minimum useful GUI

The first GUI should have five dense, practical screens:

### Queue

Show:

* queued, blocked, running, canceled, failed, and completed jobs;
* priority;
* declared solver cap;
* host reservation;
* live memory;
* elapsed active time;
* last lower and verified upper;
* phase and termination state.

Actions:

```text
submit
clone
change priority
pause queued work
cancel
retry
open attempt
```

Running pause should remain unavailable until scheduler-aware checkpoint parity exists. Before then, “pause” can stop new dispatch but cannot truthfully mean a resumable running solve. The Lab report makes this distinction explicitly.

### Run detail

Show:

```text
certified lower
verified executable upper
candidate estimates separately
exact evaluated policy cost
absolute and multiplicative gap
states / rows / transitions
carriers / obligations
logical work
live and peak memory
phase and stop owner
bound trajectory
logs and partial artifacts
```

### Compare

Compare two or more attempts on:

```text
request identity differences
time/work to first verified upper
best verified upper at fixed horizons
certified lower trajectory
state/row/transition work
peak memory
policy graph size
action-family composition
termination reason
```

### Strategy

Show:

* graph size;
* exact evaluated cost;
* expected resource vector;
* action-family counts;
* properness;
* success and off-policy mass;
* route failures;
* strategy artifact identity.

### Experiment matrix

Create a cross-product over:

* cases;
* executables;
* profiles;
* solver caps;
* watchdogs;
* scheduling variants;
* option/model variants;
* replicates.

The matrix must expand deterministically into immutable jobs.

## LLM interface

The LLM should not receive arbitrary shell access. It should get typed solver operations such as:

```text
list_cases
get_case
submit_job
submit_matrix
list_jobs
get_job
cancel_job
retry_job
change_priority
get_run_summary
get_bound_trace
compare_runs
get_strategy_summary
evaluate_strategy
export_investigation_bundle
```

Mutating operations should require:

* an idempotency key;
* explicit immutable request content;
* optional dry-run output;
* requester identity;
* an audit record.

The “investigation bundle” is especially useful. It should package a compact structured summary instead of forcing an LLM to consume megabytes of telemetry:

```text
request identity
profile and action scope
terminal status
bounds and milestones
dominant work owners
dominant memory owners
policy summary
action-family composition
relevant warnings
artifact IDs
```

## Benchmark corpus

The PDR witness remains an anchor, but the acceptance corpus must prevent another target-case-only scheduling success.

Initial corpus design:

* four- and five-requirement goals;
* 3+1, 2+2, and 3+2 side splits;
* clean starts;
* one- and two-goal partial starts;
* dirty opposite-side starts;
* fractured progress;
* protection-heavy paths;
* cleanup-heavy paths;
* armour and non-armour bases;
* ordinary influence and Eldritch-compatible cases;
* direct multi-goal jump opportunities;
* Imprint disabled throughout;
* the same frozen Allflame economy throughout.

Split whole semantic strata into:

```text
development
validation
frozen_test
```

The learned-guidance report correctly rejects random state or row splits, because they would leak the same goal, run, carrier family, and trajectory across roles.

## Primary bounded-policy metrics

For this program, primary metrics should be:

1. Fraction of cases producing a proper independently evaluated policy.
2. Deterministic work to first verified upper.
3. Best verified upper at predeclared fixed horizons.
4. Ratio of candidate upper to baseline upper at the same horizon.
5. Peak memory to reach that upper.
6. Policy graph size and exact-evaluation work.

Certified lower and gap remain recorded, but they are secondary unless the experiment explicitly targets proof.

The learned report’s event-based comparison framework is appropriate:

```text
E_U(U*) = first independently verified upper at or below U*
E_L(L*) = first public admissible lower at or above L*
E_G(g*) = first certified gap milestone
E_X     = exact closure
```

Comparisons must use the same authority and threshold.

---

# Workstream B — Verified executable graph fragments

This is the highest-priority solver-quality change.

## Core invariant

> **An option proposal may choose what control flow to verify, but it never supplies authoritative probabilities and is never itself the published policy.**

The exact workflow should be:

1. Propose an executable control-flow fragment.
2. Interpret it over exact executable state.
3. Rebuild every primitive action transition from engine mechanics.
4. Verify complete probability mass.
5. Verify option absorption and finite resource use.
6. Compose fragments into a finite meta-controller.
7. Flatten the meta-controller into one ordinary strategy graph.
8. Parse and exact-evaluate that graph independently.
9. Promote it only through the existing incumbent portfolio.

That is the central decision of the option report.

## Do not implement a coarse semi-MDP as upper authority

Exact option exit kernels can be used to estimate and rank meta-policies. They cannot be the thing that executes or publishes.

Flattening should copy:

* primitive operation nodes;
* route nodes;
* observed-choice nodes;
* exact conditions;
* explicit recovery edges.

It should not multiply, rewrite, or read option exit probabilities. The final evaluator reconstructs primitive mechanics from scratch. This makes the historical missing-mass failure structurally unavailable in the flattening step.

## Run in shadow mode first

The first version should be an isolated incumbent generator:

```text
main solver graph and scheduler
        remain unchanged

option lab
        receives copied start/goal/scope/economy
        owns private state and transition caches
        proposes and verifies candidates
        returns only flattened strategy candidates

existing exact evaluator
        evaluates candidate

incumbent portfolio
        decides whether to retain it
```

It must not mutate:

* action-envelope ledger;
* core scheduler;
* main sparse graph;
* proof store;
* public lower;
* action ordering.

The option report recommends exactly this isolation because an earlier planner experiment accidentally grew the core graph and improved a different policy.

## Initial library

Start with concrete parameterized templates:

```text
same_side_from_clean
preserve_side_then_progress
cleanup_for_continuation
fracture_prepare
fracture_attempt
fracture_miss_recovery
economic_restart, only when caller-authorized
renewal_to_anchor
```

The same-side three-prefix and three-suffix exact policies are ideal first oracle fragments, but they establish only those concrete instances—not universal option validity.

Imprint stays entirely out of the initial library.

## Multiple subgoal orders

The meta-controller must be state-conditioned and non-monotone. It cannot be a fixed list such as:

```text
prefix goals
then suffix goals
then cleanup
```

Candidate generation must preserve:

* multiple first subgoals;
* same-side-first and cross-side-first variants;
* preservation-first variants;
* contextual cleanup;
* Fracture before or after partial progress;
* direct multi-goal jumps;
* different continuations after destructive failures;
* recovery cycles.

The option report explicitly rejects requiring 3→4→5 progress when a direct 3→5 transition is possible.

## First acceptance gates

### Gate B0 — Authority firewall

Option estimates, certificates, meta-policy estimates, flattened candidates, and verified incumbents must be separate types. No conversion to a proof lower or public upper is allowed before final exact assertion.

### Gate B1 — Exact leaf verifier

Test:

* deterministic success;
* stochastic success and recovery;
* total probability exactly one;
* missing and duplicate mass;
* observed choices;
* proper retry cycles;
* improper closed SCCs;
* resource reconciliation;
* predicate expressibility;
* explicit failures.

### Gate B2 — Historical mass-loss regression

Create a row such as:

```text
progress              0.50
recoverable failure   0.30
blocked wrong carrier 0.20
```

The verifier must reject:

* dropping the 0.20 branch;
* renormalizing the remaining 0.80;
* merging physically different exits only because their carrier projections match;
* promoting the lower-looking malformed candidate.

The option report gives a detailed regression design.

### Gate B3 — Meta-controller and flattening

Require agreement between:

* compositional option properness;
* embedded meta-chain properness;
* flattened primitive-graph properness;
* independent exact evaluation.

### Gate B4 — General bounded-policy qualification

On validation cases:

* do not reduce the number of cases with a verified upper;
* improve best verified upper on multiple semantic strata;
* include exact option construction and evaluation work in the comparison;
* keep the existing lower and scheduler unchanged in shadow mode.

### Gate B5 — PDR safety

PDR improvement is not required for initial safety qualification.

It must prove that:

* option failure cannot erase or worsen the existing incumbent;
* malformed or capped candidates leave the result unchanged;
* any promoted candidate is proper, completely priced, zero off-policy, and independently exact-evaluated.

### Gate B6 — Five-T1 stretch target

The historical `87361.169...` policy remains valuable evidence but is not a fresh current-source incumbent. The first functional target should be:

> Beat the fresh current verified baseline on several current-semantic cases.

Recovering or beating the historical 87k five-T1 result is a strong stretch qualification rather than the only v1 pass criterion. The archaeology emphasizes that historical strategies are evidence and controls, not fresh candidate sources.

---

# Workstream C — Scheduling and learned guidance

## First reopen deterministic scoped ordering

The previous global carrier comparator had mixed evidence:

* it improved the target five-goal upper by about 9.25×;
* the first control suite was invalid;
* corrected controls exposed significant non-armour regressions and policy loss;
* therefore the global comparator was correctly removed.

A materially different retry should be:

* within existing goal-subset buckets;
* within an already enumerated action list;
* under hard fairness and exact-closure service;
* enabled only for selected treatment classes;
* compared on matched current-semantic controls.

Do not replace the global schedule with one heuristic scalar.

## Data plumbing starts early; live learned control waits

Feature and outcome logging should be designed into the Lab immediately. That does not mean a model should control the solver immediately.

Recommended stages:

1. Record exact candidate sets and decisions.
2. Build current-order, FIFO, stable-random, linear, and tree-based baselines.
3. Run model predictions in shadow mode.
4. Enable carrier ordering within exact buckets.
5. Enable action ordering within exact carrier action sets.
6. Add option-proposal ranking.
7. Add strict-obligation ranking later, after faithful replay and sidecar identity work.
8. Consider neural set models only after tree/listwise baselines plateau.

The learned report’s first recommendation is a resource-aware work-order ranker rather than a value network or policy generator.

## Optimize for your real objective

The initial learned target should emphasize bounded-policy milestones:

```text
work to first verified upper
work to a better verified upper
probability of obtaining a verified policy before cap
peak memory before that upper
option-verification work and success
```

Proof-oriented labels such as obligation retirement and lower milestones remain available, but should be a later head or separate model.

## Hard safety wrapper

A model may return only:

```text
relative priority score
work/memory prediction
proposal ID from a closed engine-owned grammar
uncertainty estimate
```

It may not provide:

* a proof lower;
* a prune;
* an action-envelope status;
* a transition probability;
* state equivalence;
* exact expected cost;
* properness;
* a public upper;
* an exactness declaration.

That boundary is laid out explicitly in the learned report.

The hard fairness rules should include:

* no candidate removal;
* mandatory exact-closure and fairness lanes;
* forced service for overdue work;
* outer goal-subset round-robin retained;
* oldest-first override;
* exact exhaustive fallback;
* deterministic baseline order on inference failure.

The report provides a completeness-preserving wrapper and the telemetry needed to detect attempted starvation.

## Strict obligations need a separate guidance sidecar

At the pinned ref, `scheduling_priority` participates in canonical strict-obligation identity and hashing. A model score must not be written there.

Use an external overlay keyed by:

```text
obligation identity
graph and partition generations
action/admission/price generations
model hash
feature schema
```

The learned report identifies this as a particularly delicate integration point.

## GPU use

GPU availability is useful, but it does not change the first model choice.

Start with:

```text
linear ranker
pairwise/listwise tree model
quantile resource predictor
```

Only move to a batched set model when the best classical baseline is limited by interactions it cannot represent. Model inference should live outside the exact engine and be batched across candidate lists or jobs.

The exact engine should have a deterministic fail-open path when the model:

* times out;
* runs out of GPU memory;
* returns stale generations;
* returns missing candidates;
* produces NaN;
* has a schema mismatch.

---

# Workstream D — PDR replay, memory attribution, and RCASSP

This is the parallel exactness track.

## D1 — Scheduler-aware checkpoint

The smallest plausible replay seam is a quiescent pre-strict dispatch point after prior work has committed and before the next scheduler ticket or strict operation is selected.

It must include:

* carrier orders, generations, and cursors;
* delayed rows and statuses;
* action-ledger lifecycle;
* scheduler ticket and lane state;
* frontiers and support masks;
* restricted values;
* complete incumbent and properness state;
* consumed resource counters;
* graph generations;
* cap-accounted memory ownership.

The ordinary/save/replay triplet must reproduce the same search continuation, not merely the same mechanics.

## D2 — Strict proof-memory attribution

Only after replay parity:

* split proof-store and quotient bytes by owner;
* identify live versus historical generations;
* measure kernels, rows, reverse dependencies, cells, carriers, oracle state, and container capacity;
* avoid another undirected “reduce memory” refactor.

The archaeology makes clear that the stopped experiment never actually reached memory attribution.

## D3 — Action-specific RCASSP

The RCASSP should not be a new global goal-mask PDB.

Its correct form is:

* small portfolio;
* selected goal identities;
* actual side and capacity;
* action-relative disposable/persistent/local junk classes;
* blockers;
* protection;
* destructive survival;
* whole normalized kernels selected through favorable nondeterminism;
* action-local exact pushforward where necessary;
* fallback for every unrefined action;
* independent admissibility at every stage.

That is the RCASSP report’s central recommendation.

Start with:

```text
1 side/capacity parent
4 selected singleton patterns
1–2 selected pairs
at most 1 action-local refinement
```

Do not start with an exact four-goal carrier.

Use:

```text
16 MiB shadow budget initially
24 MiB hard retained maximum after qualification
```

Under pressure, discard unfinished children and retain validated parents.

Most importantly, it must have a consumer. A larger displayed root lower is not enough.

Promotion should require actual evidence such as:

* strict obligations retired before row materialization;
* fewer retained strict rows;
* lower proof-store/quotient memory;
* reduced strict work;
* exact closure.

The archaeology’s strongest lower-bound finding is that generic bounds separated nothing, while action-specific retention proofs paired with a proper carrier-local upper retired 79,799 obligations.

---

# Recommended dependency order

```text
A0  Freeze profile, economy, action scope, cases, and fresh baselines
 |
A1  Native Lab catalog, supervisor, attempts, JSON API and CLI
 |
A2  Thin practical GUI + typed LLM adapter
 |
 +---------------------------+
 |                           |
B1 Option authority firewall D1 Scheduler-aware checkpoint
 |                           |
B2 Exact fragment verifier   D2 PDR memory attribution
 |                           |
B3 Initial option library    D3 RCASSP shadow proof
 |
B4 Meta search + flattening
 |
B5 Current 4–5 mod qualification
 |
C1 Offline scheduling baselines and resource predictors
 |
C2 Shadow learned inference
 |
C3 Live carrier/action ranking
 |
C4 Learned option proposal
 |
C5 Strict-obligation ranking after faithful replay
```

The exactness branch and upper-quality branch can proceed concurrently after the Lab foundation.

---

# First active implementation boundary

## Native Solver Lab Core and Baseline Freeze

This should be the next selected repository plan.

### Included

* `native_allflame_no_imprint_v1` profile;
* fixed Allflame economy identity;
* explicit Imprint disablement;
* current-semantic anchor corpus;
* persistent SQLite experiment/job/attempt catalog;
* one existing benchmark process per solve;
* attempt-specific immutable artifact directories;
* memory-aware admission;
* watchdog and no-survivor handling;
* cancel, retry, clone, and priority operations;
* stable JSON control API;
* CLI;
* thin desktop GUI;
* typed LLM operations;
* run comparison;
* exact-evaluated strategy inspection;
* feature/event logging suitable for later ranking experiments.

### Explicitly excluded

* option behavior changes;
* scheduler changes;
* neural inference;
* strict-obligation model scores;
* scheduler-aware checkpoint;
* PDR memory repair;
* RCASSP;
* Imprint;
* web-product redesign;
* multi-machine execution.

### Acceptance

1. GUI and LLM clients can submit the same immutable job.
2. Restarting the GUI or control service loses no queued or completed work.
3. Retries never overwrite previous attempts.
4. Concurrent runs respect declared host reservations.
5. Watchdog or crash cleanup leaves no surviving process.
6. Valid partial trajectories remain analyzable but are never called complete.
7. Every attempt pins source, executable, data artifact, case, Allflame economy, profile, action scope, caps, and instrumentation identity.
8. Existing native solve output is unchanged when executed through the Lab.
9. The baseline matrix resolves the PDR-number discrepancy before later proof gates are written.
10. The frozen-test partition is sealed before learned feature or scheduling tuning begins.

---

# Approaches that should remain closed

Do not reopen these without a genuinely different mathematical premise:

* the original goal-mask probability lower;
* generic unresolved-action descriptors;
* transferred coarse lowers over all strict obligations;
* success-only partial broad-row upper construction;
* shared cross-action reforge DAG;
* cross-generation exact-row replay;
* global carrier comparator;
* generic global anytime scheduler profile;
* coarse carrier-option policies compiled after the fact;
* historical strategy injection as a fresh incumbent;
* learned transition probabilities;
* learned state merging;
* direct neural value on the admissibility path.

The archaeology’s closing list is well supported by the later experiments.

# Final priority

1. **Native Solver Lab Core and fresh benchmark freeze.**
2. **Verified executable graph-fragment system.**
3. **Meta-policy generation and general 4–5 mod bounded-policy qualification.**
4. **Scoped deterministic scheduling and resource baselines.**
5. **Learned carrier/action and option-proposal guidance.**
6. **Scheduler-aware replay and PDR proof-memory attribution in parallel.**
7. **Action-specific RCASSP only after attribution and consumer qualification.**
8. **Strict-obligation learned scheduling only after faithful replay.**

The most important architectural sentence across all five reports is:

> **The policy-quality path should compose exact executable control flow; the proof path should compose independently admissible action-specific abstractions; the learned path should only decide what exact work to try next.**
