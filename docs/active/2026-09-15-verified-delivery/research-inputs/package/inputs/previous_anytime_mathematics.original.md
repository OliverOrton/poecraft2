# Anytime delivery: mathematical supplement and documentation audit

**Research pin:** `4594e44b6b851511ae5471e52f232bce57d16856`  
**Date:** 2026-09-15  
**Scope:** read-only conceptual research for **Usable anytime results and trustworthy progress**.

This supplements the selected plan; it does not reopen the completed Conquest/Ring
programme or select a larger solver rewrite. No repository edits, native builds,
solves, simulations, or browser checks were performed. Exact synthetic checks and
a finite lifecycle specification were executed separately; their actual results
are in `reference_results.json`. Canonical prose below is prepared for integration,
not already committed. [Sources](SOURCES.md).

## 1. Direction and evidence disposition

The existing plan's runtime direction is sound: repair export/role presentation,
make an already verified answer usable through graceful finish, and attribute one
post-start responsiveness failure. The missing work is to specify the semantic
commitments behind those changes and make the durable mathematical record match
them. A finish button is not an action-budget solver; smaller cooperative slices
are not automatically a different stochastic policy or an equivalent schedule.

The last programme established about 81% earlier native verification at unchanged
Conquest cost, native recovery of Ring's known cheaper controller, and preserved
strong controls. It did not change the four-minute default delivery, settle the
250-ms worker-step limit, or close full-scope optimality. These are committed
receipts, not reruns in this review. [P6](SOURCES.md#p6)

The useful research question is now: **what must remain invariant between a
verified policy becoming available, an asynchronous finish request, interrupted
solver work, and delivery of the actual checked graph?**

## 2. Are the mathematical documents being maintained?

**Substantially, but not completely.** This is not a case of all the useful
reasoning being stranded in chat.

| Mathematical knowledge | Current canonical state | Needed action |
|---|---|---|
| Fixed-policy cost, resources and visits | Present in `policies.md` | Reuse, do not rederive as a new algorithm |
| New-controller occupancy and performance difference | Present with a counterexample to old-occupancy savings | Add/fix a stable identity anchor; one link currently points to the scalar return subsection |
| Separate primitive-action reward | Present, including mean versus capped-execution distinction | Preserve; no hard-limit optimization follows from it |
| Stopped response `g + H v_B`, one-shot/repeated return | Present with full mass and properness conditions | Reuse for actual continuations, not compilation-prefix bookkeeping |
| Graph-local decision provenance | Present in math and upper-authority pages | Retain its root-only and real-entry limits |
| Whole-controller comparison without scalar local class values | Present in `policies.md` | Preserve as an alternative verification route, not permission to invent local values |
| Clean-entry implication avoiding redundant junk predicates | Present, with disjoint/exact-count limitations | Make its stable anchor and latest application link explicit |
| New Conquest timing and Ring recovery in claim/RQ history | In living record and receipts, not fully joined into relevant ledger/RQ entries | Add compact scoped application histories without changing global status |
| Graceful finish as a committed-artifact selection contract | General stop/snapshot principles present; precise cut/invariants missing | Add the conditional argument in search/resumption and the source contract in resources |
| Cooperative slicing and passive telemetry | Resource/benchmark rules present; reusable refinement argument missing | Add a short canonical argument and its failure cases |

Relevant reads include the complete policy and search chapters, RQ-003, claim
histories CLM-0002/0004/0021/0022, the resource and upper-authority pages, and the
benchmark/change-impact contracts. [P1–P4](SOURCES.md#p1)

CLM-0002's latest visible application history is September 13 selective growth.
CLM-0021 still preserves the September 9 failed continuation mutation. Those
histories must not be erased, but the subsequent graph-local success and distinct
early-verification result deserve scoped application entries. CLM-0004, CLM-0021
and CLM-0022 remain **open** in the current ledger; finite native tests are not a
reason to promote their broad statements. CLM-0002 retains its existing accepted
conditional theorem. [P4](SOURCES.md#p4)

The recent graph-local and implied-predicate mathematics is already incorporated
in `policies.md`; the latest benchmark transport semantics are also present.
The requested maintenance is therefore a **targeted completion**, not a new
backbone project. The existing generated `research-state.md` is a declared
retention/lower series, not an exhaustive current strategy scoreboard. Do not
hand-edit it or force unrelated new outcomes into its schema. [P2/P4](SOURCES.md#p2)

Two small navigation repairs are appropriate. The general performance-difference
link currently targets `#first-return-improvement`, although its general matrix
identity is earlier. Give that earlier derivation its own stable anchor. Also,
several introductions say “current mechanisms” while linking source-era
`f3e7c0f` pages. Add current relative links or explicitly label those as historical;
do not mass-rewrite historical evidence links or erase original integration dates.

## 3. Three identities and two stopping times

Let the fixed crafting target be

\[
\theta=(S,A,P,c,G,s_0,\Pi),\qquad
V^*_\theta(s_0)=\inf_{\pi\in\Pi^{\rm proper}_\theta}J_{\theta,\pi}(s_0).
\]

This is the repository's existing target notation. Theta binds actual start,
mechanics/data, goal, action/program scope, prices and observation/control
semantics. It is not just the displayed base or goal count. [P1](SOURCES.md#p1)

Separately, let `run_id` identify one invocation, including its incarnation and
control lifetime. Two runs with identical theta must not accept each other's
finish commands. Let an artifact identity bind the actual rooted executable graph
and its certificate. A graph-local node ID is neither of those identities.

Let `n` index committed **solver computation** and let
\(T=\inf\{k:X_k\in G\}\) count future **crafting execution**. A finish request
stops computation at a cut `n`; it does not cap T. A one-state craft retry with
success probability 1/2 and cost 1 has total expected cost 2 even when planning
is stopped immediately after that policy is verified. A separate one-action
capped crafting session has expected spend 1 and success probability 1/2.
These are different questions, not competing calculations of the same upper.

## 4. F1 — Safe interruption is a committed-witness invariant

### Conditional statement

At computational step n, let E_n be the conceptual set of compatible, completely
checked root artifacts committed by then. This is a mathematical history, not a
request to store every historical graph. For each e in E_n, require an allowed
proper finite-cost policy pi_e, complete native execution/cost correspondence,
and its declared numerical certificate. In exact mathematics write

\[
U_n=\min_{e\in E_n}J_{\theta,\pi_e}(s_0),\qquad E_n\ne\varnothing.
\]

Then, for any finite computational stopping index sigma at which a matching
artifact is actually retained and returned,

\[
V^*_\theta(s_0)\le J_{\theta,\pi_{e_\sigma}}(s_0)=U_\sigma.
\]

**Proof.** Every checked pi_e lies in the feasible policy class. Its cost bounds
the infimum from above. Selecting one of those witnesses preserves the inequality
at every prefix; substituting sigma is a pointwise argument. No optional-stopping
theorem or solver-convergence assumption is needed. An adaptive stop based on
valid observations does not turn an unverified estimate into a certificate.

If E_n grows without semantic invalidation, its mathematical minimum is
nonincreasing. A bounded implementation need only retain a compatible no-worse
witness for every discarded best candidate; it need not retain the whole history.
A scalar minimum is not enough: evicting the cost-7 graph while retaining only a
cost-10 graph cannot support publication of 7. The weaker property “the returned
graph is valid” can still hold while “best verified found is preserved” fails.

With an independent same-target lower L, the existing sandwich gives
\(L\le V^*\le J_{\pi}\). Stopping does not prove equality or retire any remaining
action. Root-only evidence stays root-only. [P1/P2](SOURCES.md#p2)

### Numerical boundary

The exact formula is about true costs, or rigorously justified enclosing endpoints
with the corresponding inequality. Current native reports instead carry their
existing complete-evaluation and reconciliation contract. Do not claim their
printed decimals become exact rational upper endpoints through this argument.
If safe intervals exist, minimizing interval upper endpoints minimizes that
conservative score, not necessarily the unknown true policy costs. This programme
does not change numerical acceptance. [P3](SOURCES.md#p3)

### Implementation consequence

“Verified policy available” must mean a retained, compatible graph **and** its
complete root certificate under the publication classifier, not merely a trace
stage, numeric estimate, or old UI value. Confirm this at the native safe point.
This is a new acceptance obligation for graceful finish, not a claim that a new
artifact-loss defect has been observed in current main.

## 5. F2 — Define the finish operation's selection cut

The previous plan says to preserve later better candidates accepted “before
delivery.” That phrase is underspecified if serialization and message transport
are in progress. Replace it with a finite, source-owned **result-selection cut**:
the point where ordinary publication seals the artifact/certificate selected for
this response. Reuse the existing finalized-result owner rather than add a new
portfolio framework.

Distinguish intent received, safe-point finish acknowledgement, result selection,
terminal response commitment, and graph/UI delivery. No fresh speculative search
starts after acknowledgement. A check already completed before the selection cut
must transfer any eligible no-worse artifact from its owner, including a strict
owner with a cheaper already-verified result. Do not discard that artifact merely
because the central scalar/slot was not updated yet. An unfinished cost-1 estimate
is not such an artifact and need not delay a retained cost-10 result.

At the selection cut select the best compatible fully verified eligible artifact
under existing ordering; bind its bytes, value, root certificate and context as
one immutable result. Serialization does not silently change the winner afterward.
An applicable cancellation received before terminal response commitment may win
under the declared worker ordering; after commitment it
must not create a second response or affect a new run. Duplicate requests are
idempotent. An exact result already completed remains exact rather than being
downgraded by a late finish message.

This explicit effect point is informed by linearizability's request/response
reasoning, but is a proposed small protocol contract, not a proof of the current
worker's concurrency implementation. Linearizability-style safety alone would
not establish bounded response time. [L2](SOURCES.md#l2)

A sufficient state-level invariant is:

1. Every exposed verified endpoint identifies a complete owned eligible witness.
2. The selected result equals the best eligible witness at the recorded cut.
3. The response's graph/cost/certificate share theta and run incarnation.
4. A terminal response is committed at most once; later stale control is inert.

The finite oracle explores 23 distinct model states and 230 command transitions
for a deliberately small instance. It tests these invariants under the stated
model, including an in-flight cost-7 commit after finish intent but before sealing.
It does not prove native correspondence, unbounded fairness or eventual response.

## 6. F3 — Slicing a transaction must not publish its prefix

Represent a concrete resumable computation by W, containing committed evidence K,
a task cursor and staged scratch. Let alpha(W)=K forget incomplete scratch.
Suppose every finer step either leaves alpha unchanged, or performs a permitted
complete logical commit of the old algorithm. Suppose finish/cancel discards only
uncommitted scratch and preserves committed evidence, selected-controller identity
and cumulative work debit. Then every projected finite trace is a valid sequence
of complete logical commits with additional stuttering observations.

**Proof.** Induct over fine steps. A scratch-only step leaves the committed
invariant unchanged. A logical commit satisfies the original transition's
preconditions. An interruption discards no committed premise. Thus complete-row,
certificate and artifact invariants survive every finite prefix.

This is a safety argument. Finite work per task, eventual service and no endless
restart are separate liveness premises. It is also not a claim of equal timed
trajectories. If every new yield returns to the action scheduler, changes an
adaptive quantum, changes arithmetic reduction order, or resets a logical budget,
the same-trace premise has not been established. [P3/L3](SOURCES.md#p3)

The current native `can_prepare_requested_bounded_finish` comment explicitly
relies on public steps **not suspending inside sparse-row append**. Adding a yield
there is therefore not a local no-op: the append must remain atomic or become a
properly staged transaction, with all consumers unable to see an incomplete row.
The incremental upper pass also owns a moved lower snapshot and has a dedicated
restore path. Preserve both obligations. [P5](SOURCES.md#p5)

A row with known 0.9 goal mass and an unfinished 0.1 trap branch is not a complete
0.9-success action. Normalizing its prefix creates a different, falsely proper
controller. Discarding staged data does not refund the computation already spent.
Restarting an allowance of three units on each call allows two two-unit slices
to consume four, violating the original total budget.

### Frozen-memory accounting is the same kind of dependency argument

Let F be a previously audited transitive immutable footprint, D_k changing owned
storage, S_k overlapping scratch, and A a proposed allocation. Reuse of F is safe
only while the entire audited storage/capacity/alias ownership is unchanged; the
admission check must still include \(F+D_k+S_k+A\), with shared ownership counted
according to the existing ledger. A hidden lazy cache or growing vector invalidates
the frozen audit. Cancellation releases live scratch but preserves cumulative
logical work. This is conditional bookkeeping equivalence, not permission to
freeze an estimate while its dependencies mutate. [P3](SOURCES.md#p3)

## 7. F4 — Reuse the evaluated executable object, not a coincident value

For a complete finite rooted policy, a relabeling/bijection that preserves actual
entry, goal/failure truth, permitted observations, chosen actions, all positive
transition probabilities and rewards defines the same stochastic execution law.
Consequently J, expected primitives N and priced resource totals are unchanged.
One proof is induction on finite execution prefixes followed by nonnegative
summation; another is permutation of the same transient fixed-policy equations.
This does not require proving equality for unselected competing actions.

For the new finish path, unchanged retained bytes plus the complete context and
existing certificate is a conservative starting point. A trusted already-existing
presentation mapping can cover node positions/renaming where its semantics are
established. Do not build a general equivalence checker merely to skip a final
check. Equal costs, equal node counts, the same first action or matching goal
masks are not a semantic equality proof. Changing a router guard or priority can
make previously unreachable behaviour reachable. [P2/P3](SOURCES.md#p2)

### The Ring clean-entry simplification is a useful existing corollary

If a router's retained condition C entails a predicate F over **all possible
semantic entries that this router can receive in the composed controller**, then
\(C\land F\iff C\). Removing F changes no choice at that router. It need not
establish an all-action state quotient.

For each affix side, let the exact occupied count be k and let the entry predicate
supply k distinct acceptable affixes assigned injectively to k required goal
slots. All occupied affixes are then accounted for, so no additional junk remains.
The native guard's disjoint side-specific satisfying masks provide a sufficient
way to ensure this injectivity. Below-tier family membership is not satisfaction.
Two overlapping requirements both matched by a single affix do not account for a
second occupied junk affix; the cardinality inference fails there.

This is the argument **already present** in the current math chapter and used by
the narrow single-pass compiler. It should receive a stable anchor and be preserved
as an explicit conditional lemma. It does not authorize removing guards from dirty
entries, shortening mandatory programs, changing whole-controller scope, or using
one observed representative to establish a universal implication. Full emitted-
controller evaluation remains the native acceptance owner. [P2](SOURCES.md#p2)

## 8. F5 — Passive observation is not timed noninterference

A read can leave all logical solver state unchanged and still consume enough time
or retained memory to alter a bounded run. With a ten-unit deadline and two-unit
tasks, five commits fit without read overhead. A one-unit pure read after each
task permits only three in the same interval. This synthetic example says nothing
about native timing; it disproves the general inference “passive means free.”

The actual worker's time-adaptive quantum is another dependency: inserted overhead
can affect a future scheduling decision even when the observation itself has no
side effects. Test separately: (a) no semantic mutation at a fixed logical
checkpoint; (b) timed overhead and resulting policy preservation at the unchanged
budget. The prior report-persistence regression is relevant native evidence for
this distinction. [P4/P5/P6](SOURCES.md#p4)

An event stream must disclose coverage and drops. An absent candidate-ready event
establishes no readiness only within a completely observed declared event interval.
Two histories becoming ready at seconds 2 and 9 can have identical samples at 0
and 10. Sequence numbers provide source order; source and host elapsed clocks need
explicit origins/mapping before subtracting their values. No missing sample is a
zero-work measurement.

## 9. F6 — Responsiveness is a composed obligation

For a successful graceful-finish request, decompose latency within a consistently
mapped clock into dispatch, remainder of current noninterruptible work, staged
release, transfer of already-complete eligible evidence, sealing/packaging,
transport and UI readiness. A bound on the first native stepped call bounds only
one term. Setup, trace serialization, destructor work and graph preparation may
sit outside that measurement.

An observed maximum below 250 ms is a test outcome, not a worst-case theorem for
all requests or a guarantee of a ten-second completed response. The existing plan's
10-second request-to-usable and 65-second total targets remain empirical gates.
Declare which total clock is used: worker solve time includes synchronous native
begin but may exclude earlier Calculator request preparation. Report end-to-end
UI time separately rather than silently resetting the clock. [P3/P4/P6](SOURCES.md#p3)

Giving the user a finish control needs no optimal-stopping policy. A flat recent
upper and many rounds do not prove that further search lacks value. Automatic
metareasoning would require a separate model of future benefit and computation
cost, as the anytime-control literature studies. It is not selected here.
[L4](SOURCES.md#l4)

## 10. Integration plan and evidence requirements

Keep the existing runtime M0–M3. Add the following requirements before declaring
closeout:

- Resolve the selection-cut ambiguity and native artifact-availability condition
  before implementing graceful finish. Exercise actual same-run IDs, existing
  strict-owner transfers, pending check abandonment and final artifact identity.
- Any inserted cooperative boundary must identify its atomic transaction, staged
  storage, complete-commit point and budget debit. A status-only phase mutation is
  not scheduling progress and must not reroute a suspended coroutine.
- Use the stable artifact/context equivalence argument only where all dependencies
  are unchanged. Keep new graph checks and numerical acceptance unchanged.
- Retain the source report/script once, put accepted derivations in their canonical
  chapters, add scoped claim application histories, and link recent outcomes from
  RQ-003. No broad docs/index rewrite or new knowledge database.
- Run affected link review and existing `solver_knowledge lint --base` where the
  checkout supports it. A clean lint is syntax/reference evidence, not a proof.
  Report actual reviewer and actual native coverage; do not turn these research
  examples into a native acceptance count.

The companion `CANONICAL_DOC_INSERTIONS.md` gives prose to integrate, and
`CODEX_ADDENDUM.md` is portable without any other attachment. The combined
`CODEX_PROMPT.md` contains that addendum and the original current plan. New
mathematical requirements clarify authority but do not add a second solver feature.

## 11. Checks actually executed

The standard-library script executed **21 named checks**. These include exact
rational fixed-policy rewards and relabeling, a root-unreachable trap, missing
positive mass, committed portfolio prefixes, a 23-state/230-transition finite
lifecycle oracle, four complete slice partitions of a three-term exact law,
256 abstract side-occupancy assignments for the clean-entry implication,
an overlapping-goal counterexample, pure-read timing, memory and latency examples.

All passed. The finite lifecycle test establishes safety only for that enumerated
specification; it does not prove liveness or native implementation equivalence.
The script's exact arithmetic tests are checks of the written formulations, not
independent PoE-mechanics oracles. No existing repository lint was executed here,
because no maintained checkout was modified. The documented native and WASM
receipts remain their original authors' measurements. [P6](SOURCES.md#p6)
