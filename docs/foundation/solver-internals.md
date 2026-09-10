# Solver Internals And Source Ownership

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This is an implementation map. The [mathematical reference](../solver/mathematics/README.md) explains why the contracts compose; [solver mechanism pages](../solver/README.md) describe the individual boundaries. Read the row relevant to the task rather than treating this entire map as startup material.

## Execution Flow

```text
native request / product request
  → resolved native goal, action grammar, prices, and layout
  → exact row construction and explicit open obligations
  → search, lower proofs, and executable candidate construction
  → direct assertion / strict refinement when required
  → ordinary compiled strategy and independent evaluation
  → compatible bounded or exact result
```

Native mechanics are shared, but physical items, calculator states, strict carriers, quotient cells, and strategy operations remain different identities. The evaluator's product includes the operation and applicable observation/checkpoint context as well as item state.

## Phase And File Map

Paths in this table are under `engine/src/` unless indicated otherwise.

| Boundary | Main owners | Mathematical correspondence |
|---|---|---|
| Request, goal, registry, family scope | `solver_api.cpp`, `solver_model.hpp`, `solver_registry.cpp`, `solver_action_family_contract.hpp` | Complete target/action scope; CLM-0008, CLM-0009 |
| Layout and exact observation classes | `solver_abstract.cpp`, `solver_calc_types.hpp`, `solver_calc.cpp` | Markov information, equality and uniform coverage; CLM-0005, CLM-0006 |
| Fixed and automatic programs | `solver_options_build.cpp`, `solver_options_automatic.cpp`, `solver_options_temporary.cpp`, `solver_options_import.cpp`, `solver_options_semantics.cpp`, `solver_options.cpp` | Choice timing, mandatory costs, exits; CLM-0003, CLM-0004 |
| Primitive and destructive transitions | `solver_calc.cpp`, `solver_reforge.cpp`, `solver_calc_types.hpp` | Complete native row, applied effects, coefficient provenance; CLM-0015, CLM-0017, CLM-0023 |
| Reachability and source-local obligations | `solver_solve_expand.cpp`, `solver_solve_incremental.cpp`, `solver_action_envelope_ledger.hpp` | Missing work is not excluded scope; CLM-0008 |
| Scheduling and focused work | `solver_anytime_scheduler.hpp`, `solver_solve_focused.cpp`, `solver_solve_priority.cpp` | Ordering/proof separation and progress assumptions; CLM-0022 |
| Sparse Bellman and fixed-policy work | `solver_sparse_policy.cpp`, `solver_solve_bellman.cpp` | Proper-policy equations and numerical contract; CLM-0001, CLM-0002, CLM-0023 |
| Bounds and their ordinary consumer | `solver_proof_pattern_manager.hpp`, `solver_solve_bounds.cpp`, `solver_solve_carrier_pattern.cpp`, `solver_solve_operator_proof.cpp`, `solver_solve_envelope_proof.cpp` | Native lower validity, composition, eligibility; CLM-0007 through CLM-0013 |
| Native phase/retention proof | `solver_phase_lower.*`, `solver_phase_probability.cpp`, native integer-pool seams | Conditional events, frozen minima, complete domain/program bridge; CLM-0014 through CLM-0019 |
| Lower-only numerical query | `solver_quotient_lower.*`, `solver_action_coverage.hpp`, existing `QuotientBellmanGraph`/`ProofStore` | Declared-model feasibility is not native validity; CLM-0008, CLM-0012, CLM-0023 |
| Candidate construction and retention | `solver_solve_constructive.cpp`, `solver_solve_finish.cpp`, `solver_solve_types.hpp`, `solver_joint_policy_continuation.hpp` | Entry-scoped upper, snapshot/release contract; CLM-0002, CLM-0021 |
| Exact refinement/partition | `solver_refinement*.cpp`, `solver_refinement_graph_core.hpp`, `solver_refinement_graph_discovery.hpp`, `solver_quotient_partition.cpp`, `solver_quotient_proof.cpp` | Carrier-wide rows and strict alternatives; CLM-0005, CLM-0020 |
| Production policy oracle | `solver_policy_refinement.cpp`, `solver_policy_oracle_*.inc`, `solver_policy_assertion.cpp` | Exact observations, choice/entry scope, publication premises |
| Compilation | `solver_compile.cpp`, `solver_compile_conditions.hpp`, `solver_compile_serialization.hpp` | Executable route and literal operation preservation; CLM-0002, CLM-0004 |
| Independent graph evaluation | `solver_eval.cpp`, `solver_eval_resolve.cpp`, `solver_eval_report.cpp`, supporting type/helper headers | Properness, full mass, resources, numerical endpoints; CLM-0002, CLM-0023 |
| Result, telemetry, and replay | `solver_api.cpp`, `solver_solve_telemetry.cpp`, `solver_solve_telemetry_json.cpp`, `solver_development_checkpoint.cpp` | Provenance, semantic dependencies, final classification; CLM-0021, CLM-0024 |

These registered claim IDs map implementation responsibilities to propositions in the [claim ledger](../solver/claims.md). Each claim's latest history records its status and scoped source correspondence; registration, source annotations and traceability lint do not discharge its mathematical or native premises. See the explicit [research gaps](../solver/research.md).

## Retained Authorities

`ActionEnvelopeLedger` owns source/operator work and proof retirement. `SolveScheduler` owns service order. `ProofPatternManager` owns admissible contributions and maximum composition. `IncumbentPortfolio` owns verified artifacts separately from estimates. `PublicationPipeline` owns assertion, repair, classification, and packaging. `SolveTelemetrySnapshot` observes their typed state.

None of these changes mechanics or scope by naming an item complete. A lower-only certificate is not an executable policy; a completed numerical solve is not native projection proof; a retained candidate is not a public incumbent.

