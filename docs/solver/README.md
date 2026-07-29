# Solver

**Status: stable implemented architecture reference.** Historical phase plans
and acceptance narratives are archived and do not control current sequencing.

Parent: [Documentation index](../README.md)

Verified against code, the bounded-policy B6 acceptance, the mechanical solve
split, focused-round performance acceptance, WASM progress-accounting,
bounded-incumbent graph stability, the goal-progress-gated native/WASM
acceptance, the gated root renewal incumbent, interrupted root-row ownership,
the Harvest targeted-natural correction, and the rejected shared-reforge
frontier prototype: 2026-07-28. The preceding broad architecture stamp was
2026-07-24 @ `255e8f1`.
Scope: native solver,
calculation/evaluation engines, public C ABI, policy compilation, seeded
corpus generation, benchmark orchestration/analytics, and the non-visual
WASM/worker path. The pinned scaling
measurements live in
[solver-scaling v1](../../fixtures/solver-scaling/v1/README.md), and the
generated corpus lives in
[seeded natural-T1 v1](../../fixtures/solver-natural-t1/v1/README.md). This
stamp does not claim rendered-browser review or a mechanic ruling.

## Purpose

The solver turns a concrete start item, a goal, an action scope, and an
immutable economy into a minimum-expected-cost policy. Its transition and
legality authority stays in the native engine. The web app selects inputs,
shows diagnostics, and transfers compiled strategy JSON; it does not implement
crafting probabilities or Bellman logic.

The same subsystem provides four related services:

- exact outcomes for one action on one concrete item (Calculator);
- minimum-expected-cost policy solving;
- compilation of a solved policy to an ordinary editable strategy graph; and
- exact whole-graph evaluation and action/material accounting for a compiled
  strategy.

The [product Calculator reference](../product/calculator.md) describes the
current user-facing orchestration. Mechanic behavior belongs to the
[mechanics library](../mechanics/README.md). The complete UI-to-native request,
handle, cancellation, compilation, evaluation, and verification sequence is in
[End-To-End Solver Flow](flow.md).

## Goal And State Contract

A v1 goal contains one to eight slots. Each slot names either a stable modifier
group or modifier-family key and a minimum tier (`0` means any tier). The goal
also names the finished rarity and may set `min_satisfied_slots`; omission
means every slot. Goal parsing rejects unknown, overlapping, empty, or
out-of-range definitions.

Path of Exile tier numbers descend in quality: T1 is best. Therefore
`min_tier: N` accepts tiers 1 through N, while `min_tier: 0` accepts any tier;
the field is the worst acceptable tier number, not a lower numeric bound.

The abstract layout is derived from the resolved goal and the candidate action
set. An abstract state records:

- absent, below-tier, or satisfied status for every goal slot;
- blocked, crafted-goal, and fractured-goal masks;
- rarity and prefix/suffix counts;
- compact junk counts, including crafted/fractured combinations;
- corruption, mirror, split, synthesis, metamod, veiled, influence, and
  Eldritch state needed by admitted actions.

Non-goal affixes share a junk class only when the admitted actions cannot
distinguish them. Classes include side, relevant tag signature, goal-blocking
effects, and—when exact group effects are required—the complete exclusion
effect on later pools. `CalcContext` forces the strict exclusion-effect form
when Unveil, Harvest resistance conversion, Fracture, or remove-crafted-mods
participates. Because automatic product mode admits Fracture, the current
product solve does not match the old claim that strict partitioning is
evaluator-only. Automatic-candidate layouts also retain ordinary affix identity
needed to materialize the current carrier exactly, even when the explicit
primitive envelope cannot roll those affixes.

