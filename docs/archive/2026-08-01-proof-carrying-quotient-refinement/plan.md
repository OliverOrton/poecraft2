# Proof-Carrying Quotient Refinement During Solving

**Status: archived after the Gate 5 binding-core structural stop on
2026-08-01. Gates 0 through 4 are retained; the core, five-goal scale, and
WASM product qualification levels are unqualified.**

Owner: Oliver

Branch: `codex/proof-carrying-quotient-refinement`.

Starting boundary: `882e70968cd86090e9fc4e882fc6e01886aa62a4` on
`codex/solver-iteration-infrastructure`. `main` remains unchanged and no work
may be pushed or merged during this milestone.

Gate 0 provenance, fixture hashes, bounded reconnaissance, and WASM
classifications are frozen in
[`proof-carrying-quotient-gate0.json`](../../../fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate0.json).
Gate 1 proof representation and focused validation are recorded in
[`proof-carrying-quotient-gate1.json`](../../../fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate1.json).
Gate 2 shared-partition integration and structural witnesses are recorded in
[`proof-carrying-quotient-gate2.json`](../../../fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate2.json).
Gate 3 replay-backed Bellman integration, the frozen medium measurement, and
the pre-hard two-goal prediction are recorded in
[`proof-carrying-quotient-gate3.json`](../../../fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate3.json).
Gate 4 publication, compilation, exact reconciliation, and 10,000-run medium
verification are recorded in
[`proof-carrying-quotient-gate4.json`](../../../fixtures/solver-reliability/v1/evidence/proof-carrying-quotient-gate4.json).

Parent: [Archived milestone](README.md)

## Objective and non-goals

Replace reconstruct-then-merge publication with proof-carrying quotient
transitions retained during solving and counterexample-guided refinement inside
policy improvement. The solver should pay for exact distinctions only when a
selected/candidate row or downstream router observes them, while every merge
remains backed by the existing action contracts, shared partition authority,
exact evaluator, witnesses, and compiler.

The milestone must not:

- create another observation contract, signature vocabulary, partitioner,
  exact evaluator, witness system, or compiler;
- aggregate an open raw successor graph and assume later behavior is equal;
- hard-code Exalt, Regal, Chaos, Restart, Fracture, or any recipe;
- change Path of Exile mechanics, prices, product action filtering, strategy
  JSON, solver objective, or frontend authority;
- raise the `1,073,741,824`-byte product cap or case watchdogs; or
- treat a bounded lower relaxation as an executable upper policy;
- reconstruct the complete strict graph before quotient refinement;
- merge an open graph merely because currently known successors agree;
- add named behavior for any primitive, option, or recipe;
- change action filtering, prices, mechanics, solver objectives, the public C
  ABI, strategy JSON, action vocabulary, or frontend authority;
- add checkpoint/replay during this milestone; or
- perform rendered or browser visual review.

Any ambiguous Path of Exile mechanic still requires Oliver's ruling. The
implementation does not research or infer mechanic behavior.

## Reusable authority and integration map

The implementation is an integration of existing authorities:

| Authority | Existing owner | Required structural use |
| --- | --- | --- |
| Action observation/preservation/destruction | `ActionRefinementContract`, `SelectedAction`, and observation helpers in `solver_refinement.hpp/.cpp` | Derive requirements and transition provenance; no second contract or action-name switch. |
| Collision-free state/operation identity | `StableKey`, `canonical_operation_state_signature`, semantic operator keys | Full tuples are authoritative; hashes may index only with equality checks. |
| Exact mechanics and stochastic rows | strict `CalcContext`, primitive/option kernels, `ProductionPolicyOracle` | Produce exact row evidence and replay witnesses; never select representative modifiers. |
| Price-independent solve graph | `SolveTransitionCache`, stable global `SparseRow` IDs, shared variant arena | Host quotient rows and their proof/provenance handles alongside ordinary rows. |
| Bellman/policy improvement | `SolveWork::Impl` in `solver_solve_bellman.cpp` and incremental predecessor/worklist logic in `solver_solve_incremental.cpp` | Schedule affected quotient states/rows after splits and reuse SCC/value-cache machinery. |
| Backward observation fixed point | `propagate_policy_observations` | Recompute only impacted contract dependencies, preserving the same fixed-point semantics. |
| Partition/lumpability | `refine_closed_probabilistic_partition` | Certify closed proof slices and final publication; do not fork incremental partition logic. |
| Counterexamples | `RefinementCounterexample`, improper-component witnesses, compatibility witnesses | Drive splits and candidate-row scheduling inside policy improvement. |
| Exact value/properness | `evaluate_refined_policy_exact` | Prove current selected quotient policy proper and compute exact class values. |
| Compilation and executable assertion | `RefinedPolicyCompileRouting`, `compile_policy_strategy_json`, `assert_compiled_policy_exact` | Emit the unchanged strategy format only from a closed certified final quotient. |
| Resource/evidence contracts | existing solve/refinement ledgers, telemetry, benchmark runner | Charge retained certificates, dependencies, scratch, and results to the same 1 GiB/work/round caps. |

