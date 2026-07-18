# S8.4R.1 bounded evidence

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
