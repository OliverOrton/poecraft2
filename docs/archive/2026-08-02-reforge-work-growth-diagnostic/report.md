# Exact Reforge-Work Growth Diagnostic Report

**Status: completed slope measurement; no production behavior change.**

Measured on `codex/reforge-work-growth-diagnostic` from
`41d6a243947b0205a6e29c6373e79b21b0d8292a`, 2026-08-02. The final archive
commit is reported in the final handoff.

## Outcome

Eager exact candidate certification is an approximately linear
pre-partition grind after a short startup, not a falling-cost path toward
closure. From 20M to 50M and 50M to 100M, every additional completed kernel
produced exactly 172,596 transitions. Marginal work was 2.000M and 2.174M per
kernel respectively. The modest 8.70% increase is consistent with a linear
row cost plus different amounts of censored work in the row that hit the cap;
it is not evidence of falling cost.

At 100M the exact phase had completed 40 kernels and reported 42 exact states
and 6,903,840 transitions. It still had zero quotient classes, zero partition
rounds, zero certificates, and no policy. Higher work bought more raw exact
rows but no quotient-level or executable progress.

The archived conclusion is confirmed and sharpened: changing the product cap
would not repair the ordering problem. Competitive lazy alternative
certification is now the selected next boundary.

## Work charge and cap semantics

`CalcContext::consume_reforge_work` saturates the context telemetry at its cap
and throws `max_reforge_work` before an over-cap charge is applied. Significant
charges are:

| Owner | Charge | Primary physical work |
| --- | --- | --- |
| `solver_reforge.cpp` raw-weight construction | one per physical pool entry while identity is deferred | raw identity entries |
| deferred identity expansion | one per internal and leaf node | complete raw identity expansion tree |
| Harvest guaranteed first pick | bucket count before the initial scan | roll buckets |
| ordered roll frontier | `1 + bucket_count` per frontier state | frontier nodes and remaining-weight scans |
| `solver_options_automatic.cpp` | propagates nested context deltas | automatic-option child discovery and comparisons |

Emitted transitions correlate with this work but are not the charge authority.
The direct cause is frontier and raw-identity expansion inside an exact compact
action row.

`solver_solve_finish.cpp` subtracts completed coarse reforge work from the
configured total before exact publication. The streamed quotient then loops
over its growing locator worklist. For every carrier it completes compact
rows, interns every successor locator, and only after the entire locator list
closes does it call the shared partition. Affected carriers attempt every
already-admitted operator. Consequently first partition installation requires
complete pre-partition locator closure, not merely one carrier, one selected
row, or the predicted final quotient population.

## Initial models frozen before Run A

The preserved 20M prefix supplied 5,922,368 exact work, four reported exact
states, two completed kernels, and 345,192 transitions. The naive averages
were 1,480,592 work/state, 2,961,184 work/kernel, and 17.16 work/transition.
They were treated as hypotheses, not constants.

| Model | Initial requirement | 50M / 100M / 200M plausibility |
| --- | ---: | --- |
| 13,076 independent carrier-like units at 1,480,592 | 19.360B exact work | no / no / no |
| Archived 10,466 kernels at 2,961,184 | 30.991B | no / no / no |
| Archived 423,756 transitions at 17.16 | 7.272M | yes / yes / yes, but expected to undercount current candidate rows |
| One 27-row affected carrier at 2,961,184 per row | 79.952M | no full partition at any diagnostic cap; only one carrier slice |
| Sharply falling early cost | unknown | 100M or 200M only if Run A showed the fall |

The required naive projection is valid only if a final quotient cell maps to
one independently expensive pre-partition carrier unit. The implementation
does not prove that relationship.

The smallest evidence-backed population model was also deliberately
separated:

- carriers: four observed; 183,062 archived selected-policy carriers are a
  planning proxy, not a proved current population;
- unique kernels: two observed; 10,466 archived selected-policy kernels are a
  lower-order planning proxy because eager alternatives add rows; and
- candidate rows: two completed-row/kernel proxies and at least a third begun;
  the 27-action vocabulary bounds rows per affected carrier, but the number of
  affected successor carriers was unknown.

No final quotient-cell count was substituted for this source population.

## Coarse cap independence

The coarse phase is exactly identical at 20M, 50M, and 100M:

| Field | All three runs |
| --- | ---: |
| Discovered / expanded / frontier | `171 / 171 / 0` |
| Rows / transitions | `7,107 / 4,292` |
| Coarse reforge work | `14,077,632` |
| Lower / executable incumbent | `752.9009075663787 / 3323.6694369790375` |
| Transition hash | `4f26d305a908c59f` |
| Policy hash | `3e384d3eb52f9ab7` |
| Registry / candidate actions | `178 / 27` |
| Product action-order SHA-256 | `63bf653ba73e7fae48dc0a68815a558268d95b09c64d14937ef48072d0d60df5` |

Simple total-minus-coarse subtraction is therefore valid for this measured
range.

## Per-run results

