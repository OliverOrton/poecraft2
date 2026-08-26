# Gate 2 — On-demand semantic refinement repair

Recorded 2026-08-26 from Gate 0/1 checkpoint `22c7881` plus the Gate 2
implementation.

## Retained repair

The persistent proof quotient can now import the complete already-filtered
operator vocabulary without eagerly materializing every alternative. When an
inherited selected policy is improper it can:

- name the first dead path and its exact-source coverage;
- certify one unresolved alternative on a singleton dead-path cell;
- grow the strict carrier set in place when that exact row exposes a successor;
- preserve the proof-obligation lifecycle across target invalidation and
  repartition; and
- reconstruct the inherited selected projection using only current proof use
  sites, excluding invalidated historical rows.

Frontier-introduced carriers are explicitly local-repair carriers. They no
longer borrow an unrelated coarse selected action merely because their
abstract parent was discovered by the broad solve.

## Product boundary

The repair is not an unbounded product-finalization mandate. A product solve
with a nonzero `max_policy_refinement_states` allowance and an already finite
current policy directly certifies that current policy before attempting an
open-ended strict repair of an unverified selected snapshot. The deferred
snapshot remains diagnostic evidence; exhaustive callers that leave the
allowance at zero retain strict-lift behavior.

This ordering is required by the measured witness. The selected snapshot was
estimated at `12197.2774883927`, while direct certification produced a proper,
zero-off-policy, exactly evaluated `2698.87479601436` policy. Spending product
finalization time repairing the worse snapshot could not improve publication.

## Witness and rejected expansion

Final retained product witness:

- case: `conquest-lamellar-allflame-fractured-4-to-5-product8`;
- lower: `36.4286171890972`;
- exact published upper: `2698.87479601436`;
- direct certification: complete, proper, cost-complete, zero off-policy, and
  reconciled;
- core policy hash: `9c020a941326a237`;
- selected snapshot: `strict_deferred_to_current_direct_candidate`; and
- wall: `30.628 s` in
  `build/performance/solver-quality-gate2-fractured-directed-v17.json`.

The repair path itself was characterized before product deferral. Two local
rows were certified and 19 frontier states were added without rebuilding the
strict session. Continuing through a third one-state frontier, exposed by
`option:protected_side:prefix:scour:goal:23`, caused recursive exact carrier
discovery to reach 37,761 states and 31,257 kernels and still had no executable
upper at the 120-second watchdog. This candidate is rejected for product use.
It proves that the remaining owner is pre-partition exact closure under a
newly selected local program, not the original missing-child or stale-row
exception. A future repair must interleave semantic quotienting with carrier
discovery rather than materializing that closure first.

## Controls

- Native build: pass.
- `--solver-policy-refinement-only`: 2,063 checks, zero failures.
- `--solver-refinement-only`: 362 checks, zero failures.
- `--solver-quotient-proof-only`: 616 checks, zero failures.
- The complete solve suite reaches the pre-existing 13 stale
  goal-progress-gated expectations and then parses the expected-empty strategy;
  no new failure appears before that retained boundary.
- Clean five-T1 Conquest returns the retained exact-evaluator-owned
  `14454067.4260706` upper over `36.4885317287664` in `66.236 s`, rather than
  expiring in optional selected-policy refinement.

Gate 2 therefore passes for the product boundary: the misleading failure no
longer delays the cheaper exact incumbent, local repair is proof-lifecycle
sound where exercised, and the unqualified recursive-closure continuation is
recorded rather than silently promoted.
