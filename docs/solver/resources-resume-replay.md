# Resources, Resume, And Replay

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

This page owns resource and lifetime contracts. [Search and resumption](mathematics/search-and-resumption.md#snapshots) explains the semantic dependencies that determine whether cached or resumed evidence is still usable.

## Resource Dimensions

States, expanded states, rows, transitions, V1-equivalent reforge work, strict kernels/classes, refinement work, compiler output, evaluator pairs, and solver-owned memory have distinct limits. Wall time and cooperative slice latency are recorded separately.

A larger allowance in one dimension may expose a different first stop owner. A cap is computational evidence, not a finite-horizon reformulation of the native goal. Never treat unvisited probability or ungenerated actions as absent because a cap fired.

Primary owners include `solver_solve_contracts.hpp`, `solver_calc_types.hpp`, `solver_solve_telemetry.cpp`, `solver_policy_refinement.cpp`, `solver_eval.cpp`, and the option parsing in `solver_api.cpp`.

## Candidate checker and native headroom

The additive public `pc_solve_options.candidate_max_states`,
`candidate_max_pairs`, `candidate_max_transitions` and
`candidate_max_owned_bytes` fields control the actual candidate evaluator.
Zero preserves each owner's inheritance. `struct_size` guards old callers; the
extension starts after the legacy aligned prefix. The return evaluator retains
its historical 1-GiB ceiling when the memory field is omitted. Explicit checker
memory still fits the aggregate remaining solver bytes after parsed graph,
economy, parent, child and compiler overlap. It is not an additional allocation.
Logical reforge work is debited cumulatively, including interrupted evaluation.

`PC_SOLVER_FLAG_DIRTY_CONTINUATION_SEARCH` opts a native Calculator-profile
request into fresh private search. It raises no limit itself. The selected large
native profile uses 8 GiB aggregate solver memory, 4 GiB candidate/final checker
ceilings, 2M physical states, 10M pairs, 40M transitions and 200M logical work.
Ordinary search caps remain separate. Browser defaults retain their existing
allowances; native measurements do not establish browser latency or capacity.

A native caller can enable the mode on its existing initialized Calculator
options before `pc_solver_solve_begin` (the remaining request fields retain
the caller's declared settings):

```c
options.struct_size = sizeof(options);
options.solve_profile = PC_SOLVE_PROFILE_CALCULATOR_PRODUCT_V1;
options.solver_flags |= PC_SOLVER_FLAG_DIRTY_CONTINUATION_SEARCH;
options.max_solver_owned_bytes = UINT64_C(8) << 30;
options.max_reforge_work = UINT64_C(200000000);
options.candidate_max_owned_bytes = UINT64_C(4) << 30;
options.candidate_max_states = 2000000;
options.candidate_max_pairs = 10000000;
options.candidate_max_transitions = 40000000;
```

The host still owns bounded-finish timing, total watchdog, fresh memory
admission and any separate final evaluator. The mode is optional; an exhausted
candidate check preserves the compatible verified portfolio.

Corpus `--worker-headroom-bytes` adds process/final-evaluator overlap to the
solver reservation and records both terms in the resolved command and ledger.
Wide runs reserve 14 GiB and retain at least the greater of 8 GiB or 20% of
physical memory outside that reservation, using current host admission.
Solver-owned estimates, checker peaks and process memory remain distinct.
Time-only follow-throughs identify search finish, native total watchdog and
outer cleanup independently. A final-check timeout remains unknown even when
the solver retained a verified graph. The [living record](../active/2026-09-11-dirty-state-continuation/README.md)
owns profiles and observations.

## Cooperative Work

Solve work, automatic admission, broad exact reforge rows, strict refinement, compilation assertion, and evaluation retain explicit continuation state. A public step advances bounded logical work, but cancellation is observed only when the relevant cooperative boundary returns control.

An unfinished exact row owns scratch and cannot install a normal cached distribution. A yielded candidate can retain its bounded preference; a terminally refused candidate must release its active payload and stop suppressing ordinary joint-policy work. Small diagnostic tombstones are not active continuation ownership.

Cancellation abandons unpublished work. Previously verified evidence survives only where the owning publication contract permits it. A successful cancellation of a process or cursor is not a successful solve.

A requested bounded finish remains latched during refinement, compilation and
certification. At a strict-work suspension, a compatible portfolio artifact can
return through ordinary publication when its independently evaluated cost is
no greater than the best verified cost reported by strict work. A cheaper
verified strict artifact must first reach publication; an unverified estimate
cannot replace either one. This stop retains only the existing independent
lower and reports `requested_bounded_finish`, without declaring unresolved
alternatives closed or manufacturing a resource cap. When no verified artifact
is available, the finalizer still needs compilation and evaluation. Completed
exact closure remains an acceptable stronger result. The
[bounded-publication measurements](../active/2026-09-09-empty-start-partial-continuation/README.md#bounded-finish-publication)
separate this contract from construction of new continuation rows.

Missing selected-policy continuations use the existing refinement queue. A
candidate's prefix/publication walk discovers at most the current
`q_refinement_batch` missing entries before yielding; byte-cap checks include
the existing walk and request storage. The next batch services eligible named
entries and retires them at selection, so requests do not become permanent
head-of-line reservations. This changes work order only. At resource-stop
finalization the first missing continuation still refuses immediately.

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

The optional dirty guidance queue uses the existing candidate checker and
aggregate remaining ownership. Static and adaptive treatments have identical
admission and checker limits; the run-local correction table reports 88 bytes.
Lookup/update counters are computational telemetry, not crafting costs. Setup
is not separately timed (the current zero setup field is not a measured zero).
An interrupted construction is censored work evidence and supplies no cost
label or executable continuation. The [matched v2 application](../active/2026-09-12-adaptive-dirty-guidance/README.md)
keeps the qualified 600/840/870-second native profile separate from original
profiles and browser defaults; it does not increase a capacity to obtain its
policy improvement.

The Lab adds process-level watchdog, cancellation, host reservation, and supervisor recovery. Its host headroom is separate from the native solver cap. See [Solver Lab](../foundation/solver-lab.md#statuses-resources-and-recovery).

Pausing a queue stops new dispatch; it is not pausing and checkpointing a live solver. A terminated worker's valid partial report remains partial. PID identity, no-survivor checks, and lease release belong to the supervisor rather than to a numerical proof owner.

## Reading interrupted evidence

Use the first named cap/stop owner, current phase, retained incumbent status, open action/frontier obligations, and partial-report identity. Read [benchmarking](benchmarking.md#durable-partial-reports) before treating watchdog output as a censored trajectory. Missing observations are not zero work, and resource failure is not exact inapplicability.

Selective dirty growth retains rows, kernels, selected decisions and numerical
seeds only within one compatible private layout/session/price context. Selected
row IDs are append-only there. After a complete native check, equality of the
root-reachable selected row vector permits reuse of that root check when only
unselected alternatives changed. This is not a new entry certificate: no parent
binding or off-path upper is synthesized. A changed selected row requires the
usual compile/properness/evaluation route. Context changes discard the vector;
there is no persistent library or disk-resume claim.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md) and [2026-08-30-carrier-ladder-released-candidate-reclamation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md), [2026-09-05-native-applied-reforge-preparation-v1](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md), [solver-lab.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-lab.md). Claim IDs are registered in [the ledger](claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](research.md#open-obligations); an argument link does not confer runtime authority.

The exact evaluator can materialize an identity replay partition cooperatively
under its declared memory limit. Every retained native outcome, route, state,
observation and probability survives; the existing full-row mass check remains.
Replay tokens and new concrete transition/absorption vectors overlap during
conversion and are charged before allocation and across suspension. A cap during
partial conversion cannot enter the ordinary identity-graph fallback. This
replaces the former fixed 4096-pair refusal, not any numerical acceptance test.
