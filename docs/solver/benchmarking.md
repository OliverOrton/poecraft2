# Solver benchmarks and cumulative research

The native benchmark and existing corpus/Lab/reporting code are the measurement
owners. The mathematical-backbone reporting and context tools are implemented;
[the integration record](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md)
records their actual validation. This page distinguishes implemented views from
unimplemented analysis and from runtime proof authority.

## Observation contract

A run is an anytime trajectory, not only a final status. Observable samples after
complete `pc_solver_solve_step` calls include time/phase, bounds and incumbent kind,
states/frontier, rows/transitions/logical reforge work and owned memory. Round,
incumbent, bounded-interval and completion samples follow the native contract.
A long blocking step does not contain invented internal observations.

The September 9 useful-proof-time experiment has separate case and profile
identities: 240-second requested finish, 300-second native watchdog and
315-second outer process cleanup, with the existing 1 GiB and other work caps.
The original 60/90-second fixtures remain short regressions. Read observations
near 60, 120, 180 and 240 seconds from one trajectory; an artifact first verified
at finalization is not an earlier verified upper. Compare treatment with control
at the same profile, including setup, proof and delivery time.

The optional native benchmark `--proof-handoff-seconds` requests a candidate
handoff before terminal finish. Its request time and actual handoff state/row
counts are reported separately. A request does not establish that strict work
started: check the phase trace, strict row counters and finalization attribution.
This diagnostic is not a public default or an extension of the finish deadline.

Lower decreases and upper increases are diagnostics, not violations of an assumed
monotonicity guarantee. Compare upper values only when compatible executable
incumbents exist. A finite implementation ceiling is not an incumbent.

The progress/cap `reforge_work` field is the `logical_work_v1` envelope.
V1/V2/V3 physical effort is explanatory, version-specific work, not a universal
runtime score. Strict-row comparisons default to V3; the existing
`raw_strict_reforge_oracle_diagnostic` and
`projected_reforge_frontier_diagnostic` select V1 and V2 respectively.
`factored_terminal_reforge_diagnostic` preserves explicit V3 selection for
historical evidence. These do not change the coarse V1 evaluator.

## Primary outcome and intermediate questions

The project outcome is expanding valid certified exact closure. Use a predeclared
cohort and resource envelope for comparison, and a separately labelled growing
development frontier. For N planned eligible cases, report qualifying closures
by observed time over N, with failures, unavailable evidence and strata visible.
A formatted zero gap or status string alone is not proof.

An exactness profile is an interpretation of existing evidence, not another native
checker. Its supported schema must be explicit. Unsupported evidence must not be
reported as a mathematical refutation or silently discarded from N.

Lower research can instead select a checked lower at a fixed total preparation
budget, time to an unchanged compatible target, or memory at a proof milestone.
Upper research can select a verified policy target. These are intermediate
outcomes, not exact solves. There is no weighted universal score.

The legacy reporter retains its original rates and
`primary_comparison_metric_selected: false` unless an explicit outcome profile
is supplied. The optional profile does not retrospectively change legacy report
semantics.

## Gap and trajectory semantics

For nonnegative costs the defined normalized gap is:

```text
g(t) = (U(t) - L(t)) / max(U(t), 1 cost unit)
```

Clamp only numerical noise. An actually invalid bound is a correctness failure.
Before a valid executable incumbent exists, gap is one. Incumbent kind and its
supporting evidence matter, not `isfinite(U)` alone.

The defined integral uses a right-continuous, piecewise-constant trajectory with
g(0)=1. A sample applies from its timestamp; do not smooth a lower decrease.
At a common horizon, exact completion extends at zero gap; completed cap/target
measurements extend with their last certified gap. A watchdog is right-censored
only with a usable atomic partial trajectory. Crashes, OOM, cancellation, invalid
bounds, memory refusal, runner failure and watchdog without a usable trajectory
remain distinct failures.

Relative-target time uses U/L - 1 only for positive L and a compatible incumbent.
Exact-target time needs its actual proof/evaluation status. These definitions do
not claim every integral or survival estimator is implemented. No adaptive
accumulated-gap racing is implemented or authorized by this research profile.

## Durable partial reports

