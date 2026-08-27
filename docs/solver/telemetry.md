# Telemetry

Parent: [Solver](README.md) | Verified against current source: 2026-08-27.

## Ownership

Each phase records typed counters in its own result or progress structure.
`solver_solve_telemetry.cpp` snapshots those records, accounts retained bytes,
and `solver_solve_telemetry_json.cpp` owns JSON serialization. Telemetry is
observational: it cannot change scheduling, pruning, values, caps, policy
selection, or publication authority.

## Compact Triage Set

For ordinary diagnosis retain at least:

- execution phase, stop owner, named resource cap, and solution scope;
- lower, upper, gap, lower provenance, incumbent kind, and exactness status;
- action-envelope closed/open state and remaining obligations;
- states, rows, transitions, reforge logical/V3 work, and dominant action
  family;
- strict cells/kernels/transitions, obligation lifecycle, frontier growth,
  policy improvements, and dominant proof-memory owner;
- cooperative row resumes/suspensions/cancellations, maximum slice wall, and
  retained cursor bytes; and
- compiler/evaluator stage, graph/pair counts, properness, off-policy mass,
  exact cost, and reconciliation.

## Full Evidence

Full-evidence mode may additionally retain bounded samples for automatic
candidate reasons, action/carrier attribution, proof patterns, reforge row
structure, exact observation features, and compiler/evaluator census. Every
sample list is capped and reports omissions. Aggregate totals remain the
authority for population claims.

## Reading Rules

- Compare logical work separately from wall time and owned memory.
- `lower == upper` is meaningful only with their provenance and envelope
  status.
- A finite upper is useful even when strict closure stops; it is not exact.
- Sampled Simulator rate and mean are finite-sample evidence. Exact graph
  evaluation owns expected success and cost.
- Missing or null phase timing means uninstrumented, not zero duration.
- A counter that never reaches its consumer is a telemetry fix, not a solver
  improvement.
