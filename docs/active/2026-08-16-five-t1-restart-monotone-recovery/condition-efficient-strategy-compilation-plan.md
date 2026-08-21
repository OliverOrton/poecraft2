# Condition-Efficient Strategy Compilation And Consolidation

**Status: complete and accepted.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](README.md)

Supersedes before implementation:
[Cooperative Exact-Reforge And WASM Graph Provenance](cooperative-reforge-wasm-graph-plan.md).

## Objective

Make solver-compiled strategies use the existing condition system efficiently.
Reduce dense routers, repeated same-target edges, redundant condition structure,
condition bytes, and graph size while preserving the exact policy domain,
priority behavior, product versus certification defaults, operation/accounting
semantics, and independent evaluation.

The binding hypothesis is Oliver's observed dense graph regions in which many
edges leave one router and converge on the same destination. The compiler
currently splits on the widest and most balanced feature, emits one branch per
feature value, and only then hash-conses byte-identical completed router
subtrees. It does not optimize for distinct continuation targets, outgoing
edge count, condition bytes, or existing `any` composition. This plan measures
that gap first and changes only what a complete routing proof authorizes.

The plan also pays adjacent technical debt by introducing one typed condition
authority and one reusable decision-DAG builder, then splitting condition,
routing, and emission responsibilities out of the monolithic compiler. It is
not a broad solver rewrite.

WASM responsiveness subdivision, browser-product presentation, and broad
solver-proof recovery are not active objectives. Normal native/WASM and full
repository acceptance still run after the compiler work is complete.

## Source-Confirmed Starting Point

1. The stored v1 condition vocabulary already supports nested `all`, `any`,
   `not`, and `at_least`; the native simulator evaluates them recursively.
   No new condition kind is required to express a union of same-target route
   branches.
2. `all_of()` and `any_of()` currently concatenate serialized JSON. They
   handle empty and singleton inputs and chunk very wide composites, but do
   not flatten nested equal operators, remove identities inside a composite,
   deduplicate exact children, or factor shared terms.
3. Both `build_refined_parent_route()` and `build_policy_route()` choose the
   varying feature with the most distinct values, breaking ties by the most
   balanced partition. Neither heuristic considers distinct child targets,
   repeated destinations, emitted edge count, condition size, or downstream
   subtree sharing.
4. Each selected feature-value group becomes a separate `PolicyRouteEdge`.
   Completed route nodes share only when their ordered destination IDs and
   serialized condition bytes are identical. Branches within one node are not
   grouped by destination.
5. Generated route edges receive stable increasing priorities and every route
   has a product-safe Restart or certification fail-closed default. Any
   coalescing must prove priority equivalence and preserve the exact accepted
   domain; matching the same action is not sufficient by itself.
6. The refined-parent builder intentionally continues splitting even when all
   represented members choose one target, because the conditions also prove
   the accepted domain. A compact result must retain that proof, normally as
   a union guard plus the same default, rather than deleting the router.
7. The native strategy compiler already structurally interns equivalent
   compiled conditions into memo slots and has priority-preserving dispatch
   acceleration. Reimplementing runtime condition caching is not this plan.
   The target is emitted graph quality and a smaller, clearer decision
   authority.
8. Current native Witness B is 92 nodes / 338 edges / 150,813 JSON bytes. It
   contains 84 policy routers, one local gated router, three primitive
   operations, four infrastructure nodes, 248 condition-bearing edges, and
   116,972 condition bytes. Conditions are 77.56% of its serialized graph.
9. The retained four-goal evidence reduced 767 Fracture route/operation copies
   to seven behaviors and reduced total nodes from 1,813 to 292, but the
   resulting graph still used 4,594,437 condition bytes in 4,737,473 JSON
   bytes. Conditions, not operation nodes or compile time, became the dominant
   representation cost.
10. Existing complete-behavior Fracture sharing and fixed-point gated
    operation sharing are retained. Current Witness B contains no selected
    Fracture operation. This plan does not add another Fracture key or alter
    action selection.

## Invariants

1. The emitted strategy must route every reachable concrete item to the same
   operation or terminal as the reference graph. Exact success/failure mass,
   expected cost, expected action/material consumption, edge accounting, and
   off-policy mass remain unchanged.
2. Compiler-generated route predicates may be combined only from a structural
   feature partition whose domains are proved disjoint, or by a transformation
   that preserves the original ordered first-match result for every input.
3. Product and certification graphs remain separate authorities. Product
   defaults retain their exact safe-Restart behavior; certification defaults
   remain fail-closed to `offpolicy`.