The runner supplies a unique sidecar path, atomically replaced by the benchmark
after observable steps. After watchdog process-tree cleanup, a partial observation
is analyzable only if it contains the selected case and at least one bound sample.
The ledger remains `watchdog_expired`, not completed. A pre-first-step timeout
has no censored trajectory; setup duration does not manufacture one.

Native exits 0 or 2 with final reports are completed measurements. Exit 2 can
record a native resource-cap expectation miss. A completed measurement is not
necessarily a completed solve. Preserve known and unknown process failures
rather than coercing them into one timeout category.

## Experiment identity and comparability

Existing ledgers pin source and dirty paths, executable path/hash, corpus and
generator/schema identity, native artifact/data/string identity, machine/OS/
processor/Python context, concurrency, memory, evaluation/role settings and each
selected case's inputs, prices, action scope and resolved actions.

Calculator-quality requests pin `calculator_product_v1`. Profile-owned controls
stay with that native owner; labelled overrides remain visible. Resume refuses
incompatible prior output directories. A comparison can declare executable hashes
as its treatment without dropping the other identity checks.

A solver-only change normally leaves the target unchanged. A native transition,
price, terminal or scope change can alter the target despite unchanged JSON.
Label that semantic change or establish an explicit compatibility argument.

Paired performance requires compatible target, budget, machine/build/load and
clock origin, apart from declared treatments. Different resources can support a
labelled scaling study, not an unqualified speedup. Different starts/goals/prices
cannot be pooled as one raw Chaos value. Different auxiliary models can be
compared as native lower producers only with their valid native bridges.

A derived cross-run interval may combine independently compatible certificates
for the same target. It is not one run's trajectory or time-to-gap achievement.
Preparation and verification costs must be included, without adding them twice
when the native total already includes them.

## Corpus roles and result kinds

Keep original development/validation/frozen-test strata and acceptance tiers.
Record actual exposure: repeatedly tuned cases are not unseen evaluation simply
because an old filename or role contains `frozen`. Treatments and reused artifacts
are not independent replications. Keep all planned eligible cases in project
profiles, even when legacy summaries analyze only completed reports.

| Result kind | Interpretation |
|---|---|
| Finite-model optimum | Answer under its declared model/property/coefficient semantics |
| Native exact closure | Complete native proof and evaluated artifact under its recorded contract |
| Verified policy value | Entry-scoped executable upper, not optimum |
| Certified native lower | Source/domain/scope-qualified lower |
| Optimistic policy ceiling | Limit of an auxiliary model, not a native upper |
| Conditional/empirical observation | Only the stated evidence and assumptions |

Distinguish donor, action/program, complete-model, portfolio and public lower.
A stronger component may not change the complete minimum. A coupled unfractured
entry of an anchored certificate is not automatically an ordinary empty-start
certificate. Calls are not unique states. Fewer rows in a fixed-time run do not
prove work avoided. Diagnostic export and preparation costs remain explicit.

## Existing reporting

Legacy commands remain supported:

```text
python -m poecraft_ingest.solver_reports \
  --run baseline=PATH --run candidate=PATH \
  --pair baseline:candidate --output OUTPUT
```

A run directory is the runner's ledger/artifact collection, not arbitrary JSON
relabeled as a run. `compare_runs` retains its input mismatch exclusions. The
research-series adapter reads declared historical artifacts without inventing
missing trajectories or rerunning a solver.

<a id="research-series"></a>
## Research series and selected context commands

From the repository root, set `PYTHONPATH=tools/ingest`:

```powershell
py -3 -m poecraft_ingest.solver_knowledge lint
py -3 -m poecraft_ingest.solver_knowledge context --question RQ-002
py -3 -m poecraft_ingest.solver_knowledge check-metadata experiments/solver-research/backbone-pilot-v1.json
py -3 -m poecraft_ingest.solver_reports --research-series experiments/solver-research/backbone-pilot-v1.json --output build/research-state.json --markdown docs/solver/research-state.md
```

`lint --base REVISION` checks original statements/preconditions and append-only
history against that base. An appended responsible `Editorial:` event declares a
nonsemantic correction; lint does not verify that declaration's truth. Missing
base evidence is not a passed comparison. Current lint checks the syntax and
references it implements, not mathematical truth, exhaustive source correspondence
or a machine proof of a prose premise.

