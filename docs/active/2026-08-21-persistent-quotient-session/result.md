# Persistent Quotient Session And In-Place Frontier Growth Result

**Status: complete on 2026-08-22.**

The production strict lift now extends one durable quotient session through
competitive frontier growth. The checked four-T1 primary kept one strict
session, performed four real in-place insertions, and performed no production
full restart. It preserved the accepted verified upper and materially reduced
the open exact alternative envelope. The initial Gate 6 record stopped because
retained native ownership exceeded its original 150 MB ceiling. Oliver
reviewed the measured ownership and replaced that milestone-only threshold
with 512 MiB (536,870,912 bytes), so the 419,316,840-byte result now passes
Gate 6. No engine resource limit or proof behavior changed. Release WASM and
the full repository acceptance pipeline pass.

## Retained implementation

The implementation checkpoints from `18e4640` through `bb29378` retain:

- one production owner for the strict oracle, selected closure, stable
  split-only partition, Bellman graph, proof store, published rows,
  alternative obligations, and verified compiled incumbent;
- explicit in-place frontier phases with collision-checked carrier insertion,
  stable cell generations, localized source/target invalidation, row
  reprojection, and reverse proof dependencies;
- cooperative alternative carrier certification and worklist Bellman closure;
- compact shared obligation, observation, action, and dependency identities;
- retained independently evaluated publication across frontier growth, with
  final-policy recompilation still required before an exact promotion when
  the retained assertion no longer reconciles with current Q;
- obligation-lifetime calculator cache release after the compact proof row or
  partial result becomes authority; and
- bounded native progress batching: 32 items in construction phases and 128
  already-cooperative items in alternative proof, measured below the public
  step ceiling.

The temporary within-obligation absolute-row cache was removed after the
checked trial produced only nine hits. No cross-generation carrier-row cache,
weakened proof, action trimming, engine cap increase, or fallback publication
was retained.

## Checked native primary

The final native report is the Allflame four-natural-T1 Conquest Lamellar case
with verification disabled and the existing 300-second watchdog.

| Measurement | Accepted parent | Final persistent session |
| --- | ---: | ---: |
| Solve wall | 300,102.42 ms | 300,096.02 ms |
| First verified strict upper | 38,943.05 ms | 36,462.28 ms |
| Final live upper | 3745.7295960574743 | 3745.7295960574743 |
| Global lower | 0 | 0 |
| Strict session constructions | repeated passes | 1 |
| Production full restarts | repeated passes | 0 |
| In-place insertions / states | 0 / 0 | 4 / 138 |
| Open alternatives at stop | 17,584 | 6,963 |
| Largest public step | 1,484.77 ms | 1,194.53 ms |
| Native peak owned estimate | 98,661,450 bytes | 419,316,840 bytes |

The open envelope fell by 10,621 obligations, or 60.40%. The live session
retained 17,334 cells across updates, created 5,820, superseded 126, recorded
126 source and 126 target splits, performed 6,576 reverse invalidations, and
reprojected 28,802 rows. The final proof store had certified 17,805
alternative obligations, partially evaluated 6,569, and marked 6,302 stale.
There was no unsupported lower or exactness promotion and no upper increase.

The first strict upper and maximum public step pass their 42.84-second and
1.65-second boundaries. The one-session, zero-restart, real-growth, value,
and greater-than-10% progress requirements also pass.

## Gate 6 owner adjustment

Memory was the sole failed criterion under the original 150 MB boundary. The
quotient telemetry reports 363,854,278 total solver-owned bytes; the complete
live solve reports
419,316,840 peak owned bytes. Named retained proof categories include
104,935,928 row-kernel bytes, 18,905,936 alternative-obligation bytes,
4,465,104 dependency-sidecar bytes, 3,070,552 coverage bytes, and 2,359,296
certificate bytes. The remaining approximately 230 MB is an unsplit
oracle/adapter remainder that includes the live strict calculator and its
exact carrier, policy, and mechanic state; current telemetry does not identify
which subowner dominates. Process working set after the watchdog was
214,188,032 bytes, but the milestone explicitly qualifies on the conservative
owned-memory ledger, not working set.

