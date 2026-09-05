# Lower And Pruning Authority

Parent: [Solver](README.md) | Verified against current source: 2026-08-27;
benchmark-private lower-query contract added 2026-09-04.

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

The benchmark-private `QuotientBellmanGraph::solve_lower/check_lower` consumer
owns no second graph or scheduler. `LowerOnly` graph mode uses the existing
sparse rows, observed choices, reverse dependencies and ProofStore accounting,
and refuses executable solve/projection. Canonical expected action sets and
residual-family exclusions must form a disjoint complete cover. An uncertified
ordinary row alone is not lower provenance.

Candidates use shared sparse arithmetic; acceptance checks finite nonnegative
values against every raw Bellman inequality with exact binary dot products.
Raw stored coefficients, explicitly normalized reference coefficients, and
exact binary model declarations remain separate. Arithmetic feasibility is
not native coefficient or uniform-class authority. Certificates bind the full
request/model snapshot and generations; independent prior evidence stays
outside changed-model feasibility. Zero-cost components may return a weak
finite lower, without any greatest-proper-policy or infinity claim.

No current product manager consumes these private certificates. The
[v2 pilot](../archive/2026-09-04-operator-complete-frontier-bellman-lower-pilot-v2/README.md)
records native parity, focused acceptance and the conditional donor stop.

The opt-in [phase lower producer](../archive/2026-09-04-uniform-phase-lower-certificates-v1/README.md)
adds a separate native support proof. Its existing reachability helper can omit
fresh-base positivity to cover every item tag signature. A monotone frozen
goal-mask potential is checked against all priced primitive support unions;
the relation composes through the complete generated program grammar and all
observed choices. Phase views bind exact Eldritch tiers without changing the
old identity-clean guard. The native mandatory Ichor/Exalt composer streams
complete integer weight mass, checks every failure exit and uses directed
cost-plus-failure arithmetic. These are private validating constructors, not
coefficient-only declarations or executable authority. The measured donor is
0.01165 and program lower 3.683885; no production consumer or portfolio gain.

Internal lowers drive operator pruning, fringe gap priority, constructive
certificates, and gap targets. Telemetry should show which pattern supplied a
bound and whether `state_incumbent_operator_lower` actually pruned rows. A
larger displayed number without a measured proof consumer is not accepted as
an improvement.
