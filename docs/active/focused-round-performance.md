# Focused-Round Performance Attribution And Scheduling

**Status: selected on 2026-07-23. Gate 0 boundary verification and fresh
baseline capture are the exact next steps; no source work has started.**

Parent: [Active work](README.md)

Input:
[Post-B6 Solver Reconnaissance](../archive/2026-07-23-post-b6-reconnaissance/README.md)

## Objective

Attribute the measured focused-round cost increase on bounded, fast cases and
accept at most one focused scheduling-default change if fresh evidence shows a
material improvement without weakening exactness, bounded certificates,
determinism, step responsiveness, memory, or control-case performance.

This is not a broad solver optimization pass. The selected source baseline is
mechanical-split commit `042a281`; the documentation boundary before this
selection is `5eb66d4`. Historical B1/oracle reports are hypotheses and context
only. Every baseline used for a gate must be captured fresh from the selected
tree.

## Fixed Starting Evidence

The archived reconnaissance observed:

- the retained exact two-natural-T1 solve rose from 22 to 326 focused rounds;
- mean expanded states per round fell from approximately 2,602 to 175;
- focused optimization grew by a measured 29.23x while the product of round
  count, policy-evaluation calls per round, and collapsed states per call
  predicted 29.28x;
- current private defaults are 64 members per fringe class, a 256-state global
  focused batch, and a 64-state lower quota;
- an executable upper pass and constructive fallback validation are charged
  per focused round; and
- recursive owned-byte ledger work is a supporting lead, not a dominant
  measured cost.

These are not accepted causal conclusions. The exact oracle must not be run to
confirm them.

## Scope

In scope:

- fresh current-tree native counts, hashes, timings, certificates, memory, and
  watchdog evidence;
- exclusive diagnostic attribution for focused scheduling, fallback
  validation, and owned-byte ledger fan-out;
- benchmark-side solve-step wall-time distribution;
- run-local source-copy variants of the three private focused scheduling
  defaults;
- the bounded 2k product case plus the existing strict, quotient, millisecond
  exact, and refusal controls;
- at most one accepted tuple of focused scheduling defaults, only after the
  decision gate; and
- final native/WASM/non-visual web acceptance, evidence, stable docs, and
  HANDOFF closure.

Out of scope:

- the exactly-once two-natural-T1 oracle;
- mechanics, goals, action scope, action prices, public caps, exact quotient
  rules, Bellman comparisons, or certificate definitions;
- removing, weakening, or rescheduling the executable upper pass;
- changing constructive fallback validity or properness rules;
- optimizing identity hashing or owned-byte accounting in this chunk;
- the 120-case corpus campaign, deep stage, general solver profiling, LTO,
  inlining, container rewrites, or unrelated cleanup;
- economy repair/publishing, natural-T1 corpus design changes, Calculator work,
  source-folder moves, `solver_internal.hpp` splitting, and web-test splitting;
- rendered, screenshot, or browser visual review.

The natural-T1 generator is intentionally T1-only by Oliver's 2026-07-23
decision. Tier-range support is not a missing feature or candidate
improvement.

If a mechanic question arises, stop for Oliver. Do not research or infer a
Path of Exile ruling.

## Process And Watchdog Contract

Every build, benchmark, exact policy evaluation, simulation, and test-pipeline
process runs under a detached 900-second watchdog with process-tree termination
and a survivor check. Record command, working directory, start/finish time,
exit code, timeout, survivor status, wall time, log path, log SHA-256, and
produced artifact/report SHA-256.

The exact oracle is prohibited even if a watchdog is available. The
millisecond case named `oracle-real-two-mod` is only a small deterministic
control and is not the prohibited natural two-T1 oracle.

Run-local tooling and raw reports belong under `build/focused-round/`.
Accepted summary evidence belongs under
`fixtures/solver-scaling/v1/evidence/`. Do not depend on surviving ignored
files from the mechanical-split run.

## Gate 0 — Boundary And Provenance

Before modifying a file:

1. verify branch `codex/bounded-policy-contract`;
2. verify a clean selection commit and no source change after `042a281`;
3. record `git status --short --branch`, `git rev-parse HEAD`,
   `git rev-parse 042a281`, and
   `git diff --name-only 042a281..HEAD -- engine bindings scripts apps/web`;
4. record compiler, CMake, PowerShell, Python, Node, npm, and Emscripten
   availability and versions;
5. hash `data/compiled/current/manifest.json`;
6. inspect `engine/src/solver_internal.hpp`,
   `solver_solve_focused.cpp`, `solver_solve_constructive.cpp`,
   `solver_solve_telemetry.cpp`, `solver_calc.cpp`, the benchmark runner, and
   the current telemetry/report tests; and
7. create the detached watchdog before any build or benchmark.

