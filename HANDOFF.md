# Session Handoff

**Status: versioned reforge resource accounting Gates 0-1 are complete. Gate
2, observational baseline and matched V1/V2/V3 calibration, is active.**

Current plan:
[Versioned Reforge Resource Accounting And V3 Production Qualification](docs/active/versioned-reforge-resource-accounting-and-v3-production-qualification.md).

Latest completed milestone:
[Certified Fallback Publication And Final-Depth Reforge Accumulation](docs/archive/2026-08-03-certified-fallback-and-terminal-reforge-factorization/README.md).

Preceding completed milestone:
[Selected-Closure Scaling And Exact Broad-Row Projection](docs/archive/2026-08-03-selected-closure-broad-row-scaling/README.md).

Earlier completed milestone:
[Competitive Lazy Alternative Certification](docs/archive/2026-08-02-competitive-lazy-alternative-certification/README.md).

Completed diagnostic:
[Exact Reforge-Work Growth Diagnostic](docs/archive/2026-08-02-reforge-work-growth-diagnostic/README.md).

Source milestone:
[Proof-Carrying Quotient Refinement Structural Stop](docs/archive/2026-08-01-proof-carrying-quotient-refinement/README.md).

Completed enabling milestone:
[Solver Iteration Infrastructure And Decomposition](docs/archive/2026-08-01-solver-iteration-infrastructure/README.md).

Completed milestone branch: `codex/competitive-lazy-alternative-certification`

Completed milestone branch: `codex/selected-closure-broad-row-scaling`

Completed milestone branch: `codex/fallback-and-terminal-reforge-factorization`

Current branch: `codex/reforge-resource-accounting-v3-qualification`

Current milestone starting commit:
`00718908c866dc71a2e7e3864ebc5c5015ff063f`

Current milestone commits: Gate 0 is
`a683b8cbd9c5ac60b6e042d819a3ee6b3aa6fb0b`. Gate 1 is recorded by the
current local commit. Gate 2 is the active implementation boundary.

Certified fallback milestone starting commit:
`0b4ba38aa5754a39715640ad6d7a48cab2dc2b6c`

Certified fallback milestone commits: Gate 0 is
`abe8e525d59cc6ae90663a1d56736d411da7ca6c`. Gate 1 is
`feef64a25a1351b7111aab1fcb4d02e70404c80c`. Gate 2 is
`a663bce98e24b793277f01bd4b56727aa8fceb9d`. Gate 3 is
`4dcf51d7e65dc483c5d8119c30d2919187f1de7a`. Gate 4 is
`6b0fec599b7342faf8021f2c369c0cc6731ba9ad`. Gate 5 is
`bf5fa6b8a5af7cbbe0dce630fb215caeeebef85a`. Gate 6 is
`1389bfb1296aae1d2c51efb5dbeffdead99047d1`. Gate 7 is skipped. Gate 8 is
`54a459547b719e8c63dc0d136c967081484e54dc`. The archive and handoff are
recorded by the current local commit.

Selected-closure milestone starting commit:
`a6f7e13cf7d5cb202874c210992689d601c0e650`

Selected-closure milestone commits: Gate 0 is
`9f562eb5c80ffe76ae667a0f3d0895f91f8720b2`. Gate 1 is
`c9481fb0a5b6666ce73afbdafc497296dd369937`. Gate 2 is skipped. Gate 3 is
`1ee6bc8481b47bc2e83228e577a930e9c9b72975`. Gate 4 is
`ac41a0db96acd45a26a55a1e268502573f2fca74`. Gate 5 and the archive are
recorded by the current local commit.

Milestone starting commit:
`bd288c9041a5b54fa4ec134c7e1dec90486ac385`

Current milestone commits: Gate 0 is
`222ef42242bb1c51cddb38444750bbd30cda6875`. Gate 1 is
`53c538143b3599b99b5b8cd8317557ada14e39c7`. Gate 2 is
`c10b2e3cdc85aaab0ec7c0fc6293a9392e66bc16`. Gate 3 is
`4c19d2cc8d3b0569800457b90f91d9c62ad4e9e4`. Gate 4 is
`68591b7d5bc20d7525fb8b7137e0061a05cdf4f7`. Gate 5 and the archive are
recorded by the current local commit.

Preserved proof-carrying quotient commits: Gate 0 is
`4193f086bc7deffb5ce0e3b81f4045a42a4fe3c9`.
Gate 1 is `5c531d0c9eff204954a5d3d6883a0a2e6d99726a`. Gate 2 is
`9e0ae6f3135515a9b358ee178a16b3658bea9939`. Gate 3 is
`62ca542e76829d39a27323fa2d5c1cc6266ba567`. Gate 4 is
`dac7c6f9670a17e788381fd1ce4c33fc8c4925e2`. Gate 5 is
`41d6a243947b0205a6e29c6373e79b21b0d8292a`. The diagnostic is recorded by
the final archive commit.

Nothing has been pushed or merged. `main` remains unchanged at
`25d5bbe6791beb61eae803219563575346def2dc`.

## Versioned reforge resource accounting Gate 0

- The branch begins from clean commit
  `00718908c866dc71a2e7e3864ebc5c5015ff063f`. Source tree, native binaries,
  release WASM, artifact, Mirage economy, corpus/hard fixture, 27-action order,
  V1/V2/V3 evidence, representative family/identity rows, and existing
  transition/policy/strategy hashes are frozen in
  `fixtures/solver-reliability/v1/evidence/reforge-resource-accounting-v3-qualification-gate0.json`.
- The native `SolveOptions` and benchmark omission default is 50M. Null/zero C
  options and omitted/zero WASM JSON inherit that default. TypeScript defines
  no separate production default. The reliability generator and canonical
  hard fixture explicitly override to 20M; preserved 18M/100M diagnostic and
  every other tracked fixture/test override are separately inventoried.
- The binding row remains V1 2,097,355 work / 0.809 seconds, V2 923,141 /
  1.004 seconds, and V3 2,514,591 / 0.395 seconds. V3 is reopened because its
  prior work rejection compared evaluator-specific operations, not because
  exactness or wall time failed.
- Qualification is frozen before implementation: V1 target/probability/value/
  semantic-action equivalence; at least 25% median native binding improvement;
  no aggregate eligible-row runtime regression; deterministic V1 dispatch for
  any nontrivial row above 10% regression; at most 10% peak-memory and
  cooperative-step regression; and adequate release-WASM responsiveness.
