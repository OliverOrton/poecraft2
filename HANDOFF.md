# Session Handoff - S8.4R.3A Measured Expansion Boundary Is Next

Updated 2026-07-19. Read [AGENTS.md](AGENTS.md), this file,
[docs/direction.md](docs/direction.md), then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4, S8.0-S8.4, S8.4R.1-R3, and R3F are complete. R3A's
carrier-relative representation, telemetry, relevance, and state-order audit
are implemented and focused validation passes, but **R3A is not closed**: the
normal-cap `conquest-lamellar-mirage-r3f-product` attempt was stopped before it
emitted a report or entered Bellman optimization. No cap was raised.

The sole next boundary is the measured R3A expansion-time follow-up below. Do
not begin R4 browser transfer/lifetime work, R5 verification truth, R6
integrated acceptance, S8.5, or later work. Mechanic questions remain Oliver's
authority; never research or infer them.

The owner-decision docs that transferred the R3F gate to R3A were committed
first and remain intact in commit `50f7825`.

## What R3A Implemented

### Carrier-relative exact kernels

- Imprint, renewal, protected-repeat, temporary-bench-repeat, and explicit
  authored Fracture preparation encode entry retry/self as `kNoId` rather than
  retaining the absolute entry state in the template.
- Sparse rows resolve that sentinel to their owner carrier for self-loop
  algebra, choices, compilation, and direct option evaluation.
- Complete transition templates share across carriers independently from exact
  planner routes and resource quantities. Carrier-local state/operator caches
  remain available for solve and primitive compilation.
- Planner operators are reused by exact program/target/resource identity, so
  the pinned 1,024-state sample uses 669 registry operators instead of 66,888.
  Dependencies are admitted only for a newly retained planner route.
- Fractured, locked, crafted, influenced, capacity-distinct, or otherwise
  mechanically distinct substrates remain separate through complete-kernel and
  planner equality. The focused renewal test proves two carriers differing only
  in wiped modifiers share one kernel pointer/template, while a fractured
  carrier is illegal and distinct.

### Telemetry and measured costs

Automatic telemetry is split into Imprint, renewal, protected-side,
temporary-bench, authored Fracture preparation, permanent bench, Multimod
finish, and primitive Fracture. It reports candidates, carriers,
per-carrier maxima, unique templates/hits, rows, raw outcomes, retained
transitions, admission/row time, and selected bytes.

Primitive telemetry is split into currency, Essence, Fossil, Harvest, bench,
Bestiary, Fracture, and other. It reports requests/cache hits, rows, outcomes,
transitions, build/row time, and cumulative attributed construction bytes.
Those primitive bytes are not retained live bytes; evaluator caches are
released after sparse copy.

R3A added this timing before optimization. It showed that repeated full
owned-byte estimates and extraction scratch audits were initially dominant:

- 64 carriers: 12.27 seconds before batched per-carrier audits, 1.14 seconds
  after them, with the same transition/policy hashes at that checkpoint.
- 1,024 carriers before extraction/share repair: 362.50 seconds total, of
  which 285.78 seconds was extraction; 66,888 registry operators; 154,577,630
  peak / 109,475,643 live selected bytes.
- Final corrected 1,024-state boundary: 8.91 seconds total, 8.82 seconds
  expansion, 15.56 ms extraction; 669 registry operators; 46,165,870 peak /
  22,737,809 live selected bytes.

The final sample's temporary-bench work was 515,857 candidates over 1,023
eligible carriers, 2,944 unique transition templates, 63,775 template hits,
2,944 retained rows, 20,691 transitions, and at most 12 templates/rows per
carrier. Automatic admission accounted for 1.626 seconds and row work 0.041
seconds. Roughly 7 seconds of outer expansion remains unattributed.

The next R3A follow-up precompiled the temporary-bench blocker vocabulary once
per solver context and intersects it with each carrier's exact eligible add-mod
pool before transient option construction. The final 1,024-state comparison is:

- 399 precompiled classes, built in 5.29 ms and 194,072 selected bytes;
- 276,054 applicable blocker/resource variants collapsed to 5,654
  carrier-effective classes before exact evaluation;
- temporary raw outcomes 882,313 -> 38,634 and automatic admission 1.626 s ->
  0.314 s;
- expansion 8.819 s -> 4.443 s and total 8.905 s -> 4.559 s;
- identical 29,637 discovered states, 15,230 rows, 91,959 transitions, 2,944
  retained temporary rows, 20,691 temporary transitions, and 12 maximum
  temporary rows per carrier.

The candidate flood therefore caused about four seconds of transient and
downstream expansion work that its old direct timer did not expose. Exact
separately priced blocker variants are retained rather than conflated, so the
planner registry is 1,470 and live/peak selected bytes are 29,297,385 /
62,538,285. These remain far below the checked-in caps. The 2,048-state sample
then rises to 18.252 seconds expansion with only 29,942 discovered states and
1,512 registry operators, exposing a separate superlinear outer-expansion
phase. Do not run 4,096 or the normal-cap request before that phase is timed
and repaired.

The compact checked-in record is
[r3a-carrier-scaling-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r3a-carrier-scaling-summary.json).
Full diagnostic reports remain ignored under `build/`, including
`build/s8.4r3a-boundary-1024-final.json`,
`build/s8.4r3a-temp-precompile-1024-final.json`, and
`build/s8.4r3a-temp-precompile-2048-final.json`.