| Field | Preserved 20M | Run A 50M | Run B 100M |
| --- | ---: | ---: | ---: |
| Exact allowance / consumed work | 5,922,368 | 35,922,368 | 85,922,368 |
| Reported exact states | 4 | 19 | 42 |
| Completed exact rows / kernel proxy | 2 | 17 | 40 |
| Row attempts begun, lower bound | 3 | 18 | 41 |
| Kernel cache hits | 0 | 0 | 0 |
| Exact transitions | 345,192 | 2,934,132 | 6,903,840 |
| Classes / partition rounds / certificates | `0 / 0 / 0` | `0 / 0 / 0` | `0 / 0 / 0` |
| Total report wall | 5.787 s | 14.566 s | 25.074 s |
| Native peak owned | 375,483,167 | 375,483,167 | 375,483,167 |
| Native live owned | 305,293,988 | 305,293,990 | 305,293,990 |
| Working set after | 386,867,200 | 413,233,152 | 388,255,744 |
| Result | work cap, no policy | work cap, no policy | work cap, no policy |
| Reference calls | 0 | 0 | 0 |

Candidate-specific scheduled/evaluated counters serialize as zero because the
exception occurs before final Bellman telemetry assignment. They are not
interpreted as zero attempted candidates. The source-authoritative completed
row proxy is `strict_kernels_built`, incremented once after a compact row
finishes; the stopped next attempt supplies the begun lower bound.

## Marginal slopes

| Slope | 20M to 50M | 50M to 100M | Classification |
| --- | ---: | ---: | --- |
| Work / reported state | 2,000,000 | 2,173,913 | mildly rising |
| Work / completed kernel or row | 2,000,000 | 2,173,913 | approximately constant, 8.70% upward endpoint change |
| Transitions / completed kernel | 172,596 | 172,596 | exactly constant |
| Work / transition | 11.588 | 12.595 | approximately constant, mildly rising |
| Peak bytes / work | 0 | 0 | flat |
| Marginal total time | 0.293 us/work | 0.210 us/work | falling |

Completed exact rows/kernels are the best reported denominator. Roll-frontier
and raw identity tree nodes are the underlying causal denominator, but the
current report does not separate them inside strict publication. A least-
squares fit over all three points gives 2,111,010 work per completed kernel
and a 1,072,504-work intercept.

## Revised requirements

The fitted row model places one full 27-row affected carrier near 58.070M exact
work, or 72.147M including the invariant coarse phase. Forty rows completed at
100M, so at least one carrier slice must have completed: no carrier can emit
more than 27 admitted semantic rows plus at most one distinct current row.
That is real source progress, but it did not create a partition.

The only binding first-partition facts are therefore lower bounds:

- more than 100M total work;
- more than 42 reported exact states;
- more than 40 completed kernels; and
- more than 6,903,840 exact transitions.

If the archived 10,466 selected-policy kernels remain a lower-order proxy, the
revised linear fit projects about 22.11B total work before accounting for
eager extra alternatives, partitioning, Bellman evaluation, properness,
compilation, or verification. This is a planning model, not proof that the old
kernel population equals the new locator closure.

The required 13,076-unit naive projection is 19.360B at the preserved
1,480,592 rate and 27.604B at the revised kernel slope. Both are conditional
because quotient cells are not one-to-one with source carriers or kernels.
Applying the same slope to the archived 183,062 carrier proxy yields 386.45B,
which is deliberately reported only as the invalid one-expensive-unit-per-
carrier model.

At the latest 0.210 microseconds/work slope, 19.36B to 22.11B work would take
roughly 68 to 77 minutes before later publication phases, already far beyond
the unchanged 900-second product watchdog. First executable upper is strictly
later than first partition, so no smaller executable projection is justified.

## Run C decision

The 200M run was not performed. Run B already materially distinguished the
falling hypothesis: transitions per completed kernel were exactly constant,
work per kernel did not fall, peak memory remained safe, and no partition or
selected-closure progress appeared. Another point was not needed to classify
the path as approximately linear, and the directive forbade spending 200M
merely because it was authorized.

## Architecture decision

Additional work creates raw rows and transition expansion but no usable
quotient or executable progress through 100M. The archived statement that a
larger cap would evade the architecture is therefore confirmed, with a more
precise basis: this is a measured linear pre-partition ordering problem rather
than a memory shortage or an exceptional first-kernel spike.

The selected follow-on was
[Competitive Lazy Alternative Certification](../2026-08-02-competitive-lazy-alternative-certification/README.md):

1. certify selected-policy closure before eager alternative closure;
2. preserve every admitted uncertified alternative as an explicit unresolved
   lower-only proof obligation;
3. certify alternatives transactionally only when Bellman competitiveness or
   a counterexample requires them; and
4. retain the complete vocabulary and require current, closed, lumpable,
   proper proof before any executable upper is published.

## Evidence and acceptance

Tracked evidence is
`fixtures/solver-reliability/v1/evidence/reforge-work-growth-diagnostic.json`.
Ignored raw reports are:

- Run A:
  `build/acceptance/reforge-work-growth-diagnostic/run-a-50m/report.json`,
  SHA-256 `3a05a4f829a621e81e0b27c85c392d12e56e1fce800dff8e9bf99ba2afcd4264`;
- Run B:
  `build/acceptance/reforge-work-growth-diagnostic/run-b-100m/report.json`,
  SHA-256 `a92717eaa6049fc76c11602c00bbf18476e6ceea063a84ceeec2ee2d8b6c6afb`.

Both temporary cases validated and compared semantically equal to the
canonical fixture after normalizing only `max_reforge_work`. The temporary
runtime copies were removed after measurement. The canonical fixture remained
SHA-256
`1dc33be7a08e1049b22383886e8c6d00a8ed78c7eea8628f5de0e80f2c3cb3be`.

No source or telemetry contract was retained, so the full product pipeline
was not run. The direct diagnostic runs and benchmark validation are the
selected acceptance. No rendered or browser review was performed.
