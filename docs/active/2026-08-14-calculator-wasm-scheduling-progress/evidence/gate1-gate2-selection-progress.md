# Gate 1 Scheduler Selection And Gate 2 Progress Contract

**Status: Gate 1 complete; Gate 2 implementation retained with one explicit
open architecture boundary.**

Parent: [Plan](../plan.md)

## Scheduler selection

The historical 2026-07-29 high-impact experiment is not by itself promotion
evidence. That experiment was rejected because it admitted no improving row
on its frozen case and its behavioral prototype was restored. The current
source subsequently acquired materially different exactness machinery during
Solver Goal Realignment:

- operator-major delayed-action scheduling tracks every completed
  state/operator pair;
- it scans the complete delayed-action Cartesian ledger before closing the
  envelope, including carriers discovered after an earlier operator sweep;
- only fully materialized legal rows can be classified or admitted;
- proper selected-policy evaluation owns executable uppers; and
- exact publication still requires strict lift plus independent compiled-graph
  evaluation and cost reconciliation.

The scheduler changes work order and witness discovery, not the action set,
prices, transition kernels, objective, caps, or proof standard. The Gate 0
8/on and 1,024/on runs produced identical exact values and transition/policy
hashes.

The release-WASM breadth control then applied the proposed normal Calculator
profile—eight work items, high-impact on—to all five cases in the primary Goal
Realignment manifest. All five closed exact with no watchdog or report error:

| Case | Exact value | Solve ms | Max bounded step ms |
| --- | ---: | ---: | ---: |
| four-natural-T1 primary | `3745.7309340083884` | `216631.589` | `4573.159` |
| Oliver three-prefix witness | `2186.6911143146394` | `138079.792` | `31.449` |
| automatic Eldritch Exalt | `2.494` | `22179.581` | `64.909` |
| Warlord Exalt | `224.1238588972487` | `1018.305` | `31.88` |
| Imprint retry | `252.6535202127448` | `504.715` | `73.837` |

The four-goal primary remains under five minutes, but its one 4.57-second
native work unit means the selected witness's sub-250-ms responsiveness result
must not be generalized to every hard goal. The product witness itself meets
the bounded-step target.

Raw breadth report:
`build/calculator-wasm-scheduling/gate1-product-profile-controls.json`,
4,606,211 bytes, SHA-256
`98794a87ea0c0a37dabed15069df8f275b943387a318523fc143504270a0f85b`.

## Retained product changes

Normal Calculator Solve options now select the exact operator-major scheduler.
The worker no longer exposes native phase `Done` as a completed public result:

1. bounded native stepping emits expanding/iterating progress;
2. the worker emits `finalizing` with `done = false` before synchronous public
   policy extraction;
3. finalization wall time is reported separately from bounded step metrics;
4. after extraction, the worker emits one real `done` snapshot populated from
   the published result's value, bounds, gap, residual, states, and sweeps; and
5. a cancellation already queued at the boundary prevents finalization, while
   a cancellation received during the synchronous pass is preserved as
   cancelled rather than silently returned as success once the worker regains
   its event loop.

The Calculator now shows elapsed time, expanded and discovered states,
frontier, focused round, rows, transitions, reforge work, solver-owned memory,
and the existing bound fields. During finalization it says that policy
extraction/verification is still running and that cancellation waits for the
current native pass. No TypeScript field derives a bound or proof fact.

The changed product witness closed exact at `2186.6911143146394` in
`142434.928` ms. It used 463 bounded steps, max step `76.028` ms, then
`142123.084` ms of finalization. The trace now ends:

| Elapsed ms | Phase | Done | Lower / upper |
| ---: | --- | --- | --- |
| `300.605` | `finalizing` | false | `2244.8394347831436` / same |
| `142434.598` | `done` | true | `2186.6911143146394` / same |