After strict reachability closes, the solver refines a collision-checked
behavioral partition across every admitted action. Legality, automatic
admission, resource identity, exact probability, choices, and projected
successor classes must all match before states merge. The strict layout is the
oracle; unknown or mismatched observations remain distinct. The bounded Chaos
control merges 57,722 strict states to 3 classes, while both accepted complete
product envelopes merge none because their larger action sets observe every
difference. A choice option's concrete modifier ID is intentionally retained
even when its successor state projects to another class: extraction lifts the
representative's literal preference list, and compilation emits that modifier
both as `has_unveil_option` and as the selected Unveil operation.

Exact refinement runs only after graph closure. A cap-stopped full-evidence run
instead reports `shadow_only=true`, retains strict states as its working
`quotient_states`, and may group literal observed row payloads in the
historically named `shadow_behavioral_classes` field. Unexpanded states have no
completed action rows, so neither equality nor reduction in that field proves
an exact completed-graph quotient. No approximate global compaction exists.

Action-relative destructive-reforge reuse is separate and already implemented.
The collision-checked reforge memo keys one immutable roll distribution by
action plus complete preserved base, so states differing only in wiped junk
share the roll DP. Sparse expansion reuses that row payload and enqueues its
fringe once by distribution identity. This saves repeated work after an
identical kernel exists; it does not merge the outer states or avoid
materializing the first unique carrier's successor distribution.

That memo does not reuse structure across different actions. Reforge bucket
weights, Fossil-added support, forced modifiers, and mechanic stages remain
part of each action's exact evaluation. A measured Lucent/Jagged prototype
did build and replay an exact action-independent Chaos structural DAG with
separate action weights. After correcting canonical-bucket projection, the
shared and sequential evaluators matched outcome counts, probability bits,
discovered states, transition hashes, and policy hashes on both frozen cases.
The candidate was still rejected: faithful lane propagation retained the same
deterministic work, the product cap stopped on the same root Fossil, the
complete Fossil lanes were no faster, total wall time increased, and the DAG
retained about 47 MiB. No cross-action frontier cache or multi-weight evaluator
is implemented. See the
[archived report](../archive/2026-07-28-harvest-shared-reforge-frontier/report.md).

Code authority:
`engine/src/solver_internal.hpp`, `engine/src/solver_api.cpp`,
`engine/src/solver_abstract.cpp`, and `engine/src/solver_calc.cpp`.

### Natural-T1 corpus feasibility

`pc_solver_goal_feasibility` is a dedicated three-way native query for
benchmark-corpus construction. It reuses the resolved goal layout, session
item-level data and base weights, full exclusion groups, rare-item side
capacity, the goal-relevant candidate set, and action reachability. It returns:

- `feasible` only with a positive-weight natural T1 assignment and an admitted,
  legal ordinary reforge witness (or when the supplied item already satisfies
  the natural goal);
- `infeasible` only for structural proofs such as too few natural T1 slots,
  side-capacity failure, or exclusion-group conflict; and
- `unknown` for unsupported starts or when the admitted actions do not supply
  the required witness.

The query never runs a bounded solve, so cap or watchdog exhaustion cannot be
misreported as infeasibility. The initial generator proposes only T1
base-reach prefix/suffix families for its uninfluenced starts, then requires a
native `feasible` result before emitting a case. Generated slots use family
keys, whose acquisition-aware family identity prevents a crafted/bench member
from satisfying a natural-only slot. Bench operations remain available in the
case's priced product envelope.

That T1-only boundary is intentional. Oliver confirmed on 2026-07-23 that the
natural-T1 benchmark generator must not emit lower-tier or tier-range goals;
review and planning must not treat their absence as a generator defect.

The Python generator accepts explicit base paths, a path list, item classes,
or a named base pool; seeded side composition and goal-count strata; family
and tag filters; resource caps; and watchdogs. Its manifest pins artifact,
economy, generator, Git state, compiler, ABI, and native-binary hashes. The
generation report retains the full acceptance/rejection funnel and deliberate
feasibility probes. Run records pin the solver build separately so later A/B
runs reuse the same explicit cases.

