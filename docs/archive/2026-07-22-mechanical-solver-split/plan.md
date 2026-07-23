# Mechanical Solver Split

**Status: selected on 2026-07-22. Baseline capture is the exact next step;
no source motion has started.**

Parent: [Active work](README.md)

## Objective

Restructure `engine/src/solver_solve.cpp` into phase-scoped translation units
without changing solver behavior, public or private function signatures,
method/function body tokens, output bits, test coverage, or accepted
performance. The split makes later profiling and maintenance readable; it does
not perform the deferred oracle profiling itself.

This is a new post-B6 selection. It does not reopen or amend the completed
bounded-policy plan. The code boundary is clean commit
`edb7d8fc10eee2a8c34d86b08c5a471243bc6c8a`.

## Scope

In scope:

- split `engine/src/solver_solve.cpp`;
- add one private solver header for shared declarations and types;
- update all native, fallback, and WASM source enumerations;
- rebuild native and WASM;
- run existing tests and existing benchmark cases only;
- prove moved function bodies token-equivalent;
- prove transition/policy hashes unchanged; and
- update live documentation to point at the new responsibility owners.

Explicitly out of scope:

- `apps/web/src/app/components/pc-calculator.ts`;
- `engine/src/solver_internal.hpp`;
- `apps/web/test/engine-smoke.test.ts`;
- new tests or new product/solver behavior;
- dead-code deletion, renaming, cleanup, formatting, or comment rewriting;
- LTO, `inline` tuning, or any other response to a performance miss;
- the deferred oracle performance-profiling chunk;
- exact two-T1 oracle, exact-strategy evaluation, or simulator verification;
- mechanics, action admission, Bellman rules, ties, pruning, epsilon, bounds,
  gap stopping, compilation, or result wording; and
- edits to any existing archived document.

The Calculator split is deferred because its private lifecycle wiring lacks
direct characterization coverage and this chunk adds no tests. Header and test
splits require separate future owner selections.

## Structural Invariant

Every moved method and function body remains token-equivalent to the
`edb7d8f` boundary. The following boundary plumbing is permitted:

- declarations separated from definitions;
- required out-of-class qualification;
- linkage or namespace adjustments required for cross-translation-unit
  visibility;
- includes;
- build-system source entries; and
- private `using` declarations or equivalent internal name-visibility
  plumbing.

Forbidden changes include:

- function or method renames;
- parameter, return-type, qualifier, default, or overload changes;
- statement, expression, literal, control-flow, or ordering changes inside a
  body;
- comment edits;
- formatting churn within moved bodies;
- behavior or telemetry changes;
- deletion of any code; and
- adding performance annotations, LTO, or tuning.

If a diff hunk is not boundary plumbing or a verbatim moved body, it does not
belong in this chunk.

## Target Layout

The final file count and invariant are fixed. Exact function grouping may
adjust when dependencies make a listed home unsafe, but each responsibility
must have one clear owner:

- `solver_solve_types.hpp` — shared private free types, arenas, carrier
  facts/effects, wide arithmetic, transition cache, and the
  `SolveWork::Impl` declaration;
- `solver_solve_expand.cpp` — expansion preparation/unit work, priced rows,
  kernel signatures, action reasons/skips, preservation, and cap checks;
- `solver_solve_bellman.cpp` — iteration preparation, operator Q, kernel-value
  cache, policy selection/evaluation/repair, iteration units, and `step()`;
- `solver_solve_focused.cpp` — focused lower/upper lifecycle, fringe
  collection, scheduling, and round transitions;
- `solver_solve_constructive.cpp` — renewal/fracture witness synthesis,
  terminal fallback upper, state certificates, incumbent installation, and
  gap-target stop;
- `solver_solve_heuristics.cpp` — goal-cover and strict clean-goal-cover work;
- `solver_solve_quotient.cpp` — observation signatures, projections, quotient
  graph construction, and outer quotient preparation;
- `solver_solve_finish.cpp` — `finish()` and policy-action counting;
- `solver_solve_telemetry.cpp` — solve-log and telemetry serialization,
  `BoundedTelemetryJson`, byte accounting, and telemetry-name helpers; and
- `solver_solve.cpp` — solve entry points, `Impl` construction, owned-byte
  ledger initialization, and tolerances.

Anonymous-namespace types that must cross translation units move into one
named private solver namespace in `solver_solve_types.hpp`. Their type names
do not change. Internal visibility plumbing keeps moved body tokens unchanged.

## Commit And Gate Sequence

The required logical checkpoints are:

1. selection commit — this plan, active indexes, and `HANDOFF.md`;
2. motion commit — source/header/CMake changes and rebuilt WASM, with message
   `Split solver solve phases (token-equivalent, no behavior change)`; and
3. final docs commit — live references, final evidence, plan lifecycle, and
   `HANDOFF.md`.

