# Experiment contract and long-run budget

## 1. Do not rerun before resolving identity

Use the existing corpus runner/Lab profiles and `resolve_case_execution`/report owner. The supplied paths point to saved evidence, not automatically mounted checkpoints. Timed native cases run serially. Native fixed-eight and adaptive Calculator transport are distinct treatments. Preserve original cases, prices, admitted programme scope, 240-second requested Finish, 300-second native watchdog, 315-second host cleanup deadline and the resolved 1-GiB aggregate allowance. Inspect actual case values rather than assuming every context uses identical limits.

A one-GiB independent *post-run* evaluation allowance does not create a free extra one GiB during live search. Internal candidate limits must fit current retained ownership. Preserve cumulative logical-work ceilings and report local cap versus host timeout distinctly.

## 2. Baselines and target contrast

Baseline facts are in `evidence/baseline.json`. Reuse compatible completed evidence for correctness and graph comparisons. Source/algorithm changes can invalidate timing reuse even when graph bytes match. Never overwrite a historical ledger or edit a goal to force a match.

### Matrix A — Current target/profile separation (six runs at most)

| Arm | A4 | A5 |
|---|---|---|
| Current ordinary proof, clean L, proposal service off | baseline | baseline |
| Current neutral proof, clean L, proposal service off | treatment | treatment |
| Current neutral proof, coverage R, proposal service off | treatment | treatment |

E has no wall run if it is exactly the existing normal form. The ordinary-L versus neutral-L contrast measures profile impact. Neutral-L versus neutral-R measures the target contrast under that common profile. Neither is a same-clean quality claim about the new shared proposal family.

If no policy is available, show the actual policy-absent interval, final no-policy outcome and stop owner. Do not substitute a saved strategy. A converged selected graph is not global closure in neutral mode.

## 3. Same-clean candidate coverage

### Matrix B — eight-attempt Finder grammar comparison (four runs initially)

| Arm | A3 | A5 |
|---|---|---|
| Current K `conditional-retention`, 8 attempts | baseline | baseline |
| New selective-retention, 8 attempts | treatment | treatment |

Same target, root, native data, original prices and outer work/time/memory limits. Candidate generation and admission are intentional differences; report the exact candidate sets/order. Do not call this an order-only ablation. A3's old Current bounded-stop anomaly does not invalidate Finder target/cost checking, but cannot be used as a passing exact Current reference.

### Matrix C — ordinary Current consumes the shared family (two treatments)

Current ordinary clean A4/A5 with bounded shared service on, compared with source-matched Matrix A ordinary clean baselines. Do not run neutral here to make service look faster. Preserve ordinary proof, with upper-only external root candidates.

If compilation/adoption is exercised only in a synthetic fixture because every real candidate is worse or too large, report that limitation. The actual real run must still show invocation, refusal/headroom or economic comparison, not merely a enabled flag.

## 4. Extended candidate search and cross-context tests

After the same-eight comparison, permit a separately registered 24-attempt ceiling. Compare K and selective grammar on A5 under the same outer budget (two runs). A baseline that exhausts earlier is valid and explicitly shown. Do not extend the wall deadline to guarantee 24 completions.

One second native-feasible held-side/context case may be selected before tuning, using existing corpus tooling, plus a non-Eldritch no-op/ineligible control. Keep exposed armour/bow development cases out of a claimed untouched test set. A3/A5 prefix-vs-suffix initiation fixtures provide structural contrast even if they are not independent economic benchmarks.

Run the existing exact Regalia control for ordinary Current after proof-related code changes. Its target, prices and expected exactness come from the actual fixture, not a remembered number.

## 5. Global experiment allowance

Initial timed real-case allowance:

- Matrix A: up to 6.
- Matrix B: 4.
- Matrix C: 2 new service-on runs, reusing compatible off arms.
- Extended 24 attempt comparison: 2.
- Exact/non-Eldritch/cross-context controls: up to 4.
- Reversed-order or final-build confirmations tied to an actual timing/economic result: up to 6.

Total planned ceiling: **24 timed native case invocations**. Tiny unit/contract fixtures are not long runs and are chosen proportionately. Invalidated runs count and are recorded; do not silently keep relaunching until a preferred answer appears. A necessary build-identity rerun can use the reserved confirmation slots. When the ceiling is consumed, finish analysis/qualification using retained evidence and state the missing comparison; request an explicit extension rather than infer one.

Actual Calculator/worker validation is a separate small serial set through the existing probe: ordinary clean Current and Finder, selected new clean grammar where supported, one complete cancellation and one early-Finish control. Reuse appropriate existing tests for additional cases. Do not repeat every native matrix in WASM automatically.

## 6. Scorecard

For every run, record lane, goal/terminal, proof profile, grammar, candidate ceiling, all resource limits, actual action scope, runtime transport, build/data/economy identity and run purpose. Report:

- Best checked original-root cost and primitive count, graph identity, verification status and retained timing.
- First independently verified artifact, stronger-artifact events, final artifact and any periods without one.
- Final lower, global-lower capability and provenance, absolute/relative gap where meaningful, exact-closure availability and actual status separately.
- Discovered/queued/started/completed work, native rows/transitions and cumulative logical reforge work; cap/refusal/censoring reasons.
- Generation/compilation/checking/reached-programme validation time with parent-child nesting declared; aggregate live/peak ownership.
- Candidate lineage, semantic choices, duplicates, pending/exhausted frontier and attempt-limit termination.
- Requested Finish, native and host stop, cancelled-resource-release status, report expectations and raw process exit.

Do not derive first-event times from truncated snapshots as though exact. Do not compare sums of inclusive timers with total wall without a stated remainder. No claimed 30/60/240 event when a run already finished earlier; use a best-so-far step function with the terminal time explicit if plotting later.

## 7. Success interpretation

Engineering success is a functioning and correctly scoped capability. Semantic support success is operating Current R or correctly checking a new target, not a same-clean algorithm win. Economic success is any strict independently checked original-price decrease under a matching same-target budget. A proposed 20% improvement against the strongest compatible Current reference is a material research target, not a stop rule or a promise.

A large percentage gain from a weak Finder baseline can remain noncompetitive with Current. Extra attempts/work and longer native horizons must be disclosed. A small gain can be valuable if cheap and robust; a gain that consumes the budget and loses a stronger normal result is not an improvement to that final product comparison.

## 8. Run commands

Use actual `--help` and registered parser options before launch. Existing route skeleton:

```
powershell -File scripts/dev-engine.ps1 -Task Benchmark
# Resolve the project Python through scripts/python-common.ps1.
python -m poecraft_ingest.solver_corpus_runner \
  --root . --executable ACTUAL_EXE --artifact data/compiled/current \
  --corpus ACTUAL_MANIFEST --case ACTUAL_CASE \
  --max-workers 1 --output NEW_OUTPUT
python -m poecraft_ingest.solver_reports \
  --run baseline=BASE_RUN --run candidate=NEW_RUN \
  --pair baseline:candidate --output REPORT.json
```

These are schematic commands, not copy/paste promises for unimplemented options. The native-goal selector exists; the proof-profile, selective grammar and attempt-limit options in this packet are proposed until implemented. New values belong in the current benchmark/worker/resume identity owners, not shell-string patches or a new supervisor.
