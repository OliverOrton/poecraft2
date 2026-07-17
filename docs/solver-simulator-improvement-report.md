# Solver And Simulator Improvement Report

**Status:** point-in-time engineering report, written 2026-07-16 at Oliver's
request during S7.6. It surveys how the optimal solver and the native
simulator can be improved for complicated multi-step crafts — faster and
lighter while keeping or improving exactness. It is not an execution plan,
changes no code, sets no acceptance criteria, and does not reopen owner
decisions. Active sequencing authority stays with
[solver-depth-and-performance-plan.md](solver-depth-and-performance-plan.md);
stable architecture stays with
[crafting-solver-plan.md](crafting-solver-plan.md).

## S7.6 Outcome Addendum

The body below is retained as the pre-implementation engineering analysis.
The exact low-risk structural recommendations were subsequently implemented
and measured in the final S7.6 reports:

- fixed-policy evaluation, expansion, quotient construction, Tarjan SCC work,
  and sparse solves are resumable exact work units;
- deterministic double-double accumulation plus disabled FP contraction gives
  identical native/WASM endgame start values (`132353.19529787666`);
- the final endgame WASM maximum worker step is 29.006 ms against 50 ms;
- exact simulator routing sustains 984,077,152 endgame primitive actions in
  1,523.755 seconds (1.5484 microseconds/action) with one-run progress chunks;
- the advanced 8.743745% discrepancy was traced to non-lumpable junk
  exclusion-group merging. Full exclusion signatures reduce the fresh pinned
  delta to 0.670420% without changing the sampled primitive behavior;
- the single fresh endgame 10,000-run sample finished in 25.58 minutes with a
  1.230854% mean-cost delta, but its 0.9942 success rate missed the approved
  0.995 gate. S7.6 therefore remains open for an owner decision; the sample
  was not repeated or combined.

See [HANDOFF.md](../HANDOFF.md) for exact report names, caps, memory, material
totals, acceptance evidence, and the remaining boundary.

The analysis treats the working-tree fixed-policy improvements (exact
policy-kernel quotient, warm-started BiCGSTAB, tighter fixed-policy
tolerance, new policy-evaluation telemetry in `solver_solve.cpp`) as landed
current state. Evidence is code reading plus the existing
`build/performance` reports; no new benchmark was run for this report.

## Evidence Base

Numbers below come from the S7.6-final reports
(`build/performance/{native-solver,wasm-worker-solver}-s7.6-final-case-*.json`),
the S7.6 handoff, and the S7.5 checkpoint record.

| Case | States / rows / stored transitions | Peak solver bytes | Native solve | Native verification (10k runs) | Forecast vs empirical mean |
| --- | --- | ---: | ---: | ---: | ---: |
| oracle-real-one-mod | 8 / 12 / 38 | 0.30 MB | 0.135 ms | 0.4 s | +1.50% |
| oracle-real-two-mod | 16 / 36 / 230 | 0.31 MB | 0.185 ms | 18.6 s | +0.18% |
| ordinary-es-bench | 241 / 1,119 / 11,446 | 1.45 MB | 11.98 ms | 17.9 s | −3.03% |
| advanced-es-resist-bench | 577 / 2,700 / 83,605 | 6.72 MB | 51.53 ms | 171.7 s | −8.74% |
| endgame-fractured-es | 3,727 / 24,129 / 10,130,980 | 373.6 MB | 3,554 ms | cancelled > 30 min | unavailable |

Endgame native phase split: optimization 1,316 ms, expansion 228 ms (of
which transition calculation 99 ms), extraction 46 ms, compile 100 ms,
across 9 focused-expansion rounds and 7 policy-improvement rounds
(residual 1.7e-8, zero proof gap). The reforge cache absorbed the
expansion cost that used to dominate: 11,173 requests, 11,167 hits, 6
misses, 2,047,904 frontier work units.

Three structural facts drive most of what follows:

1. **Transition storage dominates memory.** Endgame stores 10.13 M sparse
   transition entries for only 24,129 rows (average ~420 successors per
   row) and 3 junk classes. The retained arrays (~121 MB of
   successor/probability data plus variants and hash structures, ~374 MB
   at peak) are the memory story; abstract states themselves are 144 B
   each and irrelevant.
