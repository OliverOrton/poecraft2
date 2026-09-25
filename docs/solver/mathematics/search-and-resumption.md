# Search, pruning, and resumable work

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


A search procedure decides what to compute next. A proof decides what may be concluded from what was computed. Keeping those responsibilities separate allows aggressive experimentation without turning a promising score or incomplete row into an authority.

The current mechanisms remain described by [Scheduling and Bellman Search](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/scheduling-bellman.md) and [Resources, Resume, and Replay](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md).

<a id="three-questions"></a>
## 1. Validity, eventual progress, and efficiency are different questions

**Validity:** whenever a lower, upper, or exactness result is published, is its argument satisfied?

**Progress:** if enough work is allowed, are the unresolved obligations eventually serviced and the required fixed points or closures reached?

**Efficiency:** does that happen within a useful wall-time, memory, or work budget on the intended problems?

A heuristic can rank work badly and still publish only sound results. A fair schedule can be impractically slow. A capped run with no answer is not a counterexample to a theorem whose premise is unlimited fair execution. Conversely, a sound bound does not establish that the scheduler will ever finish its proof.

The experimental finder expands finite control holes in a bounded live
frontier. A completed or capped native check may schedule a deeper child, but
the child receives no authority from its parent; it must compile and pass its
own full evaluation. Parent and graph identities are retained for experiment
audit. Beam width, deduplication and attempt ceilings limit the search and
provide no fairness or optimality guarantee.

A pending producer adds a service obligation even when its numerical read has a
safe fallback. If a one-shot certificate sees zero while a nonnegative lower is
pending, zero is admissible but does not mean the completed certificate failed.
Retain the compatible source/row obligation until a complete generation is ready,
refused, cancelled, capped or legitimately superseded. Service before the next
dependent expansion restores the original decision without making queries
blocking. This is a progress condition; the original lower/upper and strict
dominance premises still decide whether any action may be retired.

The documented carrier ordering uses progress, side capacity, blockers, and other observations as priorities, while proof values have separate owners. This is the correct conceptual separation. It does not prove the current scheduling implementation fair. [Scheduling and Bellman Search](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/scheduling-bellman.md).

### First-policy debt and later continuation cost

For a nonterminal ordinary item at the requested rarity, with distinct required
goals, let m be the required count, g the satisfying modifiers and k the explicit
affix count. `joint_policy_terminal_debt` gives `max(1,m+k-2g)`; true terminal
success is separately zero. From an empty Rare, redraw strictly reduces debt
only when `2g>k`. Two goals among four affixes and three among six therefore tie
their empty sources. Missing successor routes also precede cost in the startup
seed. Neither preference is a lower or an economic dominance test.

The complete synthetic dirty route costs 15 versus a clean alternative of 100,
including losing branches and paid recoveries. It refutes a general inference
from an immediate debt tie to economic uselessness; it is not native crafting
evidence. The [imported derivation](../../active/2026-09-11-dirty-state-continuation/research-inputs/review_and_mathematics.md)
preserves its premises and equations.

