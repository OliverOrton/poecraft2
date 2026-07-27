# Exact Solver State Scaling v1

This fixture is the pinned Q0-Q5 acceptance corpus for exact solver state
scaling. It uses the compiled artifact and Mirage economy identities recorded
in the manifests. The [acceptance manifest](acceptance-manifest.json) contains
the two bounded strict/quotient controls and the two complete product cases;
the [cap-sweep manifest](cap-sweep-manifest.json) preserves the pre-change Q4
capacity probes.

The separately selected
[real three-T1 diagnostic](real-three-t1-diagnostic-manifest.json) starts from
an empty rare item. It is not part of the accepted Q5 corpus and must not be
confused with the seeded three-slot carrier below.

## Exactness contract

- Every admitted action participates in behavioral equivalence.
- Strict `CalcContext` states are the oracle; collision equality is checked.
- Unknown observations or kernel mismatches retain strict behavior.
- Approximate compaction is forbidden.
- Compiler routing and evaluator routing remain ordinary exact v1 strategy
  behavior.

## Final native results

| Case | Exact value | Strict / quotient | Expanded | Rows | Transitions | Reforge work | Peak selected bytes | Compiled nodes / edges / JSON | Verification |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| two-T1 Chaos strict oracle | 385.1370569770711 | 57,722 / 3 | 57,722 | 110,191 | 57,721 | 0 | see report | diagnostic only | exact oracle |
| two-T1 Chaos quotient | 385.13705697707053 | 57,722 / 3 | 3 | 3 | 3 | 0 | see report | 5 / 5 / 1,258 | value delta `5.7e-13` |
| two-T1 complete product | 230.26738656962243 | 189,946 / 189,946 | 57,233 | 903,935 | 1,532,261 | 8,235,756 | 620,729,489 | 6,391 / 9,607 / 2,667,748 | 10,000/10,000; mean 230.38635779360044 |
| three-slot complete product | 3 | 169,892 / 169,892 | 56,838 | 1,214,860 | 1,846,444 | 10,982,496 | 749,827,169 | 5 / 5 / 66,749 | 10,000/10,000; mean 3 |

## 2026-07-21 exact action/state pruning follow-up

The Q5 table above remains the before baseline and cap decision. The follow-up
did not change either product envelope, either accepted value, or any cap.

| Case | Exact value | Certificate | Discovered / expanded | Rows | Solve time | Compiled | Verification |
| --- | ---: | --- | ---: | ---: | ---: | ---: | --- |
| two-T1 complete product | 230.26738656962243 | none accepted | 189,946 / 57,233 | 903,935 | 86.879 s | 6,391 nodes | 10,000 runs passed; mean 230.38635779360044 |
| three-slot complete product | 3 | bench finish, 31 strict operator dominances | 2 / 2 | 1 | 1.27 ms | 5 nodes | 10,000/10,000; mean 3 |

For the three-slot carrier, the executable bench row supplied upper bound `3`
at expanded state 1. The optimistic engine-owned goal-production cover gave
the cheapest competing operator a lower bound of `3.0058720000000001`, so the
solver did not materialize its stochastic fringe. This is a price-bound exact
dominance certificate, not state similarity or approximate compaction. Its
partial transition graph is never retained for a later price-only solve.

The protected-repeat prefilter is separately evidenced by the intermediate
complete three-slot run: protected candidates fell from 167,244 to 17,186 and
protected evaluation from 17.709 seconds to 0.268 seconds, while value,
compiled policy, and verification remained exact.

The rebuilt-WASM follow-up report closed the same three-slot case in 270 ms
end to end: 36 ms solve, 2 states, 1 row, 5 compiled nodes, and 10,000/10,000
verification. Its largest Worker slice was 19.55 ms, process RSS peaked at
373,694,464 bytes, and the 278,396,928-byte WASM heap did not grow during the
run.

## Historical crafted-finish three-slot diagnostic

