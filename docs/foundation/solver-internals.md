# Solver Internals And Source Ownership

**Status: stable implementation map.** This page explains the native solver's
private execution phases and where their contracts live. It does not select
future work or define crafting mechanics.

Parent: [Foundation](README.md)

Verified against current source: 2026-08-27. Scope: private solve
phase ownership, cooperative continuation boundaries, current
proof/publication boundaries, action-family contract, exact evaluator gated-
kernel authority, exact reforge row continuation, telemetry serialization
ownership, and the native source inventory. Final acceptance for the active
boundary is recorded in its result rather than implied by this source audit.

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
| Goal, registry, action-family contract, and layout | `GoalSpec`, `ResolvedGoal`, `ActionDescriptor`, `PlannerOperator`, `AbstractLayout`, `AbstractState` | `solver_model.hpp`, `solver_action_family_contract.hpp`, `solver_registry.cpp`, `solver_abstract.cpp` |
| Option construction and admission | `PlannerOperator`, runtime semantics, `OptionKernel` | `solver_options_build.cpp`, `solver_options_automatic.cpp`, `solver_options_temporary.cpp`, `solver_options_import.cpp`, `solver_options_semantics.cpp`, `solver_options.cpp` |
| Exact transition calculation | interned abstract states and action/option outcome rows in `CalcContext` | `solver_calc_types.hpp`, `solver_calc.cpp`, `solver_reforge.cpp`, `solver_options.cpp` |
| Reachability, action-envelope scheduling, proof values, and policy solving | sparse graph rows, values, selected actions, proof-only lowers, carrier ordering scores, `SolveResult` | `solver_action_envelope_ledger.hpp`, `solver_anytime_scheduler.hpp`, `solver_proof_pattern_manager.hpp`, `solver_solve_types.hpp`, `solver_solve_expand.cpp`, `solver_solve_incremental.cpp`, `solver_solve_focused.cpp`, `solver_solve_bellman.cpp`, `solver_solve_bounds.cpp`, `solver_solve_priority.cpp`, `solver_solve_constructive.cpp`, `solver_solve_audit.cpp`, and the other `solver_solve_*.cpp` owners |
| Shared exact refinement | `Graph`, observation requirements, collision-checked features, closed probabilistic partition, counterexamples | `solver_refinement.hpp`, `solver_refinement_graph_core.hpp`, `solver_refinement_graph_discovery.hpp`, `solver_refinement_observation.cpp`, `solver_refinement_features.cpp`, `solver_refinement_partition.cpp`, `solver_refinement_eval.cpp`, `solver_refinement.cpp` |
| Production policy adaptation | strict carrier mappings/kernels, exact runs, repaired or improved policy, compile routing | `solver_policy_refinement.hpp`, `solver_policy_refinement.cpp`, `solver_policy_oracle_*.inc`, `solver_policy_assertion.cpp` |
| Policy compilation | ordinary strategy JSON and `PolicyCompilationTelemetry` | `solver_compile_contracts.hpp`, `solver_compile_conditions.hpp`, `solver_compile_serialization.hpp`, `solver_compile.cpp` |
| Exact graph evaluation | `StrategyEvalResult`, occupancy/influence and action/material accounting | `solver_eval_types.hpp`, `solver_eval_helpers.hpp`, `solver_eval.cpp`, `solver_eval_resolve.cpp`, `solver_eval_report.cpp` |
| API, accounting, and telemetry | C ABI results, typed progress snapshots, JSON, owned-byte and work counters | `solver_api.cpp`, `solver_solve_telemetry.cpp` (collection/accounting), `solver_solve_telemetry_json.cpp` (serialization), `solver_compile.cpp`, `solver_eval_report.cpp` |
| Sampled execution | mutable item plus RNG-driven strategy traversal | `simulator.cpp` and native action owners |

The stateful Solve path has six named private authorities:

- `ActionEnvelopeLedger` owns every carrier/operator obligation from discovery
  through exact-row completion, proof retirement, or named refusal.
- `SolveScheduler` owns lane quotas, fairness, waits, yields, and starvation
  telemetry. The retained production fallback still uses the qualified legacy
  work order when no behavior-changing scheduler profile passes its controls.
- `IncumbentPortfolio` owns coarse estimates separately from independently
  evaluated executable candidates and preserves the cheapest verified upper.
- `ProofPatternManager` owns typed admissible patterns and maximum-only
  composition; ordering scores remain non-convertible.