2. **The optimization unit is indivisible.** One
   `run_policy_iteration_unit` = one full fixed-policy SCC evaluation plus
   one full Q-scan. The WASM worker's adaptive slicing
   ([engine-worker.ts:313](../apps/web/src/app/engine-worker.ts) targets
   12 ms) cannot subdivide it: endgame WASM recorded one 2,248 ms slice
   against the approved 50 ms budget, and even native with
   `solve_step_work_items = 1` recorded a 266 ms step.
3. **Verification cost is per-action throughput, not a hang.** Measured
   from completed runs: ordinary ≈ 1.2 µs per simulated action (14.7 M
   actions in 17.9 s), advanced ≈ 3.8 µs (44.9 M actions in 171.7 s). The
   per-action cost grows with compiled-router width (41 vs 133 regions).
   Endgame's forecast is 132,889 chaos-equivalents with chaos priced at 1,
   so a run plausibly executes on the order of 10^5 actions; 10,000 runs
   is on the order of 10^9 actions ≈ tens of minutes to hours
   single-threaded. The >30 minute cancellation is consistent with
   throughput, not a defect.

## Part 1 — Solver Improvements

### 1.1 Chunk fixed-policy evaluation into resumable work units

**Problem.** `evaluate_fixed_policy` (quotient build, Tarjan SCC pass,
per-component dense elimination or BiCGSTAB) runs to completion inside one
work item, making the 50 ms worker budget unreachable for large closures
regardless of scheduling.

**Change.** Make the policy-evaluation state machine resumable at three
natural boundaries: (a) quotient/kernel grouping in bounded state batches,
(b) one SCC component (or a bounded batch of small components) per step,
(c) a bounded number of BiCGSTAB iterations per step inside a large
component (the iteration loop in `solver_solve.cpp` already tracks
per-iteration state; warm-start already exists). The Gauss-Seidel seeding
passes and `select_policy_rows` scans chunk the same way the Bellman
fallback already does.

**Effect.** Structural fix for the endgame worker-step failure; no
numerical behavior change at all (identical arithmetic, identical order,
merely yielded). Also gives cancellation a bound inside optimization.

**Accuracy risk.** None.

### 1.2 Warm-start policy across focused-expansion rounds

**Problem.** Each of the endgame's 9 focused rounds resets
`policy_rows`, re-runs 4 full Gauss-Seidel seeding passes, re-runs
`prepare_priced_rows`, and re-converges policy iteration from scratch
(`begin_focused_lower_solve` / `finish_focused_lower_solve`). Values are
already carried across rounds; the policy and priced rows are not. That is
~36 full-table seed sweeps plus repeated re-pricing on the endgame case.

**Change.** Carry the previous round's `policy_rows` into the next round
as the initial policy wherever the row set for a state is unchanged
(newly expanded states start unset and fall back to the current seeding
path); re-price only rows added since the previous round.

**Effect.** Multiplies through the dominant phase: optimization cost per
round drops toward one evaluation plus few improvement rounds. Seeding is
initialization only, so any proper initial policy is equally valid.

**Accuracy risk.** None if the improper-policy repair path is kept: a
carried policy that becomes improper after expansion is exactly what
`repair_improper_policy` plus the fixed-policy properness check already
handle.

### 1.3 Incremental policy improvement

**Problem.** `select_policy_rows` re-evaluates every row of every state
every improvement round (`bellman_action_evaluations` = 289,548 endgame).
Howard iteration needs few rounds, but each round is a full scan.

**Change.** Track which states' values changed materially during the last
fixed-policy evaluation and re-scan only states with a changed successor
(reverse-dependency lists are derivable from the CSR arrays once). Keep
one final full scan as the stability proof before declaring the policy
stable, so convergence semantics are unchanged.

**Effect.** Removes most Q-scan work in late rounds; the final full pass
preserves the exactness argument.

**Accuracy risk.** None with the closing full scan.

### 1.4 Native parallelism (optional, larger lift)

Q-scans over states, the quotient build, and BiCGSTAB mat-vecs are all
data-parallel. A native thread pool with deterministic reduction order
(fixed-size block sums combined in index order, no atomics-order
dependence) would preserve bit-identical results while scaling with
cores. WASM should not follow (pthreads require COOP/COEP headers and a
SharedArrayBuffer deployment posture — an owner-level product decision),
which is acceptable: 1.1–1.3 already fix the WASM budget structurally.
This is the highest-effort solver item; profile after 1.1–1.3 land.

