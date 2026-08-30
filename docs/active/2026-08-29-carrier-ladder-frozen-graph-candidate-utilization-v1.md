# Carrier-Ladder Frozen-Graph Candidate Utilization v1

**Status: active.** Oliver selected this implementation boundary on 2026-08-29
from clean `main` checkpoint
`91b0c3db79613bcf5f1e8b222e3d91d2aefaad5e`.

Parent: [Active work](README.md)

## Objective

Determine whether the final materialized rows of the fresh current full
five-goal run already support a substantially cheaper proper executable policy
than the published approximately `1568614.098859`-Chaos / `1192167.803886`-
action strategy, and whether current joint assembly, independent exact
evaluation, incumbent comparison, or final publication loses that policy.

Fossil occupancy is not a target. The historical strategy is diagnostic
evidence only and receives no current incumbent, seed, or candidate authority.

## Frozen Evidence Inputs

- current full attempt:
  `attempt-e47e3fb3-e16c-4c3a-9518-bd096bb3da96`;
- current strategy: 329 nodes / 868 edges, SHA-256
  `957d77256010ffaac583ae082b7c25cbeac2e51bf87daa77d6855cf008aa0db0`;
- current ladder: 31 goal subsets, two epochs, 343 candidates, 95 generated
  operators, 2,251 generated rows, two joint attempts, one joint success;
- current exact value: `1568614.0988592813` Chaos;
- historical exact strategy:
  `docs/archive/2026-08-22-exact-goal-carrier-ladder/strategy-verified/conquest-lamellar-allflame-exact-zero-to-five.strategy.json`;
- historical strategy SHA-256
  `6082eef2462295c167cc6b0ec9855b397f2f3f673c15245fdef93b2300358104`;
  and
- historical exact value `87361.1690420501` Chaos with approximately 8,303
  expected primitive actions.

Large reports and strategies stay on disk. The living log records only bounded
fields, identities, witnesses, decisions, and the next executable step.

## Execution

### 1. Existing evidence and static path audit

Project the current report, strategy, bundle, and historical exact evaluation
into bounded candidate, incumbent, occupancy, and generation facts. Trace:

- `maybe_install_incremental_anytime_incumbent()`;
- `try_install_reachable_incumbent()`;
- completed-row checkpoints and final requested-finish/resource publication;
- fixed-frontier attachment, properness, compilation, and independent exact
  evaluation; and
- `IncumbentPortfolio` comparison and retained publication.

Current source already retries reachable-incumbent installation at completed-
row checkpoints and finalization. Do not add another pass unless evidence
proves that the existing final attempt omits terminal completed rows.

### 2. Bounded joint-attempt lineage

If existing artifacts cannot distinguish the live explanations, retain the
smallest benchmark-private lineage record for each joint attempt:

- ordinal and source/target graph generations;
- completed-row count/identity and delta since the prior attempt;
- incumbent identity and exact cost before the attempt;
- candidate root estimate;
- selected row/action identity by reachable state;
- certified-frontier and renewal-boundary uses;
- properness and first missing continuation;
- compiler and independent exact-evaluation results; and
- candidate exact cost plus portfolio accepted/rejected reason.

Prove whether the terminal attempt sees every row completed at terminal
ordinary publication and whether the recorded joint success is the published
1.568M strategy, what it replaced, and which completed rows caused the change.

### 3. Frozen-final-row reconstruction

Against the exact final frozen row set, run the existing policy optimizer
without generating another state, row, transition, or action. Independently
compile and exact-evaluate every candidate used for a quality conclusion.
Candidate estimates alone are not upper authority.

### 4. Historical first divergence

Compare exact evaluated occupancy of current and historical strategies. For
high-occupancy historical action/state shapes, classify current availability
as undiscovered, out of caller scope, row unoffered, incomplete, unconsidered,
not selected, continuation incomplete/improper, compiler/evaluator rejected,
semantically incompatible, or present/usable.

Identify the first high-occupancy divergence and its first continuation
cascade. Measure whether localized online completion is materially smaller
than broad global row closure; do not chase arbitrary state numbers.

### 5. Decision and retained work

- **A — better final-row policy exists and was lost:** retain only the proved
  visibility, cadence, boundary-value, deterministic selection,
  compile/evaluation handoff, incumbent comparison, or publication repair.
- **B — important continuation rows are absent:** close diagnosis-only and
  write the evidence-backed `Carrier-Ladder Online Continuation Completion v1`
  successor plan. Concrete continuations must be generated online from live
  exact ladder carriers, never stored in a permanent craft-specific fragment
  library.
- **C — historical policy is no longer current-valid:** record the first exact
  semantic incompatibility and establish a fresh current quality reference.

Do not retain historical action injection, another planner/scheduler/global
comparator, action filtering, cap increases, fragment integration, RCASSP,
learned guidance, or mechanic/probability changes.

### 6. Verification and closeout

Use only focused tests needed to distinguish an uncertainty or validate a
retained source change. Do not run Simulator during attribution. Exactly
10,000 Simulator trials apply only if a materially changed compiled strategy
is retained and needs final qualification. A diagnosis-only or benchmark-
private telemetry result uses its smallest focused tests and
`git diff --check`; the full repository pipeline runs at most once and only
for a retained production or cross-layer behavior change.

## Living Log

### 2026-08-29 — Activation and first command

- HEAD, branch, and upstream matched at
  `91b0c3db79613bcf5f1e8b222e3d91d2aefaad5e`; protected untracked `0` was the
  sole status entry and was not read or modified.
- The preceding Fossil boundary was archived diagnosis-only / evidence-
  complete without additional tests, simulations, or solver controls.
- Historical strategy exists at 1,161,987 bytes, SHA-256
  `6082eef2462295c167cc6b0ec9855b397f2f3f673c15245fdef93b2300358104`.
- Static ownership search located reachable-incumbent installation in
  `solver_solve_constructive.cpp`, incremental cadence in
  `solver_solve_incremental.cpp`, and finalization retries in
  `solver_solve_finish.cpp`.
- Next: project current saved attempt fields and trace the complete joint
  attempt through final publication before adding instrumentation.
