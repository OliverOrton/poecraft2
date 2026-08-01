# Proof-Carrying Quotient Refinement During Solving

**Status: selected structural follow-on; implementation has not begun.**

Owner: Oliver

Branch boundary: continue from the locally committed qualification-stop state
on `codex/policy-guided-exact-refinement`, or create a separately named branch
from that commit before editing.

Parent: [Active work](README.md)

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
- treat a bounded lower relaxation as an executable upper policy.

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

## Staged implementation gates

Run routine suites only at the final acceptance boundary; use the named
focused witnesses while developing each stage.

### Gate 0 — freeze provenance and acceptance inputs

- Preserve the archived red two-goal report and the current focused counts.
- Freeze exact IDs and artifact/economy hashes for:
  `natural-t1-breadth-two-4e65dda9c53b` (two goals),
  `natural-t1-full-four-47d8b909aa88` (qualified four goals), and one new
  owner-approved five-goal case selected as described under unresolved
  decisions.
- Keep the 1 GiB solver cap, existing action filter, prices, mechanics, and
  900-second ceiling unchanged.

### Gate 1 — proof identity and invalidation harness

- Add focused unit tests for full-tuple collision checks, source split,
  target split, requirement growth, action/choice change, price-only reuse,
  vocabulary invalidation, and session/goal mismatch.
- Freeze a row-sharing witness proving equal certified rows share one payload
  without sharing stale dependency generations.
- Freeze a key-dedup witness proving hashes never merge unequal full keys.

### Gate 2 — shared-partition CEGAR integration

- Route closed affected proof slices through
  `refine_closed_probabilistic_partition`.
- Pass the four-node cyclic witness, an improper two-node cycle, a
  multiple-entry distribution that splits, an equal-row cycle that remains
  merged, and a target-split predecessor invalidation witness.
- Prove no open frontier label is treated as an internal equal successor.

### Gate 3 — Bellman/policy improvement integration

- Install split children in `SolveTransitionCache`, retain stable global row
  ownership, and schedule existing predecessor worklists.
- Focused tests must show lower monotonicity, no incompatible value mixing,
  alternative preservation, price-only Q invalidation, cyclic SCC repair, and
  witness-local action changes without named-action logic.
- The two-goal native case must publish inside 1 GiB/900 seconds with materially
  less retained exact reconstruction than the archived 183,062-carrier bridge.

### Gate 4 — properness, compilation, and exact reconciliation

- Every policy-reachable selected row has a current certificate.
- Shared partition reports lumpable; shared exact SCC evaluation reports
  proper; compiled assertion reports proper, zero off-policy, and reconciled.
- Run native compilation/exact evaluation plus 10,000 simulator runs for the
  selected two-goal verification case.
- Preserve the qualified Fracture six-parent/217-root/927-state invariants and
  hashes exactly.

### Gate 5 — native scale acceptance

- Two goals: `natural-t1-breadth-two-4e65dda9c53b` must publish, compile,
  exact-evaluate, reconcile, and verify within 1 GiB/900 seconds.
- Four goals: run both the frozen Fracture
  `natural-t1-full-four-47d8b909aa88` non-regression and a natural
  representative-four case selected at Gate 0; both must satisfy their
  declared policy/verification contracts without reconstructing the complete
  strict policy graph.
- Five goals: run the frozen Gate-0 case under the same 1 GiB product cap and
  owner-approved watchdog; require a published bounded or exact executable
  policy, compile/exact reconciliation, and 10,000-run zero-off-policy
  verification. A resource refusal is a red structural gate, not permission
  to raise the cap.
- Then run the existing 27-case smoke, 49-case native reliability portfolio,
  and selected existing 10,000-run Ring/Gloves verification.

### Gate 6 — release WASM and final repository acceptance

- Rebuild through `powershell -File scripts/build-wasm.ps1`.
- Run the identical frozen two-, four-, and five-goal cases with
  `apps/web/test/solver-benchmark.ts` against release WASM; require the same
  publication, compilation, exact-evaluation, reconciliation, cap, and
  verification classifications as native.
- Run the complete release-WASM reliability corpus, then
  `powershell -File scripts/test.ps1` once.
- Record native/WASM memory, wall time, cooperative slice/cancel evidence,
  certificate reuse/invalidation counts, hashes, and 10,000-run results.
- No visual review unless Oliver explicitly requests it.

## Unresolved design decisions

Do not guess these during implementation:

1. **Carrier coverage representation.** Choose between an explicit replayable
   enumerator/range proof, an existing exact kernel support identity plus
   normalized enumeration evidence, or another collision-checked form. A hash
   or representative alone is forbidden. The choice must pass Gate 1 memory,
   collision, replay, and cap tests before solver integration.
2. **Dependency-index storage.** Decide whether reverse target dependencies
   live directly beside `SolveTransitionCache` rows or in a compact generation
   table. It must support deterministic invalidation and be included in the
   same memory ledger.
3. **Reference-adapter fallback scope.** Decide which small/debug cases may use
   reconstruct-then-merge as a parity oracle and whether production may ever
   fall back. Falling back on the two/four/five acceptance cases does not meet
   the milestone.
4. **Five-goal fixture.** No tracked case currently has exactly five goal
   slots. Before implementation, Oliver must approve one deterministic
   generated or authored case and its watchdog. Recommended selection
   criteria—not a mechanic ruling—are a feasible natural T1 `3P2S` or `2P3S`
   goal, the existing goal-relevant product action envelope, frozen Mirage
   economy/artifact identity, and no manufactured fallback recipe.
5. **Compaction threshold.** Any eviction/replay threshold must be derived from
   the 1 GiB ledger and focused evidence; it may not silently weaken proof or
   alter public caps.

Once these decisions are recorded, Gate 1 is the next implementation boundary.
Do not begin with another full natural hard-case run.
