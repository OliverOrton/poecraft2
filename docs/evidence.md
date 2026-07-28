# Evidence

**Status: authoritative index of pinned cases and measured history.** Raw
fixtures and archived reports remain the evidence; this page summarizes and
routes to them rather than duplicating complete reports.

Parent: [Documentation map](README.md)

## Rule And Data Fixtures

- [`fixtures/spec`](../fixtures/spec/) pins small session pools, weights, and
  action invariants used across native and binding checks.
- [Bestiary v1](../fixtures/bestiary/v1/) pins the owner-approved Imprint
  identity, create/restore contract, refusals, price keys, and explicitly
  unsupported conversion recipes.
- [Economy fixtures](../fixtures/economy/) pin price-key identities, Harvest
  recipe vocabulary, provider samples, and the runtime snapshot shape.

These fixtures are mechanic/data evidence. Current explanatory authority lives
in [Mechanics](mechanics/README.md), [Engine](engine/README.md), and
[Economy](economy/README.md).

## Browser Transfer And Solver Lifetime R4

The 2026-07-26
[R4 report](archive/2026-07-26-browser-transfer-lifetime-r4/report.md)
records release-WASM Node-worker evidence for the Calculator's raw
compiled-strategy transfer and scoped Solve lifetime.

- The transferred v1 strategy document was 36,224 bytes and crossed the worker
  response boundary in 38.35 ms.
- Closing the scoped solve handle changed live handles from `5` to `4` and
  selected native live bytes from `15,434,223` to `3,752`.
- WASM linear-memory high-water remained `278,396,928` bytes before and after
  close. That is expected: high-water is not live allocation or
  browser-process RSS.
- The maximum observed native solve step was 56.07 ms.
- The complete web suite and TypeScript check passed against the rebuilt
  2,337,043-byte release artifact, whose SHA-256 is
  `db1789d432ce2c8fe9b5073835b8b941c2bf7602b1e1ceb8e262b9040e87795e`.

This proves the exercised worker handle and selected-owned-byte lifecycle. It
does not establish real-browser/device throughput, total JavaScript/process
memory, or practical 4 GiB availability.

## Solver S7 Acceptance

The [S7 archive](archive/2026-07-solver-s7/README.md) preserves the final plan,
handoff, and engineering report. Native and release-WASM comparison reports
agreed across the permanent corpus and the complete automated suite passed at
closure. The final endgame simulator sample was `0.9942` against the former
`0.995` target; it remains a disclosed numeric miss, not a rewritten pass.

## S8 Frozen Before-State And Review Contracts

The [S8.0 evidence guide](../fixtures/solver-baselines/s8.0/README.md) indexes
the frozen corpus, raw compiled strategies, baseline results, review-projection
schema/examples, action-accounting schema/examples, and trimming-provenance
contract. It represents 60,000 simulator executions across new captures and the
archived endgame sample while preserving disclosed non-convergence, abandoned
captures, and evaluator gaps.

Later immutable comparison files are:

- [S8.2 preservation control](../fixtures/solver-baselines/s8.2/evidence.json),
  including unchanged exact-policy cases and the price-flip pruning proof;
- [S8.3 automatic candidates](../fixtures/solver-baselines/s8.3/evidence.json),
  including analytic blocker `4`, protected-side `23`, and Fracture `23.75`
  selection boundaries; and
- [S8.4 exact accounting](../fixtures/solver-baselines/s8.4/evidence.json),
  including the 10,000-run seed-`20260718` comparison of exact
  `2.608261376220` versus sampled `2.6069` actions/material units.

## S8.4R Product Regression And Repair Evidence

The [S8.4R evidence guide](../fixtures/solver-regressions/s8.4r/v1/evidence/README.md)
owns commands, artifact identity, runner boundaries, and interpretation.
Selected compact records are:

- [R1 before/after](../fixtures/solver-regressions/s8.4r/v1/evidence/r1-after-summary.json):
  bounded diagnostic retention, finalization caps, and selected live-memory
  telemetry. The pre-repair ordinary product diagnostic discovered 63,479
  states from one expansion in about 30 seconds; it is a regression signature,
  not a successful solve.
