# Policy-Guided Exact Refinement Report

**Status: measured qualification stop.**

Verified in the working tree based on
`acb9c975a8c04418536c1b3fa91cde8fe8d2e452` on
`codex/policy-guided-exact-refinement`, 2026-07-31. The final documentation and
source commit is recorded in the root handoff after it is created.

## Outcome

The implementation establishes one shared, engine-owned chain from action
contracts through exact partitioning, evaluation, compilation, and executable
verification. It retains collision-checked identities, shared transition and
selection authorities, exact row reuse, key deduplication, memory ledgers,
counterexamples, and witness-local re-optimization.

The required natural two-goal qualification nevertheless failed the unchanged
1 GiB publication cap. Per Oliver's decision gate, no further memory/lifetime
micro-optimization, long hard-case rerun, release-WASM build, portfolio run,
or structural implementation was started.

## Exact production phase ordering

The audited production adapter operates in this order:

1. `ProductionPolicyOracle` derives backward coarse-policy observation
   requirements from the existing admitted `SelectedAction` contracts,
   imports planner operators by semantic identity, creates the strict
   `CalcContext`, and interns the exact start carrier.
2. `discover_policy_graph` requests exact roots and selected semantic actions.
   `exact_kernel` asks strict Calculator/primitive/option authority for a full
   stochastic row.
3. Successor generation interns every strict successor first in the strict
   context and adapter carrier table and then in the refinement graph. The
   collision-checked `policy_collapse_key`/canonical-operation fast path may
   reuse a proven fixed-policy row, but local candidate re-optimization keeps
   distinct carriers.
4. Only after the selected-policy successor closure is complete does
   `canonicalize_graph` order exact identities and remap absolute edges.
5. `refine_selected_action_routing` completes route observations, then
   `propagate_policy_observations` applies the same action contracts backward
   to a fixed point.
6. Each strict carrier is projected to its canonical required observation.
   The final post-projection change releases wider unobserved feature payload,
   while retaining exact carrier identity, projected observation, strict rows,
   shared row sources, and proof inputs.
7. Closed nodes are materialized. Only then can
   `refine_closed_probabilistic_partition` seed observation classes and run
   split-only refinement by immediate behavior and successor-class
   probability to a deterministic lumpability fixed point.
8. A complete result would build counterexamples/classes, run
   `evaluate_refined_policy_exact`, perform witness-local policy improvement
   where required, compile through existing strategy authority, parse the
   artifact, and independently exact-evaluate/reconcile it before publication.

The final gate stopped between steps 6 and 7: the observation fixed point was
complete, but closed-input materialization/pre-partition initialization
exceeded memory. Partition rounds and initial/final class counts remained
zero.

## Why open-graph early aggregation is disproved

The existing shared partition requires a closed stochastic graph because
equivalence depends on probability mass into the final successor classes. A
split-only partition cannot repair carriers that were discarded before their
successor behavior was known.

The focused regression freezes the minimal cyclic witness:

```text
x and y: same initial observation and same immediate behavior
x -> 0.5 y       + 0.5 success
y -> 0.5 x       + 0.5 failure
success and failure: distinct terminal observations
```

The initial partition has three classes (`{x,y}`, success, failure). Closing
successor behavior forces four final classes because `x` and `y` have
different probability into the terminal classes. Aggregating `{x,y}` while
the graph is open destroys that witness and can certify a false row. The new
test invokes the existing `refine_closed_probabilistic_partition`; it does not
introduce another contract, signature, or refinement engine.

This rejects only the shortcut “merge raw successors before their behavior is
closed.” It does not reject proof-carrying quotient transitions whose retained
provenance names the current source/target cells, coverage, dependencies, and
partition version and is invalidated when any dependency splits.

## Final decision-gate measurement

Command shape:

```powershell
py -3 tools/ingest/benchmark_solver_corpus.py `
  --root $Repo --executable build/engine/poecraft_solver_benchmark.exe `
  --artifact data/compiled/current `
  --corpus fixtures/solver-reliability/v1/manifest.json `
  --output build/acceptance/policy-guided-exact-refinement/diagnostic-natural-projected-carrier-release `
  --max-workers 1 --watchdog-ceiling-seconds 900 `
  --goal-progress-gated-reforges `
  --case natural-t1-breadth-two-4e65dda9c53b
```

| Field | Result |
| --- | ---: |
| Wall time | `467.3043269 s` |
| Solver-owned peak | `1,089,111,449 bytes` |
| Cap | `1,073,741,824 bytes` |
| Exact carriers retained | `183,062` |
| Exact transitions | `423,756` |
| Exact kernels | `10,466` |
| Exact kernel cache hits | `162,130` |
| Exact-state reuses | `240,695` |
| Backward observation rounds | `3` |
| Shared observation rounds | `1` |
| Partition rounds / initial classes | `0 / 0` |
| Published policy | no |
| Compile / exact evaluation / simulation | not applicable |

The process working set fell from roughly 989 MiB to 338 MiB at the new
post-projection release point, proving the lifecycle change executed. The
solver-owned preflight still exceeded the cap by `15,369,625` bytes before a
closed partition could begin. This is close enough to confirm that another
small allocation change might move this one case, but the already-heavy
two-goal cost and complete strict reconstruction make that the wrong scaling
direction for four and five goals.

## Focused verification

After the final source edit, the native benchmark and test executables were
rebuilt with the repository's release fallback flags. Focused results:

- shared refinement: `301` checks, `0` failures;
- policy refinement/lifting: `4,829` checks, `0` failures; and
- `git diff --check`: clean at the verification point.

The broader acceptance sequence was intentionally not run after the red gate.
In particular, there is no claim for the remaining solver slices, native
portfolio, Fracture hash gate, selected 10,000-run cases, release WASM, web
acceptance, or `scripts/test.ps1`.

## Preserved implementation

The retained code is useful foundation for the structural follow-on:

- shared action-contract and observation authority;
- semantic operator import and collision-checked `StableKey` identities;
- strict Calculator/primitive/option kernel authority;
- selection and absolute-row sharing plus stable row-reuse proofs;
- compact graph indices and exact-key deduplication;
- bounded memory/work/round ledgers and named cap propagation;
- shared backward observation fixed point;
- shared closed probabilistic partition and final lumpability proof;
- canonical counterexamples and improper-component witnesses;
- shared exact SCC evaluator;
- witness-local candidate evaluation and Bellman comparison;
- existing strategy compiler/router format and independent compiled assertion;
- compatibility refusal as the final publication guard; and
- all existing action filtering, mechanics decisions, prices, and cap values.

The retained reconstruct-then-merge adapter is a correctness bridge for small
graphs and a reference oracle for focused parity tests. It is not the intended
four/five-goal execution architecture.

## Evidence provenance

Tracked summary:
`fixtures/solver-reliability/v1/evidence/policy-guided-exact-refinement-decision-gate.json`.

Ignored reproducible raw artifacts:

- case report:
  `build/acceptance/policy-guided-exact-refinement/diagnostic-natural-projected-carrier-release/cases/natural-t1-breadth-two-4e65dda9c53b.json`,
  SHA-256 `cf3feb1a36aeb559bb5f4bdc8b0a160639d3310e3a7f4377218365eb85734ba6`;
- run ledger:
  `build/acceptance/policy-guided-exact-refinement/diagnostic-natural-projected-carrier-release/ledger.json`,
  SHA-256 `587f3c1058e9c008ed12a1447eef33932589460efc570208b1483d6e4290c6c6`;
- benchmark executable:
  `build/engine/poecraft_solver_benchmark.exe`, SHA-256
  `808d3521ce5ed2b55ef03e8e55afc9aab862e496ead11940ad614894b10e677d`;
- focused test executable: `build/engine/poecraft_engine_tests.exe`, SHA-256
  `4969f073b37331f94005b0d0b29052863cd3ec441ce55ae80d7ae55defb4e996`.
