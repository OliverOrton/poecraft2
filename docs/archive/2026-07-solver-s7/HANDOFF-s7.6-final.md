# Session Handoff - S7.6 Had One Open Endgame Success Gate

> **Archived 2026-07-17.** This preserves the final pre-closure S7 evidence and
> cautions. Oliver subsequently directed work to move forward without another
> endgame sample. The `0.9942` result remains a disclosed miss against the
> former `0.995` target.

Updated 2026-07-16 after the final S7.6 native/WASM corpus and the one
authorized fresh endgame simulator sample. Read
[AGENTS.md](../../../AGENTS.md), [Project Direction](../../direction.md), this
file, then the archived
[S7 plan](solver-depth-and-performance-plan.md).

S7.0-S7.5 and S7.2R remain complete. Stay in **S7.6 only**: the solver,
compiler, WASM responsiveness/parity, advanced forecast, and simulator
throughput work are complete, but S7.6 must not be called complete because the
single final endgame sample recorded `0.9942` success against the approved
`0.995` minimum.

## Exact next boundary

Do not start post-S7 work and do not launch another endgame 10,000-run sample
without Oliver explicitly authorizing it. The one fresh pinned sample requested
for this pass has been consumed and must not be combined with any partial or
duplicate sample.

Oliver needs to choose the remaining S7.6 boundary:

1. keep the approved `0.995` success gate and authorize a replacement sample
   only after a concrete run-tail change, most plausibly raising the
   `500000` per-run action safety cap; or
2. explicitly accept the recorded `0.9942` result/change the product gate.

Before any authorized replacement, extend the native report to serialize the
simulator's action-limit, explicit-terminal, step-limit, no-edge, and
action-not-applied counters. The completed report has 58 failures and an
aggregate `off_policy_failures: 0`, but its schema did not preserve
enough failure-reason detail to prove whether all 58 were action-cap tails.
Use bounded probes before another expensive command. A cap change is an
operational guardrail decision, not a mechanic approximation; do not change it
or the success tolerance silently.

## What changed

- The advanced discrepancy was a real abstraction-fidelity defect. Junk mods
  with different complete generation/exclusion-group effects had been merged,
  making reforge outcomes non-lumpable. Reforge families now retain their full
  sorted exclusion-group signature, conflicting families are blocked exactly,
  and all positive-probability paths are retained. The former epsilon/frontier
  truncation is gone; explicit work/memory caps refuse instead of approximating.
- Exact expansion and policy evaluation are persistent/resumable at action,
  kernel-group, quotient, Tarjan-component, component-batch, and BiCGSTAB work
  boundaries. Immutable absolute transition kernels and exact shared reforge
  identities remove repeated billion-outcome routing/scans.
- Recurrent policy aggregation/solves use deterministic double-double
  arithmetic with FP contraction disabled in native and WASM. Native and WASM
  now select identical policies and produce identical start values in the
  final reports.
- The compiled simulator pre-resolves node prices, memoizes exact conditions
  and counts, and uses exact collision-checked direct-signature/decision-DAG
  routing with priority-preserving fallback. It still executes every primitive
  mechanic through the engine.
- Solver-emitted junk counts use the compact editable
  `mod_family_count` condition only when the selected families exactly cover
  the old concrete-mod union; otherwise the compiler retains `mod_count`.
- Native verification is one-run chunked, reports progress every 10 seconds,
  projects total/remaining time, records variance, and can stop between runs.
  Partial/time-limited diagnostics never count as acceptance.
- The WASM worker aggregates small exact engine steps for throughput but caps
  adaptive work at four items and yields after roughly 8 ms. A one-step-yield
  instrumentation mode keeps the cancellation fixture meaningful even when a
  small solve finishes before the normal yield interval.
- Solve-only WASM benchmarking no longer clones and recompiles a 130-156 MB
  strategy merely to discard the simulator handle. Verification runs still do
  the required second compile.

No mechanic behavior, carrier/state distinction, observation choice, legal
action, transition probability, policy branch, tolerance, or optimality
guarantee was weakened for speed.

## Final solve, compile, parity, and responsiveness evidence

Primary ignored reports:

- `build/performance/native-solver-s7.6-final-solve-v1.json`
- `build/performance/wasm-worker-solver-s7.6-final-solve-v1.json`
- `build/performance/solver-s7.6-final-solve-v1-comparison.json`
- `build/performance/native-solver-s7.6-final-advanced-case-advanced-es-resist-bench-v1.json`
- `build/performance/native-solver-s7.6-final-endgame-case-endgame-fractured-es-v1.json`

The final comparison passed 698 checks across all 11 shared native/WASM cases
with zero mismatches. All state/row/transition/reforge/memory/compiled-output
caps passed.

