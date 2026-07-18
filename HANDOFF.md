# Session Handoff - S8.3 Automatic Candidates Is Next

Updated 2026-07-17 after S8.2. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4 and S8.0-S8.2 are complete. Oliver waived B1.5 as a separate
acceptance checkpoint; it remains waived/deferred, not complete. Do not
backfill its full suite, 10,000-run Imprint verification, or rendered review.

S8.0 historical captures and S8.1 display-only projections remain immutable
under [fixtures/solver-baselines/s8.0](fixtures/solver-baselines/s8.0/). S8.1
projection sections, roles, labels, and ordering have no executable or solver
authority. S8.2 comparison evidence is separate at
[fixtures/solver-baselines/s8.2/evidence.json](fixtures/solver-baselines/s8.2/evidence.json).

## S8.2 Result

Oliver's mechanic correction is implemented end to end:

- Fossil and Essence ignore every metamod effect. Prefix/suffix locks preserve
  no side, and cannot-roll attack/caster modifiers filter no pool.
- Input metamods are ordinary removable crafted affixes. Multimod has no
  special protection or restriction for these rolls.
- Fractured survival is independent. A fractured metamod remains as an affix,
  but its metamod effect does not influence the Fossil/Essence transition.
- Other actions retain their existing metamod behavior. Chaos and the other
  respecting renewals still honor locks and cannot-roll restrictions.

The shared native `ActionTransitionFacts` now drives action application, pool
construction, exact reforge preservation, and solver descriptors. Exact state
materialization tracks fractured-metamod identity without widening every junk
class, so ordinary compiled graph bytes remain unchanged. The compiled native
Simulator, Python binding, C ABI, WASM action metadata, and release WASM module
all use the corrected behavior.

Preservation metadata covers goal families, satisfied subsets, junk/blockers,
crafted and fractured state, prefix/suffix sides, and active protection with
preserve/destroy/create/unreachable masks. Protection-respect booleans keep
fracture preservation distinct from metamod preservation. Exact row witnesses
record the carrier hash/facts, goal subset, crafted/fractured data, active
protection, exact effects, Restart identity proof, dominating route and bounds,
and uncertain-retention reason.

Disposable certification is deliberately strict: the carrier state id must be
the exact successor of the genuine synthetic Restart operator. A progressed
destructive renewal is pruned only when its nonnegative immediate-cost lower
bound is strictly greater than Restart cost plus the current exact feasible
continuation upper bound. Missing/nonfinite proof, focused lower-bound mode,
negative cost, and ties all retain the action. The minimum-expected-cost
objective, Bellman equations, convergence, price identities, exact-kernel
collapse, stable ordering/ties, policy compression, and strategy routing did
not change.

## Evidence And Validation

- Native engine acceptance: 468,703 checks, zero failures. This includes
  applied Fossil/Essence locks and cannot-roll cases, exact/Monte Carlo parity,
  independent fractures, respecting Chaos controls, descriptor/C-ABI metadata,
  compiled Simulator behavior, deterministic witnesses, price ties/collapse,
  exhaustive-oracle equality, and the real action-reduction gate.
- Real reduction gate: 24 destructive rows considered, 12 retained, 12
  expensive Chaos rows pruned with strict Restart-bound witnesses;
  controlled/exhaustive value and start policy match.
- Frozen comparisons, run with benchmark verification skipped:
  `oracle-real-one-mod`, `oracle-real-two-mod`, and `ordinary-es-bench` retain
  S8.0 values, state counts, node/edge counts, and byte-identical raw strategy
  SHA-256 identities. S8.1 projection manifest bytes are unchanged.
- Focused Python behavior test passed. Release WASM was rebuilt; focused WASM
  engine smoke passes 26/26 and `tsc --noEmit` passes.
- No rendered browser review, screenshots, B1.5 backfill, protected-reforge or
  endgame recapture was performed. No new S8.2 compiled-strategy verification
  was required; benchmark comparisons used `--skip-verification`. The
  monolithic native test executable did run its pre-existing embedded 10,000
  iteration checks as part of engine acceptance; those were not new S8.2
  benchmark captures.

## Exact Next Boundary

Execute **S8.3 only - Automatic Fracture, Bench, And Metamod Candidates**.

Promote the already-supported exact option machinery into ordinary candidate
generation only under the active plan's carrier, legality, kernel-change,
complete-exit, and full expected-downstream-cost requirements. Compile selected
options to primitive operations and Simulator routes. Do not use raw hit-rate,
S8.1 display roles, or subjective crafting labels as candidate authority.

Do not begin S8.4 accounting, S8.5 focus/trimming, S8.6 acceptance,
recombinators, B1.5 backfill, or unrelated mechanic work.

## Preserved Mismatches And Gotchas

- Compiler-only `mod_count` routing remains unsupported by Calculator exact
  evaluation.
- The archived endgame sample remains 0.9942 against 0.995.
- The temporary-blocker solve remains non-converged; its automatic assembly is
  S8.3 work.
- Protected-reforge captures remain abandoned after long runs; do not restart
  them without an explicit later requirement.
- Archived S7 artifact pins predate B1.4.
- B1.5 remains waived/deferred, not complete.
- Fossil and Essence ignoring metamods is Oliver's mechanic authority. Do not
  research or reverse this correction.
- Raw ordinary strategies remain the only execution authority. S8.1
  projections and S8.2 evidence are comparison/review documents only.

S8.3 is the sole next boundary.