Primary integration points are `SolveTransitionCache` row ownership,
state-to-row spans, the incremental predecessor scheduler, Bellman Q backup,
policy-improvement selection, and the current publication call to
`lift_policy_exact`. The old adapter remains a focused reference oracle, not a
second production refinement path.

## Disproved shortcut versus selected architecture

### Disproved: open-graph early merge

Do not merge raw successors merely because their current observation and
immediate behavior match. The archived four-node cyclic witness proves that
future successor classes can split them. Once their identities or rows are
discarded, a split-only partition cannot recover the missing evidence.

### Selected: proof-carrying quotient transition

A quotient transition may aggregate probability only into a currently named
quotient cell or a labeled external frontier cell and only while retaining a
certificate that proves:

1. the exact source-carrier coverage to which the row applies;
2. the selected semantic action and existing contract/program observations;
3. the exact kernel/reuse authority that produced the mass;
4. the complete labeled probability projection under the current target
   partition;
5. the source and target partition versions on which the proof depends; and
6. one replayable carrier/witness for every equality claim needed by the
   shared partition or final compiler assertion.

This is not permission to call the closed partition on a partial raw graph.
External frontier labels remain distinct until their target cell is certified.
If a target splits or a requirement grows, every dependent aggregated row is
invalidated and replayed/refined before it can support exactness or an upper
policy.

## Certificate and provenance identity

Use one internal proof record attached to retained solve rows. Its semantic
identity is the collision-checked full tuple below; a 64-bit hash may be an
index only and must verify the full tuple on hit:

```text
source cell:
  coarse parent StableKey
  + existing canonical required ObservationRequirement
  + existing canonical observed FeatureSignature

operation:
  SelectedAction.semantic_key
  + existing selected runtime-contract/program identity
  + exact choice-recipe identity when the operation has runtime choices

coverage/provenance:
  immutable session/layout/goal identity
  + strict kernel reuse key or exact row replay token
  + collision-checked source-carrier coverage identity
  + normalized total probability and labeled projected arcs

dependencies (validation, not semantic state identity):
  source partition generation
  + every target cell id and target partition generation
  + action-vocabulary/admission generation
```

The tuple must reuse existing serialization/canonicalization helpers. Do not
invent an independent “quotient signature.” Partition generation is a stale
cache token, not part of the durable cell identity; equal cells rebuilt in a
new generation retain equal semantic identities.

The certificate must retain exact total mass and deterministic summation
evidence. A coverage hash alone is insufficient. Before Phase 2, choose and
test one collision-checked coverage representation that supports replay under
the cap; see unresolved decisions below.

## Frozen representation decisions

### Coverage and live slices

Gate 1 starts with a collision-checked replay descriptor containing the
existing strict kernel/support identity; a deterministic normalized replay
recipe; immutable session, layout, goal, artifact, operation, and relevant
solver identities; exact covered-source count and probability mass; and full
tuple validation on every cache hit. It must replay the exact covered
carriers after a split. A hash or representative carrier is never proof.

There is no global retained exact-carrier vector. When the kernel cannot
replay a split slice directly, only that affected slice may retain an explicit
deterministic range or chunk. Every live slice is scheduled under a declared
ledger-derived byte budget, with current and peak live slice count and bytes
reported. Backpressure, release, or replay must not discard admitted work or
weaken exactness.

### Dependency ownership

`SolveTransitionCache` owns, or directly owns through one compact sidecar,
stable row IDs, source-cell use sites, target-cell reverse dependencies,
source/target generations, action/admission generations, and deterministic
invalidation worklists. Immutable equal-row payloads may be shared, but their
generation-stamped use sites may not be shared. Dependency generations are
validity tokens rather than durable semantic identity.