Code authority: `engine/include/poecraft/solver.h`,
`engine/src/solver_api.cpp`,
`tools/ingest/poecraft_ingest/natural_t1_corpus.py`, and
`bindings/python/poecraft_engine/_binding.py`.

## Actions And Planner Operators

`build_action_registry` enumerates plannable engine actions for one session.
Descriptors carry a stable id, display name, transition kind, legality facts,
price-key quantity vector, tag discriminators, and preservation effects. The
synthetic `restart` action consumes the `base` price and returns to the clean
base state.

Callers may name an explicit primitive subset and fixed option programs. The
product `action_mode: "goal_relevant"` instead builds a bounded,
price-independent envelope and enables state-local automatic candidates.
Those candidates cover relevant primitive Fracture, permanent bench
finishes, temporary bench blockers, protected metamod routes, Multimod
finishes, and automatic Imprint attempt/restore programs. Imprint programs are
not user-authored in product mode.

Permanent-bench admission evaluates its deterministic successor on the
already materialized exact carrier. It does not depend on a temporary abstract
context being able to synthesize another representative of that carrier. This
keeps a legal exact bench finish available even when a dense real-item junk
class has no greedily materializable alternate representative.

Fixed and automatic options are solver operators over exact primitive
programs. They carry complete exit distributions, resource quantities, choice
groups, and compilation recipes. They never become opaque simulator actions:
a selected option compiles back into ordinary strategy operations and routers.

The goal-relevant product envelope intentionally excludes Veiled and Eldritch
families at this commit even though their primitive actions and exact
evaluators exist for explicit/manual scopes. The registry makes that a bounded
product-scope choice rather than an absence of primitive engine support. It is
recorded as deferred scope work in the
[solver roadmap](../future/solver-roadmap.md).

Code authority:
`engine/src/solver_registry.cpp`, `engine/src/solver_options.cpp`, and
`engine/src/solver_internal.hpp`.

## Exact Transition Provider

`CalcContext` owns the abstract state table, representative materialization,
planner operators, and price-independent transition caches. Runtime solver and
Calculator outcomes are exact engine evaluations:

- deterministic operations emit one successor;
- single-slot operations enumerate the engine-owned weighted pool;
- reforge operations use a sequential abstract roll frontier with exact group
  removal, target-count mixing, and mechanic-specific stages; and
- special evaluators handle implemented Harvest, Veiled, Eldritch, Fracture,
  and other registered one-item mechanics.

The sequential reforge frontier is intentionally joint across prefix and
suffix rolls. A measured exact counterexample rejects composing independent
side marginals after conditioning only on final side counts: each next draw
uses the combined remaining-weight denominator
`Wprefix(local) + Wsuffix(local)`. Conditioning on final remaining side
weights restored the tiny table's rank but cost more identities than the
joint representation and would be required after every pick. See the
[action-local factorization report](../archive/2026-07-27-action-local-side-factorization/report.md).

The later
[complete first-frontier census](../archive/2026-07-27-true-successor-frontier-census/report.md)
measured the live exact Chaos supports of four hard cases. Their supports
contain 98.71% to 99.65% of the Cartesian products formed by collision-checked
one-sided payload projections. The earlier 200,000-state prefix had already
exposed every one-sided and goal-status class; completion adds joint
combinations inside those classes. Support factorization therefore does not
remove the joint Bellman continuation terms, and no compact row authority
follows from the projection.

Compound Bestiary actions use their own exact calculation API because their
state includes an optional saved checkpoint. Automatic Imprint retry is still
an exact solver operator assembled from those native Bestiary transitions.

There is no Monte Carlo fallback in the calculation engine. Sampling is used
as test or simulator evidence, not to produce `pc_calc_action_outcomes` or
solver rows. There is also no public `pc_calc_batch_outcomes` function; the
implemented public Calculator call is the single-query
`pc_calc_action_outcomes` surface.

Code authority: `engine/src/solver_calc.cpp` and
`engine/src/solver_reforge.cpp`.