- [R2 before/after](../fixtures/solver-regressions/s8.4r/v1/evidence/r2-before-after-summary.json):
  state-local automatic construction reduced Conquest/ordinary/advanced
  candidate counts from `1,785/1,318/1,773` to `17/7/5` without reopening the
  old global cross product.
- [R3 Imprint](../fixtures/solver-regressions/s8.4r/v1/evidence/r3-imprint-summary.json):
  automatic state-local Imprint discovery and the focused deterministic sample;
  the separate required 10,000-run verification was deferred.
- [R3F Fracture](../fixtures/solver-regressions/s8.4r/v1/evidence/r3f-implementation-summary.json):
  primitive Fracture product planning and structural `23.75` evidence. Stopped
  normal-cap attempts did not establish Bellman entry.
- [R3A scaling](../fixtures/solver-regressions/s8.4r/v1/evidence/r3a-carrier-scaling-summary.json):
  the retained-kernel repair path, including the roughly 433 MB 223-carrier
  boundary, about 22.7 MB live selected bytes at the corrected 1,024-state
  boundary, and the final 4,096-state result: 11.55 seconds, 38,613 discovered
  states, 67,055 rows, 539,238 transitions, and about 184.13 MB selected bytes.
  The unchanged normal-cap request did not enter Bellman in the 30-second time
  box. No cap was raised.

## Exact Solver State Scaling Q0-Q5

The [solver-scaling v1 guide](../fixtures/solver-scaling/v1/README.md) owns the
pinned manifests, diagnostic cap sweeps, final native reports, reproduction
commands, and native/WASM disclosure. Final acceptance established:

- strict/quotient Chaos value parity within `5.7e-13`, 57,719 exact merges,
  and zero observation mismatches;
- complete two-T1 and three-slot product closure at 189,946 and 169,892 strict
  states, with no unsafe merge under their full admitted action sets;
- exact compiled-strategy evaluation with zero unresolved mass and 10,000
  successful simulator runs for each required product case;
- a 6,391-node, 9,607-edge, 2,667,748-byte exact shared strategy for the
  formerly unrepresentable two-T1 policy; and
- measured smallest sufficient increases to 200,000 state/search limits,
  1,215,000 rows, and 11,000,000 reforge work. Transition, selected-memory,
  compiler, strategy-JSON, and telemetry caps remained unchanged; and
- a green non-visual Node/WASM solve and compile of the worst accepted
  three-slot product case in 142 seconds, with 678 MB selected solver
  ownership and a 1.321 GB WASM heap against the module's 4 GiB maximum.

The product reports are the real all-action acceptance. The Chaos fixture is
only the bounded strict oracle; its compression ratio is not generalized to
the complete product envelopes.

## Exact Solver Action And State Pruning

The 2026-07-21 follow-up kept the Q5 product envelopes, accepted values, and
every resource cap unchanged. Its pinned reports and reproduction commands
are indexed by the
[solver-scaling fixture guide](../fixtures/solver-scaling/v1/README.md).

- The protected-repeat producibility/setup filter reduced the three-slot
  protected admission set from 167,244 to 17,186 candidates and protected
  evaluation time from 17.709 seconds to 0.268 seconds. The 9,191 surviving
  Scour/preparation rows and their exact template reuse remained available.
- An exact constructive goal-finish certificate evaluated the legal
  `bench:EinharMasterColdResist3__` row first. Its executable upper was `3`;
  the strict minimum optimistic lower across the other 31 admitted operators
  was `3.0058720000000001`.
- The accepted three-slot solve therefore fell from 169,892 discovered states,
  56,838 expanded states, and 1,214,860 rows to 2 discovered/expanded states
  and 1 row. The final pinned report measured 1.27 ms of solve time and 179 ms
  end to end including exact compiled evaluation and 10,000-run verification.
- Exact value `3`, the 5-node/5-edge compiled strategy, one-action execution,
  mean cost `3`, 10,000/10,000 successes, and every cap check remained
  unchanged. The first finite constructive upper was recorded at expanded
  state 1.
- The rebuilt WASM module closed the same solve/compile/10,000-run path in
  270 ms, with a 19.55 ms maximum Worker slice and no heap growth from its
  278,396,928-byte starting allocation.
