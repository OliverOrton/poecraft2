# Session Handoff

**Status: proof-carrying quotient refinement Gates 0 through 2 are complete.
Gate 3 Bellman integration and medium-case projection are the current
implementation boundary.**

Current plan:
[Proof-Carrying Quotient Refinement During Solving](docs/active/proof-carrying-quotient-refinement.md).

Completed enabling milestone:
[Solver Iteration Infrastructure And Decomposition](docs/archive/2026-08-01-solver-iteration-infrastructure/README.md).

Branch: `codex/proof-carrying-quotient-refinement`

Starting commit: `882e70968cd86090e9fc4e882fc6e01886aa62a4`

Gate commits: Gate 0 is `4193f086bc7deffb5ce0e3b81f4045a42a4fe3c9`.
Gate 1 is `5c531d0c9eff204954a5d3d6883a0a2e6d99726a`. Gate 2 is
recorded by the next boundary commit.

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

## Exact next step

Implement Gate 3 in `SolveTransitionCache` and existing Bellman/policy owners:

- retain quotient cells, certified sparse rows, state-to-row spans, and reverse
  predecessor work without changing the filtered action vocabulary;
- mark uncertified aggregation lower-only and require current reachable
  certificates for any executable upper;
- reproject stale rows after source/target splits while preserving unaffected
  payload reuse and every admitted alternative;
- invalidate prices, Q values, SCC values, and policy without discarding
  structurally valid rows on price-only change;
- expose every required proof/split/replay/live-slice/ledger/reference-adapter
  telemetry field; and
- run `reliability-class-belt` through quotient construction, Bellman repair,
  properness, and compilation with zero production reference-adapter calls.

Use the Gate 1-3 medium measurements to freeze a plausible population, memory,
and wall-time range for the archived two-goal case before running it. Stop only
if measured accounting proves a lower bound above 1 GiB or unbounded growth.

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
