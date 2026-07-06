# ML Strategy Planning Architecture

**Status:** research-backed design direction, not an implementation phase

**Date:** 2026-06-30

**Scope:** Path of Exile 1 goal decomposition, crafting-policy search, model
training, and Strategy Builder graph generation

## Decision Summary

The ML system should be a neurosymbolic, model-based planner. It should not
begin as a language model or sequence model that directly emits Strategy
Builder JSON.

The intended system is:

```text
goal specification
    -> symbolic backward crafting decomposition
    -> learned policy and value guidance
    -> forward stochastic graph search
    -> compact strategy-controller synthesis
    -> authoritative native simulation
```

Responsibilities stay deliberately separate:

- The native engine owns mechanics, legality, random transitions, and final
  evaluation.
- Symbolic planning code owns the legal decomposition space and graph grammar.
- Learned models rank actions and decompositions and estimate continuation
  values.
- Search owns exploration, stochastic outcomes, retries, budgets, and policy
  improvement.
- A controller compiler turns the resulting policy into a small, inspectable
  Strategy Builder graph.

The model should become the planner's intuition. It should never become a
second implementation of the crafting engine.

## Product Objective

Given:

- a base and item level;
- a concrete starting item or start-state rule;
- a target item predicate;
- supported crafting mechanics;
- an economy snapshot;
- cost, action, and risk limits;

the system should return one or more executable Strategy Builder graphs that
trade off:

- probability of success;
- expected spend per attempt;
- expected spend per successful result;
- median and upper-tail cost;
- action count;
- strategy complexity.

The output is not a fixed sequence of currencies. Crafting outcomes are random,
so the output must be a contingent policy: inspect the current item, choose an
action, branch on the result, and retry or change phases when appropriate.

## Non-Goals

The initial system should not:

- learn PoE mechanics from trajectories;
- replace the native transition implementation with a neural model;
- generate opaque strategies that cannot be represented in Strategy Builder;
- optimize only expected cost while ignoring success probability;
- depend on exact seeded replay;
- begin with a large model before a non-learned planning baseline exists;
- use human-authored strategies as an unquestioned optimality oracle.

## Formal Problem

For one immutable crafting session, define:

```text
s  = complete concrete item state
g  = target predicate
a  = legal parameterized crafting action
b  = remaining action/cost budget
q  = internal controller or strategy phase
e  = economy snapshot
P(s' | s, a) = engine transition distribution
c(a, e)      = action cost
```

The desired controller is:

```text
policy(s, g, b, q) -> action
```

Goal states are absorbing for planning purposes. A stop/failure choice is also
needed so the planner can represent unreachable or no-longer-worthwhile states.

This is a constrained stochastic shortest-path problem. It is not ordinary
shortest-path search because:

- one action has many possible next states;
- progress can be destroyed;
- useful policies contain loops;
- cost and action limits are part of the state;
- success probability and cost distribution both matter.

The planner should retain a Pareto set rather than prematurely collapsing every
preference into one reward scalar. A useful initial ordering is:

1. maximize probability of success before the selected budget;
2. among sufficiently reliable policies, minimize expected resource use;
3. prefer lower tail risk and smaller generated graphs when primary results are
   similar.

A scalarized implementation can use a large failure penalty and a graph-size
penalty, but the underlying metrics should remain separately observable.

## Comparison With Chemistry Synthesis Planning

Retrosynthesis is the most useful neighboring research area, but it is not an
exact match.

| Dimension | Retrosynthesis | PoE crafting | Consequence |
|---|---|---|---|
| Direction | Usually searches backward from a target molecule | Must execute forward from an item | Backward analysis may propose milestones, but forward simulation validates them |
| Transition knowledge | Reactions may be missing, hallucinated, or infeasible | Supported mechanics are exactly implemented | Do not learn a transition or feasibility model |
| Typical output | Route, tree, or acyclic reaction graph | Branching policy with retries and loops | Use cyclic stochastic policy search |
| Uncertainty | Often uncertainty about whether a proposed reaction works | Actual aleatoric crafting outcomes | Model success and cost distributions |
| Repeated work | Same intermediate molecule appears in many routes | Same crafting subproblem appears across goals and searches | Use graph transpositions and shared caches |
| Data | Scarce and noisy reaction literature | Unlimited native simulation | Use planner-generated experience and strict synthetic benchmarks |

The useful chemistry pattern is learned guidance around symbolic search.
Segler, Preuss, and Waller combined neural policies with Monte Carlo tree search
and symbolic reaction rules. Retro* used a learned heuristic inside AND-OR
best-first search. RetroGraph showed that merging repeated intermediates and
searching related targets together avoids substantial duplicated work.

