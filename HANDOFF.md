# Session Handoff

**Status: proof-carrying quotient refinement Gates 0 through 4 are complete.
Gate 5 native structural and scale acceptance is the current
implementation boundary.**

Current plan:
[Proof-Carrying Quotient Refinement During Solving](docs/active/proof-carrying-quotient-refinement.md).

Completed enabling milestone:
[Solver Iteration Infrastructure And Decomposition](docs/archive/2026-08-01-solver-iteration-infrastructure/README.md).

Branch: `codex/proof-carrying-quotient-refinement`

Starting commit: `882e70968cd86090e9fc4e882fc6e01886aa62a4`

Gate commits: Gate 0 is `4193f086bc7deffb5ce0e3b81f4045a42a4fe3c9`.
Gate 1 is `5c531d0c9eff204954a5d3d6883a0a2e6d99726a`. Gate 2 is
`9e0ae6f3135515a9b358ee178a16b3658bea9939`. Gate 3 is
`62ca542e76829d39a27323fa2d5c1cc6266ba567`. Gate 4 is recorded by the next
boundary commit.

Nothing has been pushed or merged. `main` is unchanged.

## Gate 0 frozen boundary

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

## Gate 1 completed boundary

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

## Gate 2 completed boundary

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

## Gate 3 completed boundary

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

## Gate 4 completed boundary

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

## Exact next step

Run Gate 5 native acceptance exactly once per expensive case where practical:

- frozen two-goal under the unchanged 1 GiB/900-second contract, with exact
  publication, 10,000-run verification, and prediction-versus-actual evidence;
- qualified Fracture full-four with all frozen counts and hashes;
- the frozen representative four-goal core case;
- the frozen five-goal scale case, reported separately if it refuses; and
- only after those, the 27-case smoke, 49-case native portfolio, and selected
  Ring/Gloves 10,000-run verification.

Do not implement replay/checkpoint until the quotient representation is stable.
Do not change mechanics, action filtering, caps, public C ABI, strategy
vocabulary, or frontend authority without a separately selected boundary.

## Repository rules

- Local commits only unless Oliver explicitly requests a push.
- End commits with the active agent's co-author line.
- SQLite is canonical and the compiled artifact is derived; never hand-edit
  either.
- Any mechanic ambiguity requires Oliver's ruling rather than research or a
  guess.
