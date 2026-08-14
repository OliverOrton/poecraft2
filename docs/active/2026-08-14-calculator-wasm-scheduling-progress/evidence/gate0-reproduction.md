# Gate 0 Product-Equivalent Reproduction

**Status: complete on 2026-08-14.** The normal release-WASM Calculator
symptom is reproduced and attributed without changing engine behavior.

Parent: [Plan](../plan.md)

## Frozen identity

| Input | Identity |
| --- | --- |
| Engine source baseline | `191bc010aad7ece2a227766d6000117074ad9e52` |
| Plan activation commit | `9cec44699d42be23a243188533f983f929f4ebb7` |
| Release WASM | `601030d01d188a7351f85a133a3b9d855d7c708082ee64c8567aaf66cc59c5bc` |
| Release wrapper | `da21809d726bcc18046a7c19d5cd004e638b0ef193302bd45a669705452a29cf` |
| Compiled-data manifest | `030d28d4b80b2aa1a8f2a6e4d3ae286b5dc254164e5fa74e5441667b91648243` |
| Economy | published Allflame `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0` |
| Fixture | `conquest-lamellar-allflame-three-prefix-t1-product` |

The fixture is Oliver's empty rare item-level-86 Conquest Lamellar and the
three displayed T1 prefix families. It uses the Calculator product action
envelope, `goal_progress_gated_reforges = true`, no manual price override, no
full-evidence diagnostics, and the published snapshot's missing `base` price.
Sixteen actions survive product pricing; Restart does not.

## Four-mode result

All modes used the same request, action IDs, prices, public caps, and release
module. Verification was skipped because this gate isolates Solve scheduling;
compilation, exact evaluation, and 10,000-run verification belong to final
qualification.

| Work maximum | High impact | Solve result | Last step progress | Key outcome |
| ---: | --- | --- | --- | --- |
| 8 | off | no result by 300 s; externally stopped | `done` at 51.525 s | 705 states, 5,781 rows, 109 focused rounds, lower 0, upper `13882.856439060486`; final extraction remained CPU-bound and silent for the rest of the boundary |
| 1,024 | off | no result by 300 s; externally stopped | `done` at 30.413 s | same states, rows, focused rounds, work, and interval as 8/off; larger steps did not repair finalization |
| 8 | on | exact in 133.594 s | `done` at 0.286 s | exact `2186.6911143146394`; 235 states, 2,229 rows; max measured step 76.759 ms |
| 1,024 | on | exact in 136.817 s | `done` at 0.270 s | identical value, transition hash `49e54e458dabfa3f`, and policy hash `18a9e63deaaa4be2`; max measured step 109.364 ms |

The 8/on and 1,024/on results are semantically identical. Eight work items
were slightly faster and retained the stronger step-responsiveness result.
Therefore large worker chunks are rejected as the product repair. The
high-impact scheduler mode, not chunk size, is the causal difference.

The off-mode interval exactly reproduces Oliver's reported upper/gap rounded
to `13,882.86` and the absent useful lower. The precise state count differs
from Oliver's remembered approximate count, but the value, phase behavior,
request, and long-running symptom agree.

## Progress and cancellation finding

The worker treats native phase `Done` as final progress and then calls
`finishSolverSolve` synchronously. That is not a completed public result. In
the 8/on exact run, the last progress snapshot still reported
`2244.8394347831436`, while final extraction changed the exact published value
to `2186.6911143146394`. Telemetry attributes 133.301 seconds of the 133.594-
second solve to extraction. The strict lift materialized 4,219 exact states,
27,882 exact transitions, and 4,215 strict kernels.

No progress or cancellation boundary exists inside that extraction call. The
off-mode cooperative watchdog fired at 300 seconds, but the main thread could
not receive a final report while WASM extraction remained synchronous; the
diagnostic process was stopped at the selected outer boundary. This is a
reproduced product responsiveness defect, not merely a presentation issue.

## Raw ignored evidence

| File under `build/calculator-wasm-scheduling/` | Bytes | SHA-256 |
| --- | ---: | --- |
| `gate0-product-8-off-300s.json.progress.json` | 333,213 | `b3d125ee84c56d346d38b6fbc3c50f378cdb7ea2f333780f0d3262fbe27b02b3` |
| `gate0-product-1024-off-solve.json.progress.json` | 169,467 | `f9f3c8304c548e98bd10b3e5ab1be1734204c38d01a7cfe59c2f270ee5c46138` |
| `gate0-product-8-on-solve.json` | 882,545 | `ee948ef0576a6c9ff31d89959bacb0aaeff01afd1bcdc63881c013e8479d35e5` |
| `gate0-product-8-on-solve.json.progress.json` | 4,225 | `0b06fc02b09488ee7838e742f39212bed1062038182b83ab8a623ff06b873b8b` |
| `gate0-product-1024-on-solve.json` | 881,050 | `b734683d5e4f550e3df7d2419802a1bf153941339b4cb2dab50bc6b29e120bdb` |
| `gate0-product-1024-on-solve.json.progress.json` | 2,894 | `81e766c466d22beb75763c88f649455338be5f7aa2e9129f1abbcb8168536108` |

The benchmark harness now supports explicit diagnostic scheduling overrides,
solve-only attribution, and a watchdog-time progress sidecar. Those additions
do not alter engine or product behavior and preserve evidence even when
cooperative finalization cannot return.

## Gate decision

Gate 1 must audit the current high-impact scheduler's proof and breadth
history, then select a product-safe scheduler activation only with explicit
semantic controls. Independently, it must move final extraction behind real
cooperative progress/cancellation boundaries or produce a precise architecture
handoff; merely rendering a synthetic `finalizing` label cannot satisfy the
responsiveness contract.

