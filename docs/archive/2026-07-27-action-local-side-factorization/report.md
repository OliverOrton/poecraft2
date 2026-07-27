# Action-Local Side Factorization Report

**Status: final negative result and fracture boundary record.**

Parent: [Milestone archive](README.md)

## Result

Simple exact prefix/suffix convolution is rejected for destructive reforges.

The candidate conditioned on:

- the complete preserved base;
- target/final affix total; and
- prefix and suffix pick counts.

That was insufficient. In the exact synthetic Chaos distribution, the
two-prefix/two-suffix cell had full rectangular support but a non-zero 2x2
probability minor:

```text
p11 = 0.0085137085137085115
p12 = 0.0010101010101010099
p21 = 0.031056597723264381
p22 = 0.0031265031265031249

abs(p11*p22 - p12*p21) = 4.7521644443241428e-6
```

The maximum absolute difference from the product of conditional marginals was
`0.00013338996237286091`. This is far above floating-point noise and directly
falsifies rank-one side composition.

## Coupling mechanism

The current exact roll process draws sequentially from the combined eligible
pool. For prefix-local state `x` and suffix-local state `y`, the next prefix
bucket probability contains the denominator:

```text
Wprefix(x) + Wsuffix(y)
```

Selecting a modifier can remove an identity-dependent amount of future weight
through its own weight and exclusion groups. A prefix identity therefore
changes later suffix-selection odds, and a suffix identity changes later
prefix-selection odds. Final side counts alone do not preserve those path
probabilities.

Two controls pin that interpretation:

- making the one heavy suffix weight uniform reduced the maximum error to
  floating-point noise and every 2x2 minor to zero; and
- preserving that same heavy suffix as fractured also produced rank-one cells
  in this tiny fixture because its identity-dependent suffix weight was moved
  into the immutable base.

Those controls do not qualify the general architecture. They show why the
fractured case behaves differently.

## One richer boundary

The probe next conditioned on final remaining prefix and suffix pool weights.
That restored rank one, but it split six count cells into thirteen
count/weight cells. The factor representation needed:

- 31 prefix marginal identities;
- 17 suffix marginal identities; and
- 48 identities total for only 41 joint outcomes.

It was already larger than the joint representation on the small fixture.
More importantly, an online evaluator would need the remaining-weight
coupling after every sequential pick, not only in the final outcome. That
reintroduces the paired local state the factorization was meant to avoid.

The milestone therefore stopped before a real-data run. The result does not
prove that every possible algebraic or symbolic reforge algorithm is
impossible. It rejects this count-conditioned convolution and its immediate
remaining-weight refinement as the next exact solver implementation.

## Fracture boundary

The fractured-suffix observation was valid and useful:

- an ordinary starting suffix is wiped by Chaos, and its exact distribution
  matched the empty-start distribution;
- the identical fractured suffix is preserved, occupies capacity, blocks its
  groups, and changes the roll distribution; and
- fracture does not participate in the terminal goal predicate itself.

The retained native regression constructs a rare item with a fractured goal
prefix and unrelated fractured suffix junk. It verifies the exact fracture
carrier fields and confirms that the item satisfies the permissive goal.

This action-local result is separate from the outer solver slowdown Oliver
observed. A fractured carrier also disables existing clean-carrier
constructive and lower-bound paths, which can worsen solve performance even
when the reforge calculation itself has a smaller outcome table.

## Evidence and restoration

The tracked
[evidence summary](../../../fixtures/solver-scaling/v1/evidence/action-local-side-factorization-summary.json)
pins the synthetic layout, exact witness, controls, measurement executable,
compiler, and machine.

The measurement run passed 120,535 focused solver-calculation checks with zero
failures. The exploratory factorization probe was then restored, and the final
retained focused calculation suite passed 120,439 checks with zero failures.
No mechanic, goal predicate, action, transition, solver decision, cap, public
ABI, compiled artifact, binding, WASM, web, or product behavior changed.
