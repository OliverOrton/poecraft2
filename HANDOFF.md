# Session Handoff

**Status: the reconstruct-then-merge milestone is archived after a red 1 GiB
decision gate. A separate proof-carrying quotient milestone is selected and
implementation-ready, but implementation has not begun.**

Current plan:
[Proof-Carrying Quotient Refinement During Solving](docs/active/proof-carrying-quotient-refinement.md).

Archived result:
[Policy-Guided Exact Refinement Qualification Stop](docs/archive/2026-07-31-policy-guided-exact-refinement/README.md).

Branch: `codex/policy-guided-exact-refinement`.

Source at the start of the qualification continuation:
`acb9c975a8c04418536c1b3fa91cde8fe8d2e452`. The local co-authored
qualification-stop commit containing this handoff is the continuation boundary.
Nothing was pushed and `main` was not changed.

## Decision-gate result

Oliver required one final unchanged-cap natural run after the post-projection
payload release. It failed the substantive publication gate:

| Field | Result |
| --- | ---: |
| Case | `natural-t1-breadth-two-4e65dda9c53b` |
| Wall time | `467.3043269 s` |
| Solver-owned peak | `1,089,111,449 bytes` |
| Existing cap | `1,073,741,824 bytes` |
| Exact carriers retained | `183,062` |
| Exact transitions / kernels | `423,756 / 10,466` |
| Kernel-cache hits / state reuses | `162,130 / 240,695` |
| Backward / observation rounds | `3 / 1` |
| Partition rounds / initial classes | `0 / 0` |
| Policy | not published |
| Compile / exact evaluation / simulation | not applicable |

The process working set dropped from roughly 989 MiB to 338 MiB at the new
post-projection release point, so that lifecycle change executed. The
solver-owned preflight still exceeded the cap by `15,369,625` bytes during
closed-input materialization/pre-partition initialization. No partition,
lumpability proof, class-policy properness check, compiler assertion, exact
evaluation, or simulation ran.

This was the instructed decision gate. Do not perform another memory/lifetime
micro-optimization or full hard-case rerun without Oliver's explicit approval.
Do not raise the cap.

Tracked evidence:
[`policy-guided-exact-refinement-decision-gate.json`](fixtures/solver-reliability/v1/evidence/policy-guided-exact-refinement-decision-gate.json).
The raw ignored report is under
`build/acceptance/policy-guided-exact-refinement/diagnostic-natural-projected-carrier-release/`.

## Focused verification completed

The native benchmark and focused test executables were rebuilt after the last
source edit with the repository's release fallback flags.

- shared refinement: `301` checks, `0` failures;
- policy refinement/lifting: `4,829` checks, `0` failures; and
- the minimum cyclic carrier witness uses the existing shared partition and
  passes.

Not run after the red gate: the remaining native solver slices, Gate 4
portfolio, frozen Fracture hash gate, 27/49-case portfolios, selected
10,000-run verification, release WASM, web tests, `scripts/test.ps1`, or visual
review. Do not describe them as accepted.

## Exact audited phase ordering

1. `ProductionPolicyOracle` derives backward requirements from admitted
   `SelectedAction` contracts, imports semantic operators, creates the strict
   `CalcContext`, and interns the exact start.
2. `discover_policy_graph` requests selected exact kernels.
3. Successor generation interns every strict successor in the strict context,
   adapter carrier map, and refinement graph. The existing
   `policy_collapse_key` may reuse a proven fixed-policy identity; local
   re-optimization retains strict carriers.
4. Only after successor closure does `canonicalize_graph` order identities
   and remap absolute edges.
5. Selected-action routing and `propagate_policy_observations` reach the
   contract-derived backward fixed point.
6. Exact features are projected to the required observation; wider dead
   payload is released, but identities, strict rows, row sharing, and proof
   inputs remain.
7. Closed nodes are materialized. Only a complete closed graph may enter
   `refine_closed_probabilistic_partition`, which seeds observation classes
   and performs split-only successor-mass refinement and lumpability proof.
8. Complete classes would then run the exact SCC evaluator, witness-local
   policy improvement, unchanged strategy compiler, parsed exact evaluation,
   cost reconciliation, and simulation assertion.

The gate stopped in step 7 before the shared partition's first assignment.

## Soundness result

Open-graph early aggregation is unsound. The frozen minimal witness is:

```text
x -> 0.5 y + 0.5 success
y -> 0.5 x + 0.5 failure
```

`x` and `y` have equal initial observation and immediate behavior; success and
failure have distinct terminal observations. The initial three classes must
split to four after successor behavior closes. Discarding `x`/`y` identity
while the graph is open loses the proof. The regression is in
`engine/tests/test_solver_refinement.cpp` and calls
`refine_closed_probabilistic_partition`; no parallel contract, signature, or
refinement subsystem was added.

This does not disprove proof-carrying quotient transitions. They retain
source-carrier coverage, current source/target cell identities, exact
projected mass, dependencies, partition generations, and replay witnesses;
target/source splits invalidate them before exactness or publication.

## Retained reusable authorities

The next milestone must reuse, not duplicate:

