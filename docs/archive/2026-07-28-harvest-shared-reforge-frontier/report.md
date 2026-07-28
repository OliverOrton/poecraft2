# Harvest Natural Pools And Shared Exact Reforge Frontier — Final Report

Date: 2026-07-28

Plan: [Harvest Natural Pools And Shared Exact Reforge Frontier](plan.md)

Evidence:
[tracked summary](../../../fixtures/solver-natural-t1/v1/evidence/harvest-shared-reforge-frontier-summary.json)

## Decision

Retain the Harvest targeted-natural correction. Reject and restore the shared
cross-action reforge frontier and its measurement telemetry.

The shared frontier became exact after its canonical bucket authority was
corrected, but it did not reduce faithful action-lane work. Both frozen cases
stopped on the same root Fossil after the same 3,000,000 reforge-work units.
The complete Fossil lanes were no faster, the whole runs were slower, and each
Chaos structural DAG retained about 47 MiB.

No product cap, work accounting, unrestricted solver behavior,
goal-progress-gated behavior, Bellman choice, or action filter changed.

## Harvest Correction

Harvest reforge, augment, and resistance conversion now share one
`TargetedNatural` pool authority. A candidate modifier must have:

1. positive spawn weight;
2. positive ordinary generation weight;
3. the requested target tag; and
4. positive ordinary final roll weight.

No Harvest path substitutes generation percentage `100`. The regression
fixture contains a modifier with positive spawn weight and zero generation
weight and proves that sampled and exact reforge, augment, resistance
conversion, and debug reporting all exclude it. Positive-generation controls
remain reachable.

The correction passed the artifact-backed native suite with 515,093 checks
and zero failures before the structural prototype began.

## Prototype Shape

The prototype captured the Chaos roll frontier as a canonical structural DAG
for one preserved base and gated-mode identity. Nodes retained exact roll
state; edges retained canonical bucket identity and exclusion effects. Fossil
and Harvest reforge actions supplied their own bucket-weight vectors,
eligible denominators, normalized probabilities, costs, and Bellman rows.
Chaos probabilities were never reused or rescaled.

Filtered Fossils with added or forced modifiers used the sequential fallback.
Those cases would require explicit topology extensions or deterministic seed
deltas, but the support-subset cases falsified the base architecture before
that work was justified.

The first completion A/B exposed one exactness bug: a shared `RollState`
stores canonical Chaos bucket indices, while successor projection initially
looked those indices up in the Fossil-local subset vector. After projection
used the canonical bucket table, the shared and sequential full rows matched:

| Case/action | Outcomes | Terminal probability | Transition hash | Policy hash |
| --- | ---: | ---: | --- | --- |
| full-four / Lucent | 93,306 | `8.16389693564203e-08` | `2bd6f403fefe3574` | `816d94db8a2d0cbd` |
| deep-four / Jagged | 97,141 | `3.7206071645216775e-08` | `55053e54bbdeb5fe` | `364d755f6df87bed` |

Outcome count, probability bits, discovered-state count, transition hash, and
policy hash matched the sequential evaluator on both cases. The focused
artifact control also passed sampled/exact Harvest reforge parity.

## Unchanged-Cap Result

| Case | States discovered / expanded | Interrupted action | Lane work | DAG nodes / edges | DAG bytes |
| --- | ---: | --- | ---: | ---: | ---: |
| full-four | 134,508 / 1 | Lucent | 192,420 | 226,586 / 1,218,309 | 49,452,673 |
| deep-four | 123,728 / 1 | Jagged | 192,420 | 226,586 / 1,233,548 | 49,574,585 |

Both runs retained the prior Chaos transition/policy hashes and consumed
exactly 3,000,000 reforge-work units. The complete root envelope did not
advance. The shared cache therefore failed the milestone's product gate
before any cap adjustment was eligible.

## Complete A/B

Diagnostic copies changed only the two cases' ceilings so the first Fossil
row could finish: 5.2M/5.6M reforge work, 500,000 states, and 512 MiB. Product
caps remained unchanged.

| Case | Work both | Shared lane | Sequential lane | Shared total | Sequential total | Live bytes shared / sequential |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| full-four / Lucent | 1,883,672 | 430.098 ms | 430.427 ms | 1,700.718 ms | 1,346.745 ms | 118,778,826 / 69,325,977 |
| deep-four / Jagged | 2,194,169 | 381.168 ms | 369.127 ms | 1,515.151 ms | 1,140.175 ms | 95,901,770 / 46,327,009 |

The Lucent lane was effectively unchanged (`0.999x` shared/sequential).
Jagged was 3.3% slower. Whole-run shared time was 26.3% and 32.9% higher.
Every shared successor projection hit an already interned Chaos state, so the
frontier avoided no successor allocation while retaining its own dense edge
graph.

The deterministic work result is decisive. Chaos construction remained
2,807,580 work; Lucent and Jagged still required 1,883,672 and 2,194,169
action-specific work. Propagating several lanes in one outer loop can reduce
loop overhead, but every lane still needs its own positive-weight edge update
and denominator. Charging fewer work units solely because those scalar
operations share an outer loop would hide rather than remove computation.

## Consequence

Cross-action structural-frontier reuse is closed for this dense replay shape.
Do not reopen it as a cache-only change, a cap-only change, or work-accounting
discount. A future solver improvement must avoid or bound action-specific
probability propagation, not retain the dense Chaos DAG and replay it.

Harvest's corrected targeted-natural pool remains useful independently. It
also resolves the prior spawn-only uncertainty without creating a
Harvest-specific exception in solver architecture.

## Verification And Identity

- Starting source: `94fb013`.
- Plan commit: `b587c3b`.
- Harvest correction commit: `2e65e7c`.
- Rejected prototype patch hash-object: `adb509190b44ea7f535b09e9dd0aada305877452`.
- Shared measurement executable SHA-256:
  `c1fdecd434548ece80c2d0d2f4ec65e69af5b21e1f5fe735e6358445dded30c0`.
- Product portfolio manifest SHA-256:
  `a4f080e680499b95d2b6b5e635c4bc9fd6678a82f4ce698ea8c6882b37fb3efd`.
- Artifact manifest SHA-256:
  `f363ed784539c32a8ef333df87d2c3a0e3b58f3accf4d89222eff8fce08445f1`.
- Natural-T1 generator config SHA-256:
  `6c9acf7f3512b07776c5d0342f0fea24ee1dbe303ce2b198361d197ff6579512`.
- Compiler/machine: GCC 14.2.0, Windows build 26200, Intel i7-13700K,
  16 cores / 24 logical processors, 68,396,957,696 physical bytes.

The retained native artifact suite passed 515,093 checks with zero failures.
Release WASM was rebuilt after the mechanic change; affected web tests and
TypeScript type-check passed. The build reproduced only the existing GCC
optimizer warning in `prepare_goal_cover_cost`.

Wall figures are machine/compiler-bound and do not survive a hardware or
compiler change. Work, probability bits, outcome counts, and hashes are the
portable evidence.
