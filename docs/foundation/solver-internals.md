# Solver Internals And Source Ownership

**Status: stable implementation map.** This page explains the native solver's
private execution phases and where their contracts live. It does not select
future work or define crafting mechanics.

Parent: [Foundation](README.md)

Verified against code: 2026-08-01 on
`codex/solver-iteration-infrastructure`.

## Execution Flow

```text
public request + goal + prices
  -> registry and planner-option construction
  -> abstract layout and exact transition provider
  -> reachable abstract graph and Bellman policy
  -> exact policy observation/refinement and publication assertion
  -> ordinary compiled strategy graph
  -> exact strategy evaluation or sampled Simulator execution
  -> public result, accounting, and telemetry
```

The stages share native mechanics authority, but their state identities are
not interchangeable:

- A concrete carrier is a complete `pc_item_state`. Exact refinement stores a
  collision-checked `StableKey` and enough exact state to reconstruct and
  verify that carrier.
- An `AbstractState` is the solver projection selected by an `AbstractLayout`.
  `CalcContext` interns these states and owns exact action/option kernels over
  their numeric IDs.
- A refined class is a proved group of exact carrier/action observations. Its
  class or routing identity is not permission to discard a concrete selected
  modifier, option, or operation payload.
- A compiled strategy node is an executable JSON graph node. It is a product
  of policy compilation, not a solver state ID or refinement class.
- Exact evaluation walks `(strategy operation, item state)` pairs.
  `SimulatorImpl` instead executes the same strategy vocabulary with sampled
  native actions. Neither path gives the frontend crafting-rule authority.

## Phase And File Map

| Phase | Representation passed onward | Primary owners |
| --- | --- | --- |
| Goal, registry, and layout | `GoalSpec`, `ResolvedGoal`, `ActionDescriptor`, `PlannerOperator`, `AbstractLayout`, `AbstractState` | `solver_model.hpp`, `solver_registry.cpp`, `solver_abstract.cpp` |
| Option construction and admission | `PlannerOperator`, runtime semantics, `OptionKernel` | `solver_options_build.cpp`, `solver_options_automatic.cpp`, `solver_options_temporary.cpp`, `solver_options_import.cpp`, `solver_options_semantics.cpp`, `solver_options.cpp` |
| Exact transition calculation | interned abstract states and action/option outcome rows in `CalcContext` | `solver_calc_types.hpp`, `solver_calc.cpp`, `solver_reforge.cpp`, `solver_options.cpp` |
| Reachability and policy solving | sparse graph rows, values, selected actions, `SolveResult` | `solver_solve_types.hpp`, `solver_solve_expand.cpp`, `solver_solve_incremental.cpp`, `solver_solve_bellman.cpp`, `solver_solve_constructive.cpp`, `solver_solve_finish.cpp`, and the other `solver_solve_*.cpp` owners |
| Shared exact refinement | `Graph`, observation requirements, collision-checked features, closed probabilistic partition, counterexamples | `solver_refinement.hpp`, `solver_refinement_graph_core.hpp`, `solver_refinement_graph_discovery.hpp`, `solver_refinement_observation.cpp`, `solver_refinement_features.cpp`, `solver_refinement_partition.cpp`, `solver_refinement_eval.cpp`, `solver_refinement.cpp` |
| Production policy adaptation | strict carrier mappings/kernels, exact runs, repaired or improved policy, compile routing | `solver_policy_refinement.hpp`, `solver_policy_refinement.cpp`, `solver_policy_oracle_*.inc`, `solver_policy_assertion.cpp` |
| Policy compilation | ordinary strategy JSON and `PolicyCompilationTelemetry` | `solver_compile_contracts.hpp`, `solver_compile_conditions.hpp`, `solver_compile_serialization.hpp`, `solver_compile.cpp` |
| Exact graph evaluation | `StrategyEvalResult`, occupancy/influence and action/material accounting | `solver_eval_types.hpp`, `solver_eval_helpers.hpp`, `solver_eval.cpp`, `solver_eval_resolve.cpp`, `solver_eval_report.cpp` |
| API, accounting, and telemetry | C ABI results, JSON, progress, owned-byte and work counters | `solver_api.cpp`, `solver_solve_telemetry.cpp`, `solver_compile.cpp`, `solver_eval_report.cpp` |
| Sampled execution | mutable item plus RNG-driven strategy traversal | `simulator.cpp` and native action owners |