- `ActionRefinementContract`, `SelectedAction`, observation requirements and
  compiled observation programs;
- collision-checked `StableKey`, semantic operator identity, and
  `canonical_operation_state_signature`;
- strict `CalcContext` primitive/option kernels and exact choice recipes;
- `SolveTransitionCache`, stable global sparse-row IDs, shared variant arena,
  state-row spans, and existing price-independent cache compatibility;
- Bellman, SCC, policy-improvement, predecessor, and incremental worklist code
  in `solver_solve_bellman.cpp` / `solver_solve_incremental.cpp`;
- `propagate_policy_observations` and
  `refine_closed_probabilistic_partition`;
- `RefinementCounterexample`, improper-component witnesses, compatibility
  witnesses, and diagnostic sample caps;
- `evaluate_refined_policy_exact`;
- `RefinedPolicyCompileRouting`, `compile_policy_strategy_json`, and
  `assert_compiled_policy_exact`; and
- existing solver-owned memory/work/round accounting and benchmark evidence.

The current reconstruct-then-merge adapter remains a small-case correctness
oracle/reference. It is not the four/five-goal production endpoint.

## Next implementation boundary

Implement Gate 1 from the active plan: proof identity, cache attachment, and
deterministic invalidation tests only. Do not begin with a full product case.

The proof record's collision-checked semantic tuple must reuse existing
authorities and contain:

- source coarse `StableKey`, canonical required observations, and observed
  feature signature;
- `SelectedAction.semantic_key`, existing runtime-contract/program identity,
  and exact choice-recipe identity when applicable;
- immutable session/layout/goal identity, strict kernel reuse key or replay
  token, collision-checked source coverage, normalized probability mass, and
  labeled projected arcs; and
- validation dependencies for source/target partition generations and action
  vocabulary/admission generation.

Partition generations are stale-cache tokens, not a new semantic signature.
Hashes may index only after full-tuple equality. A coverage hash or
representative carrier is never proof.

Required invalidation tests: source split, target split, observation growth,
route/choice/action change, price-only structural reuse with Q invalidation,
vocabulary change, value-only predecessor scheduling, and session/goal/cache
compatibility rejection. Retained certificate and dependency bytes belong to
the unchanged 1 GiB ledger.

## Correctness requirements for the structural milestone

- The coarse graph stays an optimistic lower relaxation. Unresolved
  alternatives stay present; splits can raise/preserve but never weaken the
  lower proof through incompatible value mixing.
- An upper exists only for a complete certified executable policy. Every
  selected row is current, all frontier dependencies are closed, shared
  partitioning is lumpable, every reachable bottom SCC is terminal, exact SCC
  evaluation converges, and compiled exact cost reconciles.
- Cyclic splits invalidate dependents through existing predecessor worklists;
  no stale certificate participates in Bellman Q, properness, or compilation.
- Multiple-entry roots retain their distribution over certified cells. Never
  choose a representative or average incompatible cell values.
- Final routing uses the current strategy format and shared observation
  program. Exact evaluator and simulator remain independent publication
  assertions.

## Required staged acceptance

The active plan defines the full gates. Mandatory focused witnesses include:

- the four-node open-graph split;
- an improper non-terminal cycle;
- an equal-row cycle that remains merged;
- a multiple-entry distribution that splits;
- target-split predecessor invalidation;
- collision/full-key and row-sharing controls;
- Bellman lower monotonicity and no incompatible value mixing; and
- witness-local action change drawn only from the existing filtered
  vocabulary.

Native scale gates, all under the unchanged product cap:

1. two goals: `natural-t1-breadth-two-4e65dda9c53b` must publish, compile,
   exact-evaluate, reconcile, and verify inside 900 seconds;
2. four goals: preserve the qualified
   `natural-t1-full-four-47d8b909aa88` Fracture invariants and qualify a frozen
   representative-four natural case; and
3. five goals: qualify one owner-approved frozen case with native compilation,
   exact reconciliation, and 10,000-run zero-off-policy verification. A cap
   refusal is red and does not authorize a higher cap.

Then run 27-case smoke, native 49-case portfolio, selected Ring/Gloves 10,000
runs, release WASM copies of the two/four/five cases and portfolio, and finally
`powershell -File scripts/test.ps1` once. Oliver owns visual review.

## Unresolved decisions requiring Oliver

Do not guess:

1. the replayable, collision-checked source-carrier coverage representation;
2. where the compact reverse dependency index lives;
3. whether production may ever fall back to reconstruct-then-merge;
4. the exact five-goal fixture and its watchdog (no tracked exactly five-goal
   case exists today); and
5. any evidence-derived eviction/replay threshold.

The active plan gives allowed options and acceptance criteria. Resolve these
before implementation reaches the corresponding gate. Preserve mechanics,
current strategy format, existing action filtering, and the 1 GiB cap.

## Repository rules

- Local commits only unless Oliver explicitly asks to push.
- End commits with `Co-authored-by: Codex <codex@openai.com>`.
- Do not perform rendered/visual review without Oliver's request.
- Do not begin another full hard-case iteration without explicit approval.
- Do not start the deferred executable-anchor library or add mechanics.