Selected context preserves requested statements and preconditions and refuses an
undersized character budget. The export records actual HEAD and a canonical
repository link base. Selected and directly linked working-copy files must match
committed bytes; dirty/untracked content refuses with its source name. Current
relative links become HEAD-pinned links; historical pins and external sources
remain intact. Visibility comes from locally recorded remote refs, with explicit
absence of network verification. A local-only commit's links may be unavailable
to an external reviewer; the command does not fetch or push.

[Generated research state](research-state.md) reads declared saved observations.
An unavailable absolute portfolio is not inferred from a gain. Original source
aliases, comparison attribution and evidence digests stay in the detailed JSON.
The existing series is a retention/lower history, not a fresh exact-closure corpus
measurement. It now separates first selected actions, explicit finite constraints
and auxiliary policy ceilings; full ties/families remain in JSON. Available
preparation stages and absence of end-to-end gain are explicit. RQ-001's small
availability table links historical exact references and policy uppers without
inventing fresh qualification or matched benchmark evidence. Regenerate this
file; the knowledge CI job compares its bytes with a fresh generation.

## Implemented exact-closure profile: coverage and limitations

`--outcome-profile FILE` currently accepts `kind: native_exact_closure_v1`, unique
`case_ids`, positive `budget_ms` and `memory_bytes`.

The implementation requires a completed report, the recorded
`policy_refinement.strict_lift.global_lower_bound_closed` flag, exact/converged
policy and termination status, complete matched compiled evaluation with zero
off-policy mass and reconciled cost, no reported errors, finite nonnegative
bounds, matching memory control and measured time/memory within the envelope.
It currently additionally requires equality of the three parsed reported numbers
`lower_bound`, `upper_bound` and `evaluated_policy_cost`.

This is a **conservative profile for that supported strict-lift report shape**.
It is not an exhaustive adapter for every native proof path. Missing strict-lift
evidence or differing endpoint fields can leave a record unqualified even when
another documented native path could justify it. Such records require evidence
classification, not a silently relaxed equality test. The profile does not issue
new numerical or native correctness certificates.

Closure time is the final recorded observation, not an earlier guessed step.
All planned cases remain in the denominator. `by_item_class` and
`by_corpus_stratum` are distinct; the latter reads `input.corpus.stratum` or reports
unavailable. Legacy `stratum`/`strata` retain their item-class meaning. Additive
`evidence_coverage` separates supported qualification, unsupported proof source,
unsupported endpoint shape and contradicted evidence; reasons remain explicit.
The early exact finish and strict compiled-lift issuers, plus the saved PDR
reclamation report, were inspected for this selected coverage review. Native
benchmark reconciliation has absolute/relative controls, while v1 deliberately
retains its stronger three-field equality requirement. No v2 adapter or broader
native acceptance was added.

## Bounded native comparison and documentation CI

The native benchmark's internal `--native-retention-diagnostic cold|reuse`
selects a single ordinary case with the optional lower on in both runs. It
rejects checkpoint, validation-only and other diagnostic combinations. The
native/C ABI defaults remain unchanged and off. WASM separately selects reuse for
[Oliver's browser testing](lower-pruning.md#opt-in-ordinary-native-retention-lower).
Use the saved case's original
caps and exact evaluator; this flag does not authorize Simulator or a new corpus.
The [numerical-reuse receipt](../archive/2026-09-07-checked-numerical-reuse-v1/README.md)
records the matched compact, ordinary and development observations.

Windows keeps the existing `build-and-test` check identity. A tested conservative
classifier skips native steps only for known root/documentation Markdown paths;
unknown, mixed, C++, build/data, unavailable or empty changes retain validation.
C++ comment-only changes are not inferred from filenames. The knowledge job still
runs, and the Windows check says when native work is not applicable. Local
classifier tests do not claim a hosted workflow run or branch-protection change.

## Current sources

Current code: `solver_benchmark.cpp`, `solver_corpus_runner.py`,
`solver_reports.py`, `solver_knowledge.py` and their focused tests. The historical
[benchmark contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md)
records the original observation semantics. Current mathematical status and gaps
are owned by [claims](claims.md) and [research](research.md#open-obligations), not
by the age of a verification stamp.
