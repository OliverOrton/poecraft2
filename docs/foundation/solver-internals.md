# Solver Internals And Source Ownership

**Status: stable implementation map.** This page explains the native solver's
private execution phases and where their contracts live. It does not select
future work or define crafting mechanics.

Parent: [Foundation](README.md)

Verified against current source: 2026-08-29 @ `22c00f5`. Scope: private solve
phase ownership, cooperative continuation boundaries, current
proof/publication boundaries, action-family contract, exact evaluator gated-
kernel authority, exact reforge row continuation, telemetry serialization
ownership, benchmark-private carrier-ladder boundary observation, the
benchmark-private verified leaf-fragment core, and the native source
inventory, plus benchmark-private generated-envelope attribution.
Final acceptance for a selected boundary is recorded in its result rather
than implied by this source audit.

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
| API, accounting, telemetry, and native-development coarse replay | C ABI results, typed progress snapshots, JSON, owned-byte and work counters, versioned completed coarse graph | `solver_api.cpp`, `solver_development_checkpoint.cpp`, `solver_solve_telemetry.cpp` (collection/accounting), `solver_solve_telemetry_json.cpp` (serialization), `solver_compile.cpp`, `solver_eval_report.cpp` |
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

### Generated-operator lineage and narrow phase owner

`SolveDiagnostics::operator_lineage_json` is the bounded observational join
over those existing authorities. It does not create another registry,
scheduler, admission ledger, or row ledger. Its complete aggregate follows
the native chain from permanent registry candidate/dependency roles through
generated fixed options and automatic kinds, canonical template/effect
telemetry, priced support, carrier/operator ledger state, retained sparse rows,
joint-policy attempts, and selected-policy consumption. Per-family relations
are intentionally non-disjoint because a compound fixed option may depend on
several primitive families. Generated-operator samples are deterministic by
planner index and bounded by `max_diagnostic_samples`; the complete generated
operator count and semantic hash are not sample-derived.

The projection is never read by mechanics, pricing, admission, scheduling,
Bellman work, proof, or publication. Its retained JSON allocation is excluded
from the solver-owned resource cap so observing the solve cannot alter the cap
boundary; the existing `max_telemetry_json_bytes` serialization boundary still
applies. Finalization invokes this projection independently of older optional
row diagnostics so a named resource refusal retains lineage even if one of
those diagnostics is itself interrupted.

`SolveProgress::phase_owner` and the append-only C ABI
`pc_solve_progress.phase_owner` refine the broad solve phase into setup,
planner construction, temporary-effect precompile, dependency preparation,
primitive rows, state-local automatic synthesis, ladder scheduling, Bellman
optimization, policy assembly, compilation, exact evaluation, or done. The
WASM progress object and benchmark bound trace expose the same string
vocabulary. Setup-only work that completes before the first stepped snapshot
remains visible as aggregate phase evidence in operator lineage; live progress
never claims those owners retroactively.

The benchmark-private automatic-kind mask has deliberately closed semantics:
omission preserves the complete product default, an empty list suppresses all
generated kinds, and a non-empty list admits only those finite enum values.
The mask is applied to both the product-envelope solver used to derive
`goal.actions` and the case solver that materializes rows. It is diagnostic
scope only: mechanics, the primitive registry, public requests, product
defaults, scheduling, proof, and publication do not read it.

The 2026-08-29 qualification held the primitive envelope fixed at the complete
product scope minus Fossil, the independently measured setup blocker. Under
that envelope, Fracture, temporary Bench blockers, protected metamods,
Eldritch-side programs, and cannot-roll programs each produced nonzero
operators and serviced rows. Permanent Bench, multimod finish, Imprint,
constructive renewal, and Veiled were structurally inapplicable under the
pinned case/profile rather than unobserved. All cumulative rows completed,
including successful joint assemblies. The remaining broad owner is
continuation/missing-upper frontier work after generated rows have been
served; neither generated construction nor carrier-ladder starvation
qualified a behavior repair.

## Benchmark-Private Carrier-Ladder Boundary Observation

`carrier_ladder_exact_boundary_v1` is a disabled-by-default native benchmark
diagnostic with `off`, `record`, and `recover` modes. It observes exactly one
failed selected-policy prefix inside finalization before ordinary incumbent
restore. Its retained copy is separate from the graph, Bellman values,
scheduler, proof lower, incumbent portfolio, compiler, and publication
pipeline. Ordinary finalization is frozen before any strict replay begins.

Record mode closes the complete routed selected prefix and emits typed stops:
exact goal success, a same-identity independently evaluated/proper/executable
ordinary frontier, or unresolved. Unresolved captured stops are explicit
non-goal absorbing observations during recovery; they never become successful
requested entries and are not executed as if they owned a selected action.
Recover mode starts from the authored exact item and follows only those
selected rows through the existing production strict oracle and native
primitive/fixed-option kernels. It does not
materialize an abstract representative, enumerate alternative actions,
optimize a replacement policy, or publish a value. Exact members at a named
coarse stop remain distinct by collision-checked complete item key.

Capture and replay have independent caps for prefix states, exact states,
rows, transitions, logical work, owned bytes, wall time, and samples. Missing
selected rows, stale identities, incomplete support, unsupported option
semantics, caps, and cancellation all fail closed. Private bytes and clock
time are excluded from ordinary solve accounting. Benchmark qualification may
also request an exact ordinary completed-row fence; zero preserves the normal
wall-clock requested-finish behavior.

