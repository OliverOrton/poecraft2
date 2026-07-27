# Anytime Benchmark Completion

**Status: active.** Oliver selected this bounded implementation chunk on
2026-07-26 and reduced it before implementation. Execute Gates 0, 1, 2, and 5
in order.

Parent: [Active work](README.md)

## Objective

Complete the existing bounded-solver benchmark substrate so fixed-budget runs,
including timeouts, retain useful trajectories. Freeze the future metric
semantics, strengthen experiment identity, and make durable incomplete
trajectories the core deliverable. Do not implement gap-integral analytics,
survival summaries, profiles, paired uncertainty, or racing in this milestone.
Do not change solver mechanics, admitted actions, proof authority, production
resource caps, or browser behavior.

## Fixed boundaries

- The native engine remains the sole authority for mechanics, transitions,
  analytical bounds, executable policies, and certification.
- A finite numeric upper value is not an incumbent. Before
  `incumbent_kind != none`, normalized gap is exactly `1`.
- Focused lower-bound monotonicity is not assumed. Existing graph-growth traces
  contain lower-bound decreases; this milestone records and diagnoses them
  without converting a research question into an invariant.
- Exact completion extends a trajectory with zero gap through the common
  horizon. A completed resource-cap result extends its final certified gap.
  Administrative watchdog expiry is right-censored only when a durable partial
  trajectory exists. Crash, OOM, invalid bounds, cancellation, harness error,
  and memory refusal remain distinct failures.
- No adaptive racing is implemented. The proposed accumulated-integral rule is
  rejected because pre-incumbent gap is the maximum value, so it
  preferentially culls slow-to-first-incumbent runs rather than measuring
  eventual exactness.
- Frozen evaluation uses identical cases, budgets, resource caps, environment,
  and stopping policy for every candidate.
- Root-action certification, action-level Q intervals, learned guidance,
  persistent solved-state caching, and GPU work are out of scope.
- Acceptance is limited to affected ingest/benchmark tests after Gate 2.
  Oliver owns visual review; this milestone has no rendered UI surface.
- Commits remain local-only and end with
  `Co-authored-by: Codex <codex@openai.com>`.

## Gate 0 - Metric and termination contract

1. Freeze normalized-gap, horizon-extension, exactness, censoring, failure,
   target-reaching, and numerical-floor semantics.
2. Define deterministic piecewise-constant integration from `t=0` for a future
   analytics milestone without selecting it as a primary score.
3. Preserve explicit lower-bound-decrease and upper-bound-increase diagnostics.
4. Record the contract in stable solver documentation and tests.

## Gate 1 - Experiment identity and corpus roles

1. Strengthen comparable-run identity to include session/start/goal/caps,
   economy, action scope, corpus/data identity, executable identity, benchmark
   configuration, and the natural-T1 generator-config hash where available.
2. Add deterministic development, validation, and frozen-test roles by corpus
   family/template, preserving the existing v1 acceptance corpus.
3. Refuse or explicitly exclude paired observations whose required identity
   differs.

## Gate 2 - Durable incomplete trajectories

This is the milestone's core deliverable. The cooperative native snapshot
mechanism is the highest-risk item and lands before the runner/report changes.
It must expose step-boundary state without changing solver scheduling,
decisions, or proof semantics.

1. Give each isolated case an atomic partial-result sidecar owned by the
   runner.
2. Preserve partial native reports or trace snapshots before watchdog cleanup
   whenever the child emitted them.
3. Load analyzable watchdog observations without relabelling them completed.
4. Keep resource-cap completion, watchdog expiry, crash, OOM, cancellation,
   and harness failure separate.
5. Preserve survivor checks and resumability.
6. Include incomplete analyzable cases and explicit failures in run summaries.

The current native benchmark cannot observe inside one blocking
`pc_solver_solve_step`. Gate 2 establishes durable step-boundary snapshots and
states that limitation plainly; it does not claim sub-step checkpoints.

## Gate 5 - Acceptance, baseline, evidence, and handoff

1. Run the affected ingest/benchmark tests once. Do not run the full repository
   pipeline: this scope does not touch mechanics, SQLite, the compiled
   artifact, bindings, or web.
2. Produce the baseline by actually running the benchmark under the new frozen
   semantics. Existing evidence predates this contract and is not comparable.
3. Report the fresh baseline in deterministic work units as well as wall time.
   State plainly that wall-time figures are machine-bound and do not survive a
   hardware or compiler change.
4. Update stable solver/corpus/evidence documentation with limitations,
   especially step-boundary sampling and censoring assumptions.
5. Archive this plan, restore `docs/active/` to no-active status, update
   `HANDOFF.md`, and create the local milestone commit.

## Completion criteria

The milestone is complete when timed-out observations are retained whenever a
partial trajectory exists; future gap-integral and target observations have
pinned semantics without being selected as a primary score; failures are not
hidden as censoring; comparable-run identity includes the generator-config
hash; a fresh frozen-semantics baseline records wall and deterministic work;
the affected tests pass; and the durable contract and evidence are archived.