The important difference is that chemistry planners often need a learned
single-step reaction predictor and an in-scope filter. `poecraft2` already has a
better source: an exact, inspectable engine. ML capacity should be spent on
search guidance and value estimation instead.

## Three State Layers

The design must keep three representations distinct.

### Concrete Engine State

This is the authoritative Markov state. It includes every field that can affect
future legality or probability:

- exact session mod IDs;
- family, group, tier, side, and classification relationships;
- crafted, fractured, veiled, and related flags;
- rarity, influences, implicits, and open slots;
- any preservation or action-specific state;
- remaining run-wide limits when applicable.

Concrete affixes should have a canonical order for hashing because affix order
is not mechanically meaningful.

### Model Representation

The model should receive structured tokens rather than rendered item text:

- one token per explicit modifier;
- item-global context tokens;
- target-clause tokens;
- budget and preference tokens;
- candidate action descriptors;
- optional economy features.

A modifier token can include learned IDs plus grounded features such as side,
tier, groups, flags, and classification tags. Grounded features help the model
generalize beyond memorized modifier IDs.

### Strategy-Controller State

This is the small human-readable phase represented by a Strategy Builder node
or subgraph, for example:

```text
acquire target suffix
protect suffixes
reroll prefixes
finish deterministic craft
```

A milestone predicate alone is not necessarily Markov. Two items satisfying
"has target suffix" may have different blockers or open slots. Concrete state
must remain underneath abstractions unless their equivalence has been proved.

## Goal DSL

Existing graphs define success by reaching a success terminal. Generated
strategies additionally need an input target that exists before the graph does.

Introduce a separate compiled goal object that reuses the condition AST and
modifier-family vocabulary:

```json
{
  "type": "all",
  "conditions": [
    {
      "type": "has_mod_family",
      "family_mod_key": "target-life-family",
      "min_tier": 2
    },
    {
      "type": "has_mod_family",
      "family_mod_key": "target-resistance-family",
      "min_tier": 1
    },
    {
      "type": "open_prefix_count",
      "min": 1
    }
  ]
}
```

The long-term goal language may need:

- family and minimum tier;
- exact group where group identity is the real rule;
- fractured, crafted, veiled, or related flag constraints;
- prefix/suffix and open-slot requirements;
- rarity and influence requirements;
- implicit requirements;
- exclusions;
- nested `all`, `any`, `not`, and `at_least`.

The compiled target predicate should be usable by the planner, the native
evaluator, benchmark fixtures, and the generated graph's success route.

## Symbolic Backward Decomposition

Backward reasoning should generate legal high-level possibilities without
pretending that crafting actions are invertible.

For each requested target clause, enumerate supported ways to produce it:

```text
target modifier family
    normal weighted roll
    essence guarantee
    fossil weighting
    Harvest targeted action
    bench craft
    veiled outcome
    influence-specific action
```

Each compiled action schema needs symbolic metadata describing:

- preconditions;
- reachable families/tags/modifiers;
- guaranteed versus weighted additions;
- rerolled and preserved sides;
- destructive effects;
- required rarity, influence, or open slots;
- incompatible groups;
- canonical economy price keys.

Conjoined target clauses create AND relationships. Alternative mechanics create
OR relationships. Ordering and preservation interactions remain a forward
planning problem because later actions may erase earlier progress.

The model ranks candidate decompositions; the engine-derived catalog defines
which decompositions exist.

## Forward Stochastic Graph Search

The main planner should use a transposition graph rather than a pure tree:

```text
item/controller state
    -> action decision
    -> chance outcomes
    -> next item/controller states
```

Required properties:

- repeated concrete states share one node;
- related targets may reuse state-action transition evidence;
- retry policies can contain cycles;
- action expansion is ordered by a learned prior;
- unexpanded states receive learned value estimates;
- stochastic transitions are sampled through the native engine;
- important uncertain transitions receive additional samples;
- final policies are independently re-evaluated.

An LAO*/LRTDP-style cyclic stochastic-shortest-path solver is the best target
architecture. A graph-aware MCTS should be kept as a baseline because it works
directly with a generative simulator, but plain tree MCTS duplicates repeated
failure and retry subproblems.

### Transition Cache

Cache engine observations by mechanics rather than by goal:

```text
(engine/data version, session, concrete state, action)
    -> observed next states and counts
```