- The natural two-T1 product remained the broad control: no certificate was
  accepted, exact value stayed `230.26738656962243`, 189,946 strict states and
  903,935 rows remained, compilation succeeded, and its 10,000-run
  verification passed. This is product-envelope evidence, not a Chaos-only
  optimization.

The certificate-disabled native oracle test retains the same value and policy,
and a price-bound partial graph is explicitly refused as a price-independent
reprice cache. No approximate state merge or terminal canonicalization was
introduced.

## Real Three-T1 From Scratch Diagnostic

The 2026-07-21
[empty-rare diagnostic](../fixtures/solver-scaling/v1/evidence/real-three-t1-from-scratch-summary.json)
corrects the scope of the seeded value-`3` carrier. With the same three slots,
32-action envelope, prices, exact behavior, and unchanged production caps:

- the first state alone discovered 74,563 states, produced 331,960 retained
  transitions, and consumed 7,863,354 reforge-work units;
- the full bounded run stopped at 200,000 discovered / 55,088 expanded states,
  1,152,570 rows, 9,304,122 reforge work, and 725,411,658 selected-owned bytes;
- the incomplete run did not execute exact quotient refinement:
  `shadow_only=true`, `strict_states=quotient_states=200,000`,
  `shadow_behavioral_classes=55,090`, and
  `shadow_expanded_states_observed=55,088`; these fields do not prove zero
  completed-graph merges;
- focused lower reached `5.3503139241737685`, but the executable upper remained
  infinite after 19 rounds, so there is no exact value or policy; and
- temporary-bench synthesis accounted for 659,762 candidates and 5,133,587
  variants, taking 6.853 seconds inside 13.644 seconds of automatic admission.

The run establishes a cap stop, not acceptance. A state-cap increase alone is
not supported because the row and reforge budgets were already nearly spent.

## Natural Three-T1 Constructive Foothold

The corrected 2026-07-21 target requires ordinary-pool T1 rarity, maximum
life, and life regeneration on an empty rare item-level-86 Dire Pelt. The
complete product envelope contains 23 priced actions and no bench or essence
action. The rejected C4 rare-class prototype was removed before measuring this
target.

- Exact primitive destructive renewal repeats the engine-selected Life +
  Quality fossil and proves start upper `4893.92255176662`, with full retry
  signature equality and real Restart/setup composition.
- Exact progressive fracture selects a six-mod, goal-mask-1 acquisition class
  with probability `0.09005427939272441`, retains useful fractured progress,
  and lowers the standalone start upper to `4116.0146281888519`.
- The bounded unchanged-cap run proves
  `261.05161071365512 <= V* <= 4104.7066630770487`; it stopped at exactly
  11,000,000 reforge-work units after 544 expanded / 127,661 discovered
  states, 9,631 rows, and 430,232 retained transitions.
- Solve time was 50.779 seconds, the longest solve step was 10.623 seconds,
  and selected-owned memory was 89,274,767 live / 155,223,616 peak bytes.
- The cap-3 diagnostic attributes 20.927 seconds to constructive policy,
  0.460 seconds to strict clean-goal cover, and 0.622 seconds to expansion;
  its 10.744-second step was the maximum measured in this session.

The upper is materially tighter than naive renewal, but 541 extra expansions
raised the lower by only `0.11330375945527`. No cap increase is supported.
Rare states with permanently fractured satisfied targets, blocker/group
identity, and Exalt/Harvest/destructive routing are the next exact lower-bound
bottleneck; broader progressive acquisition and post-fracture modes are the
next executable-upper opportunity. The concise record is
[real-three-natural-t1-constructive-summary.json](../fixtures/solver-scaling/v1/evidence/real-three-natural-t1-constructive-summary.json).

## Bounded Policy Results And Benchmarking

The 2026-07-22 B1-B6 boundary is summarized in the tracked
[B6 acceptance record](../fixtures/solver-natural-t1/v1/evidence/b6-acceptance-summary.json).
It preserves separate exact and bounded claims:

- the exactly-once two-natural-T1 oracle report retained exact value
  `230.26738656962243`, 57,182 expanded states, 738,139 rows, 1,165,840
  transitions, and constructive-witness counters 3 syntheses / 323 reuses / 0
  refreshes;