### Goal-progress-gated reforge mode

The unrestricted exact solver remains the default. An opt-in
`goal_progress_gated_reforges` solve option changes only primitive destructive
reforge continuation semantics:

- every goal-satisfying outcome contributes to one exact terminal exit;
- every outcome with zero satisfied goal modifiers contributes to a virtual
  retry basin for its exact preserved boundary;
- every partial-progress outcome remains its complete exact abstract state;
  and
- terminal, retry, and partial probability/resource mass is preserved without
  deletion or renormalization.

The retry basin is not an ordinary physical item state. It may select only a
legal primitive destructive reforge whose next kernel is independent of the
discarded zero-progress affixes. It cannot select Annul, Exalt, Bench,
protection, or another salvage operation. Ordinary retained partial states
still receive the full admitted action envelope, including removal, addition,
protection, finishing, reforge switching, and staged side strategies.
Eldritch Chaos is conservatively excluded from basin actions because its
preserved side can observe affixes that the basin discards.

Enumeration may fold a remaining branch only after an exact monotone proof:
the goal is already irrevocably satisfied, or the zero-progress branch has no
remaining eligible positive-weight goal-satisfying roll. The mode has its own
cache identity and deterministic kernel-bit hash. Compiled policies, when one
is available, emit an explicit post-reforge route to the selected basin
action.

This mode solves a different, restricted executable MDP. Its result scope is
`exact_within_zero_progress_reroll_restriction`; it is not globally optimal
over excluded zero-progress salvage routes. Fixed-option kernels keep their
existing exact program semantics, and the unrestricted mode keeps its prior
state, transition, hash, and policy contract.

After a complete gated root row for a priced primitive destructive reforge,
the solver can publish an early bounded incumbent for the fixed policy
“repeat this reforge until the goal.” It does so only when every
positive-probability non-goal exit can legally repeat the same action and the
engine-owned exact reforge-kernel signature matches the root signature. With
terminal probability `p` and immediate action cost `c`, the proved executable
policy has value `c / p`. This action-local witness does not merge retry or
partial states and does not assert that their other available actions are
equivalent.

The incumbent is an upper bound within the gated policy restriction. All
competing actions remain in the lower-bound and discovery problem, so a
resource-capped solve reports a bounded feasible policy rather than exactness.
Only the root plus the fixed policy's reachable gated exits belong to its
authoritative policy domain; unrelated diagnostic states do not receive a
fabricated action.

Compilation independently rechecks action legality and the exact kernel
signature on every reachable non-goal carrier. A valid witness compiles to a
four-node loop: route goal states to success and every other proved carrier
back through the selected reforge. This compact form is an exact compilation
of the witnessed fixed policy, not a Bellman quotient or a global-optimality
claim.

#### Chaos-anchored incremental action envelope

In goal-progress-gated mode, the solver no longer requires every filtered
operator for a carrier to finish before releasing that carrier's successors.
It completes Chaos and inexpensive ordinary rows first, immediately interns
and queues their successors, and optimizes the currently admitted restricted
graph. Filtered Fossil, corrected Harvest reforge, and goal-relevant Essence
rows are then evaluated exactly as delayed alternatives at every reached
compatible state.

Sparse rows retain stable owner links, so a delayed row may be appended after
Bellman work has begun without relying on per-state contiguous storage. A
completed alternative is admitted when its exact Q interval proves an
improvement and then triggers Bellman reoptimization. It is marked
non-improving only when its complete exact Q value cannot improve the final
restricted value. New support introduced by Fossil added/forced modifiers or
Essence guarantees is interned and expanded before classification. Otherwise
the row remains unresolved.

Chaos support is a state-ID membership authority, not a retained structural
DAG. Collision-checked exact reforge-kernel signatures can reuse a completed
distribution across equivalent carriers; action-specific probabilities are
still calculated and charged. Compatible Fossil and Harvest outcomes resolve
through the ordinary state interner. Telemetry reports action lifecycle
counts, exact Q intervals, unique kernel evaluations, carrier reuse,
outside-Chaos states, Bellman reoptimizations, and the remaining envelope.

