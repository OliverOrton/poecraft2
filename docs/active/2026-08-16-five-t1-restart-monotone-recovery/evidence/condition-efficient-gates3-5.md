# Condition-Efficient Compilation Gates 3-5

**Status: Gates 3-4 are measurement-gated skips; Gate 5 consolidation is
complete.**

## Gate 3 decision

Gate 2 removes every reproduced same-target group. Every retained compiler
route reports equal non-default edge and distinct-target counts, while the
existing bottom-up signature table continues to share complete identical
children. There is no remaining measured edge or node ceiling that justifies
replacing the widest/balanced split heuristic.

A continuation-aware heuristic could choose different but equally correct
trees. Without a positive projected reduction, that would add policy-routing
risk and compiler work for aesthetics. Gate 3 is therefore skipped under its
explicit decision rule.

## Gate 4 decision

Typed route composition already performs the safe current-vocabulary boolean
rewrites that reproduced materially: identity removal, flattening,
deduplication, and same-target unions. The remaining four-T1 condition payload
is dominated by distinct `observation_signature` leaves. Each leaf embeds the
same large observation requirement plus a different exact signature.

Those objects are opaque public v1 predicates, not conjunctions that can be
factored by wrapping them in `all`/`any`. Exact range fusion and `at_least` do
not apply. The repeated requirement can only be shared through a versioned
condition-reference table or a dedicated observation-decision representation,
both expressly deferred. Gate 4 is therefore skipped rather than introducing
an implicit schema change.

## Gate 5 result

The touched compiler now has explicit internal authorities:

- `solver_condition_expr.hpp` owns immutable composite construction,
  canonicalization, structural equality, size accounting, and v1
  serialization;
- the existing `solver_compile_conditions.hpp` remains the feature and
  primitive predicate authority;
- `solver_policy_route.hpp` owns route provenance, priority-safe coalescing,
  collision-free signatures, and the reduced multi-valued decision-DAG
  builder; and
- `solver_compile.cpp` retains graph emission, accounting, caps, and telemetry.

The formerly duplicated refined-parent and exact-policy recursion now uses one
callback-driven builder. `ProveRepresentedDomain` keeps splitting uniform
refined parents so omitted values still fail closed.
`StopAtUniformTarget` preserves the exact-policy early-leaf rule. Both modes
share deterministic feature scoring, structural partition provenance,
coalescing, signature interning, and cap enforcement.

Refined observation edges are also staged through the same typed route edge
authority before emission, replacing the former one-off serialization loop.
The focused compiler suite passes 834 checks after consolidation.
