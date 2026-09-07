# Ordered action coverage and relation construction

September 7, 2026, from clean reviewed `14eec9a353240c6bf4a16a8a98a9675ed1b7368a`.
The retained engine checkpoint is `3d8093d`. This run implements the smallest
measured improvement found in the remaining relation-construction work. The
completed numerical-reuse boundary and WASM's enabled browser-testing mode are
preserved. There is no new solver, filter, lookup cache, raised cap or reduced
native scope.

Oliver's explicit request and latest **Native Solver Research** summary select
this work. The thread reader did not expose its generated `implementation_plan.md`
or companion attachment, and the browser was logged out. Those files were
requested from Oliver; this checkpoint does not claim compliance with acceptance
criteria in an unread attachment. The old numerical-reuse plan was not substituted.

## Retained mechanism and review

`validate_canonical_action_coverage` keeps the complete scope/generation guards.
An allocation-free full-key scan accepts strictly ordered explicit sets with
matching actual keys, no families and no exclusion payload. Every unmet premise
falls through to the unchanged general partition checker, retaining its error
behavior and acceptance of valid unordered sets. This is a per-call proof, not
memoization or a cardinality shortcut. The [canonical mechanism](../../solver/lower-pruning.md#coverage-and-lower-only-queries)
records the uniqueness/partition argument under existing CLM-0008.

No action admission, native successor geometry, probability, minimizing choice,
row allocation, numerical seed, price shortcut or exact checking rule changed.
Current-vector final minimization remains complete. The 17-line fast path adds
no owned allocation or persistent state. Existing family checks remain the
fallback authority.

The coordinator wrote/integrated the source and tests and exclusively launched
timed work and staged commits. Three nonrecursive helpers separately reviewed
performance/mathematics, updated only canonical documentation, and recovered and
reviewed qualification evidence. Review added paired-invalid tests with matching
duplicate or empty keys on both sides; mismatched keys alone would not falsify
omitted uniqueness/nonempty guards. All helpers completed before 14:31 Vancouver.

## Matched results

Both sides use current checked **and** untrusted numerical reuse, the same native
inputs, complete product action scope, 32 MiB additional proof and 1 GiB total
limits. Baseline executables were preserved before edits; their hashes match the
preceding boundary's final native executables. One sequential baseline/candidate
pair was run, with no repetitions seeking a favorable budget crossing.

| Compact stage | Current baseline | Ordered coverage |
|---|---:|---:|
| Total preparation | 29.6357040 s | 25.6612006 s |
| Relation construction | 21.1384239 s | 17.5377853 s |
| Coverage checking (inside relations) | 2.9037419 s | 0.0411238 s |
| Numerical solve | 6.5125168 s | 6.3425446 s |
| Checking | 1.7986808 s | 1.5936630 s |
| Diagnostic export (inside relations) | 0.3075899 s | 0.2429053 s |
| Additional peak | 31,680,508 B | 31,680,508 B |

Preparation falls **13.41%**, relation construction **17.03%**. Coverage allocation
is the directly targeted cost; other stage differences are single-run
observations, not separately established algorithmic improvements.

Complete compact semantic payloads match exactly: all 3,958 coordinates/values,
172 native witnesses, 26,259 final relations, both source/program/complete-model
payloads and controls. Non-timing preparation work and resource ownership also
match: 56 outer rounds, 6,985 sweeps, 12 checked seeds and 17 untrusted seeds,
with no seed refusal, cold numerical restart or checked-zero fallback.

| Accepted quantity | Primary source | Prefix-removed source |
|---|---:|---:|
| Donor / complete model / portfolio, both builds | 352.31017033879505 | 355.43265454362336 |
| Mandatory program, both builds | 355.789813661076 | 358.87229571369824 |

The unchanged development case is
`conquest-lamellar-allflame-fractured-4-to-5-product8`: original start, goal,
Allflame economy, product scope, 8-work-item steps, **30-second** bounded finish,
60-second watchdog and original exact-evaluator limits. Its independently
evaluated historical upper target remains **2698.87479601436**, tolerance **1e-7**.
Full recorded input and artifact identities match between builds.

| Development observation | Baseline | Ordered coverage |
|---|---:|---:|
| Native setup | 33.3833226 s | 30.2236184 s |
| Reported total | 33.9262079 s | 30.7668097 s |
| External process wall | 34.4169419 s | 31.2443910 s |
| Discovered / expanded / frontier states | 327 / 1 / 326 | 327 / 1 / 326 |
| Verified upper | 10,401,458.65388722 | 10,401,458.65388722 |
| Lower | 352.31017033879505 | 352.31017033879505 |
| Absolute gap | 10,401,106.34371688 | 10,401,106.34371688 |

Setup still exceeds 30 seconds. Both runs expand only the root and miss the
target; exact closure remains **0/1**. The returned fallback's exact evaluator
completes with probability one, no off-policy mass, matching cost and no errors.
Native peak ownership is unchanged at 45,450,253 B; both cap checks pass. Faster
preparation has not established useful discovery, better policy quality or closure.

## Validation and next closure question

Passed: incremental native build; **822 quotient-proof** and **217 phase-lower**
checks; the existing exact compact audit over **all 26,259 relations**, native
integer/program witnesses and minimizing allocations; full semantic comparison;
and independent evidence review. Existing focused suites include malformed
scope/family coverage, seed identity/refusal, cancellation, resource rollback,
scratch caps and zero fallback. No broad suite or Simulator was run; strategies,
mechanics, ABI and vocabulary are unchanged.

The next-exact-closure analysis still locates the obstruction in setup. Strict
lift never runs, strict selected/alternative rows remain zero, and discovery and
action envelopes remain open. Matching the returned executable fallback does
not repair the earlier coarse-estimate cost mismatch or close those obligations.
The unchanged auxiliary ceiling also remains; faster construction does not
strengthen it. A future selected experiment should preserve this request/target,
first establish usable discovery time, then identify the actual surviving native
proof obligation. Increasing the finish request or declaring the current result
exact would not answer that question.

The retained profile localizes the remaining relation cost: support generation
6.648 s, quotient-row construction 5.183 s and event allocation 2.988 s. One
unmeasured source candidate is the additive-action loop in
`solver_phase_probability.cpp`: many modifier IDs map to the same `(side, goal
mask)` successor geometry before the existing sort/deduplication. A future
experiment could measure collapsing those identical support signatures while
retaining first-occurrence ordering, every native probability witness and final
minimization. This is an analysis lead, not a retained change or measured gain.

Canonical knowledge is integrated in [lower authority](../../solver/lower-pruning.md)
and [RQ-002](../../solver/research.md#rq-002). Existing mathematical claims and
preconditions remain unchanged; no blanket native-proof or general convergence
claim is added. Helper command-recovery notes were consolidated here.

The release WASM rebuild passed using `scripts/build-wasm.ps1`, retaining the
existing facade activation and unchanged JS wrapper. The focused retention test
passed both fractured and unfractured sources: checked lower
352.31017033879505 for the anchored source, no issued retention certificate for
the unsupported unfractured request, and zero live handles after cleanup.
The build retains warnings in unchanged source files; its full log is preserved.
No rendered UI review, broad web suite or TypeScript rerun was required for this
native-only implementation; no TypeScript contract changed.

Claim traceability passes with zero errors and the same 25 open-premise warnings.
The evidence comparison reproduces byte for byte from the archive, and all 57
selected local link destinations resolve. Source/doc whitespace checks pass.
The full staged whitespace check flags preserved Windows CRLF endings in raw
process/check records and generated JSON. Those evidence bytes and hashes were
retained; that broader check is not reported as passing.

## Evidence and reproduction

[Comparison](comparison.json), [exact audit](coverage-exact.json),
[provenance](provenance.json), [baseline development](baseline-development.json),
[candidate development](coverage-development.json), [compact raw inputs/outputs](compact-raw.zip)
and [process/check records](checks/) retain the measured evidence. Extract
`compact-raw.zip` into a temporary evidence directory and copy the JSON files,
four compact/development process records and `compare.py` there, then run:

```text
py -3 engine/benchmarks/verify_quotient_lower_probe.py EVIDENCE/coverage-compact.stdout.txt EVIDENCE/new-exact-audit.json
py -3 EVIDENCE/compare.py EVIDENCE
```

The exact verifier's second positional is an **output**, never an archived
reference. `compare.py` writes its own `comparison.json` in the temporary evidence
directory. Native commands, complete inputs and executable hashes are recorded
in process records/provenance; the one-run `run.py` is preserved for audit and
intentionally refuses launches after this deadline. It reuses the existing
`solver_worker.run_isolated_process` watchdog and process-tree termination.

Clock/deadline: initial check 14:19 Vancouver; no new scope after 17:15, helper
handoffs 17:30, helpers closed 17:45, processes ended 17:50, final receipt 17:58,
absolute stop 18:00 (September 8, 01:00 UTC). Every long operation checks the
actual clock and has a real process timeout and 17:50 cancellation cutoff.
Process watchdogs are not model-request cancellation. No automation/restart,
permanent configuration change, push, unrelated work or protected `0` access.
All timed native/build/evaluation runs completed by 14:36:29 Vancouver, without a timeout,
cancellation or survivor. This checkpoint stops well before the reset window;
no remaining process, helper turn or scheduled restart is being left running.