### Relevance and abstract-state audit

- Essence goal relevance now requires the exact guaranteed modifier to be a
  requested tier-satisfying goal member; a merely same-family lower member is
  excluded.
- The pinned product sample admits zero Essence and zero Harvest rows and keeps
  the already bounded requested Fossil envelope.
- A focused projection fixture proves physical prefix/suffix array order and
  equivalent junk visitation order project and hash identically. No new state
  dimension was added.
- The product envelope expectation now includes the ordinary primitive
  `fracture`, matching R3F's product representation; explicit authored
  `fracture_prepare` retains its S7 option contract.

## Transferred Normal-Cap Gate: Unmet

The unchanged command remains:

```powershell
build\engine\poecraft_solver_benchmark.exe --artifact data\compiled\current --corpus fixtures\solver-regressions\s8.4r\v1\manifest.json --case conquest-lamellar-mirage-r3f-product --output build\s8.4r3a-after.json --skip-verification
```

Its checked-in caps remain 100,000 states/sweeps/discovered/expanded,
1,000,000 rows, 10,000,000 transitions/reforge work, and 1 GiB selected solver
owned bytes.

Oliver asked to abort after about nine minutes of foreground waiting. The
wrapper stopped first, but its benchmark child survived; it was found during a
rebuild and explicitly stopped before report emission at 858.14 CPU seconds
and about 153 MiB working set. There was no report, cap hit, crash, Bellman
entry, or convergence result. Do not describe this gate as passed.

## Exact Next Boundary: R3A Measured Expansion Work Only

1. Preserve the current exact envelope, mechanics, representation, and checked-
   in caps. Do not begin with another normal-cap run.
2. The temporary candidate enumeration/evaluation boundary is repaired and
   attributed. Add reconciled timing counters around the still-unattributed
   outer work: `prepare_state_expansion`, transient-context construction and
   projection outside the temporary effect evaluator, state interning,
   sparse-row/effect construction and enqueue, operator/pricing refresh, and
   exact owned-byte audits. The attributed pieces must sum closely to the
   outer expansion timer per carrier and automatic kind.
3. The bounded 1,024/2,048 curve is already recorded and is superlinear. Do
   not run 4,096 or normal caps until the hot outer phase is identified and
   the curve predicts usable completion. Do not raise caps.
4. Partition discovered states by normalized observable facts. Require a
   concrete admitted-action legality/transition witness for any remaining
   goal-slot distinction, while preserving the passing physical-affix/junk-
   ordering equivalence fixture.
5. Repair only the measured hot phase, re-run the focused/native correctness
   checks, and attempt the unchanged normal-cap Bellman-entry gate only when
   the bounded curve predicts a usable completion.
6. If the exact full goal-relevant envelope remains unusable after that repair,
   implement the already-planned mechanic-neutral focused/custom action scope.
   Label it "optimal within the selected action scope" and never claim global
   optimality. Any narrower mechanic-specific retry decision requires Oliver.

R4 remains blocked until the normal-cap request completes expansion and enters
outer Bellman optimization and retained automatic kinds remain bounded.

## Validation Completed

- `powershell -File scripts/build.ps1`: passed.
- Full native engine suite: 164,835 checks, 0 failures.
- Focused `--solver-s8-3-only`: 198 checks, 0 failures. The added oracle
  covers differently priced blockers with distinct global conflict masks but
  one carrier-effective add-mod pool.
- `powershell -File scripts/build-wasm.ps1`: release WASM rebuilt from the
  final native source. No C ABI or strategy vocabulary changed.
- Remaining non-visual web tests passed; `npx tsc --noEmit` passed.
- The broad `npm test` run stopped at the known pre-R3A automatic
  permanent-bench group-goal non-convergence in `engine-smoke.test.ts`. Do not
  attribute it to R3A.
- The corpus pin test separately exposes the already-disclosed stale historical
  S7 `9776797b...` game-data pin versus current `af41b8f4...`; the active S8.4R
  manifest correctly pins `af41b8f4...`.
- No 10,000-run verification was performed; R6 owns it. No rendered or visual
  review was performed; Oliver owns it.

## Gotchas

- `set_solve_resource_caps(..., reserve_storage=false)` is intentional only for
  transient local evaluators. Outcome interning can reallocate state storage,
  so callers that evaluate outcomes must not retain `AbstractState&` references
  across that work.
- Static authored options use kernel-derived expected resources; state-local
  automatic operators use the exact planner resource vector. In particular,
  authored conditional Fracture quantities must not be replaced with a static
  planner vector.
- Keep legacy absolute sparse-kernel equivalence unchanged. The additional
  owner-relative self-probability comparison applies only to entry-relative
  kernels; applying it globally makes existing policy iteration oscillate.
- A whole transient-context cache was measured at about 106 seconds and 81 MB
  for 64 carriers with no useful cross-carrier hits and was rejected.
- `functions.wait` or a shell-wrapper cancellation may leave a spawned native
  benchmark child alive on Windows. Verify the exact process after aborting a
  long run before rebuilding.
- SQLite is canonical and compiled data is derived. Neither was edited in R3A.