4. A router that proves a bounded or refined policy domain cannot be deleted
   merely because its represented members share a destination. Unrepresented
   values must still reach the existing default.
5. Condition canonicalization uses a typed internal representation. Hashes are
   bucket selectors only; complete structural equality grants reuse.
6. Deterministic child and edge order is preserved or replaced by a separately
   specified deterministic canonical order. Native and WASM must emit the same
   graph bytes for the same source, request, and economy.
7. The public v1 strategy vocabulary and native execution semantics remain
   unchanged in the main implementation path. A condition-reference table,
   BDD, or routing bytecode requires a separately selected versioned contract.
8. Compiler memory and output caps remain enforced on complete owned data.
   Telemetry is cap-accounted and cannot move a resource boundary except where
   the measured retained representation itself becomes smaller.
9. Solver expansion, Bellman comparisons, candidate admission, prices,
   mechanics, and policy selection are unchanged.
10. Existing result truth remains fail-closed. This work cannot turn a bounded
    upper into an exact optimum or invent a lower bound.

## Gate 0 - Dense-Route And Condition Census

**Result:** complete. See
[Gate 0 evidence](evidence/condition-efficient-gate0.md). The three frozen
graphs contain 140 proof-safe same-target sibling groups and an exact
817-edge reduction ceiling before additional condition factoring.

Persist the actual compiled strategy JSON, not only aggregate evaluator
telemetry, for the current native controls:

- Restart-free Witness A;
- priced-base five-natural-T1 Witness B;
- the dense four-natural-T1 case; and
- one small exact strategy that is easy to inspect exhaustively.

Add behavior-neutral, cap-accounted compiler diagnostics for:

- nodes and edges by owner, router out-degree, distinct destinations, and
  default mode;
- per-router same-target branch groups, adjacent and non-adjacent groups, and
  their priorities;
- groups known disjoint because they originate from distinct values of the
  same structural feature;
- exact condition literals, canonical expression fingerprints, repeated
  occurrences/bytes, operator counts, arity, nesting depth, and maximum size;
- common conjunction prefixes/suffixes and same-target union size using the
  existing condition vocabulary;
- route depth, feature chosen at each split, value-group cardinality, distinct
  child continuations, and the current width/balance heuristic score; and
- projected nodes, edges, condition bytes, and JSON bytes after each
  individually proof-safe transformation.

Record the downstream cost of the reference graphs: compiler time/peak bytes,
JSON parse/compile time, exact-evaluator pairs and routing time, Simulator
throughput, and serialized size. This establishes whether edge density is only
visual or also material to evaluation and memory.

Before implementation, publish numeric targets from the measured proof-safe
ceiling. The final result must remove every same-target sibling group that the
structural partition proves safely coalescible and realize a material share of
the measured byte/edge opportunity. Do not invent a target after seeing the
implementation result.

If the reported dense regions do not reproduce in current native output, stop
and obtain the exact saved strategy before changing the compiler.

## Gate 1 - Typed Canonical Condition Authority

**Result:** complete. See
[Gate 1 evidence](evidence/condition-efficient-gate1.md). Witness B retains
identical nodes, edges, value, terminal mass, and defaults while canonical
route composition removes 10,206 condition/JSON bytes.

Replace compiler-internal JSON-string composition with a typed immutable
condition expression used by feature extraction, route construction,
fingerprinting, size estimation, and final v1 serialization. Keep parsing and
execution authority in the native simulator.

Canonical constructors must initially perform only truth-preserving,
well-bounded rewrites:

- flatten nested `all` inside `all` and `any` inside `any`;
- remove `always` from conjunctions;
- preserve false-empty-`any` and true-empty-`all` identities;
- collapse singleton composites;
- deduplicate structurally identical children; and
- retain deterministic first-seen order unless a proved canonical order is
  required for complete expression interning.

Do not initially apply De Morgan transformations, distributive expansion,
complement elimination, arbitrary range fusion, or mechanics-aware predicate
implication. Those can increase expressions or require a stronger semantic
proof.

Add small truth-table controls for nested `all`/`any`/`not`/`at_least`, empty
and singleton identities, exact duplicate children, advanced compiler-only
conditions, and malformed/default behavior. Compare reference and canonical
expressions through native condition execution, not string reasoning alone.

## Gate 2 - Priority-Safe Same-Target Coalescing

**Result:** complete. See
[Gate 2 evidence](evidence/condition-efficient-gate2.md). All 140 measured
groups and the exact 817-edge ceiling are realized with independent exact
parity. Structural partitions permit non-adjacent grouping; refined
observation routes use only the separately proved adjacent-run rewrite.