The solver may return the existing executable gated incumbent while the
envelope is open, but that result is bounded and incomplete. Exact closure
within the zero-progress-reroll restriction is allowed only after every
filtered action is admitted, proved non-improving, or inapplicable.
Unrestricted mode retains its previous atomic complete-envelope behavior and
global exactness contract.

## Solve And Reprice

A solve performs these implemented stages:

1. Project the concrete start item and expand reachable abstract states.
2. Admit legal state-local candidates. Exact action producibility and setup
   legality reject impossible protected-repeat programs before option-kernel
   construction.
3. Evaluate deterministic goal finishes and Restart before broad stochastic
   kernels. A price-bound constructive state certificate may stop a carrier
   early only when every other admitted operator has an optimistic lower
   bound strictly above an executable row upper bound. The lower bound grants
   an operator every goal slot any constituent primitive could possibly
   produce, then prices the cheapest relaxed primitive cover of the remaining
   goal requirement.
4. Copy required exact outcomes into one sparse
   transition graph, subject to state, row, transition, reforge-work,
   diagnostic, output, and owned-byte caps. Collision-checked observation
   signatures reuse exact kernel payloads without changing strict states.
5. Price each operator by dotting its resource quantities with the pinned
   economy. Missing prices exclude the affected operator and are diagnosed;
   absent never means free.
6. Refine a completed all-action strict graph into the exact quotient, then
   optimize cyclic components with SCC-based policy iteration and sparse
   component solves. A prioritized Bellman path remains the explicit fallback
   if policy evaluation fails.
7. Extract deterministic policy choices, observation-owned Unveil choices,
   values, reachability, diagnostics, hashes, and optional solve-log records.

Focused expansion computes finite constructive upper bounds and global lower
bounds while extending relevant fringe states. A zero gap is an exact closure
proof and may finish directly without a separate outer Bellman phase. A
complete executable incumbent can also survive a resource-cap stop or an
enabled product gap target. In that result, `L` is the certified optimal-cost
lower bound, `U` is the incumbent certificate, and `J_pi` is the exact returned
policy cost with `L <= J_pi <= U`. The gap targets are checked only after a
complete focused lower/upper round; they do not participate in Bellman
comparisons, ties, admission, pruning, or exact closure. Resource exhaustion
without an executable proper fallback reports no finite upper bound.

Action-search telemetry attributes completed rows after they return. If a
row instead throws a solver resource cap, exception-safe attribution records
the carrier state, root ownership, complete planner action, operator index,
stable cursor, cap name, work/cache deltas, and wall time before the existing
cap handler records the same refusal. An interrupted row is not counted as
retained and does not become a transition or pruning certificate.

The current private focused scheduler admits at most 64 members per fringe
class, 256 states per global round, and 64 lower-bound states before upper-side
admission. These are implementation defaults, not public caps. A fresh
run-local matrix confirmed that increasing the global batch reduces focused
rounds and repeated whole-graph policy evaluation, but no tested tuple passed
the fixed worker-step responsiveness gate, so the defaults remain unchanged.

Exact focused closure uses the absolute numerical proof tolerance
`epsilon * 10`. Separately named value comparisons may retain their historical
value-scaled roundoff allowance. Neither tolerance is a product gap target,
and requested absolute or relative gaps never relax exact closure.

Once a constructive renewal/progressive-fracture fallback has been
synthesized, focused rounds retain it in the atomic incumbent strictly as an
executable upper-bound/output witness. Reuse validates goal, economy, action
vocabulary prefix, referenced row/operator ownership, and properness.
Monotonic graph growth and lazy action-vocabulary extension are allowed while
the complete prefix present at synthesis remains identical. A new focused
round or lower-bound update does not trigger re-synthesis; a missing witness,
changed existing executable dependency, or failed validation does. The
retained witness never guides focus, admission, pruning, ties, or Bellman
comparisons.

