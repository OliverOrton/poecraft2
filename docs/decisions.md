# Decisions

**Status: authoritative durable engineering-decision index.** Mechanic behavior
and rulings belong in [Mechanics](mechanics/README.md), not here. Historical
discussion remains in the linked archives.

Parent: [Documentation map](README.md)

Entries are append-only. A later decision may supersede an earlier one, but the
earlier entry remains with a link to its replacement.

## 2026-06-15 — Native Rule Engine And Stable Boundaries

**Decision:** The hot engine is C++20 behind a C ABI. Python owns ingest and
the first binding; the browser uses WASM; the web app uses Vite, TypeScript,
native Web Components, and Dockview rather than React.

**Context:** The original architecture and implementation plans are preserved
in [the project-roadmap archive](archive/2026-07-19-project-roadmap/README.md).
The verified current layer map is [Foundation](foundation/README.md).

**Consequences:** Bindings adapt one native authority. UI code does not become
a second crafting engine.

## 2026-06-15 — Canonical And Derived Data

**Decision:** `data/sqlite/poecraft.db` is canonical. Compiled runtime data is
derived and must be rebuilt, never hand-edited. Economy data remains a separate
versioned pipeline rather than volatile rows in the game-data database.

**Context:** [Engine Data](engine/data.md) and [Economy](economy/README.md).

**Consequences:** Rule-data changes enter through ingest and validation;
league-price changes do not change the canonical mechanic artifact.

## 2026-07-12 — Mechanic Authority

**Decision:** Oliver decides Path of Exile mechanic rules. Agents do not use
external research, memory, or inference to fill a mechanic gap. The native
engine owns implemented rule behavior; the mechanics library transcribes code
and dated Oliver rulings.

**Context:** [AGENTS](../AGENTS.md) and [Mechanics](mechanics/README.md).

**Consequences:** Unresolved behavior is named as an open question or explicit
unsupported boundary rather than approximated.

## 2026-07-15 — Solver Objective And Verification Count

**Decision:** The one-item solver minimizes expected cost and returns one
policy. Risk-adjusted objectives and generic top-k alternatives are not part of
the implemented contract. Required compiled-strategy verification uses exactly
10,000 simulator runs unless Oliver explicitly changes the count.

**Context:** [Solver](solver/README.md) and the
[S7 archive](archive/2026-07-solver-s7/README.md).

**Consequences:** Review projections and sampled results explain or validate a
policy; they do not silently change its optimization objective.

## 2026-07-17 — Exact Graph Remains Executable Authority

**Decision:** Solver review sections are deterministic display-only projections
over the raw compiled strategy. The exact editable graph remains the execution
authority. A future empirical trim, if selected, must create a separate derived
document with provenance and impact evidence.

**Context:** [Solver](solver/README.md), [Strategies](product/strategies.md), and
[S8.0 evidence](../fixtures/solver-baselines/s8.0/README.md).

**Consequences:** Presentation labels cannot prune actions or alter routing.

## 2026-07-18 — Browser Repricing Uses Rebuild By Default

**Decision:** After successful strategy transfer, the product should release
the solved native handle and transition closure. Repricing rebuilds the solve.
No retained-cache mode is added until live-byte telemetry can enforce a product
memory budget.