### 1.5 Transition storage: measure, then compress the cold copy

**Problem.** 10.13 M stored transition entries (~121 MB) at endgame, with
kernel dedup already sharing byte-identical rows across states. Harder
crafts (more goal slots, more junk classes — the stress case derives 122
junk classes / 42 discriminating tags) grow this multiplicatively toward
the 60 M cap.

**Changes, in order of safety:**

1. **Telemetry first:** per-action-type histograms of row successor
   counts and cross-row sharing rates, so compression targets the actual
   bulk (single-slot rows vs reforge rows) instead of a guess.
2. **Compress the retained price-only cache, not the hot solve.** The
   compatible closure kept on the solver handle for re-pricing is a cold
   cache; delta-encoded successors (sorted u32 gaps, varint) with lazy
   inflation on reuse would roughly halve retained bytes without touching
   hot-loop layout. Dropping the retained closure under memory pressure is
   also legitimate — it is a cache, and `estimated_owned_bytes` already
   accounts it.
3. **Float32 probabilities only as an owner-approved mode.** It halves
   the dominant array but injects ~1e-8-relative rounding into a pipeline
   whose cross-backend value tolerance is 1e-7 absolute; if ever wanted,
   it must be an explicit reported-precision mode, not a default.

**Accuracy risk.** Items 1–2 none; item 3 changes numbers and must be
disclosed per invariant 2.

### 1.6 Reforge evaluator: surface truncation, then scale the frontier

**Problem.** The roll DP (`solver_reforge.cpp`) drops paths below
`kPathEpsilon = 1e-9` and truncates frontiers above 400,000 states, then
silently renormalizes (`finalize`). Today the S3 fixtures assert the loss
is negligible; nothing reports it per solve. Fossil-heavy sessions with
many distinct family weights and more goal slots grow the bucket space
combinatorially, so harder crafts will lean on truncation more.

**Changes:**

1. **Honesty first (cheap):** accumulate dropped path mass and truncated
   frontier mass per distribution, propagate the worst case into
   `SolveDiagnostics`, and let the existing `exact_abstract` status
   degrade to a reported bound when loss exceeds a threshold. This makes
   the current behavior visibly exact instead of assumed exact, and makes
   any future, deliberately coarser fast mode safe to offer.
2. **Scaling option (larger):** a side-factored roll DP. Prefix and
   suffix fills interact only through the shared target total, side caps,
   and goal/blocked occupancy masks, so per-side frontiers convolved over
   (picks, occupancy) could replace the joint frontier's product space
   with something closer to a sum. This is a real algorithm change and
   would be gated by the existing Monte Carlo cross-check fixtures like
   every S3 evaluator.

**Accuracy risk.** Item 1 improves honesty; item 2 must reproduce the
current exact distributions bit-for-bit on the pinned fixtures before
replacing anything.

### 1.7 Cross-backend numerical agreement by construction

**Problem.** The endgame native/WASM start-value delta (1.86e-6 vs the
approved 1e-7) is structural: convergence-critical dot products
(BiCGSTAB `dot` in `solver_solve.cpp`) accumulate in `long double`, which
is 80-bit x87 on the MinGW native build and 128-bit soft-float under
Emscripten. The two backends literally compute different arithmetic. The
landed tighter fixed-policy tolerance narrows the disagreement; it does
not remove the mechanism.

**Changes:**

1. Replace `long double` accumulation in convergence-critical paths with
   an explicit double-double compensated sum (TwoSum/Kahan) — identical
   IEEE-double operations on both backends, and slightly *more* accurate
   than 80-bit x87.
2. Pin `-ffp-contract=off` (or equivalent) for solver translation units on
   both toolchains so FMA contraction cannot introduce divergence as
   compilers or flags evolve.
3. Keep the new `sparse_policy_iterations` /
   `max_sparse_policy_iterations` telemetry in the cross-backend
   comparison so iterate divergence is caught before it reaches values.

**Effect.** Native/WASM agreement stops depending on tolerance luck; the
1e-7 comparison becomes routinely satisfiable.

**Accuracy risk.** None; accuracy strictly improves or is unchanged.

### 1.8 Toolchain and build flags (measure-first)

