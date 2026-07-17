# Session Handoff - S7.6 Exercised, Three Product Gates Remain Open

Updated 2026-07-16 after the S7.6 acceptance and final benchmark pass. Read
[AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

S7.0-S7.5 and S7.2R remain complete. S7.3 remains the earlier out-of-sequence
fixed-option commit. S7.6 was implemented and exercised, but it must not be
called complete: four native 10,000-run samples finished, the endgame sample
was owner-authorized to be cancelled after more than 30 minutes, the advanced
sample missed its stored tolerance, and the endgame WASM result missed two
approved cross-backend/responsiveness limits.

## Exact next boundary

Continue **S7.6 only**. Do not start any parked post-S7 scope.

1. Make endgame compiled-strategy simulation practical or at least externally
   progress-reporting/cancellable, then run one fresh pinned sample of exactly
   10,000 native runs. Do not combine partial or duplicate runs.
2. Investigate the advanced sample's 8.743745% mean-cost delta against its 8%
   approved tolerance. Do not widen the fixture tolerance without Oliver.
3. Bring the endgame WASM solve within the approved 50 ms worker-step budget
   and its native/WASM start value within the approved `1e-7` absolute delta.
   The final measurements were 2,248.229 ms and `1.86154e-6` respectively.
4. Re-run only the acceptance layers invalidated by those fixes, regenerate
   the affected native/WASM reports and strategies, and present the remaining
   gates to Oliver. No rendered or browser visual checks unless requested.

The complete repository suite is currently green. The three items above are
performance/statistical product-gate failures, not vocabulary gaps or automated
test failures.

## S7.6 implementation result

- The documented endgame solve blocker is removed. Equivalent one-action
  renewal kernels now use the immutable reforge cache's pointer identity
  instead of repeatedly copying/comparing distributions. Fixed-policy SCCs
  switch from cubic dense elimination to the residual-checked sparse solver
  above 512 states, focused lower solves retain their previous admissible
  values, and improper policy components repair all certified exits together.
- Stable Howard policies use a scale-aware termination residual while the
  fallback retains the absolute epsilon. Convergence additionally requires a
  complete reachable policy. Observation-choice reachability follows only the
  Bellman-selected successor; unselected Unveil offers no longer cause a false
  `policy-reachable state has no action` compiler refusal.
- The native harness can save ordinary editable strategy JSON, print 10-second
  solve progress, enforce `run_if_compiled` verification expectations, and
  report the simulator action distribution. `scripts/benchmark-solver.ps1`
  exposes those controls.
- Acceptance found and fixed a stale economy snapshot hash check: `sources`
  is now included in the reconstructed canonical content. Stale tests were
  aligned with approved fossil abstraction, currently supported solver-eval
  vocabulary, cache reuse, compiler region deduplication, and approved corpus
  enablement. Full-pool option behavior remains owned by the permanent
  performance corpus; bounded synthetic pools retain the exact observation and
  fracture contract tests without multi-gigabyte unit-test allocations.
- The release WASM engine was rebuilt. No visual check was performed.

All five approved real crafts now solve and compile from their declared start,
with no hand-authored intermediate stage or vocabulary refusal. Delivered
ignored strategy artifacts are under
`build/performance/strategies/s7.6-final/`:

| Strategy | JSON bytes |
| --- | ---: |
| `oracle-real-one-mod.strategy.json` | 76,723 |
| `oracle-real-two-mod.strategy.json` | 187,527 |
| `ordinary-es-bench.strategy.json` | 1,650,569 |
| `advanced-es-resist-bench.strategy.json` | 4,844,538 |
| `endgame-fractured-es.strategy.json` | 6,630,717 |

## Final native and WASM solve/compile evidence

Final ignored reports are named
`build/performance/{native-solver,wasm-worker-solver}-s7.6-final-case-<id>-v1.json`.
The native endgame report is the final solve-only rerun made after cancelling
its long simulator process; it intentionally says verification skipped.

| Case | Native solve / compile | WASM solve / compile | Discovered / expanded | Nodes / edges | Start value native / WASM | WASM max slice | Caps native / WASM |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `oracle-real-one-mod` | 0.135 / 2.627 ms | 16.222 / 16.887 ms | 8 / 8 | 6 / 11 | 8.020144284129 / 8.020144284129 | 5.870 ms | pass / pass |
| `oracle-real-two-mod` | 0.185 / 4.133 ms | 22.256 / 26.841 ms | 16 / 16 | 9 / 23 | 356.459177852551 / 356.459177852432 | 6.388 ms | pass / pass |
| `ordinary-es-bench` | 11.977 / 22.838 ms | 96.397 / 145.309 ms | 241 / 241 | 45 / 179 | 780.177791245363 / 780.177791245069 | 14.328 ms | pass / pass |
| `advanced-es-resist-bench` | 51.528 / 64.830 ms | 163.905 / 388.533 ms | 577 / 577 | 137 / 538 | 5345.975710412209 / 5345.975710408555 | 21.142 ms | pass / pass |
| `endgame-fractured-es` | 3,554.145 / 99.912 ms | 5,710.394 / 576.934 ms | 3,727 / 3,725 | 104 / 714 | 132889.41981577268 / 132889.41981391114 | 2,248.229 ms | pass / **fail: worker step** |

The first four native/WASM values and structural counts are within the corpus
contract. Endgame structure also matches, but its start-value delta is
`1.86154e-6`, so the generated comparison fails the approved `1e-7` absolute
tolerance in addition to the worker-step cap. Endgame's native solve reports
9 focused rounds, equal lower/upper bounds of `132889.41981577268`, zero proof
gap, 612 policy-reachable states, 7 policy-improvement rounds, a
`1.72295e-8` measured residual, and exact-abstract status.

Native solver-owned estimates are 0.299, 0.315, 1.452, 6.724, and 373.619 MB;
WASM estimates are 0.210, 0.225, 1.335, 6.572, and 372.454 MB. The endgame
worker grew the WASM heap by 421.134 MB and peaked at 708.841 MB process RSS.

The retained S7.2 native solve baselines for the first four cases were 0.365,
33.051, 688.372, and 12,671.034 ms. The final native speedups are 2.70x,
178.65x, 57.47x, and 245.91x, a 51.12x geometric mean. No completed pre-S7.2
endgame report exists, so no endgame speedup is claimed. The S7.2 WASM reports
that exist moved from 83.570 to 16.222 ms for one-mod and from 1,193.949 to
96.397 ms for ordinary; later cases did not have a completed comparable WASM
baseline.

## Native compiled-strategy verification

Each completed report is one corpus-pinned invocation requesting exactly
10,000 runs. All completed runs succeeded with zero unmatched/off-policy
routes. WASM verification was deliberately skipped because the S7.6
correctness gate specifies the native simulator.

| Case | Runs | Forecast | Empirical mean | Relative delta | Success / off-policy | Result |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `oracle-real-one-mod` | 10,000 | 8.020144 | 8.140470 | 1.500294% | 100% / 0 | pass |
| `oracle-real-two-mod` | 10,000 | 356.459178 | 357.114030 | 0.183710% | 100% / 0 | pass |
| `ordinary-es-bench` | 10,000 | 780.177791 | 756.540375 | 3.029747% | 100% / 0 | pass |
| `advanced-es-resist-bench` | 10,000 | 5345.975710 | 4878.537200 | 8.743745% | 100% / 0 | **fail: 8% tolerance** |
| `endgame-fractured-es` | incomplete | 132889.419816 | unavailable | unavailable | unavailable | **cancelled after >30 min** |

The endgame native process continued consuming one core with a stable roughly
176 MB working set and emitted no completed report after more than 30 minutes.
Oliver had explicitly authorized cancelling work that did not appear likely to
finish, so it was terminated and was not restarted. Do not infer a completed
run count from that attempt.

The empirical per-run material/action totals captured for the completed
strategies are:

- one-mod: 1.0000 Transmute and 80.9047 Alterations;
- two-mod: 1.0000 Transmute, 29.8164 Augments, and 3,555.7321 Alterations;
- ordinary: 707.2173 Alchemies, 707.2173 Scours, 16.6066 Exalts, 27.0810
  Annuls, and 11.7988 bench crafts;
- advanced: 4,393.2315 Chaos, 30.1433 Exalts, 87.9261 Annuls, and 16.0410
  bench crafts.

## Action control, caps, and optimality

The five cases admitted 3/4/7/5/7 priced supported candidates from the explicit
goal envelopes. They pruned 15,549/15,548/15,545/15,547/15,545 non-permitted
registry entries; ordinary also recorded 234 certified equivalent-kernel
dominance reductions. No case had a missing price, unsupported requested
action, or unsupported observed action. Fossil generation remained lazy:
15,275 loadouts were deferred in the first four cases; endgame generated its
one requested Dense loadout and deferred 15,274 others.

Every native solve reports `exact_abstract_within_tolerance` with no state or
resource cap. The first four WASM solves pass every per-case cap. Endgame WASM
passes its state, row, transition, reforge, memory, and compiled-output caps;
only the 50 ms worker-step check fails. Cancellation acknowledgement was not
remeasured for these ordinary solve cases.

## Acceptance performed

`powershell -File scripts/test.ps1` completed green after fixing the failures
it exposed:

- ingest: 4 tests;
- economy: 8 tests;
- Python bindings: 14 tests;
- native engine: 370,185 checks, 0 failures;
- web/WASM: all suites green, including 25/25 main tests and 3/3 benchmark
  corpus tests.

Artifact validation/compilation and binding builds also passed inside the
pipeline. The final release WASM module was rebuilt before that green run. No
browser, screenshot, rendered UI, or other visual check was performed.

## Implementation cautions

- The reforge pointer-identity shortcut is exact only because the reforge cache
  owns immutable shared distributions keyed by action and preserved base. Do
  not generalize it to ephemeral or mutable distributions.
- Stable-policy scale-aware convergence is intentionally not used by the
  residual-prioritized fallback. The endgame native/WASM value mismatch is
  still open; do not hide it by loosening the comparison tolerance.
- Policy reachability through an observation choice follows its selected
  successor. Compiling every unselected offer alternative reintroduces false
  missing-action failures.
- Full canonical protected-repeat, Veiled Chaos, and fracture pools can allocate
  10-28 GB when forced through a unit-test contract. Keep exact bounded tests
  for semantics and the approved corpus for full-pool performance/caps.
- Final benchmark comparison reports for completed native verification versus
  solve-only WASM naturally list verification-field mismatches. Use the native
  report as the correctness sample and compare solve structure/value separately.

## Scope that remains parked

S6 Phase 3 ambient Emulator odds was skipped entirely and must not reappear.
Economy E0-E7 is complete except external production activation. Phase 12
accounts, publishing/community, mechanic track M1-M5, Phase 18 recombinators,
and ML remain deferred, blocked, parked, or later as documented.
