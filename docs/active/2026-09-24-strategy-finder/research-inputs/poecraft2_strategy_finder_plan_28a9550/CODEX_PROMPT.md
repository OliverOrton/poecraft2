# Implement a separate experimental strategy finder: F0–F4

Repository: OliverOrton/poecraft2  
Reviewed main: `28a955056a41699d1fa9996ee345b70678937f21`  
Selected task: **F0–F4 below**. This replaces automatic continuation of the closed N0–N4 boundary-patch experiment.

## 1. Objective

Build a genuinely separate heuristic policy-discovery lane that shares native mechanics, macro semantics, ordinary strategy compilation/execution and independent checking. It starts from the user's original item and returns the best complete request-bound checked strategy it finds within the original budget. Initially STOP at that artifact. No learned estimate, finder state, candidate-local lower or approximate representation enters the existing optimality prover.

The first algorithm is bounded best-first/beam search over native conditional controller structures, with handcrafted ranking and lazy expensive native construction. It is NOT a second simulator, a beam of fortunate action trajectories, a full CUDA port, or the old solver with a proof flag disabled. A trained model is a later interchangeable ranker; no neural/GPU/concurrency project is selected now.

Oliver wants broad role-based transfer across unequal tiers/weights and a Calculator mode selector. Relative features can be approximate. Backend probabilities, legality and checked values cannot be borrowed across merely similar states.

## 2. Working rules and baseline

Read current AGENTS/HANDOFF and reconcile current main/local changes. Preserve unrelated work and protected root `0` without inspecting/staging/changing it. Work sequentially, no subagents or unrequested push. Native engine/data own all mechanics; ask Oliver about genuinely ambiguous mechanics. Use current runner/supervisor and long waits, not LLM polling loops.

The last programme checked all 55 A5 compiler decisions and 53,851 physical entries. A single exact-context recurrent Eldritch patch improved C85558.70618560436 to C85520.72841064134 (0.04439%) for 45.416 seconds of speculative service; another complete candidate cost 94,699.18161992717. Source was restored, no production mode/WASM change retained. That positive graph is a regression/economic fixture, not a saved answer to seed claimed discovery. Do not replay this narrow one-source programme or require the old clean-one-goal-missing entry gate.

The latest CI snapshot is in evidence/review.json. Recheck current status rather than assume prior results. A real new failure gets a focused disposition, not an automatic broad maintenance campaign.

## 3. Required read map

Start with IMPLEMENTATION_PLAN.md and ARCHITECTURE.md, then SEARCH_DESIGN.md and VALIDATION.md for the selected checkpoint. Use DATA_AND_LEARNING.md and PRODUCT_AND_ROADMAP.md for later boundaries, not tasks to execute all at once.

Relevant current owners:
- `solver_api.cpp`, public solver.h: parse/create/options/direct SolveWork ownership and every lifecycle endpoint.
- `solver_calc.*`, `solver_registry.cpp`, native action-family contracts: physical/model/scope identities.
- `solver_options_helpers.hpp` / automatic/build/semantics owners: existing descriptor synthesis and expensive native kernels.
- `solver_compile_contracts.hpp`, `solver_compile.cpp`, conditions/serialization: ordinary native program emission and graph-local provenance.
- `solver_eval_types.hpp`, `solver_eval_helpers.hpp`, `solver_eval.cpp`: full stochastic product, observations, properness, original-cost evaluation.
- existing retention validity/complete-bundle helpers; do not copy all legacy Impl into the finder.
- `solve-workspace.ts`, engine-protocol/client/worker/wasm, `components/pc-calculator.ts`, and current result/export owners (under `apps/web/src/app/`).
- `docs/foundation/tooling.md`; existing natural-T1 generator and evaluation-role files.

The fragment experiment is benchmark-private, single-exact-entry and FinalSuccess-flattening only. Its source contract may help tests; it is not an already generalized product finder to enable by flag.

## 4. Authority contract (implement before search)

