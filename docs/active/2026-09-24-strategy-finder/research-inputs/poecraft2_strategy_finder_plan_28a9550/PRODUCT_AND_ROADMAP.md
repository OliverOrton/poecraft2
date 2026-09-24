# Product modes, parallelism, GPU and later proof integration

## 1. First product interface

Keep one Calculator form and one request/economy/action-price resolution path. `buildCalculatorSolverGoal` already distinguishes odds, product-envelope and priced scoped-solve requests. Do not add a second form that reconstructs goals or action prices in JavaScript. The native odds handle remains separate from the scoped solve handle. [R08–R09]

Initial selector:
- **Current solver** (unchanged default).
- **Strategy finder — experimental** (only when the loaded native/WASM build advertises support).

Do not label the first item “guaranteed exact” when a run can return bounded. Do not advertise a learned solver before training exists. Portfolio/hybrid modes remain absent or explicitly unavailable, not aliases for the same implementation.

Freeze mode/algorithm configuration in the submitted request, import/export and telemetry. Editing the selector during a running solve affects the next invocation, not the current one. Old saved drafts/old smaller options structs select the current solver. Unknown future modes fail explicitly. If a finder cannot service the request, return that limitation; a legacy fallback must be an explicit separately identified portfolio treatment, not hidden work.

Use existing begin/step/compact-progress/Finish/cancel/export/handle-release transport. Finder progress displays best independently checked policy and search stage. Keep prediction diagnostics visually separate. Do not display optimality gaps from heuristic scores. Per-state legacy queries, proof handoff or converged/exact flags must not acquire invented values.

A mode selector is enabled after F1/F2 native pipeline correctness on real requests, not only after a large cost improvement: the user asked to experiment directly. It stays experimental and default-off until fair quality/latency coverage supports promotion. Returned strategies remain ordinary editable/executable documents; no model needed to run them.

## 2. Parallelism

Initial execution is serial: one finder owner and one active checker. This makes budget, cancellation, reproducibility and error diagnosis tractable.

First later parallel option: independent proposal/check workers with independent mutable native contexts and immutable request/artifact data where genuinely safe. The source map does not prove thread safety of lazy session/calculator caches. Audit mutation before sharing a session across threads; separate processes may be simpler, but per-process memory is not free.

A future bounded pool needs global admission for concurrent owned/RSS memory, one request generation, stale-result rejection, cancel/join/release, reproducible candidate identity, and fair total CPU-work accounting. Scheduling order can change which candidate fits before the deadline, so deterministic single-worker controls remain useful.

Do not parallelize a coroutine by concurrently resuming it. Do not give each worker the entire aggregate memory limit. OS read-only sharing and logical per-solve charging are separate measurements.

## 3. GPU

No CUDA port is selected. The likely first use is optional offline training of a small role/set model, or large batched inference after CPU feature/inference cost becomes significant. The current checker/calculator are irregular native workloads with mature ownership contracts; a GPU rewrite is a different project.

Measure batch construction, transfer, device execution, result transfer and achieved search/check throughput end-to-end. If the finder makes only a few strategic ranking batches while spending seconds checking candidates, faster inference may have negligible product benefit.

The browser must retain a CPU-capable mode and deterministic score fallback. Adding a browser inference runtime/model distribution format is a later product decision, not a dependency of the first selector. No training data leaves the user's project through an external service as part of this plan.

## 4. Learning gates

See [DATA_AND_LEARNING.md](DATA_AND_LEARNING.md). Start with heuristic ranking, then a simple supervised ranker, then a small role-aware network only if data/generalization justify it. Freeze model/feature versions per invocation. Record training compute separately from inference solve time; do not hide extensive offline expert work when reporting amortized results.

Expert iteration is an appropriate later loop: search → checked candidates → labelled failures/successes → new model → stronger search. The current two candidate checks and many correlated entries of one policy are not enough to infer that this loop is already learnable.

## 5. Later hybrid into exact proof

Only after the finder is useful and its artifact contract is qualified:
1. Confirm exact original problem/allowed grammar/prices and numerical contract.
2. Import the immutable complete graph/certificate through an explicit incumbent API.
3. Recheck when source/model/goal/scope identities require it.
4. Give the proof solver a root feasible witness; leave any statewise continuation import to its own correspondence check.
5. Keep lower producers, all-action coverage and exactness closure entirely owned by the existing proof path.

No heuristic vector, learned Q table, role-state ID or candidate-local lower crosses this boundary. A better root upper can help existing valid pruning but need not move the lower ceiling.

## 6. Distinct future action-budget feature

Expected primitive count, a hard per-execution horizon and a completion-probability constraint are different objectives. Initial finder still minimizes expected original Chaos subject to proper goal completion. Count is measured, not silently added to the cost. A later search bias using c+lambda*n must identify its proposal role, and accepted winners are still compared by the requested objective.

## 7. Roadmap order

F0–F4: request-bound checker + standalone peer finder + heuristic sketch search + experimental UI + fair baseline/data.

Then select ONE: improve demonstrated grammar/verification bottleneck; supervised ranking; or independent proposal/check parallelism. ML, GPU and proof import do not all become the automatic next session.