`solver_policy_refinement.cpp` deliberately remains one translation unit for
the anonymous `ProductionPolicyOracle`. Its named `.inc` files separate setup,
kernel construction, state mapping, choices, observations, lifting, resource
accounting, evaluation, and improvement without changing internal linkage or
object lifetime. Similarly, `solver_eval.cpp` keeps the tightly coupled
`StrategyEvalWork::Impl` lifecycle together while resolution and report
serialization are independent translation units.

## Private Header Layers

The private phase contracts form a one-way dependency chain:

```text
solver_model.hpp
  -> solver_calc_types.hpp
  -> solver_eval_types.hpp
  -> solver_solve_contracts.hpp
  -> solver_compile_contracts.hpp
```

Implementations include the narrowest phase they require. Refinement's shared
contract depends only on `solver_model.hpp`; the production policy adapter adds
solve contracts explicitly. `solver_internal.hpp` is a compatibility umbrella
for deliberate cross-phase callers such as the public solver adapter and broad
native tests. New leaf implementation files should not include it by default.

The core chain is intentionally not advertised as low-fan-out. A change to
`solver_model.hpp` still rebuilds nearly every solver owner; use the narrow
phase headers to localize later-phase declaration changes, not to infer that a
core model edit is cheap.

## Where Should I Make This Change?

| Change | Start here |
| --- | --- |
| Goal projection, junk classes, or abstract legality | `solver_model.hpp`, `solver_abstract.cpp` |
| Registered action facts or observation contracts | `solver_registry.cpp` |
| Primitive outcome probabilities or exact carrier materialization | `solver_calc.cpp`, `solver_reforge.cpp` |
| Automatic/compound option admission or construction | the matching `solver_options_*.cpp` owner |
| Reachable graph scheduling or row expansion | `solver_solve_expand.cpp`, `solver_solve_incremental.cpp` |
| Bellman values, properness, incumbent, or policy finalization | the matching `solver_solve_bellman.cpp`, `solver_solve_audit.cpp`, `solver_solve_constructive.cpp`, or `solver_solve_finish.cpp` owner |
| Exact carrier discovery or canonicalization | `solver_refinement_graph_discovery.hpp` |
| Observation projection or partition proof | `solver_refinement_observation.cpp`, `solver_refinement_features.cpp`, `solver_refinement_partition.cpp` |
| Production exact-policy repair or improvement | the matching `solver_policy_oracle_*.inc` owner |
| Strategy condition routing or JSON emission | `solver_compile_conditions.hpp`, `solver_compile_serialization.hpp`, `solver_compile.cpp` |
| Exact strategy operation resolution or accounting report | `solver_eval_resolve.cpp`, `solver_eval.cpp`, `solver_eval_report.cpp` |
| Solver progress, memory, or work telemetry | `solver_solve_telemetry.cpp` and the owning phase's counters |

Before changing a contract that crosses these rows, follow the
[Change Impact Map](change-impact.md). Mechanics questions still require an
Oliver ruling and belong in the [mechanics library](../mechanics/README.md).

## Determinism, Exactness, And Lifetime Invariants

- Preserve stable ordering, tie breaks, canonical keys, transition insertion
  order, and floating-point evaluation order. Native solver builds retain
  `-ffp-contract=off`.
- Abstract or quotient equality is accepted only by the implemented exact,
  collision-checked contract. Unknown observations and mismatches remain
  distinct.
- Selected actions retain literal resource, modifier, choice, and executable
  recipe identity even when successor states share an abstract or refined
  class.
- `CalcContext`, solve work, refinement graphs, compiled JSON, and evaluation
  work retain their existing ownership and named memory/work caps. Moving a
  declaration is not permission to shift retained-byte responsibility.
- `max_reforge_work` is a V1-equivalent logical search-envelope cap independent
  of the active V1/V2/V3 reforge implementation. Legacy active work and each
  version's physical effort remain observational telemetry; wall time, owned
  memory, and cooperative-step latency are separate qualification dimensions,
  not inputs to a universal weighted score.
- Crafting legality and probabilities stay in the native engine. Compilation,
  evaluation, WASM, and TypeScript consume native results; they do not recreate
  mechanics.
- Private state IDs, refined class IDs, compiled node IDs, and simulator
  handles are scoped to their owner. They are not durable cross-run identities.
- Public C ABI types and exports live under `engine/include/poecraft/`; the
  private headers on this page are not ABI.

Replay/checkpoint remains intentionally deferred until the proof-carrying
quotient representation is stable. A future development-only format must
validate artifact, configuration, compiler, and source identity and must never
become product proof authority.
