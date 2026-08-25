# Imprint Solve Scope And Controlled Comparison Result

**Status: complete and accepted (2026-08-22).**

Parent: [plan](plan.md)

Implementation checkpoint: `ee5e507`

## Result

Calculator now has a checked-by-default “Consider automatic Imprint
checkpoint/retry programs” control. The option crosses TypeScript, worker JSON,
release WASM, the append-only C ABI flag set, native `SolveOptions`, retained
cache identity, and carrier-local automatic admission. Clearing it bypasses
only generated Imprint programs. Eldritch and the other automatic families
retain their existing authority.

This is an explicit reduced action scope, not a claim that Imprint is
dominated. Result presentation, native telemetry, solution scope, and compiled
strategy provenance disclose the exclusion. Omitted options remain `true` for
historical native/WASM compatibility.

## Controlled four-T1 comparison

The checked Allflame four-natural-T1 Conquest Lamellar fixture was run twice
from the same implementation and production caps. Verification was skipped so
the comparison measured Solve itself. The second run temporarily added only
`consider_imprint_programs: false`; the canonical fixture was then restored.

| Measurement | Imprint enabled | Imprint disabled |
| --- | ---: | ---: |
| Total wall | 44.823 s | 300.016 s watchdog |
| Solve wall | 43.903 s | 299.983 s |
| Stop | `max_imprint_program_work` | external case watchdog |
| Certified/latest lower | 21.772459401332767 | 21.772459401332767 |
| Verified policy upper | 3759.9763122101763 | none published |
| Latest unfinished raw upper | n/a | 22103.31023739946 |
| Discovered / expanded / frontier | 3,324 / 1,207 / 2,117 | same |
| Retained rows / transitions | 11,405 / 31,126 | 11,454 / 31,184 |
| Imprint programs evaluated / pruned | 256 / 169 | 0 / 0 |
| Imprint outcomes merged | 2,570,418 | 0 |
| Incremental upper-policy passes | 780 proper | 785 started, 784 proper |
| Rows reconsidered | 1,692,268 | 1,711,657 |
| Open action alternatives | 18,154 | 18,210 |
| Optimization wall | 2.053 ms | 284.232 s |
| Compiled graph | 87 nodes / 241 edges | none; solve did not finish |

Evidence:

- [enabled native report](primary-imprint-enabled.json)
- [disabled native report](primary-imprint-disabled.json)
- [enabled bounded strategy](strategies/imprint-enabled/conquest-lamellar-allflame-four-natural-t1.strategy.json)

## Characterization

Imprint is a real first blocker: disabling it eliminates all Imprint discovery
work and passes the 45-second resource refusal. It is not the only blocker.
Once that refusal no longer triggers finalization, the solve spends almost the
entire five-minute remainder in delayed/automatic executable-upper
reoptimization. Only five additional upper-policy passes start, the last does
not complete, and 18,210 alternatives remain open. The external watchdog then
abandons the solve, so no bounded policy is packaged even though an early
incumbent existed.

The next product owner is therefore not “disable Imprint by default.” It is a
native bounded stop/continuation contract for the incremental automatic action
envelope, followed by batching or epoching of dirty alternatives so one newly
admitted carrier/action row does not repeatedly demand another whole proper
upper-policy pass. Remaining obligations must stay visible and must block an
exact claim, while the best already certified incumbent is finalized and
published at the bounded stop.

## Carrier-equivalent Imprint dominance

Oliver's economic intuition is retained as the exact research successor. An
Imprint restore can be pruned when a proper non-Imprint policy is proved no
more expensive from every relevant failure class back to the same observable
Magic checkpoint class, with the same downstream continuation value. A cheap
route to an arbitrary Magic item is insufficient: goal progress, affixes,
fractures, influence/item flags, checkpoint identity, success distribution,
and continuation all matter. Conversely, proving Imprint restore cheaper than
ordinary recovery makes it potentially useful, but does not by itself prove
the entire attempted program optimal.

## Acceptance

- `powershell -File scripts/build.ps1`: pass.
- Native solver suite: 96,543 checks, zero failures.
- Native API suite: 2,980 checks, zero failures.
- Native compiler suite: 840 checks, zero failures.
- Release `powershell -File scripts/build-wasm.ps1`: pass.
- Complete `npm test`, including 28/28 release-WASM engine smoke checks: pass.
- `npx tsc --noEmit`: pass.
- Full repository pipeline: deliberately not run.
- Rendered/visual review: deliberately not run; it remains Oliver's.
- No new 10,000-run strategy verification was required: the disabled solve
  published no policy, and the enabled strategy is the unchanged previously
  verified bounded incumbent.