- the acceptance bounded case stopped on its requested absolute gap with
  `3.0923164022485841 <= V* <= 8.0902955687919231`, compiled 947 nodes / 2,831
  edges, and exactly evaluated the returned policy at
  `8.0902955687899443`, within `1.98e-12` of the certified `U`;
- its required simulator corroboration completed 10,000/10,000 successes,
  zero failures and off-policy failures, mean cost `8.0376181789999475`, and a
  passed verification result; and
- the final downstream pass rebuilt native and WASM, passed 513,211 native
  checks, all historical benchmark validation, 13 generator/runner/report
  tests, the complete web/WASM suite, TypeScript typecheck, and the full
  cross-layer pipeline under the standing watchdog.

The bounded case is not an exact-optimality claim. Its exact evaluator proves
the returned policy cost, while `L` and `U` bound the unknown optimum. The
sample is empirical corroboration only. Action-utility and search-cost reports
remain observational; an unused action is never a pruning certificate.

## Durable Anytime Trajectory Baseline

The 2026-07-26
[anytime trajectory baseline](../fixtures/solver-natural-t1/v1/evidence/anytime-trajectory-baseline-summary.json)
pins the first run under the durable partial-observation and v2 experiment-
identity contract.

- All 14 development-role smoke cases completed as analyzable measurements:
  two `bounded_near_optimal` and twelve explicit `refused_state_cap` results.
- Native total wall time ranged from 70.15 to 2,496.58 ms, with a 106.77 ms
  median. Isolated-process wall ranged from 361.70 to 2,815.83 ms, with a
  417.77 ms median.
- Deterministic work ranged from 1 to 3,038 expansions, 10,759 to 25,000
  discovered states, 1 to 19,132 rows, 1 to 82,498 transitions, and 583,205
  to 1,530,480 reforge-work units.
- A real one-second watchdog probe recovered four atomic step-boundary samples,
  remained labelled `watchdog_expired`, counted as administrative censoring,
  and left no process survivor.

Wall figures are machine-, load-, build-, and compiler-bound and do not
survive a hardware or compiler change. This is smoke/development evidence,
not a candidate comparison, frozen-test result, or exact-policy evaluation.
The governing semantics and snapshot limitation live in
[Solver Benchmark Trajectories](solver/benchmarking.md).

## Focused-Round Performance Attribution

The 2026-07-23
[focused-round acceptance summary](../fixtures/solver-scaling/v1/evidence/focused-round-performance-summary.json)
pins fresh uninstrumented/instrumented baselines, the complete seven-tuple
run-local matrix, deterministic hashes, bounds, step distribution, memory,
watchdog facts, and final native/WASM/web checks.

- Diagnostic instrumentation preserved focused counts, statuses, caps,
  certificate bounds, and every transition/policy hash. Instrumented primary
  medians were 0.981x the 2k solve baseline, 0.945x strict extraction, and
  0.979x quotient solve.
- Increasing only the global batch from 256 through 512, 1,024, and 4,096
  reduced focused rounds from 20 to 14, 12, and 8 and policy-evaluation calls
  from 142 to 86, 68, and 33. The corresponding solve medians fell from 22.54
  seconds to 18.48, 17.07, and 14.65 seconds.
- The combined 4,096-member/4,096-batch diagnostic reached 13.15 seconds,
  6 rounds, and 18 policy-evaluation calls. It was not eligible: all variants
  retained an approximately 11.5-second maximum step, and larger batches made
  that spike the p95, violating the fixed 5-second worker-step gate.
- Per-class-cap removal did not reduce rounds, and doubling the lower quota
  made the case slower. Start properness owned 99.93% of measured fallback
  validation component time. Owned-byte ledger work was only 0.38% of solve
  wall time, below its action threshold.

The accepted result is diagnostic instrumentation and no scheduling-default
change. The exact natural two-T1 oracle did not run, no candidate required
exact evaluation or 10,000 simulations, and the economy pipeline was not
touched.

## WASM Solver Progress Accounting

The 2026-07-24
[progress-accounting acceptance](../fixtures/solver-scaling/v1/evidence/wasm-progress-accounting-fix-summary.json)
implements the dominant owner identified by the headless-WASM diagnostic:
per-step solve progress now uses the existing conservative owned-byte ledger,
while full selected-allocation walks remain at exact accounting checkpoints.