- `PublicationPipeline` owns direct assertion, strict repair,
  classification, packaging, and their retained cooperative task.
- `SolveTelemetrySnapshot` freezes typed progress and measurement before C ABI
  and JSON serialization.

These types establish lifetime and authority boundaries. They do not broaden
mechanics, action scope, exactness, or the public ABI.

Strict exact-evaluation layouts also retain feature-specific uniformity facts
on goal-member and junk classes. Observation extraction may reuse only a fact
whose layout partition recorded it complete; otherwise it rescans the exact
member mask. The optimization therefore removes repeated derivation without
turning a compact/coarse class into observation authority.

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
| Reachable graph scheduling, delayed action envelopes, or row expansion | `solver_solve_expand.cpp`, `solver_solve_incremental.cpp`, `solver_solve_focused.cpp` |
| Carrier-only ordering | `solver_solve_priority.cpp`; keep it separate from proof values |
| Admissible carrier/operator bounds or public lower authority | `solver_solve_bounds.cpp`, `solver_solve_carrier_pattern.cpp`, `solver_solve_operator_proof.cpp`, and `solver_solve_envelope_proof.cpp`, then the consumer/publication boundary in `solver_solve_constructive.cpp` or `solver_solve_finish.cpp` |
| Bellman values, properness, verified incumbents, or policy finalization | the matching `solver_solve_bellman.cpp`, `solver_solve_audit.cpp`, `solver_solve_constructive.cpp`, or `solver_solve_finish.cpp` owner |
| Exact carrier discovery or canonicalization | `solver_refinement_graph_discovery.hpp` |
| Observation projection or partition proof | `solver_refinement_observation.cpp`, `solver_refinement_features.cpp`, `solver_refinement_partition.cpp` |
| Production exact-policy repair or improvement | the matching `solver_policy_oracle_*.inc` owner |
| Strategy condition routing or JSON emission | `solver_compile_conditions.hpp`, `solver_compile_serialization.hpp`, `solver_compile.cpp` |
| Exact strategy operation resolution or accounting report | `solver_eval_resolve.cpp`, `solver_eval.cpp`, `solver_eval_report.cpp` |
| Solver progress, memory, or work telemetry | the owning phase's typed counters, `solver_solve_telemetry.cpp` for collection/accounting, and `solver_solve_telemetry_json.cpp` for serialization |

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
- Automatic finite-program discovery closes only through exact mechanical or
  resource dominance, or a conservative positive-price certificate against a
  proper carrier-local upper. An arbitrary depth limit cannot close the action
  envelope or support global exactness.
- High-impact anytime mode constructs the one-time universal/clean proof model
  during measured solve setup; default solves retain lazy construction so a
  pre-proof root-row cap keeps its established attribution. Focused proof
  snapshots, support growth, post-upper
  classification, dynamic automatic preparation, policy work, and publication
  retain explicit cursors or tasks and return them through public step
  boundaries. The native step also applies an internal 32-logical-unit ceiling
  even when a caller requests a larger batch.
- Exact V3 destructive rows retain a deterministic cursor and publish only
  after completion. Strict policy refinement drives those rows cooperatively;
  synchronous calculator callers use the completion wrapper. A partial or
  cancelled row cannot populate the ordinary cache or a proof obligation.
- A strict alternative that discovers a successor outside the current closed
  partition returns that frontier to the persistent grow-in-place owner before
  replaying unrelated old-generation obligations.
- While incremental action generation remains open, its restricted optimum is
  scheduling evidence rather than a global lower. Public lower authority falls
  back to independently admissible proof patterns until the requested action
  envelope closes.
- Proof patterns compose by maximum only unless a separate cost partition
  proves disjoint immediate resources. A completed start-state Bellman
  envelope may publish after certification only when every admitted ordinary
  and automatic row is covered; missing coverage falls back locally. A
  fixed-identity pattern may enter focused fringe gaps only while the exact
  protection, Fracture, junk, influence, and Eldritch identity remains fixed.
- An unmaterialized action obligation can close as incumbent-dominated only
  when a complete immediate-price authority plus that action's proved
  successor pattern strictly exceeds an exactly optimized proper
  carrier-local upper. The ledger retains that named proof without inventing
  a row or treating proof closure as materialized closure. Authored conditional
  programs remain excluded while their lower charges only the first step.
- A finite public upper belongs to the independently evaluated emitted graph,
  not its coarse, selected, direct, strict, or fallback source estimate. Later
  candidates cannot replace a cheaper verified artifact without beating that
  exact evaluated cost.
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