After one retained witness passes full anchor/start properness validation, the
same solve may reuse that successful proof. Proof version, exact immutable
policy ownership, semantic policy identity, goal, economy, action-vocabulary
prefix, graph owner, mechanics owner, and the complete published row,
successor, probability, choice, and choice-option prefixes must still match.
Later append-only graph/vocabulary growth is allowed; replacement or mutation
misses the cache and runs the full validator. The proof is solve-local,
successful-only, and has no lower-bound, scheduling, admission, pruning, or
policy-construction authority.

The atomic incumbent captures same-round values, selected row IDs and costs,
stable policy references, frontier operators, fallback, and provenance while
the selected transition graph and pricing are still current. It also copies
choice options only for selected rows that own them. A later graph replacement
or equivalent-row repricing therefore cannot reinterpret the policy whose
upper bound was certified. Graph-sized aligned Unveil and fixed-option
preference vectors remain deterministic derived output and are materialized
once if that incumbent is returned.

The constructive state certificate is not compaction and does not infer
equivalence from similarity. Its witness records the executable upper, the
strict minimum competing lower, and the number of kernels avoided. Because
the proof depends on current prices, a partial graph produced by it is never
retained as the price-independent transition cache; a later reprice rebuilds
or safely reuses only a separately completed all-action graph.

It is also not a public certified-next-action result. A 2026-07-27 shadow
audit grouped the complete hard-case root envelopes into 91 to 107 exact first
executable-action classes. The existing proof qualified on its small
constructive oracle, but every hard case lacked a finite executable incumbent
before its first broad row and then hit the 200,000-state cap. A future
verified-next-action surface therefore requires new pre-expansion
proof-producing upper/lower work; the current witness cannot simply be
promoted into an early product result. See the
[feasibility report](../archive/2026-07-27-certified-root-action-feasibility/report.md).

A follow-up
[graph-free probability-lower audit](../archive/2026-07-27-pre-expansion-probability-lower-audit/report.md)
then paired the archived fixed-renewal uppers with complete root-action-class
lowers. The probability MDP changed no graph-work counter, but all four hard
cases overlapped. Non-goal bench-first classes fell back to their exact first
price because the goal-mask/count abstraction does not retain the blocker
effect needed to charge continuation work. That audit source was restored;
the production solver still uses its prior cover and certificate behavior.

Selected-allocation enforcement uses incremental owner ledgers with periodic
full audits. On the accepted two-T1 product, per-state preparation byte audits
fell from the 22.47-second baseline to 7.3 ms (0.04% of expansion); audited
undercount is a hard error.

Stateful solve progress reports the conservative incremental estimate after
each bounded step, in both native and WASM callers. Whole-graph accounting
remains authoritative at audit/cap checkpoints, telemetry snapshots,
finalization, and explicit memory-statistics requests. This distinction keeps
frequent progress cheap without weakening selected-owned-byte enforcement or
final reporting.

Transition caches are price-independent and can be reused by a solver handle.
The browser product deliberately does not retain its scoped Solve handle:
after summary, telemetry, and compiled-strategy transfer it closes the handle
and rebuilds on a later Solve or reprice. A future retained-cache product mode
would require an enforced live-memory budget; it is not current behavior.

Code authority: `engine/src/solver_solve_types.hpp` holds shared private solve
types and declarations; `solver_solve.cpp` retains construction and the solve
entry point; `solver_solve_expand.cpp`, `solver_solve_bellman.cpp`,
`solver_solve_focused.cpp`, `solver_solve_constructive.cpp`,
`solver_solve_heuristics.cpp`, `solver_solve_quotient.cpp`,
`solver_solve_finish.cpp`, and `solver_solve_telemetry.cpp` own their named
phases.

## Policy Compilation

