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