### Reference adapter and compaction

Reconstruct-then-merge remains only a bounded synthetic parity oracle or
debug assertion. A reference-adapter invocation on the medium production path
or any frozen acceptance case is red; it is never a production fallback.

No arbitrary compaction threshold is permitted. First measure the complete
ledger. Add compaction only when the unchanged 1 GiB cap, measured fixed
costs, variance, required live work, and replay cost demonstrate that it is
required and determine its limit.

## Cache invalidation rules

Structural row/certificate reuse is price-independent when all operator
variants remain represented, matching `SolveTransitionCache` today. Apply
these rules:

- source-cell split: invalidate every row whose source coverage spans more
  than one child; rows proven wholly inside one child may be reattached after
  full-tuple validation;
- target-cell split: invalidate/reproject every row naming the old target,
  using an explicit reverse dependency index;
- observation requirement growth or routing-program change: invalidate the
  affected source cell, its rows, and transitive predecessors;
- semantic action/choice-recipe/kernel identity change: invalidate only rows
  carrying that identity, then schedule their source states;
- admitted-action vocabulary or legality change: invalidate policy/Q caches
  and affected row certificates according to existing option/cache
  compatibility; never silently retain a proof over a narrower vocabulary;
- price-only change: retain structural transitions/certificates, but invalidate
  priced rows, Q values, selected policy, SCC values, and executable upper
  cost;
- value-only change after a valid split: retain structural proof, invalidate
  kernel-value caches and schedule predecessor Bellman backups; and
- session, artifact, goal, start, cap-relevant solver option, or action-mode
  mismatch: reject the cache through existing compatibility boundaries.

No cached row may be used for exactness, upper publication, compilation, or
properness when any dependency generation is stale. Telemetry must separately
count structural proof reuse, row reprojection, source splits, target-driven
invalidations, exact carrier replays, and fallback to the reference adapter.

## In-solve counterexample-guided loop

The policy-improvement loop becomes:

1. Solve the current coarse/quotient relaxation with existing action
   filtering and Bellman machinery. Uncertified aggregation is marked lower
   only.
2. For the selected row at each policy-reachable quotient cell, obtain or
   replay exact transition evidence through strict authority. Accumulate mass
   into current target cells while retaining the certificate above.
3. Submit each closed affected proof slice to the existing shared partition.
   A shared-partition split installs child cells and a canonical
   `RefinementCounterexample`; it does not create a second incremental
   partition algorithm.
4. Invalidate dependent rows, update state-row ownership/reverse predecessors,
   and schedule Bellman backups through the existing incremental worklist.
5. Evaluate admitted alternatives only in witness-affected cells. The action
   comes from the existing filtered vocabulary; the witness never prescribes
   Exalt or another named mechanic.
6. Re-run policy improvement/SCC evaluation until no selected-row certificate
   is stale and the selected quotient policy is closed, lumpable, and proper,
   or a declared cap is reached.
7. Build `RefinedPolicyCompileRouting` from that certified quotient, compile
   the unchanged strategy document, parse it, exact-evaluate it independently,
   and reconcile its cost before publication.

An implementation that reconstructs the complete strict graph before step 3
has not met the structural objective even if the two-goal case happens to pass.

## Bellman and policy correctness

### Lower bound

The broad/coarse graph remains an optimistic minimization relaxation. A
quotient row may contribute to the lower calculation when it is exact for its
current cell; unresolved alternatives retain their existing optimistic lower
envelopes. Splitting a cell may raise or preserve the global lower bound, but
must never lower it through value mixing or discard an admitted alternative.
Store lower provenance so a displayed exact gap cannot use an uncertified row.

### Upper policy

An upper bound exists only for a complete executable policy. Every reachable
selected row must have a current certificate; all target dependencies must be
closed; the shared partition must be lumpable; bottom SCCs must be terminal;
the shared exact evaluator must converge; and compiled exact evaluation must
reconcile. A partial quotient, unknown frontier mass, or stale proof may guide
search but cannot publish an upper.

### Cycles

Splits are monotone within one partition generation. A source/target split
invalidates cyclic dependents through the predecessor worklist, then the
existing SCC evaluator solves the current fixed policy. Properness remains the
existing structural rule: every reachable bottom SCC is terminal. The
four-node open-graph witness and improper two-state cycle remain mandatory
focused regressions.

