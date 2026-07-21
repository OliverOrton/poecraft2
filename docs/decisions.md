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

**Implementation status:** approved but not implemented at the checked
baseline. Calculator currently retains its keyed solve handle after strategy
transfer and can reuse its price-independent transition data on a later Solve.
See [End-To-End Solver Flow](solver/flow.md#current-repricing-and-lifetime).

**Context:** The decision is preserved in the
[B1/S8 plan](archive/2026-07-19-bestiary-solver-s8/plan.md); delivery remains
deferred in the [solver roadmap](future/solver-roadmap.md).

**Consequences:** Stable docs do not promise cheap in-place browser repricing
or authorize retaining both large native and JavaScript graph copies as the
permanent design. Until delivery, implemented-behavior references must still
state that the handle is currently retained.

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
