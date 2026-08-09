# Verified Best Executable Strategy Publication Report

**Status: implementation and selected acceptance complete.**

## Outcome

Publication authority now belongs to the exact final emitted strategy JSON.
Every direct, strict-refinement, and renewal candidate considered for public
use is parsed and independently evaluated. A graph is eligible only when the
evaluation converges, is proper, reaches success with probability one, has
complete pricing, has zero off-policy mass, and has finite expected cost. That
evaluated cost is its upper bound.

The solver retains every eligible incumbent on non-exact exits, compares all
verified candidates by evaluated cost, and publishes the cheapest. Cost
reconciliation with a source estimate is still recorded, but mismatch now
blocks exact optimality rather than executability. One centralized finalizer
normalizes bounds and recomputes gaps, so no public result has `lower > upper`
and no unsupported lower can produce a false exact or `1.00x` claim.

Compiled-but-unverified artifacts remain in bounded diagnostic telemetry with
no finite upper and no publication authority.

## Before/After Case Matrix

| Case | Frozen before | Final result |
| --- | --- | --- |
| `reliability-class-fishingrod` | Renewal near `16.482913436844125` retained internally, zero publication attempts, no public policy. | Renewal independently evaluates at `16.482913436819231` and wins over verified direct `16.482913436844086`; bounded `7.8747601236763485..16.482913436819231`, 6 nodes/7 edges, 100/100 simulations. |
| `reliability-class-ring` | Direct certification consumed about 5.07M scoped work and strict received zero. | Direct debits actual `5,074,364`; strict receives the real remaining 27 and reports `max_reforge_work (27)`. Both artifacts are compiled-unverified, so policy remains honestly unavailable. Aggregate logical work is `19,999,973`; legacy active work remains separately saturated at `20,000,027`. |
| `reliability-class-amulet` | Core policy/artifact existed, verification did not complete, nothing published. | Direct 4/4 artifact remains compiled-unverified at the unchanged 20M work cap; strict records the exact `coarse_mapping_failure`; no public upper or policy. |
| `natural-t1-representative-three-bd85c8c40df7` | Core estimate about 13,098 could not verify; renewal about 14.28M published. | Direct remains compiled-unverified with a measured 36.4GB evaluator peak against an 849MB budget. Renewal independently evaluates and publishes at `14280275.642902469`; lower `13098.303701763865`, success 1, off-policy 0, 6/7 graph. |
| `natural-t1-representative-three-60546fa5c271` | Retained-core recovery control. | Verified direct `1873.2170927467109` beats verified renewal `88650.863329981585`; bounded lower 0, 109/222 graph, success `1.0000000000001918`, off-policy 0. |
| `natural-t1-representative-three-d0a2115efe54` | Tiny lower/upper inversion. | Direct final graph publishes bounded at `18323.486578591921`; its `9.9014869192615151e-8` estimate delta is within tolerance, but the unsupported inverted lower is normalized to 0. No false exact claim. |
| `natural-t1-deep-four-dense-a64d3d932590` | Material lower-above-upper publication. | Verified direct `36618614.140514001` beats verified renewal `39967846.540786296`; the 8.38% estimate mismatch forces lower 0 and bounded status. The 100-run sample reaches the unchanged action cap, while exact success remains 1 and off-policy 0. |
| `natural-t1-breadth-two-c4c462fd3d77` | Published strict graph required about 4.5GB to evaluate. | Verified direct `695.22599151909276` beats verified renewal `1393.0422018933812`; the strict 3,030-node artifact remains compiled-unverified with its 4.49GB transient request recorded. Lower is normalized to 0. |
| `reliability-class-belt` | Simple direct exact control. | Exact direct remains exact at `9.143792577895411`, with 10/14 graph, success 1, off-policy 0, and 100/100 simulations. |
| Dire Pelt two-goal | Independently verified bounded control. | Native and WASM select verified direct at `232.024879234610`, 6/7 graph, success `1.000000000000828`, off-policy 0, and 10,000/10,000 simulations. The compiled-unverified strict artifact cannot displace it. |
| Runic Gauntlets five-goal | Independently verified bounded strict control. | Native and WASM retain bounded strict at `923509.244023527`, 195/723 graph, success 1, off-policy 0. Each performs 10,000 runs: 2,521 success, 7,479 action-cap failures, zero off-policy. |

The matrix's “before” column is the provenance-frozen boundary evidence from
the selected plan. Expensive old binaries were not rerun merely to recreate
those observations.

## Candidate And Bound Contract

Candidate telemetry records identity, kind, stage, verification state,
evaluated cost, source estimate, reconciliation deltas, disposition, and
selection reason. In particular:

- Fishing Rod selects the renewal by a `2.4855e-11` evaluated-cost advantage
  over the independently evaluated direct graph.
- `60546`, `a64d3`, and `c4c4` select the cheaper direct graph even though
  each also has an independently evaluated renewal.
- The `bd85` direct graph and `c4c4` strict graph remain diagnostic-only
  because final evaluation refused their memory projections.
- Ring and Amulet retain artifacts and first failure evidence but expose no
  finite upper because no candidate completed final evaluation.

