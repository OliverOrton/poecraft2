# Scheduling And Bellman Search

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page describes how the current work owners cooperate. [Search and resumption](mathematics/search-and-resumption.md) owns the distinction between soundness, eventual progress, and bounded performance.

## Search Flow

`SolveWork` grows reachable abstract states, installs completed price-independent sparse rows, prices compatible variants, and advances Bellman and fixed-policy work. Focused mode alternates bound and policy work around retained carrier epochs. Incremental mode also grows delayed action families and generated programs through an explicit action-envelope ledger.

The ordinary ladder remains the planner. Candidate-local continuation is retained state from one ordinary joint-policy attempt, not a second planner or a permanent fragment library.

Primary owners are `solver_solve.cpp`, `solver_solve_expand.cpp`, `solver_solve_incremental.cpp`, `solver_solve_focused.cpp`, `solver_solve_priority.cpp`, `solver_solve_bellman.cpp`, `solver_sparse_policy.cpp`, and the joint-policy continuation owner.

## Ordering Versus Proof

Goal-subset bucket service retains carrier diversity. Within the relevant work order, progress, protection/fracture, capacity, blockers, or unrelated occupancy may guide preference. Those scores do not become admissible values or retirement evidence.

Keep four decisions distinct:

| Decision | Required owner |
|---|---|
| Which obligation runs next? | Scheduler and its ordering policy |
| Is an action legal or inside the request? | Native registry and scope contract |
| Can an unmaterialized obligation be retired? | Compatible lower/upper proof and the action ledger |
| Can a strategy be published? | Compiler, independent evaluation, and publication classifier |

An open restricted graph may provide useful ordering information. Its value cannot replace a full-scope lower simply because it is finite or exactly solved within the restricted graph.

## Bellman Contract

True goals have zero value. A nonterminal row has its priced immediate cost plus expected continuation, with observed groups choosing under the permitted information. Sparse selection uses strict finite objective order; policy-stability checks use the separate named tolerance, and exact ties retain deterministic order.

A fixed policy must be proper from the relevant entry before its cost can supply an executable upper. An available Restart action is not a proof that the selected policy takes it or eventually reaches the goal.

The strict-rank initializer is sufficient, not complete for mutual retries;
the full numerical seed and the joint progress seed can handle examples that
its isolated finite-Q repair misses. A complete-row support construction is
only a candidate proposal under the [safe-progress contract](mathematics/policies.md#proper-seed).
Before adding that proposal, distinguish missing native row/goal support from
failed selection. The [bounded Ring/Amulet investigation](../active/2026-09-10-proper-policy-recovery/README.md)
found the former; its selector and first-policy dependency experiment were
removed. No new runtime activation or upper issuer follows from that work.

The lower-only quotient is a different consumer of shared numerical machinery: it checks a declared complete optimistic model and cannot issue executable policy authority. See [lower/pruning](lower-pruning.md).

## Candidate-local continuation

A retained joint-policy candidate can preserve its selected prefix and current missing continuation across ordinary interleaving. Compatible completed row evidence permits it to resume rather than rebuilding the entire selection walk.

There are separate consequences for yield and terminal refusal. Active yield retains its bounded candidate preference. Terminal refusal must release active payload and allow ordinary joint-policy work to continue; a refused object is not an owner of the next schedule. The reclamation archive records this concrete distinction without making the old failed experiment a permanent scheduler rule.

New value estimates need not make a fixed executable candidate invalid merely because it is no longer greedy. Changed rows, prices, routes, scope, or other semantic dependencies require the appropriate revalidation or discard. See [CLM-0021](claims.md#clm-0021).

With no incumbent, the existing joint walk can request every missing selected
continuation after a restricted solve or its failed-policy boundary. Actual
terminal debt and missing-row work are proposal preferences only. Completing a
proper candidate starts cooperative compilation/evaluation before ordinary upper
improvement; the portfolio retains the verified result. A dynamic automatic
epoch's service yield receives one scheduler re-entry before exhaustion is
considered, only while no incumbent exists. Existing incumbents retain their
publication opportunity and strict improvement. Checkpoints, admission and cap
guards remain active.

## Progress and interruption

The retained scheduler exposes service and starvation observations. This page does not assert a general eventual-closure theorem for arbitrary generated grammars. Such a theorem needs its actual finiteness, row-completion, refinement, numerical termination, and fairness assumptions; [CLM-0022](claims.md#clm-0022) records that distinction.

A cap-stopped solve can still preserve checked independent lowers and a verified incumbent. It cannot declare the remaining action family closed, reinterpret the frontier as success, or promote an unverified candidate.

## Failure And Telemetry

Inspect current phase/owner, action-envelope state, frontier, rows and transitions, Bellman sweeps/residuals, joint-policy attempts, candidate lifecycle, and the first resource/finish owner. Ask whether a named continuation was observed early, serviced later, or actually refused; do not infer a scheduling defect from one capture-time snapshot.

Use source and existing artifacts first. Test scheduler changes against genuinely matched controls; more expanded states or fewer rows under a fixed time budget does not alone establish improvement.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/scheduling-bellman.md) and [2026-08-30-carrier-ladder-released-candidate-reclamation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md), [2026-08-29-carrier-ladder-state-213-service-coverage-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-29-carrier-ladder-state-213-service-coverage-v1/README.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