At each compiler-generated decision node, group child branches that target the
same continuation. For branches created from different values of the selected
structural feature, retain their complete child guards and combine them with a
canonical `any` only after proving that the selected feature-value predicates
are mutually exclusive.

Preserve the original domain and default explicitly:

- the union condition admits exactly the old same-target branches;
- inputs outside every represented branch still take the old default;
- product Restart and certification `offpolicy` defaults do not merge;
- accounting roles remain attached to their existing operation edges; and
- no branch may move ahead of an overlapping different-target branch.

For a possible non-disjoint or hand-authored ordering, either derive an exact
priority-preserving formula or leave the edges separate. This plan changes
solver-generated routing only; it does not rewrite user-authored strategies.

Add focused positive controls with many disjoint feature values leading to one
target, mixed-target groups, and repeated convergence after deeper recursion.
Add negative controls for overlapping predicates, interleaved priorities,
bounded omitted values, differing defaults, different operation/accounting
targets, and product/certification pairing.

After coalescing, canonicalize the complete destination/condition sequence and
run the existing collision-checked route sharing. Every structurally
coalescible same-target sibling group identified by Gate 0 must disappear.

## Gate 3 - Continuation-Aware Reduced Decision DAG

**Decision:** skipped under this gate's measurement rule. Gate 2 leaves zero
same-target groups, emitted non-default edges equal distinct continuations,
and complete identical children remain signature-shared. There is no measured
node/edge reduction ceiling that justifies changing the split heuristic. See
[Gates 3-5 evidence](evidence/condition-efficient-gates3-5.md).

Use Gate 0 and Gate 2 results to decide whether the split heuristic still
creates material avoidable routers or condition bytes. If it does, replace the
width-first choice with a deterministic continuation-aware cost model over:

- distinct child continuation behaviors after recursive reduction;
- emitted edges and route nodes;
- estimated canonical condition bytes;
- route depth and default-domain proof; and
- reuse of already interned subgraphs.

Build bottom-up over structural feature/value partitions so identical reduced
children share by complete behavior. Treat the result as a reduced
multi-valued decision DAG, not a generic JSON deduper. A feature test may be
eliminated only when its complete represented-value union and upstream guard
preserve the old domain; identical action targets alone do not authorize
elimination.

Prefer a bounded greedy or memoized construction whose compiler work and
memory are predictable. Do not introduce an exponential exact decision-tree
optimizer. Retain the current widest/balanced builder as a reference oracle
until the frozen graphs pass exact parity.

If Gate 2 already realizes the measured opportunity and Gate 3 projects no
material additional gain, skip this gate rather than replacing a proven
heuristic for aesthetics.

## Gate 4 - Existing-Vocabulary Factoring

**Decision:** skipped after measurement. The remaining dominant payload is
distinct inline `observation_signature` leaves, whose repeated requirement
cannot be shared with the current v1 boolean vocabulary. A versioned reference
or observation-DAG contract remains deferred. See
[Gates 3-5 evidence](evidence/condition-efficient-gates3-5.md).

Only after the reduced DAG is measured, apply additional compact forms already
supported by v1 when they strictly reduce the serialized expression and have
an exact structural proof:

- combine contiguous exact count values into one existing min/max range;
- reuse `at_least` only where it is exactly equivalent to the prior boolean
  expression;
- factor shared conjunction terms around a same-target `any`; and
- reuse exact canonical expressions throughout compiler memory and graph
  construction.

Do not add condition references, a new condition kind, BDD nodes, or bytecode
in this gate. If inline v1 JSON remains the dominant cost after all existing-
vocabulary reductions, record the remaining unique/repeated byte census for a
later versioned representation plan.

## Gate 5 - Targeted Compiler Consolidation

**Result:** complete. Typed condition construction and route-DAG construction
now have explicit internal modules. Refined-parent and exact-policy recursion
share one builder with separate domain modes, and refined observation edges use
the same typed route staging before emission. See
[Gates 3-5 evidence](evidence/condition-efficient-gates3-5.md).

After behavior and representation are stable, split the touched compiler
responsibilities into explicit internal modules:

- condition expression construction/canonicalization/serialization;
- policy feature extraction and exact domain keys;
- reduced decision-DAG construction and interning; and
- strategy graph emission, accounting, caps, and telemetry.

Unify the duplicated recursive shape of refined-parent and exact-policy route
construction behind one decision-DAG engine with explicit mode callbacks for
their different stopping/domain rules. Do not erase that semantic difference:
the refined-parent path must continue proving represented domain where the
exact policy path may stop at one executable region.

