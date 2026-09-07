# Request And Action Scope

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page owns the mapping from a native request to the action envelope used by a solve. The [mathematical model](mathematical-model.md#scope) defines why scope is part of the optimization problem. The [source map](../foundation/solver-internals.md) identifies the implementation owners.

## Inputs And Outputs

Inputs are the concrete start item, requested goal slots and tiers, terminal rarity and minimum satisfaction count, solve profile, explicit action IDs or product-envelope construction, disabled families, prices, and computational limits.

Native registry and option construction produce a deterministic action vocabulary and an action-envelope ledger. The ledger records what has happened to each source/operator obligation; the existence of a ledger entry is not proof that its row is complete.

Primary owners are `solver_api.cpp`, `solver_registry.cpp`, `solver_action_family_contract.hpp`, `solver_options_build.cpp`, `solver_options_automatic.cpp`, `solver_options_temporary.cpp`, and `solver_action_envelope_ledger.hpp`.

## Invariants

The registry and exact calculator own legality, modifier pools, dependencies, and action semantics. Bindings may validate shape and select a priced request, but may not recreate those semantics.

A disabled family excludes both its direct actions and its use as a dependency of a fixed or generated program. Missing prices do not mean zero cost. They exclude or qualify the affected requested scope and remain visible in the result.

Generated automatic programs close through complete enumeration or an applicable mechanical, resource, or positive-price proof. A discovery depth or time limit does not by itself prove that the remaining programs cannot improve the result. The distinction between uncomputed and excluded actions is explained by [CLM-0008](claims.md#clm-0008).

## Candidate, dependency, and materialization roles

Keep these separate:

| Role | Meaning |
|---|---|
| Caller-authorized action | Belongs to the declared optimization scope |
| Product candidate | Returned by native product-envelope construction for possible direct selection |
| Automatic dependency | Primitive operation needed by a native program; not necessarily a direct product candidate |
| Admitted source/operator pair | Eligible for the current source and request |
| Completed row | Entire supported transition/choice result has been built and installed |
| Proof-retired obligation | An authorized proof closes the obligation without creating an executable row |

An explicit product candidate list and the generated-program grammar together require complete accounting. Counting descriptions is not equivalent to identifying every semantic action. [Lower coverage](lower-pruning.md#coverage-and-lower-only-queries) handles explicit and residual-family proof constraints.

## Product and diagnostic scope

At this snapshot, Calculator product defaults disable generated Imprint programs and voluntary economic Restart, enable goal-progress-gated reforges, and use junk-free terminal success. Mechanic-owned paid recovery remains distinct from voluntary abandonment. See [upper recovery](upper-authority.md#recovery-and-exact-terminal-success).

Goal-progress gating restricts zero-progress behavior; an exact result under that restriction is not an unrestricted result. Low-level engine compatibility defaults and explicit diagnostic callers can differ from product defaults. Read the resolved scope and override telemetry rather than inferring it from an option's name.

Mirror-producing Fossil loadouts are outside product solver admission. Product starts reject Mirrored and Synthesised items, while non-product exact calculation and Simulator preserve those flags. This is a declared product projection, not a claim that the flags never matter mechanically.

An auxiliary lower model may deliberately permit extra optimistic recovery. That does not add a product action: the native-to-auxiliary relation must establish the bound direction. Conversely, excluding a cheap native action raises a restricted optimum and cannot silently produce a full-scope lower. See [CLM-0009](claims.md#clm-0009).

## Authority And Failure

The ledger distinguishes discovered, admitted, deferred, missing-price, disabled, unsupported, resource-interrupted, and completed work. A restricted or open envelope can still yield an independently executable upper. Its restricted Bellman value does not become an unrestricted lower or exactness result.

A cap stops computation, not the semantic task. Unsupported behavior is not exact inapplicability. Unknown or interrupted coverage remains open unless a separate complete proof closes it.

## Inspecting and changing this boundary

Inspect `actions`, `action_control`, `action_search_cost`, `incremental_action_envelope`, and the named cap/stop owner. Compare canonical identities and resolved scope before comparing row counts.

Relevant checks live in `test_solver_api.cpp`, `test_solver_s8_3.cpp`, and Calculator request tests. Select them when changing this contract; this reference does not mandate a routine suite for unrelated work.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/request-action-scope.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