### Multiple-entry distributions

A caller may enter a quotient region through several exact carriers. Retain
the full entry distribution over certified cells; do not choose a
representative or average values across cells. Equivalent entries may combine
only after the shared partition proves equal selected behavior and projected
mass. Root value is the deterministic probability-weighted expectation over
entry-cell values, and every entry cell must be proper for an executable
upper.

### Compilation

Class ids remain internal and deterministic. Final routes are authored from
the existing observation requirement/signature authority and
`RefinedPolicyCompileRouting`; the strategy JSON vocabulary does not change.
Exact evaluator and simulator routing consume the same observation program.
Compatibility remains a final assertion, not a substitute for proof.

## Qualification levels

Qualification claims remain deliberately separate:

- **Core structural qualification** binds the frozen red two-goal case, the
  qualified Fracture full-four regression, and the selected natural
  representative four-goal case. Each must carry exact proof through
  properness, compilation, reconciliation, and verification at or below 1 GiB
  with zero production reference-adapter invocations.
- **Five-goal scale qualification** binds only the selected five-goal case. A
  time or memory refusal does not erase an otherwise successful core result;
  the exact report is then “core quotient qualified; five-goal scale
  unqualified,” and that evidence becomes the next structural boundary.
- **WASM product qualification** is separate from native correctness. A native
  success followed by a declared WASM timeout leaves release performance
  unqualified without retroactively disproving the quotient.

## Staged implementation gates

Focused refinement suites may run when they directly validate or diagnose the
current gate. Routine complete suites run once at the final boundary.

### Gate 0 — frozen provenance, fixtures, and predictions

Gate 0 is frozen by the linked evidence file. It preserves:

- shared refinement `301/0` and policy refinement `4,829/0`;
- `natural-t1-breadth-two-4e65dda9c53b`: 183,062 exact carriers,
  423,756 transitions, 10,466 kernels, 1,089,111,449 peak owned bytes, and
  zero partition rounds;
- the Fracture six-parent, 217-root, 927-state invariants and frozen hashes;
- `reliability-class-belt` as the medium integration case;
- `natural-t1-representative-four-62bcfa21ebfe` as the representative four;
  and
- generated natural feasible `natural-t1-scale-five-d432b26dfce2`, Runic
  Gauntlets, item level 86, `2P3S`, Mirage economy, existing goal-relevant
  envelope, 1 GiB, and 900 seconds.

The medium acceptance must exercise quotient cells, Bellman selection, at
least one split, reverse invalidation, policy repair, properness, and
compilation at a small fraction of the hard two-goal cost. Gate 0 used pool
structure, the existing action envelope, archived graph evidence, and matched
small native/WASM measurements; it did not run the archived 467-second case or
construct a complete four/five-goal strict graph. Hard cases are initially
classified native-binding/WASM-reported because the measured slowdown leaves
no defensible 900-second WASM headroom; this may be tightened, never silently
relaxed, from Gate 3 measurements.

### Gate 1 — proof identity, memory accounting, and invalidation

Implement the proof types and focused harness before production integration.
Certified identity includes source coarse parent, current observation
requirement and feature signature, semantic action, runtime program, exact
choice recipe, session/layout/goal/artifact/solver identity, strict kernel or
replay authority, exact source coverage, total probability, and ordered
labeled projected arcs.

Focused tests cover full-tuple collision validation including deliberately
colliding hashes; source and target splits; requirement and routing-program
growth; action, choice-recipe, kernel, and admission changes; price-only
structural reuse with Q/policy invalidation; session, goal, artifact, start,
and option mismatch; shared immutable rows with independent generations;
deterministic replay and mass preservation; and stale/corrupt rejection.

An independent ledger-conservation witness starts from a snapshot, allocates
payloads, certificates, sidecar records, reverse edges, coverage descriptors,
and live slices, exercises growth/sharing/invalidation/replay/release, and
checks production bytes against an independently calculated capacity model.
Shared payloads are charged once, use sites individually, temporary charges
return to zero, and the final ledger equals the starting snapshot. RSS is only
a sanity comparison.

**Early stop:** stop before production integration and archive the smallest
failure if no bounded representation can replay exact split coverage, reject
full-key collisions, conserve probability and the ledger, and avoid global
strict-carrier retention.

### Gate 2 — shared-partition CEGAR integration