Baseline and acceptance reports live under `build/mechanical-split/` and are
not committed as product fixtures. Their commands, inputs, hashes, medians,
and report SHA-256 values are recorded in `HANDOFF.md`, so a fresh session can
understand and reproduce the gate without relying on ignored files.

If a gate fails, discard only the uncommitted failing motion, preserve the last
green commit, update `HANDOFF.md` with the blocker, and stop. Do not tune,
weaken the invariant, substitute cases, or proceed.

## Gate 0 — Clean Boundary And Build Inventory

Before baseline capture:

1. require branch `codex/bounded-policy-contract`;
2. require a clean tree at the selection commit;
3. confirm its source parent is `edb7d8f`;
4. record compiler version, artifact-manifest SHA-256, native test binary
   SHA-256, and benchmark binary SHA-256 after the baseline build;
5. record whether CMake is available; and
6. enforce a detached 900-second watchdog with process-tree cleanup on every
   build or benchmark process. Fast test executables may run directly.

At selection time CMake was not available, while GCC was available at
`C:\msys64\ucrt64\bin\g++.exe`. Do not install or configure another toolchain
for this chunk. Re-check at execution time and record the result.

## Gate 1 — Baseline Build And Existing Test Counts

Run `powershell -File scripts/build.ps1` before source motion. Resolve the test
and benchmark executables produced by that build and record their exact paths
and SHA-256 values.

Run these existing fast gates and record exact check/failure counts:

```powershell
<engine-tests> --solver-solve-only
<engine-tests> --solver-api-only data/compiled/current
<engine-tests> --solver-eval-only
```

No historical count is accepted as the baseline. The same binary modes and
artifact path are rerun after the split and must produce identical counts with
zero failures.

## Gate 2 — Reproducible Benchmark Inputs

Use the native benchmark executable with:

```powershell
<solver-benchmark> `
  --artifact data/compiled/current `
  --corpus <manifest> `
  --case <case-id> `
  --output <report> `
  --skip-verification