- The preferred contract makes `max_reforge_work` a stable V1-equivalent
  logical envelope and reports evaluator effort separately. It is conditional
  on a safety proof under the logical, memory, and cancellation limits. A
  separate evaluator-safety cap is allowed only if genuinely required; no
  weighted universal score is allowed.
- V1 remains the exact oracle and rollback path. V2 remains diagnostic. The
  certified fallback remains independently executable and cannot alter the
  lower or unresolved alternatives. Rare-renewal reconciliation, five-goal
  finalization, and checkpoint/replay are out of scope.
- Gate 0 changed documentation/evidence only. No test or benchmark was run.

## Versioned reforge resource accounting Gate 1

- `ReforgeEffortBreakdown` now records deterministic physical component
  counts with saturating arithmetic and deliberately defines no additive
  total. The legacy active ledger and parallel V1/V2/V3 ledgers remain
  serialized and unchanged for comparison.
- Bounded row samples cover coarse, strict selected, strict alternative, and
  exact-evaluation ownership; ordinary, Essence, Harvest, Fossil, and
  automatic-option family; evaluator version; cache hit/miss; and completed,
  interrupted, discarded, or published disposition. Exact aggregates survive
  truncation, diagnostic allocation cannot affect solver decisions, and sample
  capacities enter calculator, strict adapter/result, and exact-evaluator
  owned-memory ledgers.
- Strict publication disposition is transactional. A memory checkpoint can
  retain an in-progress completed sample, then synchronization refreshes it by
  sequence after the enclosing proof scope publishes or discards the row.
  Interrupted rows remain observable and cannot publish.
- Automatic admission and the protected-repeat comparison context receive only
  the parent's remaining work before evaluation. Comparison work is no longer
  unlimited, all executed child work merges during refusal, and nested
  grandchild subtotals are not double counted.
- Focused native acceptance passes: calculator `125077/0`, S8.3 automatic
  `365/0`, solve `6083/0`, and exact evaluator `1176/0`. Optional artifact
  sub-suites without supplied readable paths were skipped. Existing evaluator
  verification within the focused evaluator suite retained its 10,000-run
  sample.
- No mechanic, action/filter, state/goal, quotient/Bellman, compiler,
  strategy-vocabulary, fixture, artifact, economy, public ABI, WASM, frontend,
  production/default cap, cap interpretation, or production evaluator
  selection changed. No benchmark, canonical hard run, release-WASM build, or
  full acceptance suite ran.
- Evidence is
  `fixtures/solver-reliability/v1/evidence/reforge-resource-accounting-v3-qualification-gate1.json`.
  Gate 2 must keep cap behavior and production selection unchanged while it
  measures accounting overhead and matched V1/V2/V3 rows.

## Certified fallback and terminal factorization Gate 0

- The branch begins from clean commit
  `0b4ba38aa5754a39715640ad6d7a48cab2dc2b6c`; the source tree, native
  binaries, artifact, Mirage economy, canonical hard fixture, options, and
  complete 27-action order are frozen in
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate0.json`.
- Existing hard 20M and selected-only 100M evidence was not rerun. The product
  cap remains 20M, the solver-owned cap remains 1 GiB, and no mechanic, public
  ABI, strategy vocabulary, canonical input, WASM artifact, or frontend
  authority changed.
- The existing hard report contains one independently certified primitive
  Chaos renewal: success probability `0.00025094514103578676`, upper and
  bootstrap-evaluated value `3984.9346987650665`, 24 validated non-goal
  carriers, and witness hash `5e2bd0c222942bda`.
- The preferred coarse/Q-directed candidate is cheaper at
  `3323.6694369790375`, but its selected Essence policy stops on
  `max_reforge_work` during strict lift. The current failure is ownership and
  publication: the unpublishable preferred candidate leaves no retained
  executable result.
- The exact V2 boundary remains 2,097,355 V1 work, 923,141 V2 work, 51,155
  frontier states, 688,739 terminal arrivals, 172,596 unique projected
  outcomes, and 638,365 final-depth branches omitted from the earlier sparse
  proxy. Raw/V2 row wall remains approximately 0.809/1.004 seconds.
- Track A must publish the best still-current certified fallback without
  changing the lower or resolving alternatives. Track B continues afterward
  and must reduce real terminal work rather than reclassify it as free.
- Gate 0 is documentation/evidence only; no test or benchmark was run.

## Certified fallback portfolio Gate 1

- The solver now keeps a deterministic four-entry portfolio of generic
  primitive-renewal fallbacks. Each retained entry owns the captured policy
  and witness, independently recomputed cost, compiled artifact and compiler
  counts, complete identity/generation dependencies, properness/executability
  result, compilation provenance, and capacity-derived owned bytes.
- Preferred/coarse candidacy, selected publication proof, certified fallback,
  and retained compiled artifact are distinct. Incremental/direct mutation
  first preserves the current certified executable entry; a cheaper candidate
  without complete executable provenance cannot erase it.
- Preferred strict-lift and ordinary exact-publication failures try the best
  still-current fallback. Invalid price/goal/graph/vocabulary/artifact/
  generation dependencies remove only that entry. Successful cheaper
  publication releases the unused portfolio.
- A generic synthetic two-goal regression publishes the compiled renewal when
  the preferred assertion stops at 1,000 work, publishes the cheaper preferred
  exact policy at 2,000 work, and preserves the fallback through an isolated
  preferred JSON compilation cap.
- Pure tests cover multiple/equal-cost ordering, every invalidation family,
  improper and non-executable candidates, missing policy/witness/compiler/
  independent-evaluation provenance, and exact memory-cap arithmetic. The
  focused solve suite passes 98,217 checks with zero failures.
- The fallback supplies only an upper. Lower-bound and unresolved-alternative
  authorities are unchanged. No mechanic, action vocabulary, cap/default, C
  ABI, strategy vocabulary, WASM artifact, or frontend changed. Evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate1.json`.
- The canonical hard case was not run in Gate 1; its one allowed invocation is
  reserved for Gate 2.

## Certified fallback hard qualification Gate 2

- The canonical hard two-goal case was invoked exactly once at the unchanged
  20M reforge-work and 1 GiB solver-owned caps with the complete 27-action
  candidate vocabulary and 10,000 requested verification runs.
- Preferred exact publication exhausted the remaining 5,922,368 reforge work.
  The retained certified primitive-renewal fallback then published once with
  upper/evaluated cost `3984.9346987650665`, while the full-envelope lower
  remained `752.90090756637869` and unresolved alternatives remained open.
