# Solver Benchmark Trajectories

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

The native benchmark and existing corpus/Lab reporting remain the measurement owners. This page preserves their observation, identity, and failure semantics while recording the owner's exact-closure research objective. New question-linked views belong to the pinned backbone implementation; this document does not claim those tools have already landed.

## Observation Contract

A run is an anytime trajectory, not only a terminal label. After complete `pc_solver_solve_step` calls, the native benchmark records elapsed time and phase, root bounds/gaps and incumbent kind, state/frontier counts, rows/transitions/logical reforge work, owned memory, and diagnostic changes to lower/upper values.

Samples are step-boundary observations. A long blocking step has no implied internal checkpoint. Samples also occur at the documented round/incumbent changes, bounded wall intervals, and completion. Do not fabricate sub-step timing from an aggregate phase duration.

Lower decreases and upper increases are diagnostics, not violated monotonicity assumptions by definition. Compare upper values only when compatible executable incumbents exist. A finite implementation ceiling is not an incumbent.

`reforge_work` in progress/cap samples is the `logical_work_v1` envelope. Legacy active work and V1/V2/V3 physical effort are explanatory, version-specific counters. Keep them separate from wall time, memory, and maximum cooperative step latency.

For strict-row comparisons, omitted evaluator controls select V3. `raw_strict_reforge_oracle_diagnostic` selects the V1 oracle/rollback and `projected_reforge_frontier_diagnostic` selects V2. `factored_terminal_reforge_diagnostic` retains explicit V3 selection for older evidence. These do not change the coarse V1 evaluator.

## Primary Outcome And Comparison Profiles

The owner-approved project objective is to extend certified exact closure as far as practical. A fixed cohort and end-to-end time/memory envelope provide a comparison, while the separate development frontier can keep expanding.

For the project profile, report valid exact completion over **all planned eligible cases**, by stratum and against time. The exactness predicate includes compatible proof/evaluation and the declared numerical contract; a rounded zero gap or label alone is insufficient. Missing reports and correctness failures remain visible and cannot improve the score by disappearing from its denominator.

Lower-development questions may instead specify a checked lower at a fixed total preparation budget, or time to a fixed lower/gap target. Upper-development questions can specify a verified policy target. Those are intermediate research outcomes, not substitutes for exact closure. Do not combine them into a weighted universal score.

**Implementation state:** legacy `solver_reports.py` calls preserve `primary_comparison_metric_selected: false` and their original denominators. An explicit `--outcome-profile` adds the predeclared cohort view with complete-proof, evaluated-artifact and numerical-reconciliation predicates; the default legacy statistics are not retrospectively relabelled. `--research-series` is the separate read-only archived-evidence view described below.

## Frozen Future Metric Semantics

When normalized gap is reported, use the existing nonnegative-cost contract:

```text
g(t) = (U(t) - L(t)) / max(U(t), 1 cost unit)
```

Clamp only numerical noise into `[0,1]`; an actually invalid bound is a correctness failure. Before an executable incumbent exists, gap is one even if a finite upper-like field is present. `incumbent_kind != none`, with the associated valid evidence, distinguishes an incumbent from an implementation ceiling.

The defined integral uses a right-continuous, piecewise-constant recorded trajectory, with `g(0)=1`. A sample becomes known at its timestamp. At a common horizon:

- exact completion continues with zero gap;
- a completed cap or product-target result retains its last certified gap;
- watchdog expiry is right-censored only when a valid atomic partial trajectory exists;
- crashes, OS OOM, invalid bounds, cancellation, memory refusal, runner errors, and watchdogs with no usable trajectory remain distinct failures.

Relative-target time uses `U/L - 1` only for positive L and a compatible executable incumbent. Exact-target time requires the actual exact policy/termination evidence, not floating-point equality alone.

These definitions do not assert that every integral, survival estimate, or profile is already implemented. They also do not authorize adaptive accumulated-gap racing, which can preferentially discard runs slow to find their first incumbent rather than identify the best eventual exact solver.

## Durable Partial Reports

The corpus runner gives each attempt a unique partial-result sidecar. Native observable steps atomically replace it. After watchdog cleanup and the no-survivor check, a partial observation is analyzable only if it contains the selected case and at least one bound sample.

The ledger stays `watchdog_expired`; a partial report is never relabelled a completed solve. A timeout before the first completed step has no censored trajectory. No missing first-step observation is invented from setup timing.

Native exits 0 or 2 with a final report are completed measurements; exit 2 preserves expectation misses such as a native resource cap. Known abnormal/OOM statuses and unknown failures remain separately classified. A completed measurement need not have found a policy or solved exactly.

## Experiment Identity

The existing ledger pins source commit and dirty paths; executable path/hash; corpus identity/schema/generator hash; compiled-artifact manifest and data/string identities; machine/OS/processor/Python context; worker/memory/evaluation/role settings; and complete selected case inputs, prices, action scope, generation metadata, and resolved actions.

Calculator-quality requests pin `calculator_product_v1`; profile-owned low-level controls stay with the native profile unless a labelled override is intended. Telemetry records the actual override mask.

Resume refuses an existing run directory when required executable, corpus, artifact, machine, or configuration identity differs. Executable identity is part of an observation. In a declared baseline/candidate comparison, executable hashes can be the treatment while all other applicable controls must match.