Any unexpected source difference, dirty tracked file, or missing input is a
hard stop. Do not touch the economy pipeline.

## Gate 1 — Fresh Uninstrumented Baseline

Build native with:

```text
powershell -File scripts/build.ps1
```

Record fresh executable hashes and run:

```text
build\engine\poecraft_engine_tests.exe --solver-solve-only
build\engine\poecraft_engine_tests.exe --solver-api-only data\compiled\current
build\engine\poecraft_engine_tests.exe --solver-eval-only
```

Derive a run-local case from
`fixtures/solver-scaling/v1/cases/dire-pelt-two-t1-product.json`. Change only:

- `id` to `focused-round-real-product-2k`;
- `caps.max_expanded_states` to `2000`;
- add `caps.max_absolute_optimality_gap = 1e-30`;
- `expected.solve_status` to `bounded_diagnostic`; and
- `expected.verification_status` to `not_requested`.

The run-local manifest changes only `cases`. Record source/derived case and
manifest hashes plus a machine-checked changed-path list.

The fixed baseline matrix is:

| Case | Manifest | Role | Primary field |
| --- | --- | --- | --- |
| `focused-round-real-product-2k` | run-local derived | bounded certificate, schedule, memory, performance | `phase_wall_ms.solve` |
| `solver-scaling-dire-pelt-two-t1-chaos-strict` | `fixtures/solver-scaling/v1/manifest.json` | exact strict control | `solver_telemetry.timings_ns.extraction` |
| `solver-scaling-dire-pelt-two-t1-chaos-quotient` | same | exact quotient control | `phase_wall_ms.solve` |
| `oracle-real-two-mod` | `fixtures/solver-benchmarks/v1/manifest.json` | millisecond hash control | none |
| `refusal-state-cap` | same | refusal/hash control | none |

Use the benchmark executable with exact artifact, manifest, case, distinct
output report, and `--skip-verification`. For each performance case run one
discarded warmup and five measured repetitions. For each hash-only case run
five measured repetitions. Require:

- deterministic transition and policy hashes across repetitions;
- the intended status, cap, policy-availability, and certificate fields;
- five positive primary values and their median;
- focused rounds, policy-evaluation calls, partial-policy rounds, lower/upper
  bounds, live/peak selected-owned memory, and all timer leaves; and
- zero watchdog timeouts or survivors.

Historical timings cannot substitute. Commit no source at this gate.

## Gate 2 — Attribution Instrumentation

Add diagnostic-only instrumentation with no mechanic or scheduling change:

- focused schedule candidate/admission counts, global-batch cap hits,
  per-class cap hits, and lower/upper quota admissions by round;
- exclusive fallback-validation counts/times for goal identity, economy
  identity, action-vocabulary identity, structural checks, anchor properness,
  and start properness;
- owned-byte ledger child-context visits and maximum recursion depth; and
- benchmark-side wall time for every `pc_solver_solve_step` call, reporting
  count, total, median, p95, and maximum.

Document which timers are inclusive parents and which are exclusive leaves.
Do not assert that arbitrary existing timers sum to solve wall time. Extend the
existing telemetry/report validation tests for every new field; do not create
a second reporting path.

Rebuild and repeat Gate 1. Native counts and all deterministic hashes must
match the uninstrumented baseline. Each primary median must be at or below
`1.10 * uninstrumented_median`. The bounded case must retain a finite valid
certificate containing the known exact value `230.26738656962243`, and its
lower bound may not decrease or upper bound increase beyond `1e-12`.

Failure is a hard stop: revert only the uncommitted instrumentation, retain
evidence, update HANDOFF, and stop.

## Gate 3 — Run-Local Scheduling Matrix

Never mutate tracked source for exploratory variants. Create a run-local copy
of `engine/src` for each variant, change only the three initializer values in
the copied `solver_internal.hpp`, compile it with the same compiler and flags
as the accepted native fallback, and record:

- copied-source aggregate SHA-256;
- an exact three-field changed-path/token report;
- compiler command and version;
- binary SHA-256; and
- proof that the tracked header hash stayed unchanged.

Use these tuples:

| Variant | members/class | global batch | lower quota | Purpose |
| --- | ---: | ---: | ---: | --- |
| A | 64 | 256 | 64 | current instrumented baseline |
| B | 64 | 512 | 64 | global cap |
| C | 64 | 1024 | 64 | global cap |
| D | 64 | 4096 | 64 | effectively uncapped for 2k |
| E | 4096 | 256 | 64 | per-class cap |
| F | 4096 | 4096 | 64 | combined cap removal diagnostic |
| G | 64 | 256 | 128 | stated half-batch quota intent |