`pc_solver_compile_strategy` converts the latest executable policy into v1
strategy JSON. `policy_available`, not exact convergence, is the compilation
precondition. Exact policy regions with the same action and continuation share
operation nodes, and a collision-checked decision DAG routes concrete states
to those regions. The document otherwise contains ordinary start, router,
operation, and terminal nodes, deterministic prioritized edges,
`expected_cost` annotations, and non-executable accounting-role metadata.
Fixed and automatic operators expand to their primitive programs. Exact closed
policies retain the explicit off-policy failure terminal. Bounded policies use
an explicit safe Restart/fallback default for unmatched compiled states; a
frontier heuristic is never emitted as an action.

The compiled `base_state` preserves the solve start, not merely the base type:
it serializes rarity, item flags, generic influence bits, both Eldritch tiers,
and every materialized prefix/suffix modifier with crafted/fractured flags.
This matches the simulator's existing start-item parser and corrects the stale
audit claim that compiled verification always started from a fresh normal
base.

Compilation refuses a policy when the ordinary strategy vocabulary cannot
represent it or when configured graph/output caps are exceeded. It does not
invent a second execution format.

Code authority: `engine/src/solver_compile.cpp` and
`engine/src/simulator.cpp`.

## Exact Strategy Evaluation And Accounting

The exact evaluator derives a strict layout from the compiled graph's actions,
family/mod count observations, and condition targets; discovers `(graph node,
abstract state)` pairs; and solves the resulting absorbing graph by SCC. It
contracts compiler-generated policy routing without losing exact node/edge
flow and uses dense, rank-one, or matrix-free preconditioned component solves
as appropriate. It reports terminal probability,
action-not-applied/no-edge/unresolved attribution, expected actions and
materials, node/edge flow, incoming state classes, and S8.4 accounting and
review projections. The evaluator internally retains exact abstract-state,
compiled-node, and action occupancy together with immediate priced reward;
the retained occupancy/reward dot product is reconciled with exact expected
cost. Its action-utility report aggregates exact expected visits/applies,
priced spend and known-cost share, distinct reachable states, and regions by
goal progress, rarity, blockers, crafted count, and fractured subset. An
action's probability of any use is reported only when it is derivable from the
retained evidence; cyclic occupancy is not mislabeled as a hitting
probability. Quantities remain price-independent; the optional pinned economy
supplies the reward dot product.

Evaluation refuses unsupported graph vocabulary rather than estimating it.
`mod_count` and `mod_family_count`, including required crafted/fractured flags,
are exact. Concrete authored Unveil-offer conditions remain the named gap. The
stateful API has
begin/step/finish/destroy calls, cooperative progress, owned/output byte caps,
and live/peak memory statistics.

Code authority: `engine/src/solver_eval.cpp` and
`engine/src/solver_api.cpp`.

## Public And Browser Interfaces

The public ABI is declared in `engine/include/poecraft/solver.h`:

| Surface | Implemented contract |
| --- | --- |
| Registry | create/destroy, action count/info/find, candidate indices |
| Calculator | exact `pc_calc_action_outcomes` for one concrete item/action |
| Solve | synchronous solve plus begin/step/finish/abandon, product gap targets, opt-in goal-progress-gated reforge scope, and live `L`/`U`/gap, round, incumbent, work, and memory progress |
| Results | state value/policy, concrete projection, compile, solve log |
| Diagnostics | versioned telemetry and selected live/peak memory statistics |
| Exact graph | synchronous evaluate plus begin/step/finish/destroy and memory statistics |

`bindings/wasm/wasm_api.cpp` exposes the same stateful solve and evaluation
surfaces to the web worker. `apps/web/src/app/engine-worker.ts` provides
cooperative stepping, progress, cancellation, and event-loop yields. WASM
build/export/memory details are owned by the [engine WASM reference](../engine/wasm.md).

## Large-run orchestration and telemetry

The detailed observation, identity, partial-result, and frozen future-metric
contract lives in [Solver Benchmark Trajectories](benchmarking.md).