In the current native constructor, this debt key is used only for completed
seed rows while high-impact uppers and incremental action generation are active
and no *output incumbent object* exists. The later key instead prefers a true
goal or a higher satisfied-goal count. An independently verified graph and an
output incumbent object are distinct conditions. The passive September 25 A5
selector observation counted 19,667 calls, all gate-off because an output
incumbent object existed, and zero gate-on comparisons. Its status is
`measured_gate_inactive`: the synthetic gate-on reversal proves observer
sensitivity, but the actual run cannot establish whether union acquisition
credit would improve a live gate-on choice. The counterfactual key credits the
union of requested goals already present or obtainable in a completed priced
row, while retaining the existing pending-route precedence and exact-key
comparison. It orders proposals; it supplies no lower, properness or action
retirement authority. Absence from a search-filtered candidate set is not a
verified non-improvement. [S measurement](../../active/2026-09-25-seed-retention/README.md#k1--s-selector-observation).
The active carrier ordering uses a different within-goal-mask
occupancy comparison. Its neutralization changed service order but returned the
same checked original-root controller and cost; search effort and crafting
economics are separate observations. [Native disposition](../../active/2026-09-25-dirty-progress/README.md#j3--matched-original-root-result).

The native dirty proposal begins after a current-run independently verified
graph is retained. It completes acquisition and paid cleanup rows at nonempty
entries and reuses sparse policy evaluation/selection for full continuation
costs. Initial zero numerical seeds have no bound authority. Root price per
positive goal-progress probability and private-layout values only order or defer
proposals. They do not retire original actions, replace the independent lower or
certify full-scope optimality. The emitted graph requires full native evaluation.

The existing publication coroutines own this optional work, its private namespace
and evaluator checkpoint. Both initial joint-controller and ordinary selected-policy
certification can supply the first verified opportunity, with one attempt per
solve. Finish or abandon releases the attempt and preserves compatible verified
artifacts. This v1 uses that publication opportunity; it
does not rescan unchanged rejected queries or maintain a permanent basin library.
Reconsideration needs a changed candidate, entry, completed support or effective
budget, with corresponding identity checks.

<a id="count-aware-proposals"></a>
### Count-aware proposal objectives

A native research proposer may rank or improve candidates using C_hat+lambda*N_hat,
with lambda in Chaos per primitive execution and each component's evidence grade
explicit. This is a proposal objective. Canonical prices, original-cost lowers,
original action coverage and the returned cost winner retain their authorities.
Restriction and reward changes do not supply an original-scope lower
([CLM-0009](../claims.md#clm-0009)).

A few weighted proposals may expose different acquisition/continuation regions.
Every selected controller still needs complete native outcomes, observations,
paid setup/cleanup, properness and independent original-price evaluation.
Deprioritizing a proposal does not retire its original action. A shorter but more
expensive verified controller is an explicit diagnostic trade-off; minimum
expected Chaos remains the objective. A few weights need not find every
nondominated deterministic policy, and bounded local search does not inherit
the count monotonicity of exact scalarized minimizers over an unchanged domain.

Compatible price-independent rows and controller structure may be reused after
reweighting; stale numerical values or feasibility claims may not. Changed
policy decisions invalidate both C and N. Give useful count-discovered
continuations a bounded cost-only improvement pass before judging their value
to the original objective. No sampled or approximate tail becomes executable.

The [Bow entry investigation](../../active/2026-09-13-execution-aware-proposals/README.md)
asks compiler-bound, independently evaluated global decision entries. The saved
high-count Magic node is a diagnostic locator, not a hard-coded rule. An
operation inside a mandatory program may only be replaced at its legal parent
decision, preserving commitment and information timing.

<a id="pruning"></a>
## 2. A sufficient local dominance argument

### Proposal screening is not sound exclusion

For a complete candidate pi and incumbent cost U, a compatible candidate lower
L_pi >= U, or its reconciled native cost, can establish non-improvement under
the applicable tie contract. An upper estimate H_pi >= J_pi cannot: U=10,
J_pi=6 and H_pi=20 satisfy the upper inequality while the candidate improves U.
An unconnected source heuristic grants still less rejection authority.

A bounded search may use that score to withhold expensive checking. The result
is an unserviced proposal, not economic domination or closed action coverage.
Relaxing such a screen changes which proposals receive service; complete native
support, properness, entry compatibility, prices and independent evaluation
still own acceptance. Distinct selected semantics can justify reconsideration;
unrelated row growth or another timer tick alone cannot establish a new policy.
The [capability programme](../../active/2026-09-22-ordinary-capability/README.md)
treats a source-screen false negative as a hypothesis requiring a current native
witness, not as a conclusion from this scalar counterexample.

At the same semantic state \(s\), suppose a permitted executable proper continuation has cost \(U(s)\), and an alternative action has independently valid lower \(L(s,a)\). If

\[
L(s,a)>U(s),
\]

then

\[
Q^*(s,a)\ge L(s,a)>U(s)\ge V^*(s).
\]

So \(a\) cannot be an optimal first action. It can be retired without constructing its full exact row, provided every premise holds for all members represented by the retirement. [CLM-0020](../claims.md#clm-0020).

A root upper cannot generally substitute for \(U(s)\) at another state. A policy might be cheap at the root and expensive or undefined at \(s\). Likewise, an action lower from a different goal, price scope, filter phase, or action envelope is irrelevant to this comparison until compatibility is established.

Equality needs a separate retention/tie argument. Removing all tied actions can remove every proper witness or create a cyclic selected policy. A strict comparison is a simple sufficient rule; this document does not silently change any source tie or tolerance contract. The current sparse selector's strict ordering, stable exact ties, and separate stability tolerance are documented in the mechanism page. [Scheduling and Bellman Search](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/scheduling-bellman.md).

### Dominance does not transfer to arbitrary larger scopes

A policy feasible in a restricted scope remains an upper when the target scope is broadened, if it is still legal. A lower computed only for the restricted scope need not remain a lower. Broader scopes can introduce cheaper alternatives. This asymmetry is why identity checks and complete action coverage are necessary even for an apparently obvious prune. [CLM-0009](../claims.md#clm-0009).

<a id="incomplete"></a>
## 3. Incomplete computation must preserve unfinished obligations

A delayed action family, partially constructed exact row, or open strict frontier is not evidence of absence. Keep its unresolved status and independent conservative contribution where available. A partially generated distribution cannot be normalized and treated as the completed action.

This leaves room for useful bounded results. An independently valid whole-scope lower can be published while local action generation remains open. A fully evaluated proper strategy can supply an upper while alternative proof is unfinished. Neither implies exact closure.

The documented action ledger distinguishes discovery, admission, deferral, missing prices, resource interruption, and completed rows. The strict quotient retains unresolved alternative obligations rather than making a restricted graph value global. [Request and Action Scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/request-action-scope.md); [Strict Closure](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md).

Research can temporarily omit expensive computation only if the *proof semantics* still cover it. “Not evaluated this iteration” and “not part of the target” are different statements.

<a id="snapshots"></a>
## 4. Resuming a candidate means preserving a semantic object

A partial policy construction may bind selected rows, certified boundary actions, observation decisions, a missing continuation, and a traversal cursor. Its usefulness is avoiding repeated work when the missing row becomes available.

There are four separate questions:

1. Are the already fixed transitions and actions still legal and identical?
2. Are the values used to choose or evaluate them still the same evidence?
3. Does the completed policy remain proper and executable?
4. Is it still competitive enough to deserve more computation?

A global value update may change question 4 without changing question 1. A fixed policy need not remain greedy to remain a valid executable candidate. On the other hand, reusing an old value estimate after changing one of its selected rows can invalidate its evaluation.

A safe implementation can freeze a candidate's selection snapshot, allow only compatible append-only continuation growth, and perform full ordinary evaluation before publication. Alternatively, it can explicitly rebase and revalidate affected dependencies. It must not quietly mix a prefix justified by one boundary snapshot with a suffix whose numerical justification assumes a different one. [CLM-0021](../claims.md#clm-0021).

This is a sufficient snapshot discipline, not a claim that all mixed-time candidate construction is inherently invalid. Any completed policy may be independently evaluated from scratch. The risk is **reusing cached evidence** whose dependency contract no longer holds.

### A proof handoff is distinct from requested delivery

Stopping discovery at a completed restricted-iteration checkpoint can preserve
its selected rows and allow an existing exact-proof owner to check them. The
remaining action envelope stays open: numerical convergence of that restricted
iteration does not prove global closure. If ordinary discovery is suspended,
it must not silently restart underneath a borrowed policy or proof session.
If work is interleaved instead, every retained reference and generation must
remain valid under the snapshot contract above.

A later bounded-finish request is independent. It can release unfinished optional
proof while retaining the cheapest compatible independently evaluated artifact;
a previously verified cheaper strict candidate must remain eligible for delivery.
No unverified estimate justifies delaying that request. Moving the handoff earlier
therefore changes a work allocation, not the validity of the existing lower,
upper or exactness contracts. Its efficiency and candidate-readiness premises
require same-budget measurements, as emphasized in the
[proof-time review](../../active/2026-09-09-empty-start-partial-continuation/research-inputs/useful-proof-time-review.md).

<a id="interruptible-verified-results"></a>
### Interruption preserves an owned feasible witness

Fix target theta (root, goal/laws, prices, actions, programmes and observations),
separately from invocation and artifact identities. Let E_n contain compatible
complete proper policies verified before computational step n. Each pi in E_n
is feasible, so V*(root) <= J_pi(root). Selecting an owned minimizer at any finite
computational stop preserves this inequality. This is feasibility, not optional
stopping, convergence or a bound on subsequent crafting actions.

The witness is graph, full root certificate, context and evaluated value together.
A historical cost-7 scalar paired with a cost-10 graph does not certify 7.
Root-only evidence stays root-only. Selection includes complete eligible
strict/private evidence awaiting transfer before the publication owner's sealing
point; unfinished proposals cannot delay a compatible fallback. Seal the tuple
before transport and keep it immutable thereafter. Invocation-scoped intent,
acknowledgement, sealing, terminal commitment and usable delivery are distinct.
Cancellation precedence belongs to the control owner.

The first usable result and unattended final quality sample different sets of
owned evidence. With E(t) the compatible complete policies verified by time t,
U(t)=min_{pi in E(t)} J_pi(root), with infinity for an empty set. A probe
requesting Finish at the first such artifact measures U(t_first), not U(T) with
ordinary search permitted until T.
A saved feasible graph establishes existence, not discovery or internal checker
admission under the current runtime and resource profile. A current unattended
baseline is therefore required before attributing recovery to a new treatment.

An assertion may return at the existing prefix after complete graph construction
and evaluator admission but before evaluation service. Its private graph adds no
element to E_n: only the subsequent complete ordinary assertion can do that.
Forcing this already valid quantum-one boundary for larger caller ceilings thus
preserves the feasible-witness invariant, provided the frozen proof and owned
evaluator outlive suspension and cancellation destroys children before inputs.
The existing publication coroutine supplies that checkpoint and ownership. A
recovery compilation remains another unverified proposal requiring its original
checks. This changes grouping of native work, not its mathematical authority;
the [service measurements](../../active/2026-09-21-first-policy-service/README.md)
are separate from the induction argument.

Grouping is trace-neutral only under a stronger premise. Let a deterministic
logical transition act on the complete work state, including selection,
partial cursors, evidence and cumulative debit. If each public call merely
applies some positive number of those same transitions (up to its requested
ceiling), and no return boundary changes the next transition, concatenating
calls produces the same state after the same actual logical prefix by induction.
An owner decision made at a return, an observation side effect, a numerical
restart or a different work order defeats that premise while each returned
state may still be safe. A requested quantum is only a ceiling; equal completed
rows or candidate hashes do not prove equal logical work. Pending preparation
remains a positive service obligation until complete committed support,
refusal, cap, cancellation or compatible supersession. The matched-work
[investigation](../../active/2026-09-22-ordinary-capability/README.md#matched-work-recovery--current-selected-work)
tests efficiency separately from this conditional safety argument.

This is safety. Bounded response also requires bounds on dispatch, noninterruptible
work, transfer, release and packaging. Existing numerical reconciliation does not
become a rigorously rounded endpoint through Finish.

The retained-pool implementation now enforces a scoped part of this premise:
only its owner admits, prunes or takes retained bundles. Verification moves an
entry into coroutine-owned storage while a reserved identity supplies a passive
view; vector mutation cannot invalidate the suspended witness. Returning that
entry restores it only while the slot still exists. Dynamic payload and both
coroutine frames stay charged, including after detachment, and releasing them
does not refund work. Existing native identity/certificate checks and publication
sealing still supply the compatibility and completeness premises. This is
implementation correspondence for retained ownership, not a discharge of all
CLM-0002/CLM-0021 obligations or a bounded-latency theorem. Output/proposal and
setup work retain their existing separate owners.

<a id="cooperative-preparation"></a>
### Staged setup and observation

Write W=(K,S,cursor,debit), with committed evidence K and staged scratch S. If each
finer step preserves K or commits one complete result allowed by its original
owner, induction preserves complete-evidence invariants at every prefix. A
0.9-success partial row cannot normalize away its unfinished 0.1 trap branch.
Interruption releases scratch through its owner without deleting committed
premises or refunding cumulative logical work. Atomic sparse append and
moved-lower-snapshot rollback remain prerequisites.

Before ordinary search, heavy preparation may be suspended but no unfinished
lower is published. Transfer pre-ledger proof reservation to normal accounting
exactly once, without a gap or double charge. Deferred validation, cancellation,
mathematical refusal and resource stop are distinct; a broad catch cannot turn
cancellation into continued solving.

A frozen memory charge is reusable only while transitive storage, capacities,
lazy caches and alias context remain unchanged. Add growing storage, overlapping
scratch and proposed allocations. Release changes live bytes, not consumed work.
Logical read passivity is separate from deadline overhead. These invariants imply
neither fairness, identical scheduling/arithmetic order nor a latency theorem.
Source/host clocks, omissions and event coverage remain explicit.

The selected implementation keeps goal-cover scratch in its invocation's task
and admits tracked container allocations before growth. Carrier/debt, universal
and clean tables publish only at their own completed boundaries; a later refusal
does not revoke an earlier independent component. Pending completion reads use
the existing zero fallback without resuming construction. Retention owns support,
native relation rounds, quotient solve/check children and final member safety.
Its immutable view is installed only after all those obligations complete.

The induction requires more than a cursor. Native relation coefficients and
event minima belong to one frozen candidate generation; repair invalidates the
value-dependent minima and requires reconstruction and final checking. A yielded
quotient result is inaccessible until completion, and cancellation/stale-model
results cannot carry an accepted certificate. Blocking diagnostics drain the
same producer. This is correspondence for staged ownership, not a new proof of
native domination, numerical convergence or broad exact closure.

Frame admission precedes allocation, and destruction precedes release of its
charge. Cover scratch, committed copies and their overlap remain distinct;
retention child frames fit within the existing proof reservation. Synchronous
abandon includes observational snapshot/serialization, calculator rollback and
actual destruction. Its release telemetry is produced after release; it cannot
serve as an early acknowledgement. Measured responsiveness remains a separate
[qualification obligation](../../active/2026-09-20-cooperative-setup/README.md).

### Releasing logical ownership also matters

The reclamation archive records a candidate marked released while its large traversal payload and wrapper remained engaged, and its call site still suppressed ordinary assembly. That was a lifecycle defect with performance/progress consequences, not a refutation of resumable construction. [Recorded finding](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md).

Retained bytes, continuation preference, and active obligations must follow the real lifecycle. A compact tombstone can record why a candidate was refused without retaining its working graph or reserving its service indefinitely.

<a id="reuse"></a>
## 5. Reuse classes and invalidation

| Reused object | Typical sufficient compatibility | What reuse does not establish |
|---|---|---|
| Exact transition row | Native semantic state/action, artifact, complete support, control memory | Its old priced value remains valid after repricing |
| Numerical potential | Same declared relation/coefficients and checked inequalities | Native coverage if only coefficients were checked |
| Event geometry/capacity | Same native domain, effect, pool/phase, and conditioning assumptions | The old minimum-expectation allocation remains optimal |
| Fixed-policy properness | Same reachable policy transition graph and entry | A changed or newly reachable branch is proper |
| Uniform class lookup | Same full member domain and certificate scope | A representative's successful lookup covers the class |
| Failed lookup | Same reason and unchanged eligible domain | Later stronger evidence can never make it eligible |

These are semantic contracts, not a mandatory universal cache schema. The local owner should retain only the identity required by its object.

The current development checkpoint is specifically a completed coarse closure. Its documentation refuses an incomplete focused graph and does not claim a complete strict-partition checkpoint. Replay rebuilds the relevant namespaces and still runs optimization, strict refinement, compilation, and evaluation. [Resources, Resume, and Replay](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md).

Saving case JSON or a progress report is not equivalent to saving scheduler-aware state. A replay that reconstructs a different work queue can still solve the same target, but it is not evidence of identical bounded execution.

<a id="progress"></a>
## 6. What an eventual-closure argument would require

A sufficient finite-work progress argument has the following shape: the target relevant graph and action family set are finite; every necessary row can be computed in finite work; partitions split only finitely often; pending proof obligations are not starved; numerical tasks terminate under their stated assumptions; and completed witnesses are not repeatedly invalidated without new semantic input.

Under those premises, a schedule that eventually services every persistent obligation can finish the finite exploration and checking obligations. This still does not supply a useful runtime bound. If one action has enormous support or the allowed program family is not known finite, the premises must be established rather than assumed.

Fairness is not proved by one counter increasing. An action-count limit or a time window may make a run bounded but also leave unresolved obligations. A learned or heuristic priority can change order without being proof authority, but permanent suppression needs a proof or an explicit scope restriction.

No general eventual-closure proof for all currently supported implicit requests is established in this draft. [GAP-04](../research.md#gap-04) names that missing argument. [CLM-0022](../claims.md#clm-0022) records the distinction so future bounded failures are not mislabeled as mathematical refutations.

<a id="experiment"></a>
## 7. Read performance evidence against its declared question

Suppose the baseline spends an entire 60-second budget generating rows. A treatment spends 12 seconds preparing a stronger lower and 48 seconds generating rows. A lower row count by itself does not prove pruning saved work. The two methods did not reach the same proof milestone.

Useful comparison questions include time to the same verified lower/gap/upper target, work to the same exact closure, or final certified quality under a matched total budget. Preparation and checking costs belong in that budget. Distinguish lookup calls from distinct states and repeated use from expanded domain coverage.

The benchmark already records step-boundary trajectories, identities, failures, and paired comparison controls. The research workflow should interpret those measurements, not reconstruct another runner. [Benchmarking](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md); [research outcome profiles](../research.md#outcomes).

A small nonsemantic fix needs no new research record. A scheduling experiment that changes the mechanism behind a result should preserve the question, evidence, and exactly which progress or performance claim changed.

The [cross-base selected-fringe experiment](../../active/2026-09-09-cross-base-capability-recovery/README.md#causal-ranking-and-selected-experiment)
illustrates this distinction before the first policy: servicing successors of a
route chosen from optimistic continuation values does not establish that the route
has a proper executable completion. Both bounded ordering variants expanded more
states without producing a controller and were removed. That rejects their useful
bounded-progress premise on the measured families; it does not refute eventual
closure under the separate finiteness and fairness assumptions of CLM-0022.

The [goal-reaching row-delivery application](../../active/2026-09-10-goal-reaching-row-delivery/README.md)
distinguishes a consumed automatic-epoch service yield from exhausted work. A
single false scheduler return is not an exhaustion certificate when the caller
has just completed a retained preparation boundary and delayed obligations are
still queued. Re-entering the existing scheduler preserves its own closure and
resource guards. This additional re-entry is limited to the no-incumbent case:
an existing incumbent retains its ordinary publication opportunity, including
strict cost improvement. Candidate-specific continuation service subsequently produced
proper policies; goal count, extra rows and a finite estimate alone did not.
The first verified candidate is retained before improvement so a newly cheaper
but incomplete route cannot erase executable evidence. This is a scoped native
progress result, not a general proof of the finiteness/fairness premises above.

<a id="work-weighted-role-opportunity"></a>
### Complete support and work-weighted recurrence

A complete native action row can represent every positive-probability outcome
while the scheduler services only some successor states next. Represented
support, interned state identity, queued obligations, started expansion,
completed state/action rows and verified policy reachability are different
populations. Counting repeated role signatures among outcomes does not measure
repeated *served* computation, and an expansion-start flag does not certify that
every applicable action row finished.

For actually served bindings, separate reusable structural work \(S_b\),
binding-specific numeric work \(N_b\), and output/checking work \(O_b\). A
shared structure costs its first build, matching and guard work before any
saving. The optimistic saving on a finite observed workload is at most the
later \(S_b\) that a source-backed guard can actually bypass; it cannot include
unserved states, existing exact-cache hits or independent weighted value reads.
Parent and child timers may be inclusive. State counts, expected crafting visits,
timed solver invocations and completed native work items have different units.
The [downstream role investigation](../../active/2026-09-23-downstream-role/README.md)
records a bounded source census separately from its mathematical contract.

<a id="strict-preparation-prefix"></a>
### Synchronous preparation and bounded prefixes

For a deterministic backward requirement transfer, equal complete initial,
selected-program and successor inputs give equal outputs at every synchronous
round by induction. A pure transfer can also be reused for equal complete
transfer/requirement inputs. Matching a role mask or one eventual fixed point
does not establish those premises. In-place updates or a changed worklist may
reach the same final fixed point after a different number of capped rounds;
the current round order and confirmation step remain part of the bounded
runtime contract.

A prepared result stays staged until its old owner commits a complete
assignment or row. Suspension and cancellation preserve earlier verified
evidence; neither turns pending work into a failed obligation. For timing,
sum direct calls or active child-resume spans at one declared owner boundary.
A persistent session's elapsed prefix includes intervening work and gaps, so
successive prefix snapshots cannot be added as independent phases. The
[S0 attribution](../../active/2026-09-24-strict-preparation/README.md)
records the measured scope without assigning its remainder to reusable work.

<a id="cost-only-service-readiness"></a>
### Verified-base readiness and a bounded proposed service

A post-incumbent candidate starts from an actually retained compatible graph,
certificate, physical entry and current resource allowance. A mode setting or
earlier proposal attempt is not a substitute for those premises. During a
bounded private wave, only a complete independently checked graph/root/value
bundle can replace the current verified winner; refusal, cancellation and
Finish leave that owned witness available through normal publication. The
finite wave bound limits speculative work but proves neither useful final
quality nor search-order fairness. Those remain measured properties of an
implemented schedule.

The [L0 native queries](../../active/2026-09-24-cost-only-continuation/README.md)
used two actual pre-Finish verified controllers. Both had zero eligible clean
entries under the selected guard, so no service wave was installed. A zero
counter from an uncalled query would not establish that result; the first A4
contrast made that distinction explicit before the corrected query ran.

The [N0–N2 diagnostic](../../active/2026-09-24-native-boundary-repair/README.md)
froze one current-run verified A5 donor and its compiler entry before constructing
one native option family. A temporary post-verification hook completed a single
bounded wave under the original work/memory limits, checked the complete root
graph and kept the cheaper donor when the second candidate lost. Refused setup
and uncovered tails were pending or incompatible proposals, not failed economic
certificates. A production service would additionally need invocation-scoped
activation, simultaneous resource admission, cancellation/Finish ownership and
comparison to the current verified winner. The diagnostic source was removed
after its gain-versus-latency gate; no live service schedule is retained.
