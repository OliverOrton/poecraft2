# Session Handoff - S8.4R.3A Protected-Side Admission Repair Is Next

Updated 2026-07-19. Read [AGENTS.md](AGENTS.md), this file,
[docs/direction.md](docs/direction.md), then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4, S8.0-S8.4, S8.4R.1-R3, and R3F are complete. R3A's
carrier-relative representation, telemetry, relevance, and state-order audit
are implemented and focused validation passes. The selected-byte accounting
repair is also implemented and removes its measured quadratic expansion cost,
but **R3A is not closed**: a time-boxed larger sample exposed a new
protected-side admission cliff before the normal-cap
`conquest-lamellar-mirage-r3f-product` request could be attempted. Bellman
entry is not established and no cap was raised.

The sole next boundary is the measured R3A protected-side transient-evaluation
repair below. Do not begin R4 browser transfer/lifetime work, R5 verification
truth, R6 integrated acceptance, S8.5, or later work. Mechanic questions
remain Oliver's authority; never research or infer them.

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
phase.

The reconciled outer timers now identify that phase. They are additive to
within 0.8% of the outer expansion timer:

- At 1,024 states, expansion is 3.854 seconds. Carrier preparation is 3.569
  seconds: 1.994 seconds of direct byte audits plus 1.536 seconds of automatic
  admission. Calculation-context owned-byte audits total 2.945 seconds across
  3,277 calls, or 76.4% of expansion.
- At 2,048 states, expansion is 13.725 seconds. Carrier preparation is 13.225
  seconds: 7.896 seconds of direct byte audits plus 5.254 seconds of automatic
  admission. Calculation-context owned-byte audits total 11.592 seconds across
  6,385 calls, or 84.5% of expansion.
- Ordinary diagnostic retention is only 41 ms / 79 ms at those bounds. Kernel
  work is 96 ms / 142 ms and sparse-row work 94 ms / 144 ms. Textual logging
  and row construction are not the progressive slowdown.

The selected-byte repair now maintains exact selected-allocation deltas as
states, operators, caches, templates, and nested payloads are inserted or
released. Hot-path cap checks read that ledger in constant time. Full walks
remain at phase boundaries and every 1,024 carriers as a reconciliation oracle;
an undercount is a hard invariant failure. The bounded comparison is:

- 1,024 states: expansion 3.854 -> 0.974 seconds (74.74% lower); full audits
  3,277 / 2.945 seconds -> 4 / 0.021 seconds; ledger 3,270 calls / 0.147 ms.
- 2,048 states: expansion 13.725 -> 2.253 seconds (83.58% lower); full audits
  6,385 / 11.592 seconds -> 5 / 0.052 seconds; ledger 6,378 calls / 0.270 ms.
- Both bounds retain identical discovered-state, row, transition, registry,
  transition-hash, and policy-hash results. All nine full reconciliations have
  zero undercount and zero overestimate.

A cooperative 10-second larger diagnostic then stopped at 2,170 expanded /
30,462 discovered states. Its byte ledger cost only 0.291 ms, but
protected-side automatic admission consumed 7.865 seconds for 532 candidates
over 133 carriers, producing 25,272 transient outcomes but retaining only 24
rows, five templates, 7,769 transitions, and 128,017 selected bytes. At 2,048,
protected-side had only 44 candidates over 11 carriers, no rows, and cost
0.294 seconds. The cliff begins just beyond that boundary.

Temporary bench is no longer the dominant work there: 252,129 candidates over
2,170 carriers cost 0.626 seconds and retained at most 12 rows per carrier.
Textual logging, selected-byte accounting, and retained automatic storage are
not the new progressive slowdown.

The compact checked-in record is
[r3a-carrier-scaling-summary.json](fixtures/solver-regressions/s8.4r/v1/evidence/r3a-carrier-scaling-summary.json).
Full diagnostic reports remain ignored under `build/`, including
`build/s8.4r3a-boundary-1024-final.json`,
`build/s8.4r3a-temp-precompile-1024-final.json`, and
`build/s8.4r3a-temp-precompile-2048-final.json`,
`build/s8.4r3a-expansion-phases-1024.json`, and
`build/s8.4r3a-expansion-phases-2048.json`,
`build/s8.4r3a-byte-ledger-1024-final.json`,
`build/s8.4r3a-byte-ledger-2048-final.json`, and
`build/s8.4r3a-byte-ledger-4096-timecap.json`.

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