Reuse `ActionRefinementContract`, observation propagation,
`FeatureSignature`, and `refine_closed_probabilistic_partition`. External
frontiers stay distinct until certified and an open graph is never submitted
as a closed probabilistic partition. A split installs canonical children and
an existing `RefinementCounterexample`; source and target dependencies
invalidate deterministically; stale rows replay or reproject before exact or
upper use; complete entry distributions remain intact.

Focused witnesses are the four-node cyclic counterexample, improper two-node
cycle, splitting multiple-entry distribution, equal-row cycle that remains
merged, source split, target split with predecessor invalidation, requirement
growth, external/internal frontier separation, exact projected-mass
reconciliation, and deterministic repeated construction.

**Early stop:** if the four-node cycle cannot be represented without the
complete strict graph or an equivalently unbounded carrier population,
archive a red structural report and do not begin Bellman integration.

### Gate 3 — Bellman integration and medium projection

Integrate quotient cells and certified rows with `SolveTransitionCache`,
stable row ownership, state-row spans, reverse scheduling, Bellman Q backups,
SCC evaluation, policy improvement, lower/upper provenance, and the unchanged
filtered vocabulary. Uncertified aggregation is lower-only. Executable uppers
require current certificates for every reachable selected row. Splits
invalidate dependent rows, values, SCCs, and predecessors while unrelated
payloads remain reusable. Price changes retain structural transitions but
invalidate prices/Q/policy. Counterexamples never select a named action,
alternatives survive splits, incompatible mixing cannot lower the bound, and
multiple entries/cycles use existing proper-policy authority.

Telemetry separately reports proof reuse, reprojection, source/target splits,
reverse invalidations, carrier replay, current/peak live slices and bytes,
coverage, certificate, sidecar, partition, carrier, row/kernel, scratch, total
owned bytes, and reference-adapter calls. Run the frozen medium case before a
hard case; any production reference-adapter invocation is red.

Use Gate 1–3 data to publish a ranged two-goal prediction for quotient cells,
certified rows, retained coverage, peak live slices, every ledger category,
total owned bytes, and wall time. Compare it with the eventual run. Carrier
count is diagnostic; total solver-owned bytes is binding. Report proof bytes
and their percentage of total without imposing an arbitrary allowance.

**Early stop:** stop before the hard run only for a measured lower bound above
1 GiB or demonstrated unbounded growth, never for a rough pessimistic
extrapolation.

Gate 3 is qualified. The production path discovers one exact carrier at a
time, interns collision-checked compact raw kernels, retains only strict replay
locators plus payload-id sidecars, and invokes the replay-backed entry to the
shared split-only partition authority. It never retains the complete strict
carrier-key population or a carrier-to-carrier adjacency graph. The bounded
materialized implementation remains an uncalled test/debug oracle only.

The frozen `reliability-class-belt` run completed `bounded_feasible` in
770.817 ms total and 33.4882 ms solve time. Independent exact strategy
evaluation matched. It retained 266 locator-covered carriers in 19 final
classes, replayed 2,394 carriers through five shared-partition rounds, and
reported 204 certified row projections, 173 immutable-payload reuses, 15
source splits, 15 target splits, 87 reverse invalidations, 11 Bellman policy
changes, one live slice peaking at 28,040 bytes, 3,483,686 quotient-local
solver-owned bytes, and zero reference-adapter calls. The focused suite is
250/0 and includes deterministic improper-policy repair; the integrated case
exercises policy repair/improvement, properness, exact reconciliation, and
compilation.

For the frozen two-goal case, the Gate 3 point prediction is 13,076 final
cells, 10,518 certified sparse-row use sites, 10,466 shared raw-kernel
payloads, 1,647,558 replay visits, one live carrier slice peaking near 7 MB,
189,014,112 proof bytes, 561,416,670 peak total solver-owned bytes, and 560
seconds wall. The declared ranges are 9,600-18,000 cells, 430-760 MB total,
and 470-700 seconds. Proof overhead is 33.67% of the point peak; this is a
measurement-derived prediction, not an allowance. Neither the point nor upper
range establishes a lower bound above 1 GiB, so the Gate 3 early stop did not
fire and the fresh hard run remains binding.

### Gate 4 — properness, compilation, and reconciliation