The cache is independent of target predicate and usually independent of
economy. Different goals and price snapshots can therefore reuse mechanics
work. Cache entries remain versioned and bounded; they are evidence for search,
not replacements for final simulation.

### State Abstraction

Full concrete outcomes can still explode. State merging is legal only when the
retained signature is sufficient for every future action and condition under
consideration.

Domain-specific options can create safe quotients. For example, while executing
a full-reroll-until-predicate option, non-preserved failed affixes may be
irrelevant to the next attempt. This should be proved from the option's allowed
actions and termination predicate, not assumed globally.

## Learned Guidance

The first learned model should be a compact goal-conditioned set/transformer
model, not an LLM.

The engine enumerates legal structured action candidates. The model scores each
candidate using shared action embeddings instead of predicting one fixed giant
action vocabulary.

Recommended heads:

```text
policy_prior(state, goal, candidate_action)
success_value(state, goal, budget)
remaining_resource_value(state, goal)
```

Later heads can estimate:

- action-count and cost quantiles;
- failure-reason probabilities;
- option/subgoal priors;
- expected usage by canonical price key.

Success probability and cost should remain separate. A cheap policy is not good
when it almost never succeeds, and a high-success policy may be unacceptable at
its upper-tail cost.

The model should not predict `P(s' | s, a)`. The native engine already supplies
that distribution without model bias or version drift.

## Planner-Driven Training

Use expert iteration rather than starting with model-free reinforcement
learning:

1. Generate goals, starts, budgets, and economy contexts.
2. Search using the current policy/value guidance.
3. Validate promising policies in the native simulator.
4. Record expanded states, legal candidates, action values, transition samples,
   and failure information.
5. Train the policy head toward improved search decisions.
6. Train value heads from backed-up search values and held-out rollouts.
7. Repeat with harder and more diverse tasks.

Store unsuccessful searches as well as successful ones. They provide useful
negative action targets and prevent survivorship bias.

### Hindsight Relabeling

Failed runs often produce items that satisfy a different useful target. Relabel
some trajectories with achieved predicates and use them as auxiliary
goal-conditioned training examples.

Relabeling should operate on valid goal clauses and target subsets, not arbitrary
exact item snapshots. It supplements genuine requested-goal data rather than
replacing it.

### Curriculum

A useful progression is:

1. one modifier at any tier;
2. one family with a tier threshold;
3. two compatible modifiers;
4. open-slot requirements;
5. deterministic and weighted alternatives;
6. preservation, fractures, and metamods;
7. influence, veiled, Harvest, and Eldritch mechanics;
8. rare multi-stage goals under budgets;
9. economy-conditioned planning.

## Reusable Options And Goal Decomposition

Do not initially require a model to commit to one fixed subgoal sequence.
Crafting progress is reversible, and the best next milestone depends on both
the concrete item and available preservation mechanics.

After primitive planning works, mine repeated successful subgraphs and promote
them to verified options:

```text
obtain a target suffix while retaining an open prefix
protect a completed suffix side
reroll one side until a target predicate
finish with a deterministic bench modifier
```

Each option needs:

- applicability predicate;
- termination predicate;
- internal strategy fragment;
- expected resource vector;
- outcome and failure distribution;
- mechanics/data version.

The model can then propose landmarks and rank options while search remains free
to reject or reorder them. This is the safe interpretation of "decompose the
goal item into states."

## Economy And Transfer

Start with a static economy snapshot.

Longer term, represent cost through expected resource usage:

```text
expected_cost = dot(expected_resource_counts, economy_prices)
```

This separates mechanics from prices and allows much of the learned value to be
reused after an economy update. A changed price vector can still change the
optimal policy, so the system should keep a portfolio of candidate policies and
run limited policy improvement under the new economy.

## Strategy Builder Graph Synthesis

The raw search policy may contain too many concrete states for a useful graph.
Graph synthesis is a separate optimization stage:

1. Collect states reached by the selected policy.
2. Group states choosing the same continuation action.
3. Fit a small decision tree or rule list using supported condition predicates.
4. Convert tests into guarded edges or router nodes.
5. Convert action leaves into operation nodes.
6. Merge equivalent fragments and introduce retry loops.
7. Penalize node count and condition complexity.
8. Re-run the compiled graph through the native simulator.

The generated graph should optimize something like:

```text
policy utility
    - node penalty
    - condition-complexity penalty
    - validation-gap penalty
```

If an important policy cannot be expressed with the current condition language,
that is evidence for one deliberate condition-system extension. It is not a
reason to emit opaque hidden-state nodes.

## Runtime Architecture