After the byte-ledger repair, a direct 4,096 wrapper attempt exceeded its short
foreground limit and its child was explicitly stopped. The deliberate
10-second cooperative diagnostic described above then localized the next
boundary. The normal-cap command was not rerun because that bounded curve does
not predict usable completion.

## Exact Next Boundary: R3A Protected-Side Admission Only

1. Preserve the current exact envelope, mechanics, representation, and checked-
   in caps. Do not begin with another normal-cap run.
2. Add nested admission timing/rejection telemetry before optimizing. Split
   automatic synthesis, strict local-context registry/operator/layout/ledger
   construction, protected-side kernel evaluation, local-to-parent outcome
   mapping, normalization, and exact template/route matching. The current
   per-kind total says where the time is, not which subphase causes it.
3. If immutable local-context construction is material, apply the compatible
   immutable-context proposal from the reviewed audit: share or prebuild the
   registry, strict layout, and planner basis, then reset only carrier-local
   mutable state. Never assume layouts are interchangeable merely because the
   carrier changed; reuse requires the same goal, strictness, candidate set,
   and exact fixed-option dependency-action set. Keep local state/evaluator
   caches isolated.
4. If protected-side exact evaluation is material, audit the four candidates
   per carrier for carrier-invariant program/target facts that can be
   preclassified once. Memoize across carriers only on a complete normalized
   mechanic-relevant observable signature and prove equality with the existing
   complete-kernel oracle. Keep fractured, locked, crafted, influence,
   capacity, goal-slot, and resource differences exact.
5. Preserve the observed retention bound (at most two protected-side templates
   and rows per carrier). This is a transient evaluation repair, not a reason
   to raise caps, weaken the byte estimate, or revisit temporary-bench storage.
6. Partition discovered states by normalized observable facts. Require a
   concrete admitted-action legality/transition witness for any remaining
   goal-slot distinction, while preserving the passing physical-affix/junk-
   ordering equivalence fixture.
7. Keep validation narrow: use the existing `--solver-s8-3-only` checks, the
   existing byte-cap oracle, and the pinned 2,048 plus cooperative time-boxed
   larger diagnostics. Require identical admission decisions, state/row/
   transition counts, and transition/policy hashes where the bounded run
   completes. Do not add browser work, a new 10,000-run simulation, or the full
   repository pipeline for this boundary. Attempt the unchanged normal-cap
   Bellman-entry gate only when the bounded curve predicts usable completion.
8. If the exact full goal-relevant envelope remains unusable after that repair,
   implement the already-planned mechanic-neutral focused/custom action scope.
   Label it "optimal within the selected action scope" and never claim global
   optimality. Any narrower mechanic-specific retry decision requires Oliver.

Reviewed-audit disposition for this boundary: its A2 is accepted conditionally
as step 3 above. A1 is mostly superseded by commit `d1e928e`; retain periodic
full ledger reconciliation, and treat the fresh local-context base walk as part
of the A2 measurement rather than disabling the invariant oracle. A4/A6/A7 are
plausible later measured optimizations but are not the current hot phase. A3 is
R4/browser work and remains blocked. B1-B7 are broader or later action-space,
diagnostic, and usability work and are not part of this R3A repair.

R4 remains blocked until the normal-cap request completes expansion and enters
outer Bellman optimization and retained automatic kinds remain bounded.

## Validation Completed

- The current native test executable and release benchmark built successfully
  with the fallback compiler.
- Full native engine suite: 164,843 checks, 0 failures.
- Focused `--solver-s8-3-only`: 206 checks, 0 failures. The added byte-ledger
  oracle reconciles incremental and full estimates before/after automatic
  admission and kernel retention, and proves a tiny byte cap still defers the
  candidate with an explicit `max_solver_owned_bytes` reason. The existing
  temporary-bench oracle
  covers differently priced blockers with distinct global conflict masks but
  one carrier-effective add-mod pool.
- `powershell -File scripts/build-wasm.ps1`: release WASM rebuilt from the
  final native source. The foreground wrapper timed out while `emcc` continued;
  the compiler finished, emitted both final artifacts, and no child remained.
  No C ABI or strategy vocabulary changed.
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