Bounds are normalized once after candidate selection. Small within-tolerance
inversions clamp to the verified upper; material or unsupported contradictions
discard the lower to zero. Exact status requires both a verified final graph
and closure of the valid global lower at that graph's cost.

## Resource And Structural Diagnostics

Ring demonstrates corrected work transfer. Direct certification attempted to
cross its scoped cap at 5,074,391 units but committed only 5,074,364 actual
logical units. Strict refinement therefore receives 27 units instead of zero.
The aggregate logical ledger remains `19,999,973`; the legacy active ledger is
still reported independently as `20,000,027`.

Amulet records `coarse_mapping_failure`: coarse states/policy `3/3`, partition
identity `10669586994360195773`, mapping identity
`3930739389334436728`, selected operator `chaos`, and the first violated
invariant: strict class 23 for modifier 156 spans coarse parents 0 and 2
(`117` strict versus `21` coarse classes).

`bd85` records `invalid_solve_state`: source 3 reaches successor 421 at the
end of a 421-entry solved-policy table while Calculator has 422 states,
operator 224, planner `fracture`; partition/mapping identities are
`3159694671182813452` / `873597559450091416`.

The evaluator refusals are now attributable:

- `bd85` direct: 256 nodes, 504 edges, 101 regions, 53,526 reachable
  state/action pairs, 2,291,128 transitions, 2,319 components, largest 6,011.
  The sparse-component path owns about 268MB, then projects a
  `2,258,468,374 * 16`-byte exact-attribution expanded transpose. Estimated
  peak is `36,408,111,127` bytes against `849,068,603`.
- `c4c4` strict: 3,030 nodes, 3,820 edges, 756 regions. Before component
  discovery, the observation fixed point projects `3,030 * 493,944` units,
  producing `4,491,886,960` transient bytes against `895,501,968`.

Both are avoidable dense evaluator work, not accounting errors. The Amulet and
`bd85` strict failures are separate strict-partition invariants.

## Native And Release-WASM Acceptance

The complete 49-case portfolio ran in native and release WASM. Each runtime
published 45 policies; all 45 final graphs independently evaluated with
converged complete cost, success probability one, and zero off-policy mass.
There were no runner errors. The semantic comparator passed 1,320 checks with
zero mismatches. Separate Runic and Dire Pelt comparisons each passed 40 checks
with zero mismatches.

Across each full portfolio, 14,400 simulator runs produced 13,834 successes,
566 unchanged action-cap failures, and zero off-policy failures. Six frozen
legacy Monte Carlo minimum-success expectations remain unmet: `0d1a` 3/100,
`a64d` 0/100, representative-four `62bc` 9/100, representative-four `c5ba`
3/100, `bd85` 1/100, and `e03e` 18/100. Exact evaluation proves every one of
those published graphs proper; simulation remains corroborating evidence only.

The comparator intentionally excludes runner-specific `expectation_met` and
full structural telemetry equality. Native and WASM wrappers have distinct
test expectation policies and runner input profiles, while equivalent
floating-point tie paths need not produce identical topology. It compares the
publication, bounds, evaluated cost, convergence, complete pricing, success,
off-policy mass, cap checks, errors, and verification behavior that define the
milestone.

## Acceptance Record

- Native release build: passed.
- Focused Calculator: 253,642 checks, 0 failures.
- Focused exact evaluator: 16,761 checks, 0 failures.
- Focused policy refinement: 4,875 checks, 0 failures.
- Focused solve: 98,260 checks, 0 failures.
- Focused API: 549 checks, 0 failures.
- Engine CTest: one invocation ran 10/11 targets before a stale S8.3 logical
  counter assertion failed. That test-only expectation was corrected and the
  failed S8.3 target then passed 367/367 checks. The full invocation was not
  repeated.
- Release WASM rebuild: passed. The generated `.mjs` is 42,115 bytes with
  SHA-256 `cf648f0ccdc3156b30fe0d0163e1065fbf460d2f5b15d519d36b1de66f01f23a`;
  `.wasm` is 4,960,151 bytes with SHA-256
  `ae6b8d733aeb5966a129128f1970a897756548356a28357fd23266c41e25a1de`.
- `npm test`: the first invocation exposed one over-strict `1.3e-14`
  progress/final floating comparison. The assertion was corrected to the
  existing `1e-12` semantic tolerance and the complete suite passed,
  including 27/27 engine smoke checks.
- `npx tsc --noEmit`: passed.
- Rendered browser review: intentionally not run.

The first direct native portfolio report omitted the explicit CLI spelling of
the case's goal-progress-gated flag. The case itself still carried the flag,
but the comparator exposed runner-profile telemetry differences. That report
is excluded; the accepted native report is the explicit gated rerun. The
external corpus runner retains its configured 900-second per-case watchdog;
the direct harness does not implement that watchdog, and no timeout was
changed.

## Recommended Next Milestone

Prioritize an exact-evaluator memory milestone that replaces the two measured
dense projections with shared-row or budget-projected sparse attribution while
preserving the current evaluator result and cap contract. Gate the choice from
the recorded probe units and projected owned peak before allocation. Treat the
Amulet coarse-parent collision and `bd85` solved-policy escape as a separate,
smaller strict-partition invariant repair; neither should be conflated with
evaluator memory accounting.

No next implementation boundary is active until Oliver selects it.