Code can change target semantics even if the JSON is identical. Distinguish a solver implementation change from a native transition, terminal, or scope correction. A semantic correction requires the corresponding reviewed target/property version or an explicit compatibility argument.

## Corpus Roles And Planned Populations

The natural-T1 corpus assigns whole strata to development, validation, or `frozen_test`; acceptance tiers remain separate. Preserve the original role manifest and record actual exposure. Repeated use to select implementations cannot be described as untouched generalization evidence merely because the file is called frozen.

A series identifies its planned cases and attempts before results are summarized. Existing legacy rates may be conditional on completed/analyzable cases; keep those labelled and add the project-level all-planned denominator instead of silently changing the old interpretation.

Variant implementations are treatments, not independent replicas of the same stochastic experiment. Repeated reports or reused immutable artifacts are not additional samples.

## Evidence Interpretation

| Evidence kind | What it can establish |
|---|---|
| Declared finite-model optimum | Exact answer for the explicitly identified auxiliary/coefficient model |
| Native exact-closure result | Native result under its scope and stated numerical proof contract |
| Verified fixed-policy value | Executable upper from the evaluated entry |
| Certified native lower | Lower over its documented source/member domain |
| Optimistic-model policy ceiling | Limit of that particular relaxation, not a native executable upper |
| Conditional or empirical observation | Only its explicitly supported proposition and limitations |

Do not combine a lower from an anchored source with an empty-item upper. A coupled fresh state inside a special anchored proof is not automatically a new empty-request certificate. The latest retention evidence is a useful example, not a permanent benchmark target selected by this page.

A local action-bound gain can leave the complete minimum unchanged. Complete-model and portfolio gain can occur without actual pruning. Faster preparation can help the ordinary budget without proving faster exact closure. Calls are not unique-state coverage, and fewer rows at timeout are not necessarily rows avoided.

Performance comparisons need matching target, budget, machine/build context, concurrency/load conditions, and observation semantics. Same-target results with different resources can be labelled mathematical or scaling evidence; unrelated tasks cannot be pooled into a raw Chaos average. Report failures and censored observations rather than selecting only cases both methods solved.

## Existing Reporting And Cumulative Research

`solver_reports.py` already loads run ledgers, summarizes strata, and compares compatible pairs. The current interface remains usable:

```text
python -m poecraft_ingest.solver_reports \
  --run baseline=PATH_TO_RUN --run candidate=PATH_TO_RUN \
  --pair baseline:candidate --output PATH_TO_REPORT
```

Use the repository's Python environment and launcher from `AGENTS.md`. A run directory is an existing ledger/artifact collection, not an arbitrary JSON result relabelled as a run.

The pinned programme extends that same owner with question/series references, typed historical probe adapters, and generated current views. The index stores references, not a second set of case documents or manually copied raw numbers. Missing archived identity or trajectories remain unavailable; an adapter must not manufacture them.

[Research](research.md) supplies the human question and interpretation. Original artifacts remain the evidence. Existing reports can demonstrate the cumulative view without new native solves simply to test the reporting code.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md) and [solver_reports.py](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/tools/ingest/poecraft_ingest/solver_reports.py), [evaluation-roles.json](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/fixtures/solver-natural-t1/v1/evaluation-roles.json), [2026-09-05-native-applied-reforge-preparation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md). Mathematical links refer to the companion draft chapters and provisional claim IDs; they do not declare those claims accepted. Local implementation correspondence must be reconciled during integration.

<a id="research-series"></a>
## Research series and selected context commands

From the repository root, set `PYTHONPATH=tools/ingest` and use:

```powershell
py -3 -m poecraft_ingest.solver_knowledge lint
py -3 -m poecraft_ingest.solver_knowledge context --question RQ-002
py -3 -m poecraft_ingest.solver_knowledge check-metadata experiments/solver-research/backbone-pilot-v1.json
py -3 -m poecraft_ingest.solver_reports --research-series experiments/solver-research/backbone-pilot-v1.json --output build/research-state.json --markdown docs/solver/research-state.md
```

[The generated view](research-state.md) reads original archive references, preserves
missing values, and separates scoped native lowers from auxiliary policy ceilings
and conditional development controls. Explicit predecessor comparisons take
precedence over support-control diagnostics. The two saved source aliases retain
their original exact-source keys in the linked reports. They are not fresh unseen
validation or a general empty-start request.

`solver_knowledge lint --base <revision>` checks statement/precondition identity,
append-only history and affected source uses. An appended responsible `Editorial:` history event declares a typo/format correction without a new ID; lint reports that declaration, but cannot verify its truth. Context export refuses an undersized
`--max-chars` budget instead of truncating premises. Lint warns about open or
superseded prerequisites and fails active reliance on refuted/withdrawn claims;
it does not verify mathematical truth. `check-metadata` is a dry-run with no solve.

For existing `--run LABEL=PATH` reports, optional `--outcome-profile FILE` accepts
`kind: native_exact_closure_v1`, explicit unique `case_ids`, `budget_ms`, and
`memory_bytes`. The denominator is all declared cases, including missing/failed
reports. Closure requires native `policy_refinement.strict_lift.global_lower_bound_closed`,
`exact_closed`/converged classification and matched complete compiled evaluation.
The time is the recorded final observation, not an invented earlier internal
checkpoint. This reports the native numerical reconciliation contract; it does
not assert symbolic equality. Unsupported historical records remain unqualified.
Existing paired identity rejection and trajectory/gap formulas are unchanged.
