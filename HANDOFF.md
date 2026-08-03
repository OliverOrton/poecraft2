# Session Handoff

**Status: competitive lazy alternative certification Gates 0 through 3
complete. Gate 4 medium integration qualification is the active boundary.**

Current plan:
[Competitive Lazy Alternative Certification](docs/active/competitive-lazy-alternative-certification.md).

Completed diagnostic:
[Exact Reforge-Work Growth Diagnostic](docs/archive/2026-08-02-reforge-work-growth-diagnostic/README.md).

Source milestone:
[Proof-Carrying Quotient Refinement Structural Stop](docs/archive/2026-08-01-proof-carrying-quotient-refinement/README.md).

Completed enabling milestone:
[Solver Iteration Infrastructure And Decomposition](docs/archive/2026-08-01-solver-iteration-infrastructure/README.md).

Branch: `codex/competitive-lazy-alternative-certification`

Milestone starting commit:
`bd288c9041a5b54fa4ec134c7e1dec90486ac385`

Current milestone commits: Gate 0 is
`222ef42242bb1c51cddb38444750bbd30cda6875`. Gate 1 is
`53c538143b3599b99b5b8cd8317557ada14e39c7`. Gate 2 is
`c10b2e3cdc85aaab0ec7c0fc6293a9392e66bc16`. Gate 3 is recorded by the
current local commit; Gate 4 has not begun.

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

Run competitive lazy Gate 4:

1. Execute `reliability-class-belt` through the complete native benchmark,
   exact compiled evaluation, and 10,000-run verification workflow twice.
2. Require deterministic semantic results, complete admitted-action
   accounting, selected closure and a first upper before alternative
   exhaustion, exercised competitive certification, properness, lumpability,
   zero reference calls, compilation, and exact reconciliation.
3. Run a bounded scaled-breadth witness proving partition and upper progress
   before all alternatives certify.
4. Record selected/alternative work, milestone work and wall time, obligation
   outcomes, lower pruning, policy replacements, quotient size, transitions,
   memory, simulations, hashes, and Gate 0 prediction scoring.
5. Preserve the existing vocabulary, mechanics, action filtering, caps,
   public C ABI, strategy JSON, and frontend authority.

Deterministic checkpoint/replay remains deferred.

## Repository rules

- Local commits only unless Oliver explicitly requests a push.
- End commits with the active agent's co-author line.
- SQLite is canonical and the compiled artifact is derived; never hand-edit
  either.
- Any mechanic ambiguity requires Oliver's ruling rather than research or a
  guess.
