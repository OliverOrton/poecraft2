# Resources, Resume, And Replay

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page owns resource and lifetime contracts. [Search and resumption](mathematics/search-and-resumption.md#snapshots) explains the semantic dependencies that determine whether cached or resumed evidence is still usable.

## Resource Dimensions

States, expanded states, rows, transitions, V1-equivalent reforge work, strict kernels/classes, refinement work, compiler output, evaluator pairs, and solver-owned memory have distinct limits. Wall time and cooperative slice latency are recorded separately.

A larger allowance in one dimension may expose a different first stop owner. A cap is computational evidence, not a finite-horizon reformulation of the native goal. Never treat unvisited probability or ungenerated actions as absent because a cap fired.

Primary owners include `solver_solve_contracts.hpp`, `solver_calc_types.hpp`, `solver_solve_telemetry.cpp`, `solver_policy_refinement.cpp`, `solver_eval.cpp`, and the option parsing in `solver_api.cpp`.

## Cooperative Work

Solve work, automatic admission, broad exact reforge rows, strict refinement, compilation assertion, and evaluation retain explicit continuation state. A public step advances bounded logical work, but cancellation is observed only when the relevant cooperative boundary returns control.

An unfinished exact row owns scratch and cannot install a normal cached distribution. A yielded candidate can retain its bounded preference; a terminally refused candidate must release its active payload and stop suppressing ordinary joint-policy work. Small diagnostic tombstones are not active continuation ownership.

Cancellation abandons unpublished work. Previously verified evidence survives only where the owning publication contract permits it. A successful cancellation of a process or cursor is not a successful solve.

## Ownership and memory accounting

Retained arrays, nested row capacities, active scratch, emitted strategy payload, and optional diagnostics have different lifetimes. Accounting must follow the actual owner, including storage grown before an allocation failure.

The retention preparation reserves its additional proof workspace inside the existing total native cap. It reuses compatible value-independent support/caps, not stale final-value minimizers. Do not create a second uncharged cache or count discarded scratch as a durable result.

Some observational JSON projections are deliberately outside the proof's solver-owned cap and remain bounded by serialization limits. That does not make their process memory or elapsed construction time zero. Preserve the declared attribution in comparisons.

## Reuse is evidence-specific

| Retained object | Compatibility that matters |
|---|---|
| Price-independent transition closure | Start/layout/action/admission namespace and graph-affecting options; rows are repriced |
| Evaluated policy value | Actual strategy, entry/control state, probabilities, scope, prices, and numerical contract |
| Native lower certificate | Entire target/domain, coefficient/projection evidence, boundary certificates, and required generations |
| Value-independent event support or caps | Native request/domain, action, pool/forced-draw scope, and proof of independence from the candidate vector |
| Minimized event allocation | The current candidate values as well as its event constraints; it must be recomputed or re-certified after a relevant change |

A digest accelerates lookup; it does not replace the canonical equality check. Conversely, a price change need not invalidate genuinely price-independent transition structure, even though it invalidates cost-dependent evidence. Apply the actual object's contract rather than one global “cache valid” flag.

## Development Replay Boundary

`SolveTransitionCache` reuses a completed reachable closure within one compatible calculator. Native development replay serializes that closure together with the ordered state, operator, candidate/dependency, and automatic-admission namespace that gives its local IDs meaning.

The supported benchmark interface is:

```text
poecraft_solver_benchmark ... --case CASE \
  --save-development-checkpoint PATH
poecraft_solver_benchmark ... --case CASE \
  --load-development-checkpoint PATH
```

The outer checkpoint identity binds ABI/compiler, compiled artifact, canonical case, resolved economy, and relevant CLI graph overrides. This outer replay contract is narrower than the inner price-independent row cache. Format/layout guards, payload length, and checksum also apply.

Save refuses incomplete or focused graphs, active row/admission cursors, and the proof-carrying quotient graph. Load requires a fresh solver and compatible namespace/options; it may not disguise a failed replay as rebuilding the graph.

Replay skips completed coarse transition construction only. It reruns Bellman, strict refinement, compilation, and evaluation. It is not a request/result cache, public proof, or strategy publication authority, and is not a release-WASM feature.

A strict-partition checkpoint remains a separate unimplemented contract in the reviewed reference. It would need oracle/session ownership, selected closure, partition generations, proof store, obligations/dependencies, active cursors, and verified incumbent. Request/result JSON is not such a checkpoint.

## Native orchestration and host limits

The Lab adds process-level watchdog, cancellation, host reservation, and supervisor recovery. Its host headroom is separate from the native solver cap. See [Solver Lab](../foundation/solver-lab.md#statuses-resources-and-recovery).

Pausing a queue stops new dispatch; it is not pausing and checkpointing a live solver. A terminated worker's valid partial report remains partial. PID identity, no-survivor checks, and lease release belong to the supervisor rather than to a numerical proof owner.

## Reading interrupted evidence

Use the first named cap/stop owner, current phase, retained incumbent status, open action/frontier obligations, and partial-report identity. Read [benchmarking](benchmarking.md#durable-partial-reports) before treating watchdog output as a censored trajectory. Missing observations are not zero work, and resource failure is not exact inapplicability.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md) and [2026-08-30-carrier-ladder-released-candidate-reclamation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md), [2026-09-05-native-applied-reforge-preparation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md), [solver-lab.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-lab.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.
