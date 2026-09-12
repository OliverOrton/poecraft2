# Authorized native resource policy

These are **proposed effective resource profiles**, not already supported command-line flags or a corpus schema. Resolve them through the existing native options, case derivation, worker and reservation owners; add only missing typed controls. Log the actual effective limits and their parent/child owner. Do not pass `resource_profiles.json` directly to the existing runner.

Oliver's current request explicitly authorizes cap changes. The following choices are the starting policy for this programme, not permanent global defaults. Existing product/browser cases remain immutable reference controls.

## Profile table

| Effective control | Original preservation | Native-wide v1 | Targeted-deep v1 |
|---|---:|---:|---:|
| Aggregate live solver-owned allowance | Existing case, usually 1 GiB | 8 GiB | 16 GiB |
| Candidate checker-owned ceiling | Existing path (currently at most 1 GiB) | 4 GiB, also bounded by aggregate remaining allowance | 8 GiB, also bounded by aggregate remaining allowance |
| Candidate exact-state ceiling | Current inherited limit (often 200,000) | 2,000,000 | 4,000,000 |
| Candidate state/action or evaluator-pair ceiling | Existing | 10,000,000 | 20,000,000 |
| Candidate exact-transition ceiling | Existing | 40,000,000 | 80,000,000 |
| Ordinary discovered/expanded/strict-state research ceiling, if that owner blocks the selected work | Existing (often 200,000) | Up to 1,000,000 | Up to 2,000,000 |
| Ordinary state/action row ceiling, if binding | Existing | Up to 5,000,000 | Up to 10,000,000 |
| Ordinary stored-transition ceiling, if binding | Existing | Up to 40,000,000 | Up to 80,000,000 |
| Shared logical reforge work | Existing case (50M or 100M) | 200,000,000 | 500,000,000 |
| Final independent evaluator memory | Existing, often 1 GiB | 4 GiB | 8 GiB |
| Final evaluator states / pairs / transitions | Existing | 2M / 10M / 40M | 4M / 20M / 80M |
| Requested bounded finish | Existing 60s or 240s | 300s | 600s |
| Native watchdog | Existing 90s or 300s | 420s | 720s |
| Outer process cleanup safeguard | Existing | 450s | 750s |
| Public work step / numerical tolerances | Existing | Initially unchanged | Initially unchanged |
| Host process reservation, including known overlap and overhead | Existing safe declaration | At least 14 GiB or larger measured safe bound | At least 28 GiB or larger measured safe bound |

GiB means 1,073,741,824 bytes. State and pair limits are separate. A larger solver state count does not imply an evaluator inherited the larger allowance; verify the resolved call. The final evaluator may overlap retained native state, so its separate ceiling must be included in the host reservation. Candidate verification is not an additional uncharged 4/8 GiB pool.

## Selection sequence

1. Keep a current-main original-profile control and the stronger current Ring upper. Use existing evidence where identical. Do not rerun the whole core to rediscover it.
2. Begin with the candidate checker that actually stopped the gated experiment. When feasible, hold the proposed controller fixed and change only its declared checker allowance. A small typed independence between search-state and candidate-checker limits is permitted and directly useful.
3. Use Native-wide for the selected complete run after recording its resolved profile. Ordinary caps labelled “if binding” need not all be increased to examine an isolated checker. The actual frozen profile, not this maximum table, determines the comparison.
4. Use Targeted-deep only after a named wide-profile barrier, evidence of useful advancing work, and safe host admission. It is not a mandatory ten-minute sweep over the whole corpus. One or two relevant candidate/run comparisons can answer the question.
5. A finite computed cost that is worse does not improve merely by giving the same fixed controller more time. Stop that numerical evaluation and select a different continuation or action.

This policy intentionally allows moving beyond the old arbitrary 200k/1-GiB constraints. It also keeps a bounded response to an intrinsically enormous representation. Do not double every cap repeatedly or remove watchdogs.

## Host and ownership safeguards

The predecessor recorded about 63.7 GiB physical memory and 41.1 GiB available at one observation. That is evidence of a plausible native research machine, not today's guaranteed headroom. Inspect the actual local host before launch and between large attempts.

Use one timed native run at a time. Stop competing run-owned CPU/memory jobs before timing. Keep at least the greater of 8 GiB or 20% of physical memory available outside the declared process reservation; increase this safety margin if current OS commit, other applications or observed pressure warrants it. The reservation must cover retained solver/child overlap, exact evaluator, parsed data, graph serialization and instrumentation. If the deep profile does not fit, lower its allocation or continue an independent useful branch rather than forcing the host into paging.

Do not infer process RSS from a solver-owned counter. Report native retained/peak estimates, requested allocation and measured process memory separately. Validate byte arithmetic and integer conversions. Large limits must not preallocate the full theoretical capacity unnecessarily. Shared reforge work remains cumulatively debited, including interrupted candidate evaluation.

`solver_solve_return_bridge.cpp` currently has a hard candidate evaluator 1-GiB constant and derives evaluator states from the search limit. A research override must reach those exact assignments, retain safe defaults when omitted, and remain within aggregate remaining memory. Extend the existing typed options/diagnostic/worker owner, not a new supervisor or arbitrary command passthrough. Avoid an ABI change when an existing native-private route suffices.

Do not reduce precision, enlarge numerical acceptance tolerances, drop rare edges, remove observations, reinterpret unknown entries, or change target/action semantics to fit a budget. These are validity conditions, not resource caps.

## Comparability and useful outcomes

Keep three labels distinct:

- **Capacity effect:** same implementation and target, more resources.
- **Algorithmic effect:** baseline/treatment at the same actual resources and decision controls.
- **Product preservation:** original short/1-GiB/browser requests still return their compatible verified artifacts.

The previous stronger Ring capacity result is not the latest high-water mark. Current Ring is 149,977.25092497544 even on its original profile. At a new capacity, first check the current-main baseline; the acceptance target is 75% of the better compatible current reference or measured baseline, not 75% of a superseded expensive fallback.

The new goals remain useful returned strategies and stronger complete proofs, not maxed-out counters. A bound or graph known only after independent evaluation cannot be backdated to a trajectory sample. If a raw policy was already produced but a checker fails at its cap, mark the value unknown; do not infer a poor policy from a verification refusal.

## Simulation caps are a separate question

The current Ring's 22/1000 action-limit stops are not a native nontermination proof or an all-success pass. Preserve them. A new final controller can be checked at the old limits for comparability. An additional extended-limit run, if actually needed, is separately identified and retains failures in the denominator. Do not spend hundreds of thousands of simulated actions merely to compensate for missing exact evaluation. Sampling is not routine for cap resolution, layout diagnostics or unchanged policies.