The native benchmark harness builds with `g++ -O2`, no LTO, no
architecture flags ([build.ps1](../scripts/build.ps1)); the WASM engine
builds `-O2 -fexceptions` without SIMD
([build-wasm.ps1:74](../scripts/build-wasm.ps1)). Candidate levers, each
subject to invariant 7 (optimize what the benchmark identifies):

- Native: `-O3`, LTO, and a baseline like `-march=x86-64-v2/v3` for the
  benchmark/engine binaries.
- WASM: `-O3`; `-msimd128` (auto-vectorizes the dot/axpy loops that
  dominate large SCC solves); and switching `-fexceptions` (Emscripten's
  JS-based exception path) to `-fwasm-exceptions` — hot solver loops sit
  inside `try`/`catch` frames (`expand_one`), and JS-EH taxes every frame
  crossing. Browser/Node support for Wasm EH and SIMD is a compatibility
  floor Oliver should sign off on.
- Numerical caveat: any flag change must respect 1.7's determinism pin;
  `-O3`/SIMD reorders non-compensated arithmetic, which is another reason
  to land compensated dots first.

## Part 2 — Simulator Improvements

The native simulator is both the S7 correctness instrument (10,000-run
gate) and the product Simulator. Its inner loop
(`simulator.cpp: run_one`) executes, per action: string-keyed price map
lookups, an engine `apply_action`, a rollback copy for most action types,
and a linear scan of the current node's edges evaluating compiled
conditions. At ~10^9 actions per endgame verification, constant factors
here are the whole game.

### 2.1 Pre-resolve per-node prices at simulator creation

**Problem.** Every executed operation re-resolves `node.price_keys`
against the economy with per-key `std::string` hash lookups
([simulator.cpp:1051](../engine/src/simulator.cpp)) — several string
hashes per action, ~10^9 times.

**Change.** At `pc_simulator_create` (economy and strategy are both
fixed), resolve each operation node once to `{summed_price,
missing_key_indices}`; the run loop reads two scalars. Missing-price
accounting keys can stay resolved per node.

**Effect.** Removes a top-constant from every simulated action. Zero
behavioral change.

### 2.2 Route via discrimination structure instead of linear predicate scan

**Problem.** The compiled policy is a master router whose prioritized
edges each test a full abstract-state predicate; `select_edge` evaluates
them in order until one matches. Measured per-action cost rises with
region count (1.2 µs at 41 regions → 3.8 µs at 133 regions). Wider future
policies make this superlinear in exactly the wrong place.

**Changes, either of:**

1. **Compiler-emitted decision tree.** Region predicates over
   policy-reachable states are mutually exclusive and built from a small
   feature vocabulary (slot family/tier presence, counts, rarity, flags).
   The compiler can emit a hierarchical router (first split on the most
   discriminating feature, then nested routers) so each graph step
   evaluates each feature once instead of once per region. This stays
   inside the ordinary editable vocabulary — routers routing to routers —
   and also shrinks strategy JSON (see 3.2).
2. **Engine-side routing memo.** Inside the simulator only, memoize
   `projected feature signature → matched edge` per router node. The
   simulator owns all item mutations, so invalidation is trivial
   (recompute signature after each action; the signature is exactly the
   features the conditions read). This accelerates arbitrary user-authored
   strategies too, not just compiled ones.

**Effect.** Turns routing from O(regions × predicate size) per action
into O(predicate size) or O(1) amortized. Bounded, exact, and
simulator-parity by construction (same conditions, same priority
semantics; default-edge fallback preserved).

### 2.3 Parallel verification runs (needs one owner decision)

**Problem.** `run_simulator_chunk` is single-threaded and draws all runs
from one sequential RNG stream, so results depend on run order. The 10k
endgame gate is embarrassingly parallel work being executed serially.

**Change.** Derive an independent per-run RNG stream from
`(seed, run_index)` (counter-based seeding), making every run's outcome
independent of execution order and thread count, then run the 10k gate on
a native thread pool. Reduction of summaries is order-independent
(sums/counts; retained examples can keep deterministic selection by run
index).

**Effect.** 8–16× on verification wall time; the endgame gate drops from
tens of minutes–hours to minutes even before 2.1/2.2.

**Owner decision.** Per-run seeding changes the realized sample for the
pinned corpus seeds, so fixture expectations get one re-baseline. The
plan already allows random simulation seeds for fixtures (policy/value
determinism is what matters), but the semantics change should be
explicitly approved.