The real four-item one-goal/4,000-state replay fell from 257.212 to 7.994
seconds, a 32.17x speedup and 96.89% wall removal. It retained 25,863 steps,
4,000 expanded / 10,759 discovered states, identical lower/upper bounds,
live/peak bytes, status, termination, and transition/policy hashes. The
accepted release binary matches the earlier 4.468-second diagnostic overlay.

Scoped acceptance passed 497 native solve checks and all 27 release-WASM
Node-worker smoke checks. It changed no mechanic, scheduling default, cap,
public ABI, natural-T1 generation, or economy data. This is headless
Node-WASM evidence and does not establish browser/device throughput or explain
any residual browser-only rendering/yield cost.

## Bounded Incumbent Graph Stability

The 2026-07-24
[graph-stability acceptance](../fixtures/solver-scaling/v1/evidence/bounded-incumbent-graph-stability-fix-summary.json)
pins the retained-object lifetime fix for
`bounded incumbent row does not belong to its state`. Bounded incumbents now
capture selected policy operators, row costs, and sparse selected choice
payloads while their same-round graph is current; final output no longer
resolves retained row IDs through a replacement graph or later row pricing.
Graph-sized aligned preference output remains deferred.

The complete native suite passed 513,375 checks, release-WASM worker smoke
passed 27/27 checks, and bounded one-/two-T1 headless replays retained their
pre-fix bounds, status, termination, step counts, and deterministic transition
and policy hashes. The two-T1 replay was a loose bounded target, not the
prohibited exact oracle. No mechanic, schedule, cap/default, ABI, tracked
input, natural-T1 generator, or economy pipeline changed.

## Automatic-Action Constraint-Generation Qualification

The 2026-07-25
[qualification report](archive/2026-07-25-exact-automatic-action-constraint-generation/report.md)
records a negative production decision with a useful causal result.

- A complete lightweight descriptor prototype deferred 210–225
  temporary-bench candidates on each of the four hard natural-T1 cases and
  built zero exact temporary-bench templates.
- The mandatory first setup primitive's current price is an admissible
  descriptor lower envelope because every represented program pays it once
  before any exit or retry and all later costs are nonnegative.
- After automatic deferral, every hard case still expanded only the start
  state and discovered exactly 200,000 states in its first ordinary broad
  reforge. Reforge frontier work ranged from 6,120,150 to 10,145,608; every
  run stopped at `max_discovered_states` with `L=0` and no finite `U`.
- The smoke case exceeded its existing 60-second case watchdog. The outer
  portfolio watchdog recorded no survivor.
- Gate 2 therefore rejected automatic-only integration. Prototype source was
  restored; no production source, WASM, generated output, mechanics, caps,
  ABI, corpus, or economy input changed.

Raw local evidence remains under
`build/exact-automatic-action-constraint-generation/`. The Gate 2 portfolio
ledger SHA-256 is
`aac7a4ae55e08ef795276d1a72944b1c73a991097e7a44661f644c7292dd331b`.

## Broad-Action Separation And Renewal Research

The 2026-07-25
[research report](archive/2026-07-25-broad-action-separation-research/report.md)
records a negative production decision and a reusable fixed-policy result.

- The immediate current action price is an admissible lower envelope for an
  unresolved ordinary broad action, but it excludes zero actions on all four
  hard cases. The smallest renewal incumbent is about `575,497.52`, while the
  largest pinned single primitive price is `14,619`.
- An exact pre-expansion evaluator accumulated final goal mass without
  interning failed successors. Goal-targeted destructive Harvest renewals
  produced finite `c / p` candidates on all four hard cases under a
  100,000,000 diagnostic reforge-work allowance.
- The smallest fixed-policy-only compaction preserved all four probabilities.
  Diagnostic work was 48,409,673 for full-three, 14,815,748 for deep-three,
  6,789,419 for full-four, and 2,698,559 for deep-four. The two three-mod
  cases therefore still exceed the current 11,000,000 limit.
- Goal/failure aggregation is not a general exact Bellman kernel. Under each
  full admitted action set, the 200,000 broad-reforge successors had zero
  exact quotient merges and essentially every successor had a witnessed
  continuation-observation distinction.