Add or rename translation units through `engine/engine-sources.txt`. Remove old
string builders or route paths only after focused reference parity proves they
are superseded. Do not mechanically split unrelated evaluator, finalization,
reforge, Bellman, or action-mechanics files in this plan.

## Gate 6 - Bounded Result-Quality Controls

**Result:** complete. Transition/policy hashes, exact values, terminal mass,
action/material accounting, defaults, bounded classifications, cost mismatch,
carrier 5983 failure, and Fracture selection/non-selection are unchanged. See
[Gate 6 evidence](evidence/condition-efficient-gate6.md).

Re-run the frozen policies through independent exact graph evaluation. The
compiler change must not alter solver action selection or hide current proof
limitations.

Specifically preserve and report:

- Witness A's exact value, transition/policy hashes, success and off-policy
  mass;
- Witness B's exact compiled-policy value, action mix, success and off-policy
  mass;
- the solver-stored versus compiled-policy cost discrepancy;
- strict carrier 5983's current coarse-mapping status; and
- Fracture selection/non-selection and default behavior in the four- and
  five-goal controls.

Fixing the cost discrepancy, coarse mapping, lower bound, alternative
envelope, or Fracture Q completeness requires a later focused plan. If this
compiler work changes any of them, treat it as a regression or newly exposed
defect and stop rather than expanding scope.

## Gate 7 - Final Native, WASM, And Repository Acceptance

**Result:** complete. Native/WASM comparison passes 96 checks on the exact
four-T1 graph, both sides pass the pinned 10,000-run verification, focused
native and web suites pass, and the single final repository pipeline passes.
See [final evidence](evidence/condition-efficient-final.md).

After all selected implementation gates are complete, run the normal final
acceptance once:

1. native build and complete focused compiler, evaluator, simulator,
   refinement, and solver suites;
2. frozen small, Witness A, Witness B, and dense four-goal native graphs;
3. exact reference/new graph parity for routing, terminal mass, cost, action
   and material accounting, defaults, and off-policy behavior;
4. 10,000-run compiled-strategy verification for executable graph changes;
5. final release-WASM rebuild and identical native/WASM request comparison;
6. TypeScript checking and complete non-visual web tests;
7. `git diff --check`; and
8. one final `powershell -File scripts/test.ps1` invocation.

Record before/after nodes, edges, router density, distinct targets, condition
operators, repeated bytes, total/max condition bytes, JSON bytes, compiler
time/peak memory, parse time, exact-evaluation routing work, and Simulator
throughput. No frozen case may regress graph size or correctness without an
explicitly reviewed reason.

Oliver owns visual review. The agent does not perform Strategy Builder visual
inspection unless Oliver asks.

## Stop Conditions

Stop and write a precise handoff if:

1. dense same-target branches do not reproduce in the current native graph;
2. safe coalescing cannot prove mutual exclusion or ordered first-match parity;
3. an unrepresented state reaches an operation instead of the existing
   product/certification default;
4. exact value, terminal mass, action/material accounting, policy behavior, or
   non-representation telemetry changes;
5. a compact form requires mechanics-aware implication or approximate routing;
6. the reduced DAG requires exponential compile work or materially increases
   compiler memory;
7. existing-vocabulary improvements are not material relative to the measured
   Gate 0 ceiling; or
8. progress requires a new public strategy schema. Record that follow-up, but
   do not introduce it implicitly.

## Deferred But Preserved

The following discussed work is deliberately not lost and remains eligible
for later selection:

- a versioned shared-condition table/reference representation;
- a canonical condition DAG, BDD, or routing bytecode if inline v1 remains
  dominant;
- explicit serialization, behavior, policy, and provenance hash separation;
- strict carrier 5983 coarse-mapping repair, stored/exact cost reconciliation,
  lower-bound recovery, and alternative-envelope closure;
- Fracture Q completion and broader Fossil/Eldritch/five-mod strategy-quality
  work;
- cooperative exact-reforge subdivision and the remaining evaluator work-item
  split if browser responsiveness is selected again;
- broader evaluator/finalization/reforge module decomposition and removal of
  superseded algorithm paths; and
- browser worker-version, saved-artifact, presentation, and large-graph UI
  work.

## Checkpoints

Create coherent local commits for plan selection/baseline, typed condition
authority, same-target coalescing, optional reduced DAG/factoring, compiler
consolidation, and final evidence. End every commit with
`Co-authored-by: Codex <codex@openai.com>`. Do not push.