Run the 2k case for one discarded warmup plus three measured repetitions per
variant. Rank by medians, not the best run. Then run the best two eligible
variants with one warmup plus five measured repetitions on all three
performance cases and five repetitions on both hash controls.

For every run record primary fields, focused rounds, upper/lower pass counts,
policy-evaluation calls, fallback validation breakdown, ledger fan-out,
step-time distribution, bounds, memory, hashes, status, cap detail, and report
hash.

## Gate 4 — Causal And Candidate Decision

Evaluate the hypotheses separately:

- global-batch causality is supported only if B/C/D show a coherent reduction
  in rounds and policy-evaluation calls as the batch grows;
- per-class causality is supported only by A versus E with the global batch
  held fixed;
- quota impact is supported only by A versus G;
- upper-pass cost is attribution only; this plan cannot change its schedule;
- fallback identity work is actionable later only if an exclusive component
  owns at least half of fallback-validation time; and
- byte-ledger caching is actionable later only if fan-out is confirmed and
  ledger time reaches at least 2% of solve wall time.

A scheduling tuple is eligible for tracked acceptance only if its five-run
comparison satisfies all of:

- 2k solve median at or below `0.85 * instrumented_baseline`;
- 2k focused-round median at or below `0.75 * instrumented_baseline`;
- 2k lower bound no lower and upper bound no higher beyond `1e-12`;
- finite executable policy and certificate containing
  `230.26738656962243`;
- solve-step p95 at or below `1.10 * baseline` and maximum below the existing
  5000 ms worker-step budget;
- live and peak selected-owned memory each at or below `1.10 * baseline`;
- strict and quotient primary medians each at or below `1.10 * baseline`;
- exact-control transition/policy hashes unchanged; and
- no new error, cap, timeout, survivor, or refusal mismatch.

If multiple tuples qualify, select the smallest change with the lowest 2k
solve median. Do not combine values outside the matrix or tune further. If no
tuple qualifies, accept no scheduling change; retain the validated
instrumentation and close the chunk with a measured no-change result.

## Gate 5 — Tracked Candidate And Final Acceptance

If Gate 4 selects a tuple, change only the three focused scheduling default
initializers needed to match it. No function body, comment, mechanic,
certificate, or upper-pass scheduling change is permitted.

Repeat the full five-run matrix against the fresh instrumented baseline. A
hash, certificate, step, memory, status, or one-sided performance failure is a
hard stop. Revert only the uncommitted candidate defaults, retain the
instrumentation/evidence, update HANDOFF, and stop without tuning.

For an accepted candidate, derive a final 2k verification case from the same
source with `expected.verification_status = run`. Run exact compiled-policy
evaluation and exactly 10,000 simulator trials using the source case's pinned
seed and tolerances. This evaluates the returned bounded policy; it does not
run the prohibited exact solver oracle.

Then, once at the end:

1. run the three native fast gates and the complete existing native suite;
2. run the appropriate telemetry/report tests;
3. rebuild with `powershell -File scripts/build-wasm.ps1`;
4. run `powershell -File scripts/test.ps1` against the rebuilt module;
5. run `npm test` and `npx tsc --noEmit` in `apps/web`; and
6. record CMake, native fallback, and WASM source-path coverage.

All processes retain the watchdog. No visual review is required.

## Gate 6 — Evidence, Documentation, And Handoff

Check in a concise JSON summary under
`fixtures/solver-scaling/v1/evidence/` containing:

- source, artifact, tool, binary, and report hashes;
- exact commands and watchdog facts;
- uninstrumented and instrumented baselines;
- the complete variant matrix;
- all five-run values, medians, ratios, bounds, step metrics, memory, and
  deterministic hashes;
- causal decisions for the global cap, per-class cap, quota, upper pass,
  fallback validation, and byte ledger;
- the selected tuple or explicit no-change result; and
- final native/WASM/web/verification evidence.

Update solver/evidence docs and HANDOFF with conclusions, not hypotheses.
Run the scoped read-only `doc-drift` agent and the Markdown
relative-link/reachability audit. Archive this plan only after all gates close.

## Commit Boundary

The selection/plan commit is documentation-only. After Gate 5 passes, commit
accepted instrumentation and any selected default tuple as one source
milestone. Then commit evidence, stable documentation, HANDOFF, and plan
archive separately. Commits remain local unless Oliver explicitly asks to
push.

## Completion Criteria

Complete when attribution is fresh and reproducible, the exact oracle was
never run, every hard gate passed, and either:

- one predeclared scheduling tuple is accepted with full certificate,
  determinism, responsiveness, memory, performance, native, WASM, web, exact
  policy-evaluation, and 10,000-run evidence; or
- no tuple qualifies and the chunk closes honestly with accepted diagnostic
  instrumentation and a measured no-change conclusion.