Publish only when all reachable selected rows have current certificates,
target dependencies are closed, the shared partition is lumpable, reachable
bottom SCCs are terminal, exact policy evaluation is proper for every entry,
and no incompatible value mixing remains. Build existing
`RefinedPolicyCompileRouting`; do not compile provisional quotient IDs. The
unchanged JSON must parse, independently exact-evaluate, reconcile at the
root, route zero off-policy, and pass 10,000-run zero-off-policy simulation
when verification is required. Diagnose compiler issues on focused/medium
cases before any hard solve.

Gate 4 is qualified. `QuotientPublicationAudit` is now binding before a
Bellman result becomes executable: every selected-reachable row is current,
its generation-stamped target dependencies are closed, every reachable bottom
component has a terminal path, and every entry has a finite proper exact
value. Certified dead-end alternatives remain retained for lower reasoning,
but only rows whose complete support stays inside the certified terminal
attractor can support the upper. An exact successor without an inherited
coarse selection enters the existing local optimizer with the complete
already-filtered vocabulary and is still checked by per-state admission.

Focused quotient proof is 259/0, the frozen policy-refinement baseline is
4,829/0, and compiler routing is 750/0. The fresh medium run completed
`bounded_feasible` in 1,331.4421 ms including 10,000 simulations. Its unchanged
strategy vocabulary parsed to 14 nodes and 33 edges, independently evaluated
to `9.14379257789546` versus the solver root `9.143792577895411`, had exact
off-policy mass zero, and completed all 10,000 simulations successfully with
zero off-policy failures. The corrected row/kernel ledger is 181,604 bytes,
the native peak is 79,085,896 bytes, and reference-adapter calls remain zero.

### Gate 5 — native structural and scale acceptance

Run each expensive solve once where practical, carrying it through
publication, compilation, exact evaluation, reconciliation, and 10,000-run
simulation.

1. Frozen two-goal: at or below 1 GiB/900 seconds, no complete strict
   reconstruction or reference fallback, executable publication, exact
   reconciliation, zero off-policy, and prediction-versus-actual reporting.
2. Qualified Fracture full-four: preserve six parents, 217 root Chaos
   successors, 927 states, and transition/policy/compiled hashes.
3. Representative four: satisfy its frozen feasible contract at or below
   1 GiB/900 seconds, with no full reconstruction or fallback, executable
   publication, reconciliation, and 10,000-run verification.
4. Five-goal scale: run at 1 GiB/900 seconds. Success qualifies five-goal
   scale. Refusal is preserved honestly as the next structural boundary and
   does not invalidate a successful core.

Then run the existing 27-case smoke corpus, 49-case native reliability
portfolio, and selected Ring/Gloves 10,000-run verification. A red binding
core result is architectural evidence, not permission to raise a limit.

Gate 5 stopped on its first binding case. The frozen two-goal run returned
`refused_resource_cap` on `max_reforge_work` after four exact carriers, two
kernels, and 345,192 exact transitions, before partition initialization or
policy publication. Native peak owned memory was `375,483,167` bytes and
reference-adapter calls remained zero. Because this binding core result is
red, the remaining native cases, portfolios, five-goal run, and Gate 6 were
not run. Core quotient, five-goal scale, and WASM product qualification are
all unqualified; the full evidence and next-architecture recommendation are
in the sibling [report](report.md).

### Gate 6 — release WASM and final acceptance

Rebuild with `powershell -File scripts/build-wasm.ps1`. Run every frozen case
under its Gate 0 classification. WASM-binding cases require matching
publication, compilation, reconciliation, correctness, memory, watchdog, and
10,000-run verification. Native-binding/WASM-reported cases still run under
the declared product watchdog and report completion/refusal, memory, wall
time, slicing, and cancellation honestly.

Verify all 61 exports and ABI 2, run the release-WASM reliability corpus,
`npm test`, `npx tsc --noEmit`, and one final
`powershell -File scripts/test.ps1`. Do not perform visual review.

## Completion and next boundary

Use one local commit per completed logical gate and do not squash useful gate
boundaries. At completion or an explicit structural early stop, produce a
complete report, archive this plan, update indexes and `HANDOFF.md`, preserve
the smallest failed witness, distinguish the three qualification levels, and
leave a clean unpushed worktree.

Checkpoint/replay remains deferred. If core and five-goal qualification both
succeed, record the stable post-quotient identity boundary and select
deterministic replay next. If core succeeds but five goals fail, select the
five-goal evidence before replay. If a proof gate fails, recommend the next
architecture without spending the hard acceptance runs.
