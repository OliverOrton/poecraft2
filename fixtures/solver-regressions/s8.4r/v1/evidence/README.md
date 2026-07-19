# S8.4R bounded evidence

## R3A carrier-relative automatic-kernel scaling

The compact implementation and measured-boundary record is
[`r3a-carrier-scaling-summary.json`](r3a-carrier-scaling-summary.json).
R3A normalizes automatic self/retry exits to the carrier entry, shares exact
transition templates and planner/resource routes across carriers, records
automatic-kind and primitive-family time/byte telemetry, tightens Essence
relevance, and proves equivalent physical affix/junk ordering collapses.

The temporary-bench follow-up precompiles 399 blocker conflict classes and
intersects them with each carrier's exact add-mod pool. At 1,024 expanded
states this collapses 276,054 applicable blocker/resource variants to 5,654
effect classes before exact evaluation, reduces temporary raw outcomes from
882,313 to 38,634, and reduces expansion from 8.82 to 4.44 seconds without
changing discovered states or retained rows/transitions. Separately priced
blockers remain exact resource variants.

The selected-byte follow-up replaces repeated full allocation walks with an
incremental exact ledger and periodic full reconciliation. Expansion falls
from 3.85 to 0.97 seconds at 1,024 states and from 13.72 to 2.25 seconds at
2,048, with identical state/row/transition counts and hashes and zero ledger
overestimate in every reconciliation. A cooperative 10-second larger sample
then stops at 2,170 expanded states: protected-side admission consumes 7.865
seconds for 532 candidates over 133 carriers while retaining only 24 rows and
five templates. That transient protected-side evaluation is the next measured
R3A boundary. Bellman entry is not claimed, no cap was raised, and R4 remains
blocked.

## R3F primitive Fracture product planning

R3F makes goal-relevant `fracture` an ordinary priced primitive and removes
automatic per-carrier `fracture_prepare` closures from the product path.
Explicit authored `fracture_prepare` envelopes remain available. A priced
Fracture product solve also requires `base`, because misses recover through
Restart; the dedicated gate case uses a disclosed 1-chaos structural override,
not a Mirage market quote.

The reproduced pre-fix slope is pinned in
[`r3f-linear-retention-before.json`](r3f-linear-retention-before.json): under
normal product caps it retained 243 Fracture preparation operators for 243
expanded states, reached 9,989,904 transitions and 452,827,288 selected bytes,
performed zero Bellman sweeps, then refused `max_transitions`. The focused
implementation, analytic-boundary, release-WASM, and stopped-gate record is
[`r3f-implementation-summary.json`](r3f-implementation-summary.json).

Focused checks and the normal-cap gate command are:

```powershell
powershell -File scripts/build.ps1
build\engine\poecraft_engine_tests.exe --solver-s8-3-only data\compiled\current
powershell -File scripts/build-wasm.ps1
Push-Location apps\web
npx tsx test/solve-workspace.test.ts
npx tsc --noEmit
Pop-Location
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case conquest-lamellar-mirage-r3f-product --output build\s8.4r3f-after.json --skip-verification
```

Oliver directed the final long-running benchmark attempt to stop before it
emitted a report. Consequently the normal-cap Bellman-entry acceptance gate was
not claimed as passed and no cap was raised. On 2026-07-19 he closed R3F on its
structural evidence and transferred this gate to R3A.

## R3 automatic Imprint-stage discovery

The focused fixture contract is
[`automatic-imprint-to-rare-focused.json`](../cases/automatic-imprint-to-rare-focused.json),
and its compact result is [`r3-imprint-summary.json`](r3-imprint-summary.json).
The final goal is rare, while native automatic discovery creates a checkpoint
only at the reachable legal magic carrier, discovers an exact Augment attempt
and useful intermediate exit, then continues from that actual successor with
ordinary Regal policy value. Non-exits restore and retry.

The compiled strategy uses the existing create/attempt/route/restore/retry
primitives. The intentionally small deterministic simulation completed all 64
runs: 2,230 checkpoints, 2,166 restores, and 64 successful exits followed by
Regal. The required 10,000-run verification remains deferred to S8.4R.6.

```powershell
build\engine\poecraft_engine_tests.exe --solver-imprint-only data\compiled\current
```

## R2 state-local automatic admission

S8.4R.2 adds bounded ordinary and advanced product representatives alongside
the pinned Conquest/Mirage case. The compact before/after record is
[`r2-before-after-summary.json`](r2-before-after-summary.json). Every
measurement performs construction plus at most the capped first expansion;
none compiles, verifies, or continues into an unbounded solve.

The before measurement used revision `66dfb65` with the two derived bounded
case documents added but before the R2 engine change. The after measurement
uses the completed R2 implementation. Reproduce the after side with:

```powershell
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case conquest-lamellar-mirage-product --output build\s8.4r2-after-conquest.json --skip-verification
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case ordinary-es-bench-product-bounded --output build\s8.4r2-after-ordinary.json --skip-verification
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case advanced-es-resist-bench-product-bounded --output build\s8.4r2-after-advanced.json --skip-verification
```

## R1 diagnostic ownership and cap correctness

The pre-fix measurement is intentionally an honest skipped attempt: the
pre-fix native harness did not finish building inside its safe timeout, and no
solve was launched. This follows Oliver's direction to skip baseline or
benchmark trouble and fix the issue rather than extending the run.

The post-fix report is a strictly bounded diagnostic. It pins the Conquest
Lamellar base, four T1 family goals, current compiled artifact, and Mirage
snapshot. It expands at most the start state, performs no compilation or
verification, and never continues into an unbounded solve.

Commands:

```powershell
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case conquest-lamellar-mirage-product --validate-only
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case conquest-lamellar-mirage-product --output fixtures\solver-regressions\s8.4r\v1\evidence\r1-after-report.json --skip-verification
```

`r1-after-summary.json` is the compact comparison record;
`r1-after-report.json` is the complete runner output, including telemetry and
all retained bounded samples.