`tools/ingest/benchmark_solver_corpus.py` runs any native corpus manifest into
an arbitrary output directory. Cases launch in deterministic ID order in
separate process groups, use their pinned watchdogs under a hard 900-second
ceiling, kill the process tree on expiry, verify no parent survivor, and write
an atomic resumable ledger. Completed reports are skipped on resume. Default
hard-case concurrency is one; callers may opt into more workers with a memory
budget, in which case each case reserves its declared solver-owned-byte cap.

The native benchmark accepts the historical solver corpus and the generated
natural-T1 corpus. It compiles whenever `policy_available`, evaluates the
compiled returned policy exactly with the pinned economy, and records a bound
trace at every focused-round/incumbent change and bounded wall intervals. Each
sample carries `L`, `U`, both gaps, state/frontier and work counts, live/peak
owned memory, cap proximity, and whether progress raised `L` or lowered `U`.
Reports derive first-incumbent and standard gap-threshold times.

Solver telemetry attributes row attempts, raw outcomes, retained transitions,
reforge work, cache requests/hits, wall time, and retained selected-allocation
growth to action IDs. It also compares lower- and executable-upper-policy
selected abstract-state counts. These are profiling observations only:
benchmark action non-use is explicitly never a pruning certificate.

Focused diagnostics additionally report per-round and total lower/upper
candidate counts, quota/fill admissions, selected schedule admissions, and
global/per-class cap hits. Fallback validation reports one inclusive total and
mutually exclusive goal, economy, action-vocabulary, structural, anchor-
properness, start-properness, and successful-proof-identity leaves plus cache
version/check/hit/miss counts and last miss reason. Unrelated solver timers are
not required to sum to solve wall time. Owned-byte
diagnostics include recursive child-context visits and maximum depth.
Benchmark execution records every `pc_solver_solve_step` wall time and reports
count, total, median, nearest-rank-ceiling p95, and maximum.

`tools/ingest/report_solver_corpus.py` aggregates completed raw reports and
analyzable watchdog sidecars without participating in native or WASM
correctness. It reports exclusive policy and
refusal rates, target reach, median/p90/p99 time and memory, lower/upper
progress, work and graph distributions, exact policy action utility,
observational action search cost, outliers, and exact-input paired deltas.
Strata cover base, class, base/class, natural-T1 count, side mix, pool
density/probability, incumbent, policy status, and termination.

`fixtures/solver-natural-t1/v1/benchmark-stages.json` orders the iteration
funnel from smoke through full-short, deep, selected exact evaluation, and the
B6-only 10,000-run acceptance subset. Later stages require a green predecessor
ledger under the same label. Native benchmark exit code 2 is a completed
measurement when a report was written under the watchdog with no survivor;
the expectation miss remains visible so no-policy/refusal results are not
censored. An analyzable watchdog remains explicitly incomplete/right-censored
and is not included in terminal policy rates. A watchdog without a complete
step-boundary sample and process, survivor, output, or launch failures remain
explicit non-censoring failures.

## Boundaries

- Minimum expected cost is the implemented solve objective; roll-quality
  finishing is not part of this DP state.
- Product optimality is always relative to the admitted and priced action
  scope. Diagnostics must disclose exclusions and caps.
- Whole-graph exact evaluation and sampled simulation are separate evidence
  sources. A sampled cost mean is not automatically proof of Bellman parity.
- Recombinators, publishing, ML use, and remaining product-scope decisions are
  deferred; see the
  [solver roadmap](../future/solver-roadmap.md).
- Mechanic rules are never decided by this architecture file; see the
  [mechanics library](../mechanics/README.md).

## History And Notes

- [End-to-end solver flow](flow.md)
- [Archived solver architecture and S1-S8 phase record](../archive/2026-07-19-bestiary-solver-s8/solver-plan.md)
- [Solver notes](NOTES.md)