- Chaos retains lower envelope `1`, so certified separation still must
  materialize the same first kernel and reaches the 200,000-state wall.
  Diagnostic source was restored; no production behavior or generated output
  changed.

Raw local evidence remains under
`build/broad-action-separation-research/`. The compact full-three and
deep-three ledger SHA-256 values are
`f41fb5574ee469923b976df565a8df8aa1bee27c54b2019dae544dafdfb1e537`
and
`e91ec9a01c8595a4d43e190ab67fd1790ae5c52f72e44dec75a10623541aa55d`.

## Streaming Broad-Lower Rejection And Properness Reuse

The 2026-07-27
[final report](archive/2026-07-27-streaming-broad-lower-fold/report.md) and
[tracked summary](../fixtures/solver-scaling/v1/evidence/streaming-broad-lower-fold-summary.json)
pin one negative candidate and one retained secondary optimization.

- The fixed five-case measurement-only broad-lower candidate published no
  fold. Four hard cases stopped at the existing state-cap path before the
  standalone traversal; the smoke diagnostic crashed. The no-tuning gate
  restored all measurement-only solver source and rejected another
  broad-kernel representation for this roadmap.
- Successful fallback properness validation is now solve-local and versioned
  across immutable policy, goal, economy, vocabulary, graph/mechanics owner,
  row/pricing, and exact transition payload prefixes.
- On the 30M Dire Pelt owner, start-properness scans fell from 17 to 1 plus 16
  validated reuses. Validation fell from 8,902.739 ms to 557.833 ms and solve
  wall from 36,391.471 ms to 28,268.875 ms, a 22.3% reduction.
- Bounds, termination, 95,118 discovered / 2,301 expanded states, 61,661
  rows, 873,813 transitions, 18,349,624 reforge work, and transition/policy
  hashes were identical. The other four paired cases retained the same
  deterministic evidence.
- Final 11M benchmarking produced five reports without timeout or survivor;
  the smoke step error preserved its abandoned snapshot at exactly 11M
  reforge work. Native focused tests passed 518/518, release-WASM worker smoke
  passed 27/27, web tests passed, and TypeScript no-emit passed.

The wall comparison is machine/compiler-bound. The retained optimization
removes duplicate post-incumbent work and does not improve hard pre-bound
failure.

## Protected-Core Cleanup Ceiling

The 2026-07-27
[final report](archive/2026-07-27-protected-core-ceiling-census/report.md) and
[tracked summary](../fixtures/solver-natural-t1/v1/evidence/protected-core-ceiling-summary.json)
pin a cleanup-specific rejection and a separate unselected research signal.

- Four unchanged 11M/200k hard cases exposed 199,952 to 199,969 unique
  successors before the first Chaos row hit the state cap.
- Impossible free side erasure reduced those observed sets to 221–893 prefix
  cores and 817–3,601 suffix cores: `55.5x` to `904.8x` compression.
- The broadest cleanup-relevant population—nonterminal states with at least
  two satisfied goals on one protected side—was only 275 to 11,352 states,
  or `0.14%` to `5.68%` of the observed prefix. Perfect free deletion of that
  entire population still cannot create material first-row headroom.
- Every projection used collision-checked full projected-state equality and
  recorded zero hash collisions. The stream remains cap-censored and the
  signatures have no equivalence, bound, or pruning authority.
- Status, state/graph/work counts, reforge work, and transition/policy hashes
  matched the prior frozen baseline on all four cases.

Measurement-only engine source was restored. The broader all-state signal may
motivate a future exact action-local factorization study, but it does not
select cleanup or supply a completed all-action quotient result for the
cap-censored hard cases.

## Action-Local Side Factorization Rejection

The 2026-07-27
[final report](archive/2026-07-27-action-local-side-factorization/report.md)
and
[tracked summary](../fixtures/solver-scaling/v1/evidence/action-local-side-factorization-summary.json)
pin the exact synthetic rejection selected from the protected-core ceiling.

- Empty-start Chaos had 41 joint outcomes across six fixed side-count cells.
  The support was rectangular, but the two-prefix/two-suffix cell had maximum
  2x2 minor `4.7521644443241428e-6` and maximum absolute factorization error
  `0.00013338996237286091`.
