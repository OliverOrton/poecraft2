# Gate 1 — Proof and ordering ownership

Gate 1 passed.

- Goal-cover, clean-MDP, strict-clean, and operator-lower construction moved
  from `solver_solve_heuristics.cpp` to the dedicated
  `solver_solve_bounds.cpp` translation unit.
- Focused and incremental goal-subset bucketing and legacy within-bucket order
  now share `solver_solve_priority.cpp`.
- `ProofLowerValue` and `CarrierOrderingScore` are distinct, non-convertible
  types. Pruning consumers explicitly request `.value` from a proof-lower API;
  an ordering score cannot enter those call sites.

The post-refactor 1,000-expansion fixed-work report is
`evidence/gate0/fixed-work-gate1.json`. Against
`evidence/gate0/fixed-work-after.json`, it retained bit-for-bit:

- `refused_state_cap` / `refused_resource_cap`;
- lower `0.01165`, upper/evaluated cost `59810.9537769745`;
- 3,315 discovered, 1,000 expanded, 2,315 frontier, 18 goal, and 163
  policy-reachable states;
- graph census 51 nodes / 149 edges;
- transition hash `fb8dc170b29920df` and policy hash
  `1b98ca41e69ad1b1`; and
- selected actions and expected-consumption keys.

This gate changes ownership and type safety only. Gate 2 owns the first work-
order change.