Thus the final progress snapshot is no longer premature or numerically stale.
Raw report:
`build/calculator-wasm-scheduling/gate2-product-progress.json`, 883,360 bytes,
SHA-256
`f4269c1061029240b3c573df07b6e3a7245d6cbd3e74f45c5eb8896e2bd234e3`.

Focused `npx tsc --noEmit`, Calculator result tests, and the complete 28/28
release-WASM smoke pass. The smoke includes cancellation from the finalizing
boundary and proves that no successful result is published after that cancel.

## Open architecture boundary

The 142-second strict finalization itself is still one synchronous native/WASM
call. A cancel clicked after that call begins is recorded and returned as
cancelled, but it cannot reduce the wait. Elapsed time continues on the main
thread and the phase is truthful; this is not cooperative finalization.

Real interruption requires turning policy-guided strict lift—principally the
4,215 selected strict-kernel builds on this witness—into a retained incremental
work object stepped through the C ABI/WASM worker, or an equivalent isolated
worker design with reconstructible solver authority. A label, a larger chunk,
or a message-only cancel cannot make synchronous WASM yield. This is the
plan's architecture stop condition and remains open for an explicit decision;
the scheduler/product-latency repair itself is retained for final qualification.

## Five-natural-T1 breadth correction

Oliver's live five-mod report is not represented by the historical Runic
Gauntlets control. That control starts with four satisfied T1 affixes and can
finish with one `0.01165`-chaos Cold Resistance bench craft; under the selected
WASM profile it returned a bounded result in `14673.498` ms. The actual live
request starts from an empty ilvl-86 Conquest Lamellar and requires the three
natural T1 armour/evasion prefixes plus natural T1 physical-damage reduction
and spell suppression suffixes.

The live run remained in expansion after `175m 21s`: 216 expanded, 1,785
discovered, 1,569 frontier, 8,222,083 displayed sweeps, focused round 324,
1,885 rows, 8,657 transitions, 749,416 reforge work, lower zero, and upper
about `37279652.26`. The zero lower remains sound; the runtime is unacceptable.

The frozen fixture reproduced the same boundary with a 30-second cooperative
watchdog. Both scheduler modes built the same 1,347-state/7,056-row/25,554-
transition initial graph, entered a largest fixed-policy SCC of 926 states,
consumed 100,138 sparse-policy iterations, and reported
`sparse_policy_component_did_not_converge` with `max_sweeps` recorded. The
solver then continued through prioritized Bellman fallback instead of
publishing or stopping. At cancellation, eight-item high-impact mode had 2,580
fallback sweeps; off mode had 2,573. A 1,024-item high-impact control reached
3,485 sweeps with a 138.947-ms maximum step, a modest throughput improvement
that does not address the numerical failure.

The retained fallback portfolio's preferred executable upper was
`37279857.82627867`, matching the scale of the live report. The scheduler
promotion therefore did not create this first failure: it exposed a shared
sparse fixed-policy convergence/fallback defect. The next source boundary is
the shared large-SCC evaluator in `solver_sparse_policy.cpp`, not another
scheduler or cap experiment.

Raw ignored reports:

- `gate2-five-natural-t1-product-on-30s.json`, 455,150 bytes, SHA-256
  `322be395f9361a031d3210353e6c2d40e47d0f61910259a947281e41c901fa96`;
- `gate2-five-natural-t1-product-off-30s.json`, 455,944 bytes, SHA-256
  `a1dae51a9cee9899e4d8f83a9fc9fc5fb7291e70e2778511cbeafa12f58da728`;
- `gate2-five-natural-t1-product-on-1024-30s.json`, 440,844 bytes, SHA-256
  `5908c6f3ba7a03ed987a32fcab82dc64025ed29b2217c31315022ffaf2f8ef08`;
  and
- `gate2-five-mod-product-30s.json`, 418,887 bytes, SHA-256
  `79a843ac7272c7048a966c21e9fa6ef5a54de93cf3098ca4cab98614adad40aa`.