- An ordinary starting suffix was wiped and matched the empty-start
  distribution exactly. Marking the same heavy suffix fractured preserved it,
  changed the exact distribution, and reduced this tiny control to 28 joint
  outcomes.
- Making the heavy suffix weight uniform reduced every minor to zero within
  floating-point noise. This pins the coupling to identity-dependent
  remaining pool weight under sequential draws from the combined pool.
- Conditioning on final remaining prefix/suffix weights restored rank one,
  but split six cells into thirteen and required 31 prefix plus 17 suffix
  marginal identities for only 41 joint outcomes. An online evaluator would
  require the same coupling after every pick.

The exploratory probe passed 120,535 focused calculation checks and was
restored. A narrow regression remains for the independent goal contract: a
fractured goal plus unrelated fractured junk still satisfies the permissive
goal predicate.

## Exact Quotient Audit

The 2026-07-27
[final report](archive/2026-07-27-exact-quotient-audit/report.md) and
[tracked summary](../fixtures/solver-scaling/v1/evidence/exact-quotient-audit-summary.json)
pin the completed- versus incomplete-graph distinction.

- The fast completed alt-spam oracle reduces 10 strict states to 3 exact
  quotient classes with 7 merges, value/action parity, a non-identity
  representative map, and zero observation-signature mismatches.
- The completed observe-then-decide witness has 8 literal Unveil offers. Two
  projected successor classes contain multiple literal offers, with 7 in the
  largest, and compilation emits every literal offer as both a condition and
  operation. Raw `choice.mod_id` remains required.
- The cap-stopped regression retains 10 strict working states, reports 3
  literal shadow classes, sets `shadow_only=true`, applies zero exact merges,
  and publishes no representative map.
- Historical real-three-T1 evidence is now explicit: the one-expansion
  74,563-state report had 2 shadow classes with 1 expanded state; the
  production 200,000-state report had 55,090 shadow classes with 55,088
  expanded states. Neither ran completed quotient refinement.
- Exact action-relative reforge reuse was already present: action plus complete
  preserved base shares the roll distribution, sparse row payload, successor
  envelope, and fringe enqueue. It does not eliminate first-carrier
  materialization.

The native fallback build passed. Focused solver-solve tests passed 532/532 and
solver-compile tests passed 359/359. No mechanic, algorithm, cap, ABI, artifact,
binding, WASM, web, or product behavior changed.

## Complete First-Frontier Successor Census

The 2026-07-27
[final report](archive/2026-07-27-true-successor-frontier-census/report.md)
and
[tracked summary](../fixtures/solver-natural-t1/v1/evidence/true-successor-frontier-census-summary.json)
replace 200,000-state ceiling guesses with complete exact first-Chaos rows.

- Exact support is 3,204,323 for full-three, 712,877 for deep-three, and
  222,580 for both four-mod cases.
- The old prefix covered only 6.24%, 28.05%, and 89.84% of those supports.
- Complete reforge work is 151,348,836, 28,155,816, and 6,797,580
  respectively.
- The support occupies 98.71% to 99.65% of the collision-checked
  prefix-payload × suffix-payload product. Every one-sided and goal-status
  class was already present in the 200,000-state prefix.
- Complete cleanup-pattern counts remain insufficient. The closest case,
  full-four, needs 22,581 states removed to fit one carrier plus its Chaos
  support under the cap; impossible free deletion of every nonterminal state
  with at least two goals on one side removes only 21,887, leaving a
  694-state shortfall.
- Two repetitions per case produced identical deterministic census objects,
  including probability sums, work, histograms, projection classes, selected
  bytes, and ordered support/probability hashes.

Measurement-only source was restored. The census selects no production
architecture: support is a dense joint side product, and the preceding
factorization experiment already rejected a simple rank-one probability
representation.

## Engine And WASM Evidence Boundaries

The [engine performance archive](archive/2026-06-engine-performance/README.md)
contains point-in-time native hot-path audits and a decision menu. Later Phase
14 and S7 improvements superseded its work-order status.

[WASM](engine/wasm.md) distinguishes source-inspected capability, Node worker
test evidence, native-only measurements, and real-browser unknowns. Native
throughput or memory observations must not be presented as browser/WASM proof.