The accepted value-`3` carrier above already owns T1 fractured life and T1
rarity. On 2026-07-21 the separate empty-rare case retained the same goal,
32-action product envelope, economy, exactness rules, and production caps.

Its first expansion discovered 74,563 strict states and 331,960 retained
transitions from 34 rows in 582 ms. It consumed 7,863,354 of the 11,000,000
reforge-work allowance but accepted no constructive certificate.

The production-cap run stopped after 40.7 seconds at exactly 200,000
discovered states, with 55,088 expanded, 144,912 still on the frontier,
1,152,570 rows, 9,304,122 reforge work, and 725,411,658 selected-owned bytes.
The exact quotient found no merge. Focused expansion completed 19 rounds and
raised the lower bound to `5.3503139241737685`; the executable upper remained
infinite, so no value, policy, compilation, or verification is claimed.

Automatic admission consumed 13.644 seconds. Temporary-bench synthesis was
the largest named component: 659,762 carrier candidates expanded into
5,133,587 variants, 31,582 effect classes, and 21,764 unique exact templates;
609,918 later candidates reused those templates. This identifies exact
carrier-signature reuse/early rejection and a compositional executable policy
upper as distinct future targets. Raising only the state cap would immediately
approach the row and reforge caps and is not justified by this evidence.

The concise record is
[real-three-t1-from-scratch-summary.json](evidence/real-three-t1-from-scratch-summary.json);
the linked raw reports preserve the bounded first expansion and unchanged-cap
run.

## Natural three-T1 constructive follow-up

The corrected 2026-07-21 target replaces crafted cold resistance with ordinary
pool T1 life regeneration: `ItemFoundRarityIncreasePrefix3`, `IncreasedLife9`,
and `LifeRegeneration9` on an empty rare item-level-86 Dire Pelt. Its complete
Calculator-derived envelope contains 23 priced actions, no bench actions, and
no essences. The three specifications in
[real-three-t1-diagnostic-manifest.json](real-three-t1-diagnostic-manifest.json)
are now this natural target; the older 32-action records above remain
historical evidence only.

Generic exact destructive renewal supplies a Life + Quality fossil bootstrap
upper of `4893.92255176662`. Generic progressive fracture lowers its standalone
start upper to `4116.0146281888519`. The one bounded production-cap run stopped
on `max_reforge_work` and proves
`261.05161071365512 <= V* <= 4104.7066630770487` with 127,661 discovered / 544
expanded states, 9,631 rows, 430,232 retained transitions, and 155,223,616
peak selected-owned bytes. The longest solve step was 10.623 seconds.

This is a finite executable foothold, not exact convergence or compiled-policy
acceptance. The exact measurement and next structural decision are preserved
in
[real-three-natural-t1-constructive-summary.json](evidence/real-three-natural-t1-constructive-summary.json).

## Focused-round performance attribution

The 2026-07-23
[focused-round performance summary](evidence/focused-round-performance-summary.json)
records fresh uninstrumented and instrumented five-run baselines plus the
predeclared seven-tuple run-local scheduling matrix. The diagnostics preserve
the 2k case's `153.1270669513794 <= V* <= 243.25209004029114` certificate,
including known exact value `230.26738656962243`, and all baseline
transition/policy hashes.

Global batch size is causal for repeated work: increasing it from 256 to 4,096
reduced median focused rounds from 20 to 8, policy-evaluation calls from 142 to
33, and solve time from 22.54 seconds to 14.65 seconds. No tuple was accepted,
because every variant retained an approximately 11.5-second maximum solve
step; larger batches also made that spike the p95. The tracked result adds
attribution telemetry without changing the private 64-member, 256-batch,
64-lower-quota defaults.

The Chaos case proves the quotient can remove unobservable strict identity.
The complete product cases deliberately merge no states: their larger action
sets observe all strict differences. Their closure comes from incremental
owned-byte accounting, exact kernel sharing, finite focused bounds, and exact
compiler/evaluator structure—not Chaos-only assumptions.