- The bounded-feasible result compiled to four nodes, four edges, and 1,208
  bytes. Exact evaluation reconciled at `3984.9346987639233`, an absolute
  delta of `1.1432348401285708e-09`, with zero off-policy mass.
- All 10,000 simulations succeeded with zero off-policy failures. The run took
  24.669 seconds total, peaked at 375,494,320 native-owned bytes, passed all
  cap and corpus expectations, emitted empty stderr, and left no process
  survivor.
- Tracked evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate2.json`.
  The hard case must not be rerun. Gate 3 now owns deterministic attribution
  of the 638,365 exact final-depth branches.

## Terminal branch attribution Gate 3

- Bounded deterministic telemetry classifies the exact evaluator's final
  contributions without affecting solver decisions. V1/V2 ledgers and the
  active evaluator remain unchanged, and owned-memory preflight includes the
  diagnostic successor, order, and sample storage.
- The binding Zeal Essence row has 39,900 terminal predecessors, 638,365 real
  final branches, and 125,581 completed canonical successors. The other
  512,784 contributions are algebraically accumulable, representing 80.33%
  of branches and 74.60% of final probability mass.
- Every duplicate crosses predecessors. There are no same-predecessor
  duplicates; 502,320 duplicates cross final bucket/exclusion signatures,
  512,330 cross predecessor availability, and 261,625 cross side. Availability
  and exclusions therefore remain in the preterminal key until the final pick
  is applied.
- All 39,900 predecessors represent multiple roll orders with multiplicity
  24. The existing unordered frontier already collapses those orders; the
  telemetry reports 14,682,395 temporary terminal order distinctions without
  reclassifying any of the 638,365 V2 branches as free.
- Gate 4 must use the completed canonical successor as its convergence key,
  with an unordered set/subset recurrence and factored last-pick transform.
  Predecessor, last-pick, and order identity may disappear only after that
  completed successor exists. Accumulate exact integer/rational mass before
  floating-point conversion.
- The focused 18M diagnostic completes four deterministic selected rows,
  passes all corpus expectations, and does not rerun the canonical hard case.
  The native build and focused solver-calc suite pass 250,746 checks with zero
  failures. Track A's fallback is not counted toward Track B. Evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate3.json`.

## Exact V3 terminal accumulator Gate 4

- V3 is an opt-in native diagnostic layered on the exact projected frontier.
  It indexes complete live final-depth subsets, generates every completed
  unordered roll set once from its canonical live removable last pick, and
  reconstructs all incoming probability by a destination-driven subset
  recurrence before publishing the completed set once.
- Canonical ownership excludes predecessors that already terminated on goal.
  Zero-progress non-goal final picks aggregate into one checked integer retry
  numerator per predecessor. Exact bucket numerators and denominators remain
  integers until the final ratio.
- Local bucket ids bind the action, preserved base, pool/Fossil weights,
  forced/guaranteed channels, exclusions, observation layout, and artifact
  generation. Every subset hit reconstructs and compares the full RollState,
  including availability, side capacity, goal/below/blocked masks, and member
  tokens. The focused test observes zero identity mismatches.
- V1 raw-equivalent, V2 sparse, and V3 factored ledgers remain parallel. V3
  charges predecessor indexing, denominator scans, canonical-subset checks,
  candidates, every recurrence term, completed publications, retry
  aggregates, and raw-identity expansion. A one-less work cap refuses the
  active V3 row. Scratch preflight owns the predecessor and candidate stores.
- The C ABI, WASM, product default, raw oracle, and V2 path are unchanged. The
  native build and focused ordinary/Essence/Harvest/Fossil solver-calc suite
  pass 251,729 checks with zero failures, including forward/reverse V3 mass.
  The canonical hard case was not rerun. Evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate4.json`.

## Exact V3 equivalence Gate 5

- Synthetic raw/V3 comparison covers ordinary Chaos-like reforges, forced
  Essence, targeted Harvest, and Fossil positive, zeroed, added, and forced
  channels. Prefix-only, suffix-only, mixed, and below-tier goal observations
  match with forward and reversed enumeration.
- Goal-level-equivalent junk modifiers carrying different future group
  exclusions remain distinct. V3 records no subset identity mismatch and its
  forward/reverse gated kernel bits are identical.
- Real Vaal Regalia coverage compares empty and fractured carriers under
  Chaos, one forced Essence, one viable targeted Harvest reforge, and one
  single-Fossil action. Legality, total and per-`AbstractState` mass, goal and
  below-tier mass, and exclusion behavior agree with raw.
- Collision-checked abstract maps preserve the gated retry-basin bit. The
  comparison does not materialize that virtual state and thereby erase its
  Bellman identity.
- Raw and V3 gated Bellman solves both produce a policy with identical value,
  operator kind, and semantic action id. The native build and focused suite
  pass 252,035 checks with zero failures. Gate 5 changes tests only and does
  not rerun the canonical hard case. Evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate5.json`.

## V3 binding performance Gate 6 structural rejection

- One temporary 18M diagnostic reused Gate 3's semantic carrier and added only
  `factored_terminal_reforge_diagnostic=true`. It did not rerun the canonical
  hard case or change the 20M product cap.
- The binding Zeal Essence row reports 2,097,355 V1 work, 923,141 V2 work, and
  2,514,591 honest V3 work. V3 is 8.38 times the 300,000 threshold, 172.40%
  above V2, and 19.89% above raw. Its 0.395-second row wall beats the 0.809-
  second wall threshold, so only the work half of criterion one fails.
- V3 publishes 136,045 completed sets but must retain 638,365 denominator
  edges, 641,095 canonical-subset checks, and 638,365 last-pick recurrence
  terms. With predecessor indexing, candidates, commits, and 284,776
  nonterminal work, the decomposition sums exactly to 2,514,591. There are no
  subset identity mismatches.
- Selected refinement completes one row, consumes 3,922,368 reforge work, and
  reaches neither a partition nor its own executable upper. Criterion two
  fails. The separately reported `3984.9346987650665` fallback receives no V3
  credit; Gate 2 remains its exact-evaluation and 10,000-run authority.
