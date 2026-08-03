# Exact Reforge-Work Growth Diagnostic Plan

**Status: completed and archived.**

Parent: [Archived diagnostic](README.md)

Starting boundary: `41d6a243947b0205a6e29c6373e79b21b0d8292a` on
`codex/proof-carrying-quotient-refinement`.

Diagnostic branch: `codex/reforge-work-growth-diagnostic`.

## Objective

Classify exact candidate reforge-work growth as falling after exceptional
early kernels, approximately linear, or combinatorial. This is a slope
measurement, not authority to find or adopt a larger product cap.

Every run preserves the frozen two-goal fixture except
`caps.max_reforge_work`, the complete admitted vocabulary, existing mechanics
and filtering, the proof-carrying quotient implementation, the 1 GiB memory
cap, the 900-second watchdog, all other caps, zero reference calls, and no
complete strict-graph reconstruction.

Production source, defaults, scheduling, ABI, WASM, strategy JSON, and
frontend behavior remain outside scope.

## Gates

1. Audit every material `consume_reforge_work` site, the total-to-exact budget
   subtraction, pre-partition closure ordering, plausible source populations,
   and initial 50M/100M/200M projections. Do not rerun 20M.
2. Run 50M once. Record coarse/exact work, kernels, states, transitions,
   partition progress, memory, time, status, and marginal slopes.
3. Run 100M only if 50M stops solely on work with safe memory/time and a
   second point materially distinguishes the growth model.
4. Run 200M only if cost is falling, concrete near-term quotient progress
   appears, or another point is necessary to distinguish linear from falling.
5. If a policy appears, compile, exact-reconcile, and run 10,000 simulations.
   Otherwise preserve the stopped curve honestly and select the next
   architecture from the dominant boundary.

## Final gate result

Run A and Run B both stopped solely on reforge work. The coarse phase and peak
owned memory were invariant. The two marginal segments added exactly 172,596
transitions per completed kernel; marginal work was 2.000M and 2.174M per
kernel, with zero partition or certificate progress. That is sufficient to
classify the post-startup shape as approximately linear with a slight upward
endpoint-censored slope. Run C was therefore not performed.

The selected follow-on is competitive lazy alternative certification, as
recorded in the sibling [report](report.md).
