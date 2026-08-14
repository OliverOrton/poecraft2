# Gate 3 Replay And Gate 4 Classification

**Status: complete on 2026-08-13.**

Parent: [Solver Decision Provenance And Result-Truth Hardening](../plan.md)

## Available authority

The historical serialized Fossil-to-Chaos request remains unavailable. This
is therefore the plan's selected hardening-only path: the old policy symptom
is not reproduced, localized, or assigned a cause.

The replay uses three frozen authorities:

- source baseline `9914b84f2c075e84d932936a14fa0d2ac5f03156` and its preserved
  Goal Realignment Gate 8 raw reports;
- Gate 1 checkpoint `c76180f`, whose whole-flow Calculator regression proves
  selected odds construction cannot enter the product envelope or final
  priced Solve candidates; and
- Gate 2 checkpoint `2ce8bfd`, whose focused native tests exercise the shared
  Q evaluator, strict finite ordering, tolerance-suppressed policy stability,
  termination truth, and lower-bound provenance.

Gate 2A added no new hot-path diagnostic code. Existing construction,
publication, fallback, policy-hash, and proof witnesses were enough for the
available defects, so there is no provenance-build timing comparison to make.

## Calculator request replay

The controlled pre-fix witness at the source baseline failed because selecting
`fossil:lucent` changed the product-envelope request. The Gate 1 and final
Gate 2 builds both pass the whole-flow regression:

```text
ok - Calculator odds selection is isolated from the product Solve envelope
```

`npx tsc --noEmit` also passes. This localizes the current request-scope defect
to Calculator request construction without claiming it caused the unavailable
historical policy report.

## Native Imprint replay

The frozen Gate 8 Imprint report and a fresh Gate 2 report use the same
canonicalized input SHA-256:

`63aeacd88fc9374acc6b94d8f29cef3bb23a262f3bec05b31754cc6cb1a512c5`

| Field | Frozen Gate 8 | Gate 2 corrected build |
| --- | --- | --- |
| Result | exact | exact |
| Termination | `exact_closed` | `exact_closed` |
| Lower / upper / evaluated cost | `252.65352021274481` | `252.65352021274481` |
| Compiled graph | 9 nodes / 11 edges | 9 nodes / 11 edges |
| Strategy bytes | 60,911 | 60,911 |
| Strategy SHA-256 | `9f633f123e00b667826e56bae1b3f7990dc3c2e3863444db88ae20c7a7102d37` | same |
| Exact evaluation | matched; success probability `0.99999999999999978`; zero off-policy mass | same |
| Simulation | 10,000/10,000; zero off-policy failures | same |

The current raw result additionally names
`lower_bound_provenance = exact_policy_closure` and
`global_lower_bound_certified = true`. No first differing compiled choice
exists, so construction-origin comparison is inapplicable. Final selection
authority is closed exact policy proof backed by proper compiled evaluation,
not merely fallback history or a verified upper.

The API replay also retains its checked deterministic transition and policy
bit hashes, `067ac4c6510645e6` and `bfcb25789b4f99ae`, while the focused native
row-order regressions prove that a representably cheaper finite row wins and
that a tolerance-suppressed strict improvement cannot claim exact closure.

Fresh ignored raw evidence is under
`build/solver-decision-provenance/gate3/`.

## Classification

Gate 3 found no remaining behavioral defect beyond the Gate 1 and Gate 2
corrections. The available exact control is byte-identical at the compiled
strategy boundary and independently exact-evaluates. The historical symptom
remains unclassified because its request is unavailable.

Gate 4 is therefore complete with no additional source change. Making another
behavioral repair would violate its requirement for a new pre-fix failing
witness. The selected semantic reach remains Q/order/result-contract plus
Calculator request scope, so Gate 5 must run the proportional Q/order acceptance
row.