Separate immutable problem identity, algorithm/run identity and candidate artifact identity.

A candidate supplies structural control + native action/program/condition references. It cannot supply authoritative probabilities, rewards, original goal truth, properness or exactness. An accepted artifact owns graph, native request/context binding and evaluation receipt together.

Critical negative case: faithfully evaluating a graph's self-declared SUCCESS is not sufficient. Bind checking to the external original root/goal/rarity/clean terminal, permitted action/program scope, prices and artifact. Add original-target observation requirements if the chosen evaluator representation needs them. Checking only truncated terminal examples is not sufficient. Preserve the general Strategy Builder's authored semantics unless a separately selected change requires otherwise.

The main compiler takes CalcContext+SolveResult. Introduce a narrow proposal compile view/entrypoint if needed; never fabricate solved/converged/lower/proof fields to use it. Reuse native condition/program emitters and provenance. Do not reimplement PoE rules in Python/TypeScript or create a universal new DSL.

Only full request-bound native acceptance can publish a policy. Keep complete mass, zero reached illegal/off-policy/unknown/STOP/failure mass, paid recovery, properness and current numerical tolerance/provenance checks. The current evaluator is not automatically an outward-rounded rational checker: preserve its exact documented numerical meaning.

Heuristic omissions are permitted in this lane. They cannot retire proof actions or delete chance outcomes from a chosen policy. Unknown tails may guide an incomplete proposal but are not executable. A finite stage requires controller memory; a recurring router patch is evaluated as recurring.

## 5. F0 — authority and interface checkpoint

Trace the shared native request and current Calculator resolution. Define the minimal problem/candidate/checked-artifact views and branch point before legacy proof work. Audit actual requested-goal binding and implement focused fake-success/wrong-root/scope/price tests. Identify a small native from-root seed construction/emission seam. Do not begin ML, a corpus census or file-size cleanup.

Done: existing valid fixture passes; wrong-goal/self-certified proposal fails; actual source/API responsibilities are recorded. Choose/record minimal adapters rather than treating proposed names as preexisting code.

## 6. F1 — standalone native pipeline

Create a peer `PolicyFinderWork` or equivalent outside legacy Impl. Add a small lane dispatcher for begin, step, Finish, cancel, progress, telemetry and result/strategy queries. Old/smaller option structs and omitted mode select the current solver. Unknown modes fail. New mode cannot accept proof handoff or fabricate per-state exact values.

Generate native seed controllers from ORIGINAL root and admitted grammar (existing renewal/constructive mechanisms where supported), without hidden legacy SolveWork or archived strategies. A missing seed returns truthful no-verified-policy. A saved-seed diagnostic is labelled separately and never counts as standalone discovery.

One checker live initially. Charge all contexts/beam/artifacts/checking memory and native cumulative work to the original budget. Failure never clears the previous checked winner. Finish seals a checked artifact; Cancel releases actual work before reporting completion.

Done: two real supported root fixtures produce ordinary checked graphs; no-policy/cap/cancel tests pass; legacy behavior preserved. This is an engineering checkpoint, not a claim of superior quality.

## 7. F2 — heuristic controller search

Implement a bounded frontier of finite controller sketches/partial completions. Native supported program blocks, real condition branches, sequences/stages and retry/recovery are its constructs. Use descriptive native action/goal-role binding before expensive kernels. Do not invent “get role X” as a mechanic.

Include a coordinated acquisition/protection/cleanup/follow-through alternative, not just the previous exact-item patch or extra stopping-set labels. Partial candidates may look worse before a joint completion; do not demand immediate strict root improvement for every edit. Complete candidates still need every native outcome defined or checked fail-closed.

Start handcrafted scoring with stable ties and modest structural diversity. Provide a small batched ranker interface over feature views; no ML dependency. Preserve source records and feature-availability masks. Role feature equality is never a native state/transition/certificate key.

