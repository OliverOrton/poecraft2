# Anytime Benchmark Completion

**Status: active.** Oliver selected this bounded implementation chunk on
2026-07-26. Execute Gates 0-5 in order.

Parent: [Active work](README.md)

## Objective

Complete the existing bounded-solver benchmark substrate so fixed-budget runs,
including timeouts, remain statistically useful. Make normalized root-gap
integral the primary development score, retain explicit target-reaching and
failure evidence as guardrails, strengthen paired experiment identity, and add
target/performance profiles plus development-only racing. Do not change solver
mechanics, admitted actions, proof authority, production resource caps, or
browser behavior.

## Fixed boundaries

- The native engine remains the sole authority for mechanics, transitions,
  analytical bounds, executable policies, and certification.
- A finite numeric upper value is not an incumbent. Before
  `incumbent_kind != none`, normalized gap is exactly `1`.
- Focused lower-bound monotonicity is not assumed. Existing graph-growth traces
  contain lower-bound decreases; this milestone records and diagnoses them
  without converting a research question into an invariant.
- The fixed-horizon linear-time normalized root-gap integral is the primary
  comparison metric. Log-time summaries are supplemental.
- Exact completion extends a trajectory with zero gap through the common
  horizon. A completed resource-cap result extends its final certified gap.
  Administrative watchdog expiry is right-censored only when a durable partial
  trajectory exists. Crash, OOM, invalid bounds, cancellation, harness error,
  and memory refusal remain distinct failures.
- Racing may eliminate candidates only on development data. Frozen evaluation
  uses identical cases, budgets, resource caps, environment, and stopping
  policy for every candidate.
- Root-action certification, action-level Q intervals, learned guidance,
  persistent solved-state caching, and GPU work are out of scope.
- Acceptance runs once after Gate 4. Oliver owns visual review; this milestone
  has no rendered UI surface.
- Commits remain local-only and end with
  `Co-authored-by: Codex <codex@openai.com>`.

## Gate 0 - Metric and termination contract

1. Freeze normalized-gap, horizon-extension, exactness, censoring, failure,
   target-reaching, and numerical-floor semantics.
2. Define deterministic piecewise-constant integration from `t=0`.
3. Preserve explicit lower-bound-decrease and upper-bound-increase diagnostics.
4. Record the contract in stable solver documentation and tests.

## Gate 1 - Experiment identity and corpus roles

1. Strengthen comparable-run identity to include session/start/goal/caps,
   economy, action scope, corpus/data identity, executable identity, and
   benchmark configuration where available.
2. Add deterministic development, validation, and frozen-test roles by corpus
   family/template, preserving the existing v1 acceptance corpus.
3. Refuse or explicitly exclude paired observations whose required identity
   differs.

## Gate 2 - Durable incomplete trajectories

1. Give each isolated case an atomic partial-result sidecar owned by the
   runner.
2. Preserve partial native reports or trace snapshots before watchdog cleanup
   whenever the child emitted them.
3. Load analyzable watchdog observations without relabelling them completed.
4. Keep resource-cap completion, watchdog expiry, crash, OOM, cancellation,
   and harness failure separate.
5. Preserve survivor checks and resumability.

The current native benchmark writes only at process completion and cannot
observe inside one blocking `pc_solver_solve_step`. Gate 2 may establish
step-boundary durability without claiming sub-step checkpoints; any required
cooperative native snapshot mechanism must remain diagnostic-only and must not
change solver decisions.

## Gate 3 - Anytime analytics

1. Add fixed-horizon normalized gap integrals in wall-time and deterministic
   work units, plus supplemental log-time checkpoint averages.
2. Add first-hitting observations for 50%, 20%, 10%, 5%, 1%, and exact targets,
   including right-censor metadata.
3. Add restricted mean capped time-to-target, target-reaching curves, final
   gap among non-reachers, and completed-optimum upper/lower error
   decomposition.
4. Add paired differences/ratios, win/tie/loss, geometric summaries, and
   deterministic instance-level bootstrap confidence intervals.
5. Include incomplete analyzable cases and explicit failures in run summaries.

## Gate 4 - Profiles and development racing

1. Add target/data profiles over fixed time and work budgets.
2. Add Dolan-Moré-style performance profiles for gap integral and target time,
   with explicit failure penalties/caps.
3. Add a development-only racing decision that can stop a run when its
   accumulated integral lower bound cannot beat a fixed baseline margin.
4. Keep final/frozen reporting independent of adaptive racing.

## Gate 5 - Acceptance, evidence, and handoff

1. Run the complete affected ingest/benchmark tests once.
2. Run the repository acceptance pipeline because runner, corpus, analytics,
   native benchmark output, and stable docs cross the benchmark boundary.
3. Generate a pinned baseline report from existing or newly produced
   representative evidence; do not rerun expensive exact cases without need.
4. Update stable solver/corpus/evidence documentation with limitations,
   especially step-boundary sampling and censoring assumptions.
5. Archive this plan, restore `docs/active/` to no-active status, update
   `HANDOFF.md`, and create the local milestone commit.

## Completion criteria

The milestone is complete when timed-out observations are retained whenever a
partial trajectory exists; fixed-budget gap integrals and target observations
have pinned semantics; failures are not hidden as censoring; comparable-run
identity is strict; profiles and paired uncertainty are reproducible; racing
cannot affect frozen evaluation; the acceptance suite passes; and the durable
contract and evidence are archived.