### 2.4 Verify run-length headroom before the next endgame attempt

**Problem.** The endgame verification uses `max_actions_per_run =
100000`. With expected actions per run plausibly around 10^5, an
unlucky-tail run can hit the cap, count as a failure, and breach the
0.995 minimum success rate even though the policy is correct — a
gate-validity risk, not a performance one.

**Change.** Before rerunning the 10k sample, obtain expected actions per
run cheaply (see 2.5) and confirm the cap holds comfortable tail headroom;
if not, present the cap change to Oliver with the evidence. No tolerance
change is implied.

### 2.5 Use the exact strategy evaluator as a verification instrument

`pc_strategy_evaluate` already computes, without sampling noise, the
compiled graph's exact success probability, expected actions, and
expected consumption under a strictly finer junk partition than the
solver's abstraction, with simulator-parity routing
(`solver_eval.cpp`). It is stepped, capped, and already wired into the
workspace. The benchmark harness does not call it.

**Change.** Add an evaluator column to each benchmark case (subject to
its own caps): report `V(start)` vs evaluator expected cost vs empirical
mean. This decomposes any forecast gap into **abstraction drift**
(solver vs evaluator) and **sampling/semantics residue** (evaluator vs
empirical), in seconds instead of half-hour simulations, and it yields
the expected-actions figure 2.4 needs. The 10,000-run simulator gate
remains the owner-approved correctness authority; the evaluator is a
diagnostic, per invariant 8.

### 2.6 Report confidence intervals next to tolerances

The harness computes per-run cost variance only when
`verification_chunk_runs == 1`
([solver_benchmark.cpp:972](../engine/benchmarks/solver_benchmark.cpp)).
Streaming mean/variance per run costs nothing at any chunk size.
Reporting the empirical standard error next to the fixed tolerance lets a
reader separate statistical misses from systematic ones at a glance
(e.g., the advanced case's −8.74% is roughly 9σ under a
standard-deviation-≈-mean heuristic — clearly not sampling noise, whereas
the one-mod +1.50% is ~2σ — plausibly noise). Whether tolerances should
ever incorporate SE is Oliver's call; reporting both is pure information.

## Part 3 — Forecast Accuracy (Abstraction Drift)

The forecast-vs-empirical ladder — +1.50%, +0.18%, −3.03%, −8.74% as
crafts get more complex, with the two real crafts consistently *cheaper*
in simulation than forecast — is the expected signature of the
documented approximately-sound abstraction: junk collapsing and
single-representative materialization perturb pool weights, and the plan
explicitly accepts this and prescribes refinement when a goal shows
material drift ([crafting-solver-plan.md](crafting-solver-plan.md),
Soundness Check). For complicated multi-step crafts this is the accuracy
frontier; the fix sequence should be:

1. **Attribute before changing anything.** Run the 2.5 decomposition per
   case. If evaluator ≈ empirical (expected), drift lives in the solver
   abstraction, and the evaluator's strict exclusion-effect partition
   identifies *which* refinement matters. If evaluator ≈ `V(start)`
   instead, the gap is elsewhere and refinement would be wasted.
2. **Correct the forecast without growing the solve (cheap).** Keep the
   coarse solve and its policy, but annotate the compiled strategy and
   report with the evaluator's exact fixed-policy expected cost as the
   authoritative forecast for that policy. The policy remains optimal
   with respect to the coarse abstraction and must stay labeled that way
   (invariant 2); the *forecast bias* disappears because the number
   attached to the strategy is exact for the graph as compiled.
3. **Refine selectively (moderate).** Where step 1 shows a specific junk
   family's exclusion effect driving drift, refine only that
   distinction, and only along policy-relevant states — the plan's
   "option-local refinements over globally widening every solve." The
   junk-class machinery already supports exclusion-effect partitioning
   (it is what the evaluator layout uses); the cost is state-space
   growth, which is why it should follow evidence, not precede it.
4. **Bound instead of guess (if residue remains).** If materialization
   choice measurably matters, compute transition-weight envelopes
   (optimistic/pessimistic representative) and report `V(start)` as an
   interval. Honest bounds are always permitted; silent point estimates
   that drift are not.

None of this changes any mechanic rule; if attribution ever surfaces a
question about how a craft *behaves*, that question goes to Oliver, not
to research or inference.