```

Every report must contain no errors, pass its cap checks, contain both bit
hashes, and leave no watchdog survivor. A native exit code 2 is accepted only
for the derived bounded 2k case when it wrote a complete report with the
expected cap termination; the expectation miss remains recorded.

### Derived real-product 2k input

Create a run-local case and one-case manifest below
`build/mechanical-split/input/`, deterministically derived from:

- case:
  `fixtures/solver-scaling/v1/cases/dire-pelt-two-t1-product.json`;
- manifest: `fixtures/solver-scaling/v1/manifest.json`.

Copy all case content, then make exactly these changes:

- `id` = `mechanical-split-real-product-2k`;
- `caps.max_expanded_states` = `2000`;
- `caps.max_absolute_optimality_gap` = `1e-30`;
- `expected.solve_status` = `bounded_diagnostic`;
- `expected.verification_status` = `not_requested`; and
- point the run-local manifest's sole `cases` entry at that derived case.

Do not change its goal, action envelope, economy, other caps, artifact pins,
or comparison profile. Record the derived case and manifest SHA-256 values.

### Fixed case set

| Case | Manifest | Role | Primary performance field |
| --- | --- | --- | --- |
| `mechanical-split-real-product-2k` | run-local derived manifest | hash + focused/constructive performance | `phase_wall_ms.solve` |
| `solver-scaling-dire-pelt-two-t1-chaos-strict` | `fixtures/solver-scaling/v1/manifest.json` | hash + finish/extraction performance | `solver_telemetry.timings_ns.extraction` |
| `solver-scaling-dire-pelt-two-t1-chaos-quotient` | `fixtures/solver-scaling/v1/manifest.json` | hash + expansion/quotient performance | `phase_wall_ms.solve` |
| `oracle-real-two-mod` | `fixtures/solver-benchmarks/v1/manifest.json` | hash only | none |
| `refusal-state-cap` | `fixtures/solver-benchmarks/v1/manifest.json` | hash only | none |

The two millisecond-scale cases are deliberately excluded from the performance
gate because five Windows samples cannot support a meaningful 10% threshold.

## Gate 3 — Baseline Hashes And Performance

For each performance case, run one discarded warm-up followed by five measured
runs. Run each hash-only case five times. Use a distinct report path for every
run under `build/mechanical-split/before/`.

Baseline requirements:

- every repetition of a case has identical `transition_bits_hash`;
- every repetition has identical `policy_bits_hash`;
- each primary performance field exists, is positive, and has five values;
- record the median of the five primary values;
- record `phase_wall_ms.solve`, `phase_wall_ms.total`, and all non-null
  `solver_telemetry.timings_ns` fields for diagnostic comparison; and
- record each report SHA-256.

If either hash varies within the baseline, stop before source motion and record
the nondeterminism as a blocker.

The primary in-report field named in the case table is authoritative. External
watchdog wall time is recorded only as process evidence and never overrides
the in-report field.

## Gate 4 — Body-Token Baseline

Before source motion, create a run-local verifier and evidence under
`build/mechanical-split/body-audit/`. It is verification tooling, not a
committed test or product source.

The verifier must:

- tokenize the baseline `engine/src/solver_solve.cpp` as C++ while ignoring
  whitespace;
- inventory every function and method body that will move, including nested
  lambda tokens as part of the containing body;
- exclude declarations, definition qualification, constructor initializer
  lists, and outer body braces from the digest;
- retain comment text separately so comment motion can be checked;
- key overloads by unqualified name plus parameter-token signature and
  occurrence;
- write the per-body SHA-256 inventory and an aggregate multiset SHA-256; and
- fail if it cannot uniquely pair every moved definition.

After the split it compares the same inventory across all target `.cpp` files.
Every body digest, multiplicity, and retained comment payload must match.
`git diff --color-moved=blocks --ignore-space-change` is reviewed as a second
structural check, not as the sole proof.

## Motion Rules

Perform the split with no opportunistic edits. Update all three source
discovery paths:

1. `scripts/build.ps1` — inspect its `engine/src/*.cpp` enumeration and record
   that all nine target `.cpp` files resolve exactly once;
2. `scripts/build-wasm.ps1` — inspect its enumeration and confirm the same
   source set before rebuilding; and
3. `engine/CMakeLists.txt` — replace the single `solver_solve.cpp` entry with
   the complete target `.cpp` list.

If CMake is available, configure/build that path. If it is unavailable, record
`CMake source list updated but not build-tested locally` in `HANDOFF.md`.
Conversely, when CMake is available and `scripts/build.ps1` therefore skips its
compiler fallback, record that the fallback glob was inspected but not
executed.

## Gate 5 — Native And Structural Acceptance

After source motion:

1. the body-token audit passes exactly;
2. `git diff --check` passes;
3. native build passes under the watchdog;
4. the three fast native gates pass with identical pre/post check counts;
5. the same five benchmark cases run with the same inputs and repetition
   counts under `build/mechanical-split/after/`;
6. both bit hashes match the baseline for every case and repetition; and
7. no report has errors, failed caps, timeout, or surviving process.

Any body-token or bit-hash difference is an immediate failed gate. Revert the
uncommitted motion, record the mismatch, and stop.

## Gate 6 — Performance Acceptance

For each performance case calculate from unrounded report values:

```text
ratio = after_median / before_median
pass  = ratio <= 1.10
```

This is one-sided: improvements of any size pass. Every case must pass
independently. Record before median, after median, absolute delta, ratio, and
the secondary phase/timer comparison in `HANDOFF.md`.

A ratio above 1.10 is a failed gate even if hashes match. Revert the
uncommitted motion, retain the reports, record the result, and stop. Do not add
LTO, `inline`, attributes, regrouping for speed, or any other tuning inside
this chunk.

## Gate 7 — WASM And Existing Web Acceptance

After native/hash/performance gates pass:

```powershell
powershell -File scripts/build-wasm.ps1
Push-Location apps/web
npm test
npx tsc --noEmit
Pop-Location
```

Record the rebuilt `.mjs` and `.wasm` SHA-256 values. No C ABI or strategy
vocabulary change is permitted. Do not add or edit tests. No rendered,
screenshot, or browser visual review is required.

No oracle, exact evaluator, or simulator verification run belongs to this
chunk. Every individual process retains the 15-minute ceiling.

## Gate 8 — Live Documentation And Handoff

Only after source acceptance:

1. search every live document for `solver_solve.cpp`;
2. repoint each responsibility to its specific new owner;
3. inspect at least `docs/solver/README.md`, `docs/solver/flow.md`,
   `docs/engine/README.md`, `docs/foundation/change-impact.md`,
   `docs/product/*.md`, and the live mechanic pages;
4. leave all existing archived documents unchanged;
5. run the scoped read-only `doc-drift` agent over touched live docs and record
   its clean report or fix the drift;
6. run the Markdown relative-link/reachability audit; and
7. update `HANDOFF.md` with commands, counts, hashes, per-run/median timing
   tables, build-path coverage, body-audit result, exact stop, and blockers.

On successful completion, move this newly completed active plan into a new
dated archive folder and update lifecycle indexes. That move may add the new
archive and its index entry; it must not alter any pre-existing archive
document.

Commit source motion before documentation. The final docs commit closes the
active boundary and leaves a clean tree from which Oliver can select later
profiling or product work.

## Completion Criteria

Complete only when:

- the target layout exists with exactly nine `solver_solve*.cpp` files plus
  the shared private header;
- every moved body and comment payload passes the source audit;
- native fast-gate counts are identical;
- every transition and policy hash is identical;
- every meaningful performance case is at or below the one-sided 1.10 ratio;
- native, rebuilt WASM, existing web tests, and TypeScript checks pass;
- CMake and both glob source paths are updated and their local coverage is
  reported honestly;
- live docs and HANDOFF identify the new owners and evidence;
- no existing archive was edited; and
- no behavior, test, profiling, tuning, or out-of-scope split entered the
  chunk.