The two-T1 exact compiled evaluator converged in 31.354 seconds with success
probability `1.0000000000011351`, zero unresolved mass, and
`89.904730708026662` expected actions. Its 10,000-run cost delta was
`0.11897122397800786` (`0.00051666554152702973` relative). The three-slot
compiled evaluator is deterministic: success `1`, unresolved `0`, one action,
and exact cost `3`.

The worst-case non-visual Node/WASM run of the three-slot case converged and
compiled in about 143 seconds. Its selected solver ownership peaked at
`677,991,208` bytes, the WASM heap ended at `1,320,878,080` bytes against the
module's 4 GiB maximum, and process RSS peaked at `1,207,738,368` bytes. One
exact state-local automatic-admission call occupied a 17.3-second Worker
slice, so this case records a 20-second rounded measurement allowance. That
allowance is not a native solver resource-cap increase and the work remains
off the browser main thread.

## Q4 capacity decision

The unchanged 100,000-state limit did not close the motivating product. The
200,000 diagnostic closed it at 189,946 strict states, so production
state/discovered/expanded limits use 200,000. The three-slot closure required
1,214,860 rows and 10,982,496 reforge work, so the smallest rounded sufficient
limits are 1,215,000 rows and 11,000,000 reforge work. The 10,000,000
transition, 1 GiB selected-owned, 100,000-node, 400,000-edge, 64 MiB strategy,
and 1 MiB telemetry caps remain unchanged.

On the two-T1 product, per-state preparation byte audits fell from the
22.47-second baseline to 7.3 ms, or 0.04% of the 18.422-second expansion. Full
audits remain periodic and phase-boundary assertions; an undercount is fatal.
Exact kernel reuse recorded zero observation mismatches.

## Reproduction

After `powershell -File scripts/build.ps1`, the native evidence commands are:

```powershell
build/engine/poecraft_solver_benchmark.exe --artifact data/compiled/current --corpus fixtures/solver-scaling/v1/acceptance-manifest.json --case solver-scaling-dire-pelt-two-t1-product --output fixtures/solver-scaling/v1/evidence/q5-two-t1-product.json --strategy-output build/performance/q5-strategies --exact-strategy-evaluation --exact-strategy-evaluation-time-limit-seconds 120 --verification-runs 10000 --progress
build/engine/poecraft_solver_benchmark.exe --artifact data/compiled/current --corpus fixtures/solver-scaling/v1/acceptance-manifest.json --case solver-scaling-dire-pelt-three-t1-product --output fixtures/solver-scaling/v1/evidence/q5-three-t1-product.json --strategy-output build/performance/q5-strategies --exact-strategy-evaluation --exact-strategy-evaluation-time-limit-seconds 120 --verification-runs 10000 --progress
```

The follow-up reports use the same commands and are stored as
`action-state-two-t1-product.json` and `action-state-three-t1-product.json` in
the evidence directory; the rebuilt-module run is
`action-state-three-t1-product-wasm.json`. An `exact_abstract` optimization status satisfies an
`exact_supported_priced_subset` corpus expectation because it is the stronger
result: the exact proof did not reach any unsupported or unpriced operator.

The strict and quotient reports use the same command without exact evaluation
or simulator verification and select their corresponding case IDs.

## Action-local side-factorization falsification

The
[tracked summary](evidence/action-local-side-factorization-summary.json)
records the exact synthetic Chaos counterexample selected after the
protected-core census. Fixed prefix/suffix counts do not make side identities
probabilistically independent: the sequential combined-pool denominator
couples their remaining weights. The ordinary/fractured suffix controls and
one final-weight refinement are included. No production evaluator or cap
changed.

The main product timing table is native Windows evidence, not a
rendered-browser claim. The tracked WASM was rebuilt from the same sources;
the three-slot Node/Worker report above plus worker smoke, cancellation,
board-valid compilation, `npm test`, and `npx tsc --noEmit` are non-visual
WASM evidence. Oliver owns any later rendered UI review.
