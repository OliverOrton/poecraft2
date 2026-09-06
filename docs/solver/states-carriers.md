# States And Carriers

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page names the native representations and their current consumers. [Representations and abstraction](mathematics/representations.md) owns the arguments for equality, optimism, and complete-member coverage.

## Representations

| Representation | Responsibility and limitation |
|---|---|
| `pc_item_state` | Concrete native item facts; checkpoint or observation memory belongs to the appropriate surrounding state when relevant |
| `AbstractLayout` | Selects the observations and member classes represented for a solve |
| `AbstractState` | Stores goal status, member/junk classes, occupancy, flags, and other layout-selected facts |
| `CalcContext` state ID | Dense identity inside one calculator namespace, not a portable item identity |
| Strict carrier key | Collision-checked exact materialization and action-observation identity |
| Quotient cell | Class inside a particular proof/refinement partition and generation |
| Compiled strategy node | Control/routing position in an emitted strategy, not a solver state |
| Evaluator product entry | Operation plus item and applicable choice/checkpoint state under a fixed strategy |

`AbstractState` retains goal-slot status, exact goal-member classes where requested, prefix/suffix occupancy, junk classes, crafted/fractured identity, protection, influence/Eldritch state, Veiled side, and the retry-basin marker. No single list substitutes for the actual layout and admitted action contracts.

Primary owners are `solver_model.hpp`, `solver_abstract.cpp`, `solver_calc_types.hpp`, `solver_calc.cpp`, `solver_refinement_features.cpp`, and `solver_refinement_observation.cpp`.

## Terminal Contract

The implemented goal test requires the requested rarity and slot/tier threshold, and requires occupied explicit-affix count to equal satisfied goal count. Empty explicit slots are permitted. Junk, temporary metamods, below-tier goal members, and blockers may occur during planning but are not terminal success.

Use the native resolver and terminal predicate. Do not replace them with `mask == full`, assume every requested bit must always correspond to a distinct physical affix, or reinterpret overlapping slots in documentation. The mathematical model keeps native slot/overlap semantics explicit; distinct-goal assumptions belong to a particular proof's preconditions.

Compiler terminal recognition must express the same predicate. A projection satisfying a relaxed terminal is not thereby a native successful item.

## Equality and lower-domain membership

Exact behavioral equality requires the applicable action and observation contracts and probability into successor classes to agree; a shared goal mask is insufficient. Strict refinement records the observations needed to establish or refute that relation. See [CLM-0005](claims.md#clm-0005).

An optimistic lower projection can deliberately forget more, but needs a different validity argument. It is not an exact quotient just because its scalar lower passes a numerical check.

An ordinary lookup for a coarse state must cover every member represented by its retained fields. One successfully materialized representative does not establish that coverage. The opt-in retention consumer uses complete modifier member masks and keeps the previous lower on ambiguous fracture or unsupported member classes. This conservative fallback is not permission to broaden the certificate by selecting a favorable representative.

Member-wise lower and upper aggregation also have different quantifiers. A uniform scalar upper does not automatically supply one executable class router. The complete explanation is [CLM-0006](claims.md#clm-0006).

## Product Projection

Mirrored and Synthesised are excluded from product planning: the producing Fossil is filtered before product loadout admission, the parent projection clears the flags, and product starts carrying them are refused. Simulator and non-product exact paths retain literal flags.

Corrupted, Split, Fracture, influence, Eldritch, crafted/Veiled/protection, and retry-basin dimensions remain because current actions, routing, or terminal tests observe them. Removing a field therefore requires an actual new equivalence or optimistic-relation argument, not an assertion that the current winning policy does not use it.

## Failure And Telemetry

Contradictory materialization, unknown observations, identity collisions, and applicable state caps refuse or preserve unresolved work. State IDs, strict carrier IDs, quotient cells, and compiled nodes cannot be substituted at an API boundary.

Inspect state/class counts, strict carrier/cell counts, feature histograms, and retained/peak ownership. A smaller struct is not by itself evidence that reachable states, rows, or proof work decreased. Use [benchmarking](benchmarking.md) for matched comparisons.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md). Mathematical links refer to the companion draft chapters and provisional claim IDs; they do not declare those claims accepted. Local implementation correspondence must be reconciled during integration.
