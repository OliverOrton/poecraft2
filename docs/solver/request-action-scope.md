# Request And Action Scope

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Inputs And Outputs

Inputs are the exact start item, goal slots and terminal rarity, solve profile,
explicit candidate IDs or product-envelope construction, disabled action
families, prices, and solver limits. Native registry construction produces
ordinary primitives plus finite engine-owned planner operators. The output is
a deterministic, typed action vocabulary and an action-envelope ledger.

Primary owners: `solver_api.cpp`, `solver_registry.cpp`,
`solver_action_family_contract.hpp`, `solver_options_build.cpp`,
`solver_options_automatic.cpp`, `solver_options_temporary.cpp`, and
`solver_action_envelope_ledger.hpp`.

## Invariants

- The registry and exact calculator, not TypeScript, own applicability,
  dependencies, modifier pools, and action semantics.
- A disabled family removes its direct actions and prevents fixed or generated
  programs from reintroducing that family as a dependency.
- Missing prices exclude otherwise supported rows and keep the requested
  envelope visibly qualified.
- Generated automatic programs close only through exact enumeration,
  mechanical/resource dominance, or a proved positive-price bound. A depth
  limit alone cannot close the envelope.
- Calculator product defaults disable generated Imprint programs and voluntary
  economic Restart, enable goal-progress-gated reforges, and require exact
  junk-free terminal success. Diagnostic overrides change the stated scope.
- Mirror-producing Fossil loadouts are outside product solver admission.
  Product starts reject Mirrored and Synthesised carriers; exact simulation and
  non-product evaluation still preserve those item flags.

## Authority And Failure

The envelope ledger distinguishes discovered, admitted, deferred, missing-
price, disabled, unsupported, resource-interrupted, and completed rows. A
restricted or open envelope may still produce an executable upper, but cannot
silently publish unrestricted exactness or a restricted Bellman value as a
global lower.

Inspect telemetry under `actions`, `action_control`,
`incremental_action_envelope`, `action_search_cost`, and the named cap or stop
owner. Stable request tests live in `test_solver_api.cpp`,
`test_solver_s8_3.cpp`, and the web Calculator tests.