**Implementation status:** implemented by
[R4 browser transfer and solver lifetime](archive/2026-07-26-browser-transfer-lifetime-r4/README.md)
on 2026-07-26. Calculator opens a fresh scoped handle for each Solve and closes
it after summary, telemetry, and raw-byte strategy transfer. See
[End-To-End Solver Flow](solver/flow.md#current-repricing-and-lifetime).

**Context:** The decision originated in the
[B1/S8 plan](archive/2026-07-19-bestiary-solver-s8/plan.md). R4 supplied the
transfer path, scoped lifetime, and selected-live-byte evidence.

**Consequences:** Stable docs do not promise cheap in-place browser repricing.
The product rebuilds instead of retaining both the native transition closure
and redundant JavaScript graph copies. Any future retained-cache mode needs a
separate budgeted design.

## 2026-07-19 — Numeric Misses And Stopped Gates Stay Truthful

**Decision:** The S7 `0.9942` sample is not relabelled as passing its former
`0.995` target. Owner-stopped or time-boxed runs are not described as passing,
converged, or cap failures without emitted evidence.

**Context:** [Evidence](evidence.md) and the
[S7 archive](archive/2026-07-solver-s7/README.md).

**Consequences:** Historical acceptance language preserves the measured number
and the actual stop condition.

## 2026-07-19 — Solver Caps Were Not Raised At R3A

**Decision:** The exact product envelope and checked-in resource caps were
preserved at the R3A stop-and-plan boundary. The unresolved normal-cap result
was recorded without raising caps or selecting another optimization.

**Context:** [R3A evidence](../fixtures/solver-regressions/s8.4r/v1/evidence/r3a-carrier-scaling-summary.json)
and [deferred solver roadmap](future/solver-roadmap.md).

**Consequences:** Future work starts from measurement and an explicit Oliver
choice; it cannot inherit an implied instruction to continue tuning.

## 2026-07-19 — Documentation Lifecycle Reset

**Decision:** `docs/README.md` is the primary entry point. Stable contracts live
under area READMEs, rulings under `mechanics/`, durable choices here, measured
history in `evidence.md`, deferred designs under `future/`, and point-in-time
material under dated `archive/` folders. No implementation plan is active.

**Context:** [Documentation cleanup archive](archive/2026-07-19-documentation-cleanup/README.md).

**Consequences:** `HANDOFF.md` is intentionally minimal until Oliver selects a
new chunk. Archived sequencing has no current authority.

## 2026-07-20 — Exact Solver Scaling And Minimum Cap Increase

**Decision:** Solver scaling remains exact across every admitted action. State
compaction may merge only collision-checked, behaviorally equivalent states;
approximate compaction is not permitted. The default state, row, and reforge
work caps increase only to the smallest measured envelope that closes the
two- and three-T1 product corpus: 200,000 states, 1,215,000 rows, and
11,000,000 reforge work units. The 10,000,000-transition and 1 GiB selected
native-memory caps remain unchanged.

**Context:** [Exact scaling evidence](../fixtures/solver-scaling/v1/README.md)
and the [Q0–Q5 archive](archive/2026-07-20-solver-state-scaling/README.md).

**Consequences:** Chaos-only measurements remain a strict-versus-quotient
regression oracle, not the product workload. Full-action product cases must
close under the checked-in defaults, and any future cap increase again needs
measured native, compiler, evaluator, and WASM headroom.

## 2026-07-21 — Price-Bound State Pruning Is Not A Reprice Cache

**Decision:** The solver may omit unevaluated state/action kernels only when an
executable row upper is strictly below an admissible optimistic lower for
every competitor. The proof may use current prices, but a graph made partial
by that proof is never retained as the price-independent transition cache.
Later repricing rebuilds, or reuses only a separately completed all-action
graph.

**Context:** [Solver](solver/README.md),
[action/state evidence](../fixtures/solver-scaling/v1/README.md), and the
[completed archive](archive/2026-07-21-solver-action-state-pruning/README.md).

**Consequences:** Exact current-price policy behavior can avoid discovery
without approximate compaction or stale repricing. Equality, unknown
reachability, missing proof inputs, or an unsafe lower bound retains the
candidate.

## 2026-07-22 â€” Bounded Executable Policies Remain Separate From Exactness

**Decision:** A solve may return a complete executable incumbent at an exact
close, a post-round product gap target, or a resource-cap stop. Policy quality
(`exact`, `bounded_near_optimal`, `bounded_feasible`, or `none`) is separate
from termination. `L` is the global admissible lower bound, `U` is the
executable incumbent certificate, and the returned policy is evaluated
exactly. Absolute/relative product gaps only stop qualifying completed rounds;
they never alter Bellman comparisons, ties, admission, pruning, epsilon, or
eventual exact results.

Retained constructive witnesses may persist across focused rounds only as
validated upper-bound/output evidence in the atomic incumbent. They never
guide focus or search. Benchmark action utility and search cost are
observations; action non-use is never a pruning certificate. Seeded natural-T1
corpus acceptance uses the engine-owned three-way feasibility query, not
bounded solve exhaustion, and acquisition-aware natural family keys prevent
bench/crafted members from satisfying those goal slots.

**Context:** [Solver](solver/README.md), [Calculator](product/calculator.md),
the [B6 evidence summary](../fixtures/solver-natural-t1/v1/evidence/b6-acceptance-summary.json),
and the completed
[bounded-policy archive](archive/2026-07-22-bounded-policy-and-benchmarking/README.md).

**Consequences:** Product copy cannot call a bounded policy exact or call `U`
the optimum. Compilation is keyed to `policy_available`; exact evaluation and
10,000-run sampling retain distinct authority. Gap-target tuning and benchmark
analytics cannot silently become solver-search heuristics.

## 2026-07-23 — Natural-T1 Corpus Is Intentionally T1-Only

**Decision:** The natural-T1 benchmark generator emits T1 goal slots only. It
does not produce lower-tier or tier-range goals, and that omission is
intentional rather than a generator gap.

**Context:** [Solver](solver/README.md) and the
[post-B6 reconnaissance archive](archive/2026-07-23-post-b6-reconnaissance/README.md).

**Consequences:** The natural-T1 corpus remains a deliberately hard T1 slice.
Reviews and execution plans must not list tier-range support as missing,
deferred, or an easy generator improvement.

## 2026-07-26 — Accumulated-Gap Racing Is Rejected

**Decision:** Do not eliminate development runs when their accumulated
normalized-gap area can no longer beat a baseline. Before the first executable
incumbent, normalized gap is defined as its maximum value, so the proposed rule
penalizes time to first incumbent most strongly and selects for
early-incumbent heuristics rather than eventual exactness.

**Context:** [Solver benchmark trajectories](solver/benchmarking.md) and the
[completed reduced milestone](archive/2026-07-26-anytime-benchmark-completion/plan.md).

**Consequences:** The durable-trajectory milestone implements no racing.
Gap-integral analytics, target/data profiles, performance profiles, and paired
uncertainty remain deferred until a second real candidate exists; frozen
evaluation uses a common fixed stopping policy.

## 2026-07-27 — Broad-Kernel Work Stops; Successful Properness Proofs May Reuse

**Decision:** Do not continue the current solver roadmap with detached,
streamed, compact, or incrementally promoted broad-kernel representations.
The fixed streaming-lower candidate published no scalar and was restored under
its no-tuning gate. This closes the product direction without asserting a
theorem that every possible expectation fold is impossible.

A successful constructive/fallback properness validation may be reused within
the same solve only while its versioned immutable policy, goal, economy,
action-vocabulary prefix, graph/mechanics owner, row/pricing prefix, and exact
transition payload prefix identities still match. Only success is cached.
Mismatch executes the full validator. Reuse has no search, lower-bound,
admission, pruning, policy-construction, or cross-solve authority.

**Context:** [Final report](archive/2026-07-27-streaming-broad-lower-fold/report.md),
[Solver](solver/README.md), and
[tracked evidence](../fixtures/solver-scaling/v1/evidence/streaming-broad-lower-fold-summary.json).

**Consequences:** The owner case removes 16 repeated start-properness scans and
cuts solve wall by 22.3% with identical deterministic results. This is a
post-incumbent engineering improvement, not progress on the four hard 11M
pre-bound failures. Reopening broad-kernel work requires a new
Oliver-selected boundary and materially different evidence.

## 2026-07-27 â€” Existing Root Certificate Is Not A Product Result

**Decision:** Do not add a public certified-next-action result or early stop
by merely promoting the existing constructive state certificate. The hard
natural-T1 cases complete exact first-action projection but have no finite
executable incumbent before their first cap-failing Chaos row.

Certified root action remains a valid future objective only behind a new
proof-producing gate: first obtain a certified executable action-class upper
before broad-row materialization, then prove it strictly below every competing
class lower. Stable guesses, rankings, restricted scopes, incomplete
envelopes, and post-cap observations are not certificates.

**Context:** [Final report](archive/2026-07-27-certified-root-action-feasibility/report.md),
[Solver](solver/README.md), and
[tracked evidence](../fixtures/solver-natural-t1/v1/evidence/certified-root-action-feasibility-summary.json).

**Consequences:** The shadow probe is restored and no ABI, WASM, or product
work is selected. A future attempt starts from new exact action-local upper or
lower mathematics, not result plumbing.

## 2026-07-27 — Goal-Mask Probability Lower Is Not Root Separation

**Decision:** Do not wire the current optimistic pool-probability machinery
into root pruning or a verified-next-action result. Its isolated graph-free
goal-progress MDP separated zero of four hard cases from the archived exact
renewal uppers.

The candidate may remain mathematical input to a future proof, but a
goal-mask/rarity/affix-count state is not enough for complete action-class
separation. Non-goal first crafts such as junk benches can change blockers
without advancing a goal slot. When that effect is erased, the universal
conditioned lower is only the exact first price plus a free finish.

**Context:** [Final report](archive/2026-07-27-pre-expansion-probability-lower-audit/report.md),
[Solver](solver/README.md), and
[tracked evidence](../fixtures/solver-natural-t1/v1/evidence/pre-expansion-probability-lower-audit-summary.json).

**Consequences:** All measurement source is restored. Later verified-next-
action work needs both a cap-compatible executable upper and a lower that
retains enough non-goal first-action, blocker, and preservation state to
charge downstream work. A finite bracket alone is not progress.

## 2026-07-27 — Zero-Progress Reroll Is An Explicit Restricted Objective

**Decision:** Keep the unrestricted exact solver as the default. The
goal-progress-gated reforge mode is opt-in and exact only for a different
executable policy class: a zero-goal-progress reforge outcome may continue
only through a legal destructive reforge whose next kernel ignores the
discarded affixes. Goal outcomes share one terminal exit; zero-progress
outcomes share one retry basin per preserved boundary; partial-progress
outcomes remain complete exact states. Probability and resource mass are
never dropped or renormalized.

**Context:** [Final report](archive/2026-07-27-goal-progress-gated-reforge/report.md),
[Solver](solver/README.md), and
[tracked evidence](../fixtures/solver-natural-t1/v1/evidence/goal-progress-gated-reforge-summary.json).

**Consequences:** Gated results must use the
`exact_within_zero_progress_reroll_restriction` scope and must not be called
globally optimal over Annul, Exalt, Bench, protection, or other excluded
zero-progress salvage. Retained partial states keep the full solver action
envelope. The first frozen four-mod Chaos rows now fit under 200,000 states,
but corrected telemetry shows the next competing broad root reforge exhausts
the unchanged reforge-work cap before any partial state is expanded. Bounded
Pareto admission is deferred future design, not part of this decision.

## 2026-07-28 — Gated Root Renewal Is An Early Bounded Incumbent

**Decision:** Retain the gated-only fixed primitive renewal path. After a
completed gated root destructive-reforge row, the solver may publish the
executable policy “repeat the same reforge until goal” at `c / p` only after
proving action legality and the same complete engine-owned reforge-kernel
signature for every positive-probability non-goal exit.

**Context:** [Final report](archive/2026-07-28-gated-root-renewal-incumbent/report.md),
[Solver](solver/README.md), and
[tracked evidence](../fixtures/solver-natural-t1/v1/evidence/gated-root-renewal-incumbent-summary.json).

**Consequences:** This is an upper-bound witness exact within the
zero-progress-reroll policy restriction, not a globally optimal result and
not a state-equivalence claim. Competing actions remain in discovery and the
lower-bound problem. Compilation must independently revalidate the witness
and may then use one compact goal-or-repeat loop. The default unrestricted
solver is behaviorally unchanged.

## 2026-07-28 — Q-Directed Rows Stay Exact; Eldritch Setup Stays Real

**Decision:** Retain completed delayed action distributions for the lifetime
of a gated solve and direct further expansion by their exact
probability-weighted Q uncertainty. Reprice stored rows after continuation
values change; do not recompute their probability kernels. Exceptional-
support successors must be expanded before their action is admitted or
rejected. An open or overlapping envelope remains bounded and incomplete.

Expose exactly four automatic goal-relevant Eldritch compound candidates on
engine-certified eligible rare armour: Annul Prefix/Suffix and Chaos
Prefix/Suffix. Existing dominance uses the final currency directly. Missing
dominance must use and charge real Ember/Ichor setup, preserve the resulting
implicit tiers, and compile to every real operation.

**Context:** [Final report](archive/2026-07-28-q-directed-eldritch-side-actions/report.md),
[Solver](solver/README.md),
[Eldritch mechanics](mechanics/eldritch-and-influence.md), and
[tracked evidence](../fixtures/solver-natural-t1/v1/evidence/q-directed-eldritch-side-actions-summary.json).

**Consequences:** The full frozen case expands 8,908 states and raises
completed alternative lower-Q values by roughly one to two orders of
magnitude, but their upper ends still overlap the Chaos incumbent. No frozen
classification or exactness is claimed. Automatic standalone implicits,
Eldritch Exalt, Veiled, Influence Exalt, product cap changes, and the default
unrestricted solver remain outside the decision.
