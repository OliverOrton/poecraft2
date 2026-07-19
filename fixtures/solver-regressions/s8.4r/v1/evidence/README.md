# S8.4R bounded evidence

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
