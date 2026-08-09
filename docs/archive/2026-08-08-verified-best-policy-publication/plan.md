# Verified Best Executable Strategy Publication

**Status: completed and archived 2026-08-08.**

Owner: Oliver

Parent: [archive entry](README.md)

Starting commit: `c2d47d86143b2381189c6bcba47d288315ad9929`.

Branch: `codex/verified-best-policy-publication`.

## Objective

Publish the cheapest independently evaluated executable strategy found during
the solve. Claim optimality only when that strategy's independently evaluated
cost matches a valid global lower-bound certificate within the existing
tolerance.

A public strategy must be the exact final emitted JSON document that was
parsed and independently evaluated. A class-policy witness, coarse Bellman
value, successful compilation, or simulator sample cannot substitute for
final-graph evaluation.

Keep these conclusions separate:

1. Executability requires successful parsing, converged exact evaluation,
   eventual success probability one, complete pricing, zero off-policy mass,
   and finite expected cost.
2. The final graph evaluator's expected cost is the authoritative executable
   cost and candidate upper bound.
3. Exact optimality additionally requires a valid global lower bound matching
   that evaluated graph cost.

A mismatch between a coarse or strict class-policy estimate and the final
graph cost blocks exact optimality but does not invalidate an otherwise
executable graph. Such a graph remains an honest bounded candidate at its
independently evaluated cost.

## Preserved boundaries

- Change no crafting mechanic, goal semantic, action admission/filtering,
  price, core Bellman comparison, state abstraction, strategy vocabulary,
  simulator action limit, resource cap, watchdog, or reconciliation tolerance.
- Do not restructure the solver, add a parallel policy subsystem, or undertake
  a large evaluator or strict-partition rewrite.
- Reuse the incumbent, certified portfolio, compiler, exact assertion,
  evaluator, publication telemetry, and existing resource ledgers.
- Keep all publication authority native. WASM and TypeScript transport and
  present the native result.
- Retain compiled-but-unverified artifacts only as diagnostic evidence. They
  carry no finite upper and cannot be public executable, bounded, certified,
  or exact policies.
- Run the complete selected acceptance only once after implementation. Use
  10,000 simulator runs wherever compiled-strategy verification is required.
- Run no rendered browser review and keep both required commits local.

## Gate 0 — freeze current evidence

Reuse provenance-matched expensive evidence when request, artifact, binary,
and source identities agree; do not rerun expensive cases merely to reproduce
frozen numbers. Preserve a before matrix for:

- `reliability-class-fishingrod`: retained renewal incumbent near
  `16.482913436844125`, fallback candidate present, zero publication attempts,
  and no public policy;
- `reliability-class-ring`: scoped direct certification consumes about 5.07M
  reforge work, after which strict lift receives zero;
- `reliability-class-amulet`: core policy and compiled artifact exist, final
  verification does not complete, and nothing publishes;
- `natural-t1-representative-three-bd85c8c40df7`: core estimate near 13,098,
  unverified core artifact, and published renewal near 14.28M;
- `natural-t1-representative-three-60546fa5c271`: working retained-core
  recovery control;
- `natural-t1-representative-three-d0a2115efe54`: tiny lower/upper inversion;
- `natural-t1-deep-four-dense-a64d3d932590`: material lower-above-upper
  publication;
- `natural-t1-breadth-two-c4c462fd3d77`: strict graph whose independent
  evaluation estimates about 4.5 GiB;
- the Dire Pelt two-goal and historical Runic Gauntlets five-goal controls;
  and
- one simple directly certified one-goal control.

## Gate 1 — restore every eligible incumbent

On every unresolved or non-exact exit, consider the best retained incumbent
regardless of stop cause. Compare it with any current policy, preserve its
policy and provenance, and attempt final compilation and verification. Record
candidate presence independently of final `policy_available`.

Primary acceptance is the Fishing Rod renewal reaching independent
verification and publishing bounded near `16.482913436844125` instead of being
discarded on ordinary `not_converged` finalization.

## Gate 2 — correct reforge-work transfer

