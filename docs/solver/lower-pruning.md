# Lower And Pruning Authority

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Authority Rule

A lower bound may guide pruning, certify a gap, or support exact closure only
when every relaxation is proved optimistic for the requested action envelope.
Ordering scores and restricted-envelope Bellman values remain separate types.

Primary owners: `solver_proof_pattern_manager.hpp`,
`solver_solve_bounds.cpp`, `solver_solve_carrier_pattern.cpp`,
`solver_solve_operator_proof.cpp`, `solver_solve_envelope_proof.cpp`,
`solver_solve_heuristics.cpp`, and the publication classifier in
`solver_solve_constructive.cpp`.

## Current Patterns

The independent goal-cover floor grants optimistic action reach and combines
independently admissible patterns by maximum. Clean-carrier refinement adds
rarity, goal subset, side occupancy, capacity, junk-free terminal debt, tags,
weights, and destructive replacement where its eligibility proof applies.
Operator lowers first apply the action's proved survival/destruction contract,
then add optimistic successor completion cost.

If incremental action generation is open, the public lower falls back to an
independent global floor. The restricted graph value remains useful for
internal scheduling but is not global proof.

## Conservative Slack

Probability lowers use upper bounds on success probability. Clearing carrier
sides, granting strong blocker exclusions, atomizing setup-bearing programs,
or returning zero outside a represented carrier class is deliberately cheap
and capable; it weakens pruning but preserves correctness. Tightening any of
these must retain the admissibility proof.

## Consumers And Failure

Internal lowers drive operator pruning, fringe gap priority, constructive
certificates, and gap targets. Telemetry should show which pattern supplied a
bound and whether `state_incumbent_operator_lower` actually pruned rows. A
larger displayed number without a measured proof consumer is not accepted as
an improvement.