The cumulative-10 qualification closed diagnosis-only: strict replay reaches
the captured unresolved non-goal coarse state 213 from the selected root Chaos
row. State 213 has no captured or completed selected row, so no support closure
is authorized and the named missing parent remains unrecovered. Recovery
retains one bounded first predecessor/action/outcome witness and streams the
reached-stop count/identity without storing every event. Complete exact keys
remain internal; public telemetry exposes their collision-checked identities
and word counts with complete coarse, operator, semantic, probability, row,
reachability, and completion facts. This attributes the next owner to ordinary
completed-row/service coverage rather than an action-catalogue omission or
fragment composition. The diagnostic remains private to the benchmark/Lab
path and adds no public ABI, release-WASM input, strategy vocabulary, or
product behavior.

## Benchmark-Private Verified Leaf Fragments

The native benchmark/Lab lane has one deliberately narrow executable-fragment
vertical slice. Its authority chain is non-convertible:

```text
ExecutableFragmentProposalV1 (heuristic annotations only)
  -> ExecutableFragmentIRV1 (versioned, finite, probability-free control)
  -> ExactLeafFragmentVerifierV1 (engine-owned primitive transitions)
  -> VerifiedLeafFragmentV1 (exact evidence for one exact entry)
  -> VerifiedLeafStructuralControlV1 (probability-free structural view)
  -> SingleFragmentFlattenerV1
  -> FlattenedFragmentCandidateV1 (ordinary strategy JSON candidate only)
```

Only the verifier can construct verified evidence, and only the verified
structural view can reach the flattener. Proposal estimates cannot become
probabilities, proof values, lowers, scheduler priorities, action-ledger facts,
incumbents, executable/public uppers, or publication authority. The fragment
types are private to `engine/benchmarks`, are linked explicitly into the native
benchmark and focused test targets, and are absent from
`engine/engine-sources.txt`, the C ABI, release WASM, and product defaults.

Version 1 verifies exactly one exact entry product state. It rebuilds every
row from native primitive outcomes, requires complete probability mass without
renormalization, rejects duplicate or missing outcomes, and permits product-
state or exit merging only on complete exact-key equality. It proves terminal
absorption over the positive-probability SCC graph, rejects closed livelock,
and solves finite expected action/resource vectors with explicit residual and
mass diagnostics. It does not claim nontrivial lumpability, behavioral
quotients, reusable entry domains, or multi-fragment composition.

Flattening is narrower still: every positive exit must be `FinalSuccess` after
internal recovery. Subgoal, recoverable, certification-failure, and other
non-final exits fail closed. The flattener receives no probability,
certificate, expected-resource, or cost view. Its ordinary strategy is parsed,
compiled, and evaluated through the existing production path, and a separate
forward evaluator checks mass, terminal disposition, action counts, resources,
and price reconciliation. Native Simulator acceptance is independent sampled
execution, not exactness authority.

The Solver Lab shadow runs only after the ordinary result is atomically
finalized, in a separately capped native process. Control and shadow requests
must have different full identities but equal `core_solve_identity_v1`, every
enumerated core input, and every ordinary result component; only the isolated
shadow diagnostic may differ. The shadow cannot mutate or lend storage to the
core solve and has no incumbent or product integration.

Retained regression probes cover proposal-authored probabilities, missing or
renormalized mass, duplicates, projection-only carrier merges, exit-identity
collisions, near-improper cycles, closed livelock, resource-accounting drift,
forged construction paths, and non-final flattening exits. Fragment artifact
identity also excludes generated-manifest timestamps and path spelling: it is
bound to the compiled artifact schema, source-data hash, game-data hash, and
strings hash, so approved regeneration of byte-identical data does not change
the IR or certificate identity.

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
| Native cross-process coarse graph checkpoint/replay | `solver_development_checkpoint.cpp`, the cache compatibility contract in `solver_solve_expand.cpp`, and the native C ABI adapter in `solver_api.cpp` |

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
- A resource cap may finish before the start state has a Bellman value row.
  Native finalization then publishes an unavailable start value (positive
  infinity in the C ABI and `null` in JSON), a named resource-cap termination,
  and no policy. It must not index a missing row, fabricate closure, or emit a
  strategy. A previously certified executable fallback remains a separate
  bounded-feasible publication contract.
- Crafting legality and probabilities stay in the native engine. Compilation,
  evaluation, WASM, and TypeScript consume native results; they do not recreate
  mechanics.
- Private state IDs, refined class IDs, compiled node IDs, and simulator
  handles are scoped to their owner. They are not durable cross-run identities.
- Public C ABI types and exports live under `engine/include/poecraft/`; the
  private headers on this page are not ABI.

The native-development coarse checkpoint serializes a completed reusable
`SolveTransitionCache` together with the calculator state/operator/admission
namespace that gives its numeric IDs meaning. It requires exact caller and
graph-option compatibility and then reruns Bellman, strict refinement,
compilation, and evaluation. It is not a product or proof authority and is not
exported by release WASM. Checkpointing an in-progress strict partition remains
deferred until that larger proof-carrying representation has a stable joint
ownership boundary.