- V3 is sound and remains diagnostic-only. It is not integrated into selected
  or alternative production rows, so Gate 7 is skipped. Evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate6-structural-rejection.json`.

## Certified fallback release acceptance Gate 8

- Release acceptance corrected bounded diagnostic sample sizes, replaced a
  false exact/double occupancy comparison with direct high-precision quotient
  flow auditing, aligned no-executable/bounded reliability classification,
  and added the WASM 10,000-run override. The focused exact evaluator passes
  16,787 checks with zero failures.
- The final logical native portfolio reuses Gate 2's one canonical hard
  invocation and passes 47 of 49 cases. Forty compiled strategies receive
  400,000 requested simulations with zero off-policy failures. The canonical
  fallback remains qualified.
- Dense four-goal and representative three-goal rare renewals miss the frozen
  `1e-9` relative exact-cost tolerance at `3.584e-9` and `5.739e-9`; only 16
  and 66 of their 10,000 simulations finish within 100,000 actions. A full-four
  Fracture probe has the same boundary at `7.512e-9` and 15 finite-horizon
  successes. These are disclosed failures, not passes.
- The five-goal native process reached the required roughly 900-second
  watchdog with 30,775 states, 235,429 rows, 886,958 transitions, and
  10,861,463 reforge work but no finalized strategy. It was terminated and was
  not rerun.
- The rebuilt release module exposes all 61 callable exports at ABI 2.
  Exercised ring and body-armour strategies each succeed in all 10,000 runs
  with zero off-policy failure. The 27-check worker smoke, `npm test`, and
  TypeScript pass.
- `powershell -File scripts/test.ps1` was invoked exactly once and passes in
  68.9 seconds: artifact and Python layers, 12 benchmark specifications,
  3,001,413 engine checks with zero failures, and all web/WASM tests. No visual
  review was performed.
- Tracked evidence is
  `fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate8-release-acceptance.json`.
  The completed report is
  `docs/archive/2026-08-03-certified-fallback-and-terminal-reforge-factorization/report.md`.
  No follow-on implementation is selected.

## Selected-closure scaling Gate 0

- The current branch, commit, native benchmark/engine binaries, artifact,
  economy snapshot, canonical hard fixture, full 27-action order, options,
  source authorities, and selected-first 20M evidence are frozen in
  `fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate0.json`.
- The 20M result is not rerun: 14,077,632 coarse plus 5,922,368 selected work,
  two completed selected rows, 345,192 transitions, no alternatives,
  partition, obligation set, or upper, and 375,483,695-byte peak native-owned
  memory.
- Gate 1 is authorized to run the current selected-first hard case exactly
  once at 100M. A temporary case may differ only in
  `caps.max_reforge_work`; the canonical fixture and product 20M default stay
  unchanged. The 1 GiB cap, 900-second external watchdog, full action
  envelope, exact evaluation request, and 10,000-run request remain fixed.
- Required evidence includes selected/alternative rows, work and transitions;
  partition/upper work and wall; row slopes; report wall, peak memory, and
  maximum solve step; retained policy; action/obligation accounting; and all
  downstream verification if a policy exists.
- A 200M run is forbidden unless 100M reaches a partition and strongly
  projects an upper below 200M. No partition or flat/rising rows without such
  progress means no 200M run and no default increase. Broad-row attribution
  proceeds in every outcome.
- No production source, fixture, default, mechanic, ABI, WASM artifact, or
  frontend contract changed in Gate 0. No test was required.

## Selected-closure scaling Gate 1

- The hard case was invoked once with a temporary 100M case whose only
  semantic difference from the canonical fixture is
  `caps.max_reforge_work`. The canonical case remains unchanged at SHA-256
  `1dc33be7...cb3be`; the temporary case is `d5bc4701...101ad`.
- The run returned `refused_resource_cap`: invariant coarse work is
  14,077,632 and 40 selected rows consume all 85,922,368 exact work while
  emitting 6,903,840 transitions. No alternative, partition, obligation, or
  executable upper is reached.
- From the frozen 20M point, 38 marginal selected rows cost 80M work, or
  2.105M each, and add exactly 172,596 transitions per row. The selected-only
  population equals the archived eager 100M kernel/transition prefix, proving
  that this expensive prefix belongs to selected closure.
- Peak native-owned memory is unchanged at 375,483,695 bytes. The report wall
  is 28.634 seconds, solve wall is 3.979 seconds, and maximum cooperative step
  is 651.543 ms. There are zero production reference-adapter calls.
- No policy exists; compilation, exact evaluation, reconciliation, and
  simulation are not applicable. Action accounting remains incomplete and no
  carrier-wide obligation lower exists.
- The tool-level shell timeout detached the PowerShell watcher after 10
  seconds. The existing benchmark child was monitored without rerun and
  completed well inside 900 seconds with a valid report, empty stderr, and no
  survivor. Continuous watchdog attachment is not claimed.
- The decision gate forbids a 200M run and rejects a higher product default.
  Conditional Gate 2 is skipped. Tracked evidence is
  `fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate1.json`.
- Gate 3 now owns exact attribution of the broad selected programs, pools,
  exclusion state, enumeration phases, raw identity tree, projected duplicate
  targets, and reusable calculations.

## Selected-closure scaling Gate 3

- Bounded diagnostic telemetry now carries the selected/alternative row,
  source strict/coarse carrier, program, observation summary, pool weights,
  bucket/exclusion topology, frontier and terminal populations, duplicate
  projected mass, work categories, and phase timings. It is observational and
  does not participate in solver decisions or proof.
- The first 18M probe found the capture boundary too late: selected-action
  resolution had already prepared the distribution, leaving the new sample
  list empty. Its aggregate counters remain valid, but it is rejected for
  attribution. The second probe validated the fixed boundary. A final probe
  against the commit-ready binary reproduced every deterministic row field
  after attribution became opt-in to the strict publication child. All three
  attempts are disclosed in tracked evidence.
- The accepted selected row is root Zeal Essence, operator `10`, with one
  forced modifier. Its post-forced natural pool is 259 modifiers / 210,650
  weight (112 / 88,600 prefix and 147 / 122,050 suffix), split into 39
  physical families and 40 roll buckets.
- All 2,097,355 work belongs to 51,155 frontier-state visits times 41
  node/bucket probes. Raw-choice and identity-tree work are zero. Only 185,825
  of 2,046,200 bucket probes are positive, so the sparse node-plus-edge proxy
  is 236,980 before prototype overhead.
- The row commits 688,739 terminal arrivals into 172,596 strict projected
  outcomes. There are 516,143 duplicate arrivals carrying
  0.25435223351246694 probability mass after first insertion. Frontier time is
  704.269 ms and finalization 100.054 ms of 808.997 ms raw build time.
- Gate 1's immutable report cannot retroactively identify the source/action
  of rows 2–40. They retain the exact common 172,596 transition count, but no
  100M rerun is permitted. Tracked Gate 3 evidence states this boundary rather
  than inferring identities.
- Gate 4 must prototype a generic exact sparse projected roll DP. Its key must
  retain current-roll exclusions and all future-visible goal, tag, legality,
  protection, forced/guaranteed, compiler, and simulator distinctions. Keep
  raw-equivalent and projected work counters in parallel and keep the raw
  enumerator as the independent oracle.
- Tracked evidence is
  `fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate3.json`.

## Selected-closure scaling Gate 4

- The existing raw frontier remains the default and independently selectable
  oracle. The opt-in V2 frontier carries the same exact projected roll state
  plus a full collision-checked interned availability bitset.
- Availability removes buckets only for exact group conflict, exhausted
  multiplicity, occupied/blocked goal observations, closed sides, or zero
  natural weight. It therefore preserves the current goal/below, junk,
  forced/guaranteed, identity-expansion, compiler, and simulator distinctions
  without a named action or fixture branch.
- One shared parameterized path covers ordinary renewal, Essence direct
  modifiers, Harvest's separate guaranteed first-pick pool, and Fossil
  multipliers, zeroed pools, additions, and forced modifiers.
- Work is parallel and versioned. V1 retains the historic node-plus-all-
  buckets counter; V2 counts sparse nodes, availability words inspected, and
  eligible edges. Shared raw-choice, guaranteed-scan, and identity-tree work
  appears in both, while `max_reforge_work` applies to the active evaluator.
- Focused native coverage compares canonical projected maps and goal mass for
  Chaos-like, Essence, Harvest, and Fossil rows, including a positive Fossil
  multiplier and a zeroed tag. The focused engine suite passes 174,411 checks
  with zero failures. Product behavior remains raw pending Gate 5.

## Selected-closure scaling Gate 5 structural rejection

- Forward and reversed raw/projected traversals agree on canonical probability
  maps and goal mass for synthetic ordinary, Essence, Harvest, and Fossil
  rows. Sampled real-artifact checks add empty and fractured Vaal Regalia
  carriers, legality, action cost, Bellman value, and selected semantic action.
- The focused artifact command passes 250,713 checks with zero failures. No
  public ABI or strategy vocabulary changed, so neither WASM nor web rebuilds
  were required at this gate.
- The only new hard diagnostic reuses Gate 3's accepted 18M carrier and adds
  the native-only projected-frontier flag. It is not a rerun of the frozen 20M
  case or the one-shot 100M case; no 200M run was performed.
- On the binding root Zeal Essence row, parallel V1 work remains 2,097,355 and
  V2 work is 923,141. The exact 55.99% reduction still exceeds the 300,000
  ceiling by 623,141 work and measures 1.004 seconds versus 0.809 seconds raw.
- Gate 3's 236,980 proxy omitted 638,365 eligible final-depth branches that
  commit directly instead of extending the next frontier. Gate 5 charges them
  as real work; declaring them free would weaken the cap rather than optimize
  the algorithm.
- The diagnostic again stops on the work cap after four selected rows, with no
  partition, alternative obligation, executable upper, or policy. Compilation,
  exact reconciliation, and 10,000-run simulation are not applicable.
- The exact V2 research path and raw oracle are retained, but production
  integration is rejected. Raw remains the product default and the 20M default
  is unchanged. Gates 6–8 are skipped by condition.
- Tracked evidence is
  `fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate5-structural-rejection.json`.

## Competitive lazy Gate 0

- Frozen corpus, fixture, artifact, economy, action-order, options, prices,
  diagnostic reports, source, and binary identities are tracked in
  `fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate0.json`.
- New deterministic aggregate telemetry distinguishes selected and alternative
  rows begun/completed, exact reforge work, transitions, work to first
  partition/upper, and alternatives exact-materialized before the upper. It
  does not influence scheduling or proof.
- The medium before/after semantic-work fingerprint is exact. Both runs retain
  `ce5a144282753b26` / `6bee45662f66d2e4` transition/policy hashes and compiled
  strategy SHA-256
  `adf9ae9312ae1c184a3f467effde14e4c52ef789678a78ad1216bf53a4e04003`.
- Medium attribution is 115 selected rows, 39,690 work, and 1,476 transitions
  versus 88 alternative rows, 4,180,979 work, and 275 transitions. First
  partition and first executable upper both occur at 4,220,669 exact work,
  after all alternatives have already materialized.
- The frozen hard case still has only 5,922,368 exact work after its invariant
  coarse phase. Hard selected closure is not presently estimable: the archived
  10,466 selected kernels average about 40 transitions, materially unlike the
  172,596-transition broad rows. No measurement proves every executable
  selected witness exceeds the cap, so Gate 1 proceeds.
- Focused solve acceptance is 98,131 checks / 0 failures. The before/after
  medium workflows both exact-reconcile and complete 10,000/10,000 simulations
  with zero off-policy failures and zero reference calls.

## Competitive lazy Gate 1

- `ProofStore` now interns full-key, collision-checked
  `UnresolvedAlternativeObligation` records. Identities retain source cell,
  observation requirement, action/program/choice, price, vocabulary,
  requirement/source/target/partition/action/admission/price/vocabulary
  generations, scheduling priority, and resumable-work identity.
- `CarrierWideOptimisticLowerQ` has no writable proof fields. Construction is
  restricted to the explicit nonnegative-cost zero fallback or complete
  per-carrier witnesses, whose minimum is retained. Incompatible bounds cannot
  be averaged into a stronger cell lower.
- All required lifecycle states are represented. Rowless obligations cannot
  support the executable upper, potentially competitive or stale obligations
  block exactness, and conditional noncompetition is valid only under its
  unchanged source upper and Q generation.
- Complete action accounting accepts each admission exactly once as current
  selected-certified, other-certified, or an explicit obligation. Deterministic
  scheduling order and exact capacity-derived obligation memory are covered.
- Focused quotient-proof acceptance passes 316 checks / 0 failures. Evidence
  is tracked in
  `fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate1.json`.
- No production row scheduling, cap, mechanic, action filter, ABI, strategy
  vocabulary, WASM artifact, or frontend contract changed. Gate 2 may now wire
  these proof objects into selected-policy-first construction.

## Competitive lazy Gate 2

- Production quotient APIs now separately enumerate cheap alternative
  descriptors, certify the inherited/current selected row, and certify one
  requested descriptor transactionally. Descriptor enumeration does not call
  primitive outcome or option-kernel construction.
- Only selected-row successors enter initial strict closure. Alternative
  descriptor full identities participate in partition identity, but unknown
  outcomes never become stochastic arcs or mergeable frontier states.
- Every final nonterminal cell retains its current certified row plus one
  lower-only obligation per admitted descriptor. The descriptor for the
  selected operator is preserved because a different exact observed choice may
  still improve it.
- Production performs a complete action-accounting audit before Bellman solve.
  New telemetry reports obligations created/unresolved, exact rows avoided,
  accounting completion, and exact obligation-ledger bytes.
- The focused integration reaches partition at exactly its selected exact work,
  materializes zero alternatives before its executable upper, compiles, proves
  properness, and exact-reconciles. Solve acceptance passes 98,140 checks / 0
  failures; quotient proof/partition/Bellman acceptance passes 316 / 0.
- Evidence is tracked in
  `fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate2.json`.
- No cap, mechanic, price, filter, ABI, strategy vocabulary, WASM artifact, or
  frontend contract changed. Gate 3 now decides which unresolved obligation is
  competitive enough to certify.

## Competitive lazy Gate 3

- The deterministic scheduler starts only after the selected quotient has a
  proper executable upper and a compiled, exact-reconciled artifact available
  as rollback authority.
- It certifies one requested descriptor across the complete source-cell
  carrier coverage. Only action/cost/projection-lumpable rows with closed
  successors and already-satisfied routing observations enter the proof store;
  open-frontier or split-requiring work remains explicitly partial.
- Completed competitive rows enter the existing Bellman/properness path.
  Improving publications replace the retained artifact only after compilation
  and reconciliation. A later alternative or compile resource cap preserves
  the last compiled bounded upper.
- Unresolved carrier-wide lowers now participate in the optimistic lower
  relaxation without becoming executable rows. Stale obligations contribute
  neither a lower nor a noncompetition verdict.
- The focused witness scheduled 12 obligations, certified 6, retained 6
  partial blockers, and used 7,680 selected plus 7,680 alternative work, with
  zero alternative kernels before the first upper. It remained proper,
  compiled, reconciled, and honestly bounded.
- A focused exact-kernel-cap control retained the selected publication after
  one resource-interrupted obligation. Source/target, requirement, price, and
  vocabulary revocation controls all recreate a schedulable blocker and refuse
  exactness.
- Focused solve acceptance passes 98,156/0 and quotient
  proof/partition/Bellman acceptance passes 353/0. Evidence is tracked in
  `fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate3.json`.
- Gate 4 now owns the full `reliability-class-belt` workflow, deterministic
  repeat, 10,000 simulations, and scaled-breadth witness.

## Competitive lazy Gate 4

- Two frozen `reliability-class-belt` runs are semantically identical:
  `bounded_feasible`, cost `9.143792577895411`, coarse hashes
  `ce5a144282753b26` / `6bee45662f66d2e4`, compiled strategy SHA-256
  `87a5c6a5c0980b1696156aadbd8a4e94e804d66f6a0ef062dfbee105197855bb`,
  matched exact evaluation, and 10,000/10,000 simulations with zero off-policy
  failures.
- Fifteen selected rows reach partition and a proper executable upper at 882
  work with zero alternatives materialized. First partition is 21.16-27.24 ms;
  first upper is 65.46-71.80 ms.
- The post-upper scheduler attempts 180 alternative rows using 6,355,232 work,
  certifies 31 obligations, and retains 149 partial competitive blockers. The
  trivial carrier-wide lower prunes zero, so publication remains honestly
  bounded. There are no policy replacements.
- The quotient has 165 exact carriers, 19 cells, 195 kernels, and 1,687
  transitions. Peak solver-owned memory is 126,975,163 bytes under the 1 GiB
  cap; obligation storage accounts for 106,092,192 bytes.
- The 17-action / 180-obligation medium run plus the selected-row
  exact-kernel-cap control is the scaled-breadth witness. The first upper uses
  only 0.0139% of eventual exact work and precedes all alternatives.
- Gate 0's hard selected-closure prediction was “not presently estimable.” On
  the medium case, first-upper work drops 99.979% while total exact work rises
  50.595% because carrier-wide alternative attempts occur after publication.
- Evidence is tracked in
  `fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate4.json`.
- Gate 5 consumed the single frozen `natural-t1-breadth-two-4e65dda9c53b`
  invocation. Its result is recorded below; the case must not be rerun as part
  of this milestone.

## Competitive lazy Gate 5 structural stop

- The frozen two-goal case was invoked exactly once with all 27 product
  actions, the unchanged 20M/1 GiB limits, exact evaluation and 10,000
  simulations requested, and a 900-second external watchdog. The watchdog
  completed in 6.234 seconds, did not fire, and left no survivor.
- The coarse phase used 14,077,632 work. Two selected rows used the remaining
  5,922,368 exactly and emitted 345,192 transitions. The solver returned
  `refused_resource_cap` before its first partition or executable upper.
- Four exact carriers and two kernels were materialized, but no carriers,
  quotient classes, proof rows, or artifact were retained. Peak native-owned
  memory was 375,483,695 bytes under the unchanged 1 GiB cap, with zero
  production reference-adapter calls.
- No alternative row was attempted and no obligation was created. Action
  accounting is incomplete, and no carrier-wide obligation lower participates;
  the reported `752.9009075663787` lower is the pre-quotient coarse bound.
- Compilation, properness, lumpability, exact reconciliation, and simulation
  are not applicable because no policy exists. This is the directive's
  selected-closure structural failure, not a scheduler/pruning limit.
- Gate 0's numerical prediction was “not presently estimable.” Its known risk
  condition is now resolved: the selected closure does not fit the 5,922,368
  post-coarse allowance. Gate 0 was correct to continue because this had not
  previously been measured.
- Structured evidence is tracked in
  `fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate5-structural-stop.json`.
  The raw one-shot artifacts remain ignored under
  `build/acceptance/competitive-lazy-alternative-certification/gate5-hard-once/`.
- Gates 6 and 7 were not reached. No broader native portfolio, release WASM,
  web acceptance, TypeScript check, or full `scripts/test.ps1` run followed the
  binding red.

## Preserved proof-carrying quotient Gate 0 boundary

- The amended plan now binds core, five-goal scale, and WASM qualification
  separately and includes all early structural stops.
- Artifact, Mirage economy, compiler, ABI, action-vocabulary source, goal,
  solver-option, and fixture identities are pinned in
  `fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate0.json`.
- `reliability-class-belt` is the medium integration case.
- `natural-t1-representative-four-62bcfa21ebfe` is the natural representative
  four-goal case.
- `natural-t1-scale-five-d432b26dfce2` is the generated natural feasible
  Runic Gauntlets `2P3S` scale case under 1 GiB/900 seconds and 10,000-run
  verification. Its dedicated corpus does not alter the existing 49-case
  reliability portfolio.
- The archived 467-second two-goal case was not rerun. Bounded pool/action
  reconnaissance and matched medium native/WASM measurements were used.
- Reconstruct-then-merge is frozen as a bounded synthetic parity/debug oracle
  only. Production reference-adapter use on the medium or acceptance cases is
  red.

## Preserved and Gate 0 evidence

| Check | Result |
| --- | ---: |
| Uncached clean parallel all-native build | 20.383 s |
| Warm-ccache clean all-native build | 5.988 s |
| Final native no-op wrapper | 0.398 s |
| Leaf implementation edit plus relink, cache disabled | 3.165 s |
| Parallel CTest | 10/10, 12.47 s real |
| Shared refinement | 301 checks, 0 failures |
| Policy refinement | 4,829 checks, 0 failures |
| Full native executable | 2,997,866 checks, 0 failures, 35.963 s |
| Benchmark validation | 12 specs, 0 failures |
| Direct release WASM | 124.885 s |
| Incremental WASM clean / leaf / no-op | 29.607 / 17.367 / 0.711 s |
| WASM exports | 61/61 callable, ABI 2 |
| Complete `scripts/test.ps1` run | passed, 66.524 s |

The fresh medium reconnaissance completed natively in 905.005 ms total with
51 coarse states, 266 exact carriers, 19 exact classes, two counterexamples,
80,316,483 peak solver-owned bytes, compilation, and matched independent exact
evaluation. The matched release-WASM workflow took 2,946.559 ms total and
also matched exact evaluation; its classification mismatch is baseline
evidence, not a Gate 0 success claim. Matched end-to-end evidence gives a
2.2645x–3.2558x native-to-WASM range, so hard cases are initially
native-binding/WASM-reported until Gate 3 can prove adequate product headroom.

`npm test` and `npx tsc --noEmit` passed. The release WASM worker suite passed
27/27. Compiled-strategy verification used 10,000 runs where requested. No
rendered/visual review was performed.

The one-goal belt transition/policy hashes remain
`ce5a144282753b26` / `6bee45662f66d2e4`. The deliberately unrerun 467-second
two-goal evidence remains `4f26d305a908c59f` / `3e384d3eb52f9ab7` with its
tracked work counters. The tracked qualified Fracture evidence retains
`04a66ba6c6dfcabf` / `3e5d7530e7aed5fb` and compiled strategy SHA-256
`e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`.

## Preserved proof-carrying quotient Gate 1 boundary

- `SolveTransitionCache` directly owns the optional proof sidecar, while
  immutable collision-checked payloads are separated from stable row use
  sites and their source/target/action/admission generations.
- Coverage is a deterministic canonical range recipe under explicit strict
  kernel, replay-authority, and normalized-enumeration identities. Only a
  requested replay slice materializes exact carriers; no global strict-carrier
  vector is retained.
- Full equality follows every hash lookup, including a deliberately forced
  collision witness. Every identity and generation mismatch required by Gate
  1 is rejected or invalidated.
- The category ledger charges selected vector capacities and nested owned
  allocations, separately tracks live replay slices, enforces its cap, and is
  included in `SolveTransitionCache` owned-memory estimation.
- `quotient-proof` passed 111 checks with zero failures. The preserved shared
  refinement and policy-refinement suites passed 301 and 4,829 checks with
  zero failures. The tracked Gate 1 evidence is in
  `fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate1.json`.

## Preserved proof-carrying quotient Gate 2 boundary

- Transient certified carrier slices now adapt directly to the existing
  canonical observation projection and
  `refine_closed_probabilistic_partition`; there is no quotient-specific
  observation contract or partition algorithm.
- Persistent cells retain deterministic replay ranges and exact count/mass,
  not exact member-key vectors. An exact carrier population exists only while
  its ledger-tracked `CoverageReplaySlice` is live.
- Internal successors must close inside the submitted slice. External
  frontiers carry collision-free encoded label/identity pairs and remain
  distinct. An unlabeled open edge is rejected before the shared partition is
  invoked.
- Split-only installation retains stable IDs for unchanged cells, allocates
  deterministic child IDs, rejects re-merging, preserves the complete entry
  distribution, emits existing `RefinementCounterexample` records, and drives
  deterministic source and target proof invalidation.
- The four-node delayed cyclic witness splits without persistent strict-graph
  retention. Equal-row cycles stay merged; the existing exact evaluator
  diagnoses the two-node closed cycle as improper. Source split, target
  predecessor invalidation, requirement growth, multiple entries, frontier
  separation, exact mass, and reversed-order determinism all pass.
- The combined `quotient-proof` suite passed 195 checks with zero failures.
  Preserved shared refinement and policy-refinement suites remain 301 and
  4,829 checks with zero failures. Evidence is tracked in
  `fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate2.json`.

## Preserved proof-carrying quotient Gate 3 boundary

- `SolveTransitionCache` now owns stable certified quotient rows, state-row
  spans, reverse predecessors, price/Q/policy generations, and exact SCC
  evaluation. Uncertified rows participate only in the optimistic lower
  relaxation; every published upper row must have a current full-key proof.
- Production discovery materializes one exact carrier and compact kernel slice
  at a time. Immutable raw kernels are collision-checked and shared; persistent
  per-carrier state is only a strict replay locator and payload-id sidecar.
  `refine_closed_probabilistic_partition_replay` is the replay-backed entry to
  the existing shared split-only authority, so no complete strict graph or
  complete semantic-key vector is retained.
- Observation-coarse certified cells are refined monotonically. The medium run
  exercised 15 source/target splits, 87 reverse invalidations, 173 payload
  reuses, 204 row projections, and 11 Bellman policy changes. The focused
  improper two-state cycle exercises deterministic repair before publication.
- `reliability-class-belt` completed `bounded_feasible`, exact-matched, and
  compiled in 770.817 ms total with one 28,040-byte live slice,
  3,483,686 quotient-local owned bytes, and zero reference-adapter calls.
  `quotient-proof` is 250/0.
- The frozen two-goal point prediction is 13,076 cells, 10,518 certified row
  uses, 189,014,112 proof bytes, 561,416,670 total peak bytes, and 560 seconds;
  the declared total range is 430-760 MB and 470-700 seconds. No measured
  lower bound exceeds 1 GiB, so the early stop did not fire. Evidence is in
  `fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate3.json`.

## Preserved proof-carrying quotient Gate 4 boundary

- Publication is gated by current reachable certificates, closed target
  generations, terminal-reachable bottom components, and a finite proper exact
  value for every entry. The final audit is part of `Complete`, not telemetry
  inferred after publication.
- Certified dead-end alternatives stay retained for lower reasoning while the
  executable upper uses only rows whose full support remains in the certified
  terminal-attractor. A stale unselected alternative no longer blocks a valid
  current upper, and an entry without a certified terminal path is improper.
- Exact successors lacking an inherited coarse choice enter the existing
  local optimizer with the complete already-filtered candidate vocabulary;
  existing per-state admission remains authoritative.
- Streamed compilation requires canonical strict locator coverage and uses the
  existing `RefinedPolicyCompileRouting`; no quotient cell id reaches strategy
  JSON and no vocabulary or ABI changed.
- Focused results are quotient proof 259/0, policy refinement 4,829/0, and
  compiler routing 750/0. The fresh medium run parsed, exact-matched, reconciled
  at the root, and completed 10,000/10,000 simulations with zero off-policy
  failures and zero reference calls. Evidence is in
  `fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate4.json`.

## Preserved proof-carrying quotient Gate 5 structural stop

- The frozen binding case `natural-t1-breadth-two-4e65dda9c53b` was run once
  through the direct native benchmark with exact compiled evaluation and
  10,000 verification runs requested under an external 900-second watchdog.
- It returned `refused_resource_cap` on `max_reforge_work` after 5,787.0836 ms
  total and 3,679.1468 ms solve time. No policy was available, so compilation,
  exact compiled evaluation, reconciliation, and simulation were not
  applicable.
- The stopped prefix materialized four exact carriers and two kernels with
  345,192 transitions. It installed zero quotient classes, ran zero partition
  rounds, and retained zero certified rows. The peak live carrier slice was
  one slice / 4,198,696 bytes.
- Native live / peak owned memory was 305,293,988 / 375,483,167 bytes, below
  the unchanged 1 GiB cap. The production reference adapter was never called
  and no complete strict graph was reconstructed.
- Gate 3's 13,076-cell, 561,416,670-byte, 560-second point prediction cannot
  be compared with this prefix because the run stopped before partition
  initialization.
- Source tracing shows `quotient_compact_action_rows` completing rows for the
  full already-admitted candidate vocabulary through `candidate_selection`
  before a carrier reaches the partition. Candidate work, rather than global
  carrier retention or memory, is the immediate structural wall.
- Core quotient qualification is unqualified. Five-goal scale and WASM
  product qualification are unqualified and were not run after the binding
  core result.
- The Fracture and representative-four core cases, five-goal case, native
  smoke/reliability portfolios, selected 10,000-run verification, release
  WASM, web acceptance, and final `scripts/test.ps1` were intentionally not
  run after the explicit stop.
- The raw report SHA-256 is
  `fcef98a4ddadeec6d6c3cda51ab53d4710bba23097a86e1516f5dcbabfe32837`.
  Tracked evidence is in
  `fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate5-structural-stop.json`.

## Exact reforge-work growth diagnostic

- The coarse phase is cap-independent at 20M, 50M, and 100M: 171
  discovered/expanded states, 7,107 rows, 4,292 transitions, 14,077,632 work,
  identical bounds, the same 27-action envelope, and unchanged transition and
  policy hashes.
- Exact allowances of 5,922,368, 35,922,368, and 85,922,368 complete 2, 17,
  and 40 kernels and emit 345,192, 2,934,132, and 6,903,840 transitions.
- Each marginal completed kernel emits exactly 172,596 transitions. Work per
  kernel is 2.000M then 2.174M; the post-startup shape is approximately linear
  with a slight upward endpoint-censored change rather than falling.
- All points retain zero partition classes, rounds, certificates, policies,
  and reference calls. Peak native-owned memory remains exactly 375,483,167
  bytes.
- The 50M and 100M report walls are 14.566 and 25.074 seconds. Both stop only
  on `max_reforge_work`; memory, watchdog, and correctness boundaries remain
  safe.
- A fitted 2,111,010 work/kernel slope projects about 22.11B total work for
  the archived 10,466 selected-policy kernel proxy before eager extras or any
  later publication phase. The proxy is a model, not a proved current
  population identity.
- Run C at 200M was not performed because Run B already distinguishes the
  approximately linear path from falling cost. Additional work buys raw
  pre-partition expansion, not useful quotient or executable progress.
- Tracked evidence is
  `fixtures/solver-reliability/v1/evidence/reforge-work-growth-diagnostic.json`.
  No source, canonical fixture, default, cap, or public contract changed.

## Exact next step

Complete Gate 1 component accounting:

1. Add one reusable saturating breakdown for logical work and V1/V2/V3
   implementation components without manufacturing an additive score.
2. Record exact aggregate row lifecycle, pool/family/bucket, exclusion,
   frontier, terminal, successor/publication, identity, V3 recurrence, and
   nested automatic-child counts. Keep bounded row samples with owner, family,
   evaluator, cache, and disposition provenance.
3. Accumulate hot-loop counts locally, include bounded storage in solver-owned
   memory, preserve every legacy counter/serialized field, and keep telemetry
   observational.
4. Pass the remaining relevant budget to nested child contexts before they do
   work. Merge all actually executed child work even if the row is later
   interrupted, refused, or discarded.
5. Add focused saturation, overflow, nested propagation, interruption,
   reverse-enumeration, cache-reuse, and completed-publication contracts, then
   commit Gate 1 separately. Do not run routine acceptance yet.

Gate 2 then captures unchanged-production accounting and matched V1/V2/V3
medians before Gate 3 changes any cap interpretation or production evaluator.

Deterministic checkpoint/replay remains deferred.

## Repository rules

- Local commits only unless Oliver explicitly requests a push.
- End commits with the active agent's co-author line.
- SQLite is canonical and the compiled artifact is derived; never hand-edit
  either.
- Any mechanic ambiguity requires Oliver's ruling rather than research or a
  guess.
