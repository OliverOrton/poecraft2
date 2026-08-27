# States And Carriers

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Representations

A concrete `pc_item_state` is complete native item identity. `AbstractLayout`
chooses the observations needed by a solve, and `AbstractState` stores goal
slot status, exact goal-member classes where requested, side occupancy, junk
classes, crafted/fractured identity, protection, influence/Eldritch state,
Veiled side, and the retry-basin marker. `CalcContext` interns these values into
stable dense state IDs.

Strict refinement materializes collision-checked carrier keys and feature
signatures. A strict carrier ID, coarse state ID, quotient cell, and compiled
strategy node are different namespaces and are never interchangeable.

Primary owners: `solver_model.hpp`, `solver_abstract.cpp`,
`solver_calc_types.hpp`, `solver_calc.cpp`, `solver_refinement_features.cpp`,
and `solver_refinement_observation.cpp`.

## Terminal Contract

A goal state has the required rarity and requested slot/tier coverage, and its
occupied explicit-affix count equals its satisfied goal count. Empty explicit
slots are allowed. Junk affixes, blockers, temporary metamods, and below-tier
goal members remain valid intermediate state but are not terminal success.

## Product Projection

Mirrored and Synthesised are intentionally absent from product planning:
their producing Fossil is rejected before product beam/powerset admission,
product parent projection clears those flags, and a product solve refuses a
start carrying either flag. Corrupted, Split, Fracture, influence, Eldritch,
crafted/Veiled/protection, and retry-basin dimensions remain because current
actions, applicability, terminal semantics, or compiled routing observe them.

This reduction is product-only. Simulator and exact non-product calculator
paths retain literal item flags and native mechanics.

## Failure And Telemetry

State caps, contradictory materialization, unavailable observations, or an
identity collision fail closed. Inspect `states`, `state_classes`, strict
carrier/cell counts, feature histograms, and proof-memory carrier ownership.
Do not infer savings from deleting a field unless reachable state, row,
transition, kernel, or obligation measurements fall.