Debit only the scoped logical reforge work actually consumed by direct
certification. Preserve monotonic aggregate/component counters and give strict
lifting the genuine remaining global allowance. Ring must no longer report
about 5.07M scoped work followed by a strict allowance of zero. Completion of
the lift with its remainder is evidence, not an acceptance requirement.

## Gate 3 — independently evaluate every final graph

For every direct, strict, or renewal candidate considered for publication:

1. Compile its final strategy JSON.
2. Parse that exact emitted document.
3. Run the independent exact graph evaluator on it.
4. Require properness, success probability one, complete cost, zero off-policy
   probability, convergence, and finite expected cost.
5. Store the independently evaluated graph cost as the candidate upper.

The strict class-policy witness may guide refinement and check internal
consistency, but it cannot replace the independent final-graph evaluation. A
resource- or convergence-limited evaluation retains the artifact and its
failure provenance as `compiled_unverified`, with no finite public upper.

## Gate 4 — separate mismatch from executability

Publish an independently executable mismatched graph as bounded at its final
evaluated cost. Record absolute/relative reconciliation differences and false
cost reconciliation, do not claim exactness, and retain a lower bound only
when it is mathematically valid and no greater than the evaluated upper.
Otherwise use zero. Keep the existing tolerance and characterize the observed
mismatch distribution for later work.

## Gate 5 — centralize publication invariants

Apply one final validator to direct, strict, and fallback candidates:

- a finite public upper owns a retained independently evaluated artifact;
- `0 <= lower_bound <= upper_bound`;
- a within-tolerance lower/upper inversion clamps lower to upper;
- a larger contradiction discards the unsupported lower and uses zero;
- gaps are recomputed after normalization;
- exact status requires independently evaluated JSON plus global lower closure;
  and
- invalid bounds never render a `1.00x` certification claim.

## Gate 6 — publish the cheapest verified candidate

Before publication or portfolio clearing, compare every retained independently
evaluated candidate by final graph cost within tolerance. A strict lift cannot
replace a cheaper verified direct or fallback graph. An unverified coarse
estimate cannot compete as if it were an evaluated cost. Extend the existing
portfolio to record candidate identity, stage, verification, evaluated cost,
disposition, and selection reason; do not add a separate ledger subsystem.

## Gate 7 — bounded diagnostics

Add sample-count- and byte-capped deterministic diagnostics for structural
strict-lift failures (`invalid_solve_state`, `coarse_mapping_failure`, and
`observation_unavailable`). Record coarse/strict state counts, solved policy
size, offending state/successor IDs, partition/mapping identities, selected
operator, and first violated invariant.

For the roughly 4.5 GiB and 36 GiB exact-evaluator refusals, record graph
nodes/edges/regions, reachable states and state/action pairs, component sizes,
dense versus sparse path, major retained/transient allocation estimates, and
the refusal calculation. Classify each as incorrect accounting, avoidable
dense work, genuine graph complexity, or a strict-partition invariant issue.
Do not implement the resulting large fix in this milestone.

## Gate 8 — acceptance and completion

After implementation is complete, run the selected acceptance once:

1. native release build plus focused solver/API coverage;
2. engine CTest and the complete reliability corpus;
3. exact final-graph evaluation and 10,000-run simulation for required
   compiled-strategy qualifications;
4. release WASM rebuild;
5. web tests and TypeScript checking; and
6. native/WASM semantic comparison of publication status, executability,
   evaluated cost within tolerance, success, properness, off-policy mass, and
   selected behavior.

Simulation is corroborating evidence only. A run beyond 100,000 actions is
truncated; truncation does not invalidate an independently proven proper graph,
and completed-run-only averages are never certified expected costs. Equivalent
native/WASM tie paths need not have identical graph bytes or topology.

Produce a before/after case matrix, candidate identities and costs, first
failed stages, resource accounting, bound normalization, final-graph evidence,
native/WASM comparison, evaluator-memory breakdown, mapping diagnostics, and a
recommended next milestone. Archive the completed plan/report, restore
`HANDOFF.md` to no active boundary, and leave the tree clean.

Exactly two local commits are permitted:

1. this plan and handoff boundary before production changes; and
2. implementation, evidence, report, archive, generated WASM, and final docs.