| Case | Native solve / compile | WASM solve / compile | Start value N / W | WASM max step | Exact structure |
| --- | ---: | ---: | ---: | ---: | --- |
| advanced | 1,184.961 / 4,402.892 ms | 4,695.330 / 6,957.718 ms | 4911.464629420442 / same | 18.073 ms | 24,169 expanded; 2,917 nodes / 22,465 edges |
| endgame | 4,643.110 / 876.696 ms | 19,801.271 / 1,526.729 ms | 132353.19529787666 / same | 29.006 ms | 48,945 discovered; 48,943 expanded; 619 nodes / 5,124 edges |

The endgame value delta is exactly zero at binary64 report precision, well
inside `1e-7`; its worker step is below 50 ms. Native endgame reports
`exact_abstract`, zero proof gap, 4,507 policy-reachable states, 296,239 rows,
599,362 stored transitions, 3,137,365,681 logical outcome entries, and no state
or resource cap. The solve-only cancellation fixture acknowledged in 13.071
ms against the 250 ms budget.

Solver-owned estimates are 58,748,184 native / 52,107,796 WASM bytes for
advanced and 151,357,398 native / 139,068,543 WASM bytes for endgame. During
the one-worker full WASM corpus, exact strategy serialization grew the heap to
1,050,083,328 bytes and process RSS peaked at 2,319,425,536 bytes; memory has
no owner ceiling and all approved caps passed. The editable strategies are
155,798,869 bytes (advanced) and 34,174,515 bytes (endgame).

## Final native simulator evidence

Advanced used one fresh pinned `2026071520` invocation with exactly 10,000
runs:

- 10,000 successes, 0 failures, 0 reported off-policy routes;
- forecast `4911.464629420442`, empirical mean `4878.5372`;
- absolute/relative delta `32.92742942044242` / `0.006704197608021437`
  (0.670420%), inside 8%;
- standard deviation/error `4824.002367039209` / `48.24002367039209`;
- 74.063 s verification wall time.

The old and new advanced samples use the same pinned seed and have identical
45,273,419 primitive-action totals: 4,393.2315 Chaos, 30.1433 Exalts,
87.9261 Annuls, and 16.0410 bench crafts per run. Concrete strategy behavior
and sampled materials therefore did not change; the exact forecast changed
from `5345.975710412209` to `4911.464629420442` (-8.127816%).

Endgame used exactly one fresh pinned `2026071530` invocation with exactly
10,000 completed runs. It was never restarted or combined:

- 9,942 successes / 58 failures = `0.9942`, below the approved `0.995`;
- forecast `132353.19529787666`, empirical mean `130724.1208`;
- relative mean-cost delta `0.012308539240100952` (1.230854%), inside 10%;
- report says aggregate `off_policy_failures: 0`, but see the
  failure-reason reporting limitation above;
- standard deviation/error `126478.99525383864` / `1264.7899525383864`;
- 984,077,152 primitive actions in 1,523.755 s = 1.5484 microseconds/action;
- 25.58 minutes verification wall time, versus a bounded probe projection of
  roughly 32 minutes;
- sampled per-run materials/actions: 612.1873 Exalts, 3,527.8543 Annuls,
  93,883.6032 Dense Fossil operations, and 384.0704 bench crafts.

The endgame forecast changed from `132889.41981577268` to
`132353.19529787666` (-0.403512%), and the exact editable policy expanded from
104/714 nodes/edges to 619/5,124. No pre-change endgame sample completed, so
do not claim an empirical material delta.

## Acceptance performed

- `powershell -File scripts/build.ps1` passed.
- `powershell -File scripts/build-wasm.ps1` rebuilt the release WASM module.
- `powershell -File scripts/test.ps1` passed: ingest 4, economy 8, Python
  bindings 14, native engine 372,493 checks/0 failures, and all Web/WASM suites.
- Final `npm test` passed, including 25/25 main WASM tests and 3/3 corpus tests.
- Final `npx tsc --noEmit` passed.
- The final native/WASM solve comparison passed 698 checks over 11 cases.
- No browser, screenshot, rendered UI, or other visual check was performed.

## Cautions

- Reforge sharing is exact only for immutable cached absolute kernels; do not
  generalize identity sharing to mutable or query-state-dependent outcomes.
- `mod_family_count` is exact only under the compiler's full-family-cover
  proof. Keep the concrete `mod_count` fallback.
- Deterministic wide aggregation and `-ffp-contract=off` are parity
  requirements, not optional tuning.
- Do not treat the 58 endgame failures as proven action-limit tails. The
  throughput/tail shape strongly suggests that explanation, but the completed
  report did not serialize the necessary reason counters.
- Do not rerun the expensive sample merely to improve a stochastic result.

## Parked scope

S6 Phase 3 ambient Emulator odds remains skipped. Economy E0-E7 is complete
except external production activation. Accounts, publishing/community,
mechanic track M1-M5, recombinators, ML, and all other post-S7 work remain
parked as documented.