### Generated-operator lineage and narrow phase owner

`SolveDiagnostics::operator_lineage_json` is a bounded observational join across those owners. It follows registry roles through generated programs, prices, ledger states, completed rows, assembly, and consumption. Its complete count/hash is not reconstructed from a truncated sample. Program dependencies can make family relations non-disjoint.

`phase_owner` refines broad phases into setup, planner/dependency construction, primitive and automatic work, ladder service, Bellman work, assembly, compilation, evaluation, or completion. Setup before a first public step may have aggregate timing without a live internal sample.

Diagnostic JSON allocation has its documented separate serialization budget; exclusion from a solver proof cap does not imply no process memory or time. No ordinary decision may read the diagnostic projection.

## Benchmark-Private Carrier-Ladder Boundary Observation

`carrier_ladder_exact_boundary_v1` has `off`, `record`, and `recover` modes. It captures a failed selected-policy prefix before ordinary incumbent restoration, after freezing the ordinary result for the observational comparison.

Recovery follows only that prefix from the authored exact item through existing native kernels. It does not optimize a replacement policy, enumerate all alternatives, or treat a coarse representative as an exact entry. Stops distinguish true success, compatible independently executable frontier, and unresolved non-goal observation.

Capture/recovery keep independent caps and identities. Missing rows, stale identities, incomplete mass, unsupported option semantics, cancellation, and resource stops remain refusals. The two-stage service witness distinguishes an early queue observation from later natural service; its historical diagnosis does not select another scheduler repair.

The common diagnostic source can be compiled into WASM while remaining inaccessible through product/WASM inputs. That is different from the fragment sources below, which are benchmark/test-only translation units.

## Benchmark-Private Verified Leaf Fragments

The retained fragment lane is:

```text
proposal → probability-free control IR → exact native verifier
         → verified exact-entry evidence → structural control view
         → ordinary flattened strategy candidate
```

The verifier rebuilds complete native primitive mass, rejects missing/duplicate or renormalized outcomes, preserves complete exact identity, checks goal absorption and closed livelock, and evaluates finite resource/cost evidence. Version 1 verifies one exact entry; it does not establish a general reusable member domain or multi-fragment composition.

Flattening is narrower: every positive exit must be final success after internal recovery. Other exit kinds refuse. The flattener receives structural control, not authority to invent probabilities or publish costs. The resulting graph still passes the ordinary parser/compiler/evaluator and its independent checks.

The sources live in `engine/benchmarks`, are linked explicitly by the benchmark/tests, and are outside `engine/engine-sources.txt`, the C ABI, release WASM, and product defaults. The isolated Lab shadow runs after ordinary finalization in a separate capped process. Its full request differs while matching core input and ordinary-result evidence; it cannot lend state or authority to the ordinary solve.

Do not reactivate this lane merely because a new continuation problem resembles a fragment. Conversely, its regression/oracle role must be checked before removal.

## Private Header Layers

The main private declaration chain is:

```text
solver_model.hpp
  → solver_calc_types.hpp
  → solver_eval_types.hpp
  → solver_solve_contracts.hpp
  → solver_compile_contracts.hpp
```

Use the narrowest appropriate header. `solver_internal.hpp` remains a compatibility umbrella for deliberate cross-phase callers, not the default dependency of a new leaf file.

`solver_policy_refinement.cpp` keeps the anonymous `ProductionPolicyOracle` in one translation unit; named `.inc` files divide its concerns without changing linkage/lifetime. `solver_eval.cpp` similarly retains its coupled work lifecycle while resolution and serialization have separate translation units.

Core model changes can still rebuild most solver files. Header layering does not prove low fan-out or a cheap edit.

## Where Should I Make This Change?

Use the phase table to find the owner, then read its mechanism and relevant mathematical premises. A cross-layer change follows [change impact](change-impact.md). A mechanic question belongs to Oliver and the existing mechanics library, not an inferred mathematical repair.

For retention work, separate preparation/proposal coordinates, native probability/effect evidence, lower-only numerical acceptance, and ordinary uniform lookup. Changing only one does not automatically broaden the others.

For a purported unused component, search consumers, includes, tests, registrations, inventories, diagnostics, and reproduction needs locally. A removed planner's surviving projection is not listed above as an upper issuer; indexed absence alone does not authorize deleting it.

## Determinism, Exactness, And Lifetime Invariants

Stable ordering, tie rules, collision-checked keys, literal selected-operation payloads, accumulation order, and native `-ffp-contract=off` remain part of reproducibility. Preserve actual ownership and caps when moving declarations or storage.

Broad rows publish only on completion. Strict frontiers return to their grow-in-place owner. Open incremental search publishes only independent lower evidence. Proof retirement never fabricates a row. A missing start row yields the explicit unavailable value, named stop, and no invented policy; a separately verified fallback has its own contract.

The emitted evaluated graph owns the public upper. The completed coarse replay owns reusable development state, not proof authority. Final mathematical equality, numerical status, and product classification remain separate until their compatibility premises are met.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md) and [lower-pruning.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md), [publication.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md), [2026-08-30-carrier-ladder-released-candidate-reclamation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md), [CMakeLists.txt](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/engine/CMakeLists.txt). Claim IDs are registered in [the ledger](../solver/claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](../solver/research.md#open-obligations); an argument link does not confer runtime authority.

The post-review [filter continuation qualification](../archive/2026-09-05-native-metamod-first-exit-v1/README.md) adds crafted-filter domains and separately reserved final diagnostics in `solver_phase_probability.cpp`. The ordinary projection profiler remains in `solver_solve_bounds.cpp`; measured result caching was removed. See [the current lower contract](../solver/lower-pruning.md#filter-continuations-and-final-diagnostic-ownership).
