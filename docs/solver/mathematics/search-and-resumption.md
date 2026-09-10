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

The documented carrier ordering uses progress, side capacity, blockers, and other observations as priorities, while proof values have separate owners. This is the correct conceptual separation. It does not prove the current scheduling implementation fair. [Scheduling and Bellman Search](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/scheduling-bellman.md).

<a id="pruning"></a>
## 2. A sufficient local dominance argument

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