The initial stop was correctly recorded. On 2026-08-22 Oliver explicitly
accepted the measured increase and set a 512 MiB milestone ceiling. The native
peak is 117,554,072 bytes below that revised boundary, so Gate 6 passes and
conditional release qualification proceeded. This adjustment does not hide
persistent payload or alter a runtime cap. Future memory work should first
split live strict-calculator, adapter-map, selected-closure, published-row,
and proof-store ownership in the telemetry, then remove actual simultaneous
duplication or shorten safe cache lifetimes. Another cross-generation row
cache is not supported by the evidence.

## Release WASM

The tracked release module was rebuilt after a portability-only fix that makes
the retained-byte `std::max` operands explicitly `uint64_t`; the 64-bit Windows
native build had accepted the `size_t`/`uint64_t` mix while wasm32 libc++
correctly rejected it. This does not change values or resource limits.

The release-WASM five-minute primary reaches the cooperative watchdog with the
same semantic and proof-progress boundary as native:

| Measurement | Native | Release WASM |
| --- | ---: | ---: |
| First verified strict upper | 36,462.28 ms | 63,208.70 ms |
| Final live upper | 3745.7295960574743 | 3745.7295960574743 |
| Strict sessions / full restarts | 1 / 0 | 1 / 0 |
| In-place insertions / states | 4 / 138 | 4 / 138 |
| Retained / superseded cells | 17,334 / 126 | 17,334 / 126 |
| Source / target splits | 126 / 126 | 126 / 126 |
| Reverse invalidations / reprojections | 6,576 / 28,802 | 6,576 / 28,802 |
| Open alternatives | 6,963 | 6,963 |
| Largest public/worker step | 1,194.53 ms | 2,049.82 ms |
| Live owned at stop | 419,303,896 bytes | 366,916,901 bytes |
| Quotient solver-owned | 363,854,278 bytes | 315,716,514 bytes |

The verified upper never increases and the global lower remains zero. Release
WASM records 500,498,432 bytes of heap growth from a 278,396,928-byte loaded
baseline; the 5 ms Node-process sampler records a 2,795,048,960-byte RSS peak.
Those are disclosed runtime measurements, not substitutes for the
collision-checked engine ownership ledger. The engine's one-GiB request cap
does not fire.

The largest WASM slice occurs in the pre-refinement coarse focused-expansion
round, not in the persistent quotient work. A measured trial halving the
alternative-proof suspension batch from 128 to 64 changed that slice only
from 2,049.82 to 1,997.03 ms while preserving identical final counters, so the
trial was removed. The existing worker-step cap passes, ordinary release-WASM
cancellation tests remain prompt, and watchdog cleanup retains the live
session telemetry.

The primary remains a progress-only result at five minutes, so no final
strategy exists and the 10,000-run primary Simulator check is inapplicable.
This milestone improves frontier/proof continuity; it does not claim four-T1
exact closure.

## Qualification

- Native build: pass.
- Quotient partition/Bellman/proof: 614 checks, 0 failures.
- Core refinement: 362 checks, 0 failures.
- Policy refinement: 1,234 checks, 0 failures.
- Solver suite: 96,439 checks, 0 failures.
- Automatic Eldritch product control: converged at
  `0.018630169563331064`; 10,000/10,000 successful simulations.
- Warlord product control: converged at `224.1238588972487`;
  10,000/10,000 successful simulations.
- Release WASM build: pass.
- TypeScript no-emit and complete non-visual web/WASM tests: pass, including
  28/28 release engine smoke checks.
- Full `scripts/test.ps1` repository pipeline: pass, including 3,464,468 native
  engine checks with zero failures.
- Primary simulation: not run because neither primary emitted a final
  strategy.
- `git diff --check`: pass before documentation finalization.

Compact measurements are in
[native-primary-summary.json](evidence/native-primary-summary.json). Full
reports remain derived local artifacts under
`build/solver-diagnostics/persistent-quotient-session/`.