Use SEARCH_DESIGN.md pseudocode and CANDIDATE_INTERFACE.md for the minimum concrete control view. Calibration starting knobs may be frontier width 16 and maximum8 new complete checks, with one live checker, but time/work/memory caps remain authoritative and all actual values are frozen in treatment metadata. These are tunable implementation choices, not mathematical claims. No broad grid search.

Feedback categories: complete accepted/rejected-economic; invalid native contract; unsupported representation; incomplete/capped/cancelled (censored). Record stage costs and exact candidate/problem identities. No timeout→infinite crafting-cost label. An invalid score uses deterministic fallback ranking.

Done: alternative complete original-root candidates really reach the checker, including heterogeneous role bindings and a coordinated decision fixture. No model is needed to execute the emitted graph. If grammar duplicates all seed candidates, fix one demonstrated native-supported structural gap or report it; no speculative framework with no consumer.

## 8. F3 — experimental Calculator mode

Reuse the existing form and `buildCalculatorSolverGoal`, priced scope and frozen economy. Offer Current solver(default) and Strategy finder(experimental) only when supported by the loaded build. Mode/config round-trip through all bindings/export/trace; edits mid-run apply only next run. Do not silently fall back or silently change action exclusions, initial rarity, gating, Restart/Imprint or prices.

Preserve compact numeric stepping, actual cancellation/Finish/export and ordinary editable strategy output. Finder summary displays checked cost/policy status; unavailable optimality bounds/gaps are null/absent, not heuristic numbers. No exact/convergence claims from beam exhaustion.

Rebuild matching WASM and run actual Calculator/worker controls for this changed layer. Old native options and default behavior stay compatible. The historical 2.643-second C4 ordinary-call tradeoff is not a 250-ms pass or unlimited permission for regressions. Rendered visual review stays Oliver's.

## 9. F4 — evidence and retention decision

Use the current runner/corpus comparators. Same original problem/profile and total budget, intentional lane/config treatment only. Charge initialization, seed search, scores/features, laws, compile/check, failed attempts, export and simultaneous memory. Equal checker counts alone are not equal compute.

Develop on native A3/B3 and A5; preserve A4 and exact Regalia. Expand to suffix/bow/Ring contexts when the supported family warrants it. Existing six research cases are exposed development material. Select untouched native-generator strata BEFORE model/tuning/generalization tests; do not random-split correlated entries of one policy.

Report best CHECKED cost versus actual 30/60/240-second time, no-policy periods, final cost/count, work, memory, validation refusals and build/check expense. Existing C4=3746.1319409485764, A5=85558.70618560436; also report comparison to the tiny checked diagnostic cost 85,520.72841064134 where compatible without relabelling it deployed.

Engineering success and economic success are separate. A functional testable experimental lane can be retained without 20% savings. A 20% hard-case reduction is a material research target, not a correctness threshold; smaller gains get honest results. Default promotion is conditional on fair broader results and preserved controls. No assertion that a separate lane must outperform the existing one.

Collect only necessary learning-ready records and bounded data-cost calibration (existing reports first). No huge dataset, training, GPU or pool is authorized. Later proposals may choose supervised ranking, stronger grammar or checking improvements based on actual bottlenecks.

## 10. Documentation and stop

Follow DOC_MAINTENANCE.md: one living record and short HANDOFF; canonical math for authority/role/cyclic-control obligations; actual flow/API/product support. No new theorem IDs for ordinary interfaces, and no hand-edited generated research state.

Stop for unresolved mechanics, invalid request binding, unreconciled mass/cost, or unsafe ownership. After two substantive failed research variants without a new native premise, return the specific blocking boundary rather than broadening scope silently. This does not limit ordinary test-driven debugging of the selected interfaces. Do not remove cheap useful scoped lower information from search by ideology; do not compute expensive full proof models just to fill a field.

Before handoff state completed F checkpoint, source/dirty/build identities, actual supported mode, test failures/unrun checks, candidate graph/cost/status and exact output paths. No push is authorized. Learned ranking, exact proof handoff, whole-solver multithreading, action-budget optimization and GPU port remain later separate decisions.