## Part 4 — Compiled Strategy Size And Routing Width

Endgame emits 6.63 MB of strategy JSON (104 nodes / 714 edges after
region compression; advanced 4.84 MB). Two levers, both exactness-free:

1. **Region cap tuning.** Region grouping splits equal
   `(operator, cost)` groups at 8 states
   ([solver_compile.cpp:516](../engine/src/solver_compile.cpp)). The cap
   bounds one router condition's width; measuring compile/simulate cost
   against cap 16/32 is cheap and directly shrinks router width and JSON
   size.
2. **Hierarchical routers.** The same discrimination-tree idea as 2.2(1),
   emitted into the document: a top router splitting on rarity/counts,
   nested routers refining. Same vocabulary, same simulator, smaller and
   faster documents. This also keeps very large future policies (5–6 slot
   goals) inside the editor's practical limits, complementing the
   already-specified owner-controlled readability trim, which remains a
   separate, disclosed, post-exact feature.

## What Not To Do

Guardrails already recorded that this report deliberately re-affirms:

- No heuristic pruning or aggregation without a proof or a reported
  bound; never label an unproven result exact (invariant 2). The safe
  quotient is byte-identical kernels — value-approximate state
  aggregation is out.
- No reopening the engine pool picker without benchmark evidence
  (invariant 7).
- No Boolean collapses of fractured/crafted carriers, no pre-selected
  Unveil outcomes, no hiding salvage exits (Unsafe Collapses).
- The simulator executes primitives only; no compiled shortcut may bypass
  primitive execution (invariant 3) and the 10k simulator gate stays the
  plan-level correctness authority (invariant 8).
- Do not widen tolerances to make gates pass (S7.6 handoff caution); make
  the number honest instead (2.6, Part 3).
- WASM threads, SIMD, and Wasm-EH baselines are deployment-facing owner
  decisions, not defaults an agent flips.

## Suggested Sequencing

Three tiers, cheapest-exact first. Every tier is measured against the
pinned corpus with the existing report tooling; plan-level correctness
remains the compiled-strategy simulator gate.

**Tier 1 — exact, low-effort, high-leverage**

1. Simulator per-node price resolution (2.1).
2. Chunked fixed-policy evaluation (1.1) — closes the worker-step gate
   structurally.
3. Compensated-double dots + fp-contract pin (1.7) — closes the
   native/WASM value gate structurally.
4. Evaluator column + CI reporting in the harness (2.5, 2.6) and the
   run-length headroom check (2.4).
5. Parallel verification runs (2.3) after Oliver approves per-run seed
   derivation.

**Tier 2 — measured optimizations**

6. Warm-started focused rounds (1.2) and incremental policy improvement
   (1.3).
7. Router discrimination tree or routing memo (2.2), region-cap tuning
   (4.1–4.2).
8. Toolchain passes native and WASM (1.8), ordered after 1.7.
9. Transition/reforge telemetry (1.5.1, 1.6.1) to aim Tier 3.

**Tier 3 — evidence-gated structural work**

10. Selective abstraction refinement where drift attribution demands it
    (Part 3, steps 3–4).
11. Cold-cache transition compression (1.5.2).
12. Side-factored reforge DP if fossil-heavy goals hit frontier
    truncation (1.6.2).
13. Native solver parallelism (1.4); WASM threads only as an owner
    product decision.

## Open Questions For Oliver

1. May verification adopt per-run derived seeds (order- and
   thread-independent runs), accepting one re-baseline of the pinned
   empirical samples? (Gates 2.3.)
2. Should the compiled strategy's displayed forecast become the
   evaluator's exact fixed-policy expected cost (with the solve's
   optimality status still labeled against the coarse abstraction), or
   remain raw `V(start)`? (Gates Part 3 step 2.)
3. What browser/Node baseline may the WASM build assume (SIMD, Wasm
   exceptions)? (Gates 1.8.)
4. Is a raised `max_actions_per_run` acceptable for the endgame case if
   the headroom check shows the 100k cap clips legitimate tail runs?
   (Gates 2.4; tolerance untouched.)
5. Any interest in an owner-approved reduced-precision solve mode
   (float32 transition probabilities) with disclosed error status, or
   should exact-double remain the only mode? (Gates 1.5.3.)