The current whole-strategy simulator is the final evaluator. Search additionally
needs a low-overhead training environment surface:

```text
compiled goal handles
compiled action catalog
legal action enumeration
canonical state hashing/encoding
grouped batch action stepping
batch goal evaluation
no-trace structural execution
```

A likely private training runtime is:

```text
model/search coordinator
    batches inference for unique states
    caches model outputs by model version
    schedules graph expansions

native CPU workers
    share immutable sessions
    own action contexts and RNG state
    execute grouped transition batches
    write result/trajectory shards
```

Do not cross Python/native or CPU/GPU boundaries once per crafting action. The
current engine can execute actions faster than a useful neural model can perform
individual inference. Batch inference once per unique search state and keep
rollouts native.

Large trajectories belong in compressed binary or columnar shards. Postgres can
catalog jobs, model versions, benchmark summaries, and artifact locations; it
should not store per-step bulk experience.

## Benchmark And Evaluation Design

Create the benchmark before training the first model.

The frozen suite should include:

- small fixture problems with exact optimal-policy oracles;
- unseen target combinations;
- held-out modifier families;
- held-out base classes;
- held-out mechanic combinations;
- economy perturbations;
- reachable and intentionally unreachable targets;
- easy, medium, and rare goals;
- fixed engine-transition and wall-clock budgets.

Measure:

- solved-goal rate;
- simulator-validated success probability;
- expected, median, and upper-tail cost;
- expected cost per successful result;
- regret against exact toy oracles;
- search expansions and native transition count;
- neural inference count;
- graph node/edge and condition complexity;
- raw-policy versus compiled-graph validation gap;
- diversity and Pareto coverage of returned strategies.

Required baselines and ablations:

```text
random legal policy
handwritten greedy progress heuristic
unguided graph search
plain tree MCTS
value guidance only
policy prior only
policy + value guidance
tree vs transposition graph
primitive actions vs learned options
with vs without hindsight relabeling
```

Training and test splits must be defined by semantic difficulty, not only random
trajectory rows. Hold out target combinations, bases, families, and mechanics so
the benchmark measures generalization rather than memorization.

## Phased Research Program

### ML-0: Foundations Without ML

Implement or specify:

- goal DSL and compiled evaluator;
- canonical concrete state and action schemas;
- symbolic action effects and reverse reachability;
- grouped batch stepping;
- frozen benchmark suite;
- exact solver for tiny fixture domains.

Acceptance:

- Small problems have known optimal policies and reproducible benchmark
  results.
- Goals and legal actions are evaluated by the native engine rather than
  duplicated in Python.

### ML-1: Classical Stochastic Planner

Implement:

- sampled cyclic graph search;
- transposition and transition caches;
- handwritten admissible or conservative heuristics where possible;
- initial policy-to-Strategy-Builder compiler.

Acceptance:

- At least one nontrivial strategy is generated, compiled, and independently
  validated.
- Search beats random and greedy baselines at a fixed transition budget.

### ML-2: Learned Search Guidance

Implement:

- goal-conditioned candidate-action scorer;
- separate success and resource/cost value heads;
- planner-driven experience generation;
- expert-iteration training;
- inference batching and caching.

Acceptance:

- Learned guidance finds equal-or-better policies with materially fewer
  expansions on held-out goals.
- Gains survive independent final simulation and multiple benchmark seeds.

### ML-3: Hierarchical Options

Implement:

- successful-subgraph mining;
- versioned option library;
- option outcome/resource summaries;
- subgoal and option ranking;
- graph-complexity-aware planning.

Acceptance:

- Options improve search or graph simplicity without lowering validated policy
  quality.

### ML-4: Economy And Distributed Scale

Implement:

- resource-usage value representation;
- preference-conditioned or Pareto planning;
- distributed native workers;
- centralized batched model inference;
- model and trajectory artifact registry.

### ML-5: Amortized Direct Proposals

Only after the verified planner produces a large, high-quality strategy corpus:

- train a graph/subgoal proposal model;
- use direct output only as a search initialization;
- repair and verify every generated graph;
- retain the planner as the authority.

## Rejected Initial Approaches

### Direct LLM Or Transformer Graph Generation

Useful later for instant proposals, but initially it lacks optimal training data,
does not naturally guarantee legal or terminating graphs, and can hide mechanics
errors behind plausible structure.

### Model-Free PPO Or DQN As The Primary System

This discards a known high-quality transition model, makes sparse-goal learning
harder, complicates graph extraction, and offers weaker debugging than explicit
search.

### Learned Transition Model

This duplicates the engine, introduces bias, and becomes stale with mechanics
and data updates.

### Pure Backward Retrosynthesis

Crafting actions are stochastic and destructive rather than invertible. Backward
analysis is useful for candidate milestones, not for proving executable paths.

### Pure Tree Search

Retry loops and related targets repeat subproblems. A tree cannot reuse enough
of that work.

### Importance Sampling As A Correctness Shortcut

It may later help evaluate rare outcomes, but it is not the planning foundation
and must not silently change estimated policy quality.

## Research Risks

### State Explosion

Six explicit affixes still create an enormous concrete state space. Mitigate
with goal-conditioned value generalization, graph reuse, verified options,
progressive outcome aggregation, and bounded caches.

### Unsafe State Abstraction

Milestone-equivalent items may have different future transition distributions.
Keep concrete state underneath abstractions and validate any quotient against
engine transitions.

### Search Overfitting

A policy can look good because a noisy transition estimate was lucky. Use
adaptive resampling, confidence reporting, held-out rollouts, and independent
final simulation.

### Reward Pathologies

Expected cost alone rewards quitting or low-success strategies. Report and
optimize success probability and resource distributions separately.

### Unexportable Policies

A strong raw policy may depend on distinctions the Strategy Builder cannot
express. Track compression loss explicitly and expand the condition DSL only
when repeated evidence justifies it.

### Model Inference Dominating Runtime

Per-state inference can cost far more than a native crafting action. Batch and
cache inference, perform many native transitions per model call, and measure
search efficiency in model calls as well as engine actions.

### Benchmark Leakage

Procedurally generated goals can still overlap semantically. Hold out bases,
families, mechanics, target combinations, and economy regimes.

## Open Decisions Before ML-0

- Exact initial goal vocabulary and unsupported target clauses.
- Primary optimization contract: constrained success threshold, Pareto frontier,
  or both.
- Which actions/mechanics form the first toy domain.
- Concrete-state canonicalization and versioning format.
- Whether the first cyclic planner should be sampled LAO*/LRTDP or graph-aware
  MCTS, with the other retained as baseline.
- How much controller complexity users will tolerate.
- Which new Strategy Builder conditions are justified for generated policies.
- Whether training starts on one focused base fixture or a small varied base
  family.

## Research References

- Segler, Preuss, and Waller, [Planning chemical syntheses with deep neural
  networks and symbolic AI](https://www.nature.com/articles/nature25978),
  Nature, 2018.
- Chen et al., [Retro*: Learning Retrosynthetic Planning with Neural Guided A*
  Search](https://proceedings.mlr.press/v119/chen20k.html), ICML, 2020.
- Xie et al., [RetroGraph: Retrosynthetic Planning with Graph
  Search](https://arxiv.org/abs/2206.11477), KDD, 2022.
- Liu et al., [Retrosynthetic Planning with Dual Value
  Networks](https://proceedings.mlr.press/v202/liu23as/liu23as.pdf), ICML,
  2023.
- Schreck, Coley, and Bishop, [Learning retrosynthetic planning through
  self-play](https://arxiv.org/abs/1901.06569), ACS Central Science, 2019.
- Zhang et al., [A data-driven group retrosynthesis planning model inspired by
  neurosymbolic programming](https://www.nature.com/articles/s41467-024-55374-9),
  Nature Communications, 2024.
- Maziarz et al., [Re-evaluating Retrosynthesis Algorithms with
  Syntheseus](https://arxiv.org/abs/2310.19796), 2023.
- Schaul et al., [Universal Value Function
  Approximators](https://proceedings.mlr.press/v37/schaul15.html), ICML, 2015.
- Andrychowicz et al., [Hindsight Experience
  Replay](https://papers.nips.cc/paper_files/paper/2017/hash/453fadbd8a1a3af50a9df4df899537b5-Abstract.html),
  NeurIPS, 2017.
- Hansen and Zilberstein, [LAO*: A heuristic search algorithm that finds
  solutions with loops](https://doi.org/10.1016/S0004-3702(01)00106-0),
  Artificial Intelligence, 2001.
- Barreto et al., [Successor Features for Transfer in Reinforcement
  Learning](https://papers.nips.cc/paper_files/paper/2017/hash/350db081a661525235354dd3e19b8c05-Abstract.html),
  NeurIPS, 2017.
- Granqvist, Mercado, and Genheden, [RetroSynFormer: planning multi-step
  chemical synthesis routes via a decision
  transformer](https://doi.org/10.1039/D5DD00153F), Digital Discovery, 2026.
