# Numerical acceptance and exact closure

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


“Exact” can refer to a complete graph, exact arithmetic, exact evaluation of a fixed policy, or proof of optimality. These meanings must not be collapsed. This chapter completes the argument from native semantics to the result returned to the user.

<a id="layers"></a>
## 1. Four layers of numerical meaning

| Layer | What it specifies | What it does not automatically establish |
|---|---|---|
| Native target | Legal actions, actual transition law, cost/resource semantics, goal | That a particular export or projection represents it correctly |
| Declared finite/abstract model | Stored states, rows, coefficients, action coverage, boundary relations | Native validity unless a sound relation is supplied |
| Candidate calculation | A floating-point, rational, iterative, or policy-based proposed vector | The required inequality direction or chosen fixed-point semantics |
| Acceptance | Checks the candidate against the appropriate model and provenance | Anything outside the scope of those checks |

For example, computing an exact dot product of stored binary floats proves an exact statement about those represented numbers. It does not convert a rounded probability into an exact native rational. A normalized rational reference is a useful different model when explicitly labelled; normalization is not automatically an admissibility-preserving correction.

The current lower-only quotient separates coefficient declarations, native evidence, and exact binary inequality checking. Preserve that separation. [Lower and Pruning Authority](../lower-pruning.md); [CLM-0023](../claims.md#clm-0023).

Run-local dirty cost correction stays in the candidate-calculation layer. Its
bounded residual compares the completed private model and independent native
evaluation of the same frozen controller at the same entry; neither number is
a new optimal-value label. The [adaptive dirty application](../../active/2026-09-12-adaptive-dirty-guidance/README.md)
retains the existing evaluator tolerances, complete-cost reconciliation,
properness and entry checks. It improves feasible uppers without changing
lower closure or numerical acceptance. Capped candidates do not train a cost
correction or become executable boundary values.

The fixed-point certificate literature independently emphasizes producing checkable witnesses rather than trusting a numerical algorithm merely because it is sophisticated. The paper by Chatterjee and colleagues formalizes certificates for finite MDP reachability and reward properties; its checker does not automatically verify poecraft2's implicit action or member-domain bridge. [Chatterjee et al., TACAS 2025](https://arxiv.org/abs/2501.11467).

<a id="direction"></a>
## 2. The direction of the check is part of the proof

For a lower candidate, the requirement is

\[
h(s)\le B(s,a;h)
\]

for every relevant action relation. A small **positive violation** of this inequality is still a violation of the exact mathematical premise. Accepting it requires an explicit error-to-bound argument, not just a tolerance constant.

A conservative check might construct a lower bound \(B^-\) on the true Bellman right-hand side and require \(h(s)\le B^-\). Native integer weights, outward-rounded intervals, exact binary calculations, or other justified constructions can supply different parts of this contract. Each needs clear coefficient provenance.

For an executable upper, solving the fixed policy approximately also needs a declared error contract. If the target is an actual upper interval, use its safe endpoint. If the public system instead reports a numerical estimate after complete policy checking and named reconciliation tolerances, document that precise contract; do not silently promote the displayed decimal to a rigorously rounded rational upper.

This draft has not audited every numerical path to settle that correspondence. [GAP-05](../research.md#gap-05) is an explicit integration task, not a new assertion that published results are wrong.

<a id="residual"></a>
## 3. A small residual can hide a large error

For a proper finite fixed-policy chain,

\[
J=r+QJ.
\]

For a candidate \(v\), write its residual as \(e=r+Qv-v\). Then

\[
J-v=(I-Q)^{-1}e.
\]

A residual bound becomes a value-error bound only after accounting for the inverse or an equivalent sensitivity/transience bound.

For a one-state retry with success probability \(p\), \(Q=1-p\), so a residual of magnitude \(\varepsilon\) can correspond to error \(\varepsilon/p\). Let \(p=10^{-6}\), cost \(r=1\), true value \(J=10^6\), and candidate \(v=999999\). The residual is only \(10^{-6}\), yet the value error is 1.

That is not an argument against floating point. It is an argument against equating a small local residual with a small global error without the needed factor. The same issue becomes worse near nonabsorbing behavior. A zero-cost improper loop has no invertible \(I-Q\) at all.

The example also explains why a selected-policy numerical residual cannot, by itself, certify every alternative Bellman inequality.

<a id="flow-accounting"></a>
### Complete occupancy replaces preliminary accounting

For the same finite fixed-policy chain, let \(\alpha\) be its initial mass and
\(d\) its expected nonterminal visit vector. Complete flow satisfies

\[
d=\alpha+Q^T d.
\]

When the reachable chain is transient, this has one finite nonnegative solution.
Each edge carries \(d_iQ_{ij}\), and terminal and reward totals follow from the
same occupancies. Unresolved mass from a preliminary computation is a remainder
of that same initial mass. Adding it to a completed flow counts that remainder
twice; a completed, qualified reconstruction replaces the preliminary accounting.

A reachable closed component is different. If positive mass \(m\) enters a
closed component \(C\), summing its flow equations would give
\(\sum_{i\in C}d_i=m+\sum_{i\in C}d_i\), impossible for finite occupancies.
A finite entry snapshot for that component cannot discharge unresolved mass.

The native application requires completed raw/shared occupancy, disaggregation
and quotient-flow checks, and no positive-input closed component. Its existing
numerical acceptance contract remains in force; this argument supplies neither
a new error bound nor native optimality. See [CLM-0002](../claims.md#clm-0002)
and the [evaluation mechanism](../publication.md#evaluation-contract).

### Rank-one preparation of a large occupancy solve

For the same finite transient component, put \(A=I-Q^T\), let incoming
mass \(b\ge0\) have \(m=\mathbf1^Tb>0\), and write \(u=b/m\). If
\(Ax=b\), then
\[
(A+u\mathbf1^T)y=b,\qquad
y=\frac{x}{1+\mathbf1^Tx/m},\qquad
d=\frac{\mathbf1^TAy}{m}>0,\qquad x=y/d.
\]
Substitution proves the identities. The changed matrix is nonsingular because
the determinant lemma gives the positive factor
\(1+\mathbf1^TA^{-1}u\). Thus the preparation separates a possibly very
large visit total from the bounded vector \(y\); it is not a new probability
model, policy, value bound or proof of faster convergence on every component.

The native sparse occupancy fallback computes the column deficits
\(\mathbf1^TA\) from every stored internal coefficient, in `WideFloat`.
This avoids recovering \(d\) by subtracting nearly equal unit totals. Stored
mass defects remain part of those equations; native exits do not replace the
deficits to renormalize the matrix. The rank-one term is applied as a vector
operation, without a dense matrix or inverse. Its retained deficit vector is
charged to the existing evaluator allowance.

The existing four-iteration work unit and total iteration cap still apply.
The modified system is numerical preparation only: reconstructed visits must
pass the original \(Ax=b\) residual tolerance, unchanged at
`1e-18 * max(1, norm(b))`, followed by the ordinary native flow, cost and
properness checks. Failure can seed the original sparse solve, whose existing
Gauss-Seidel fallback never runs on the modified matrix. A closed component
does not gain a finite solution. No new accepted claim or error enclosure is
inferred from this algebra; the [current application](../../active/2026-09-11-dirty-state-continuation/README.md)
owns empirical qualification.

<a id="probability"></a>
## 4. Probability coefficients and minimizing relations

For a native weighted draw with integer total \(W\), an exact one-step probability can be represented as \(w/W\). If a producer instead emits an upper event capacity, it must prove that the true conditional event probability is no larger than that capacity. Rounding the upper capacity upward is conservative for the lower model's minimization problem; rounding it downward may remove the native distribution.

When a model contains observed-choice groups, the check must include the intended self and nonself alternatives. For

\[
x=1+\tfrac12\min(x,100),
\]

the relevant solution is 2. Dropping the self choice replaces the right-hand side with 51. An arithmetic backend that correctly solves the wrong expression has not verified the intended model.

The event-cap producer also has two separate minimizations: the minimum compatible successor value within an event, and the minimum weighted expectation over allowed event probabilities. Both depend on the candidate vector. A merely feasible probability allocation can overstate the latter minimum; a stale one can change inequality direction after values move. [Expectation proof](lower-bounds.md#events).

Final checking must therefore use the correct final-vector relation, not only the rows that made an earlier numerical proposal attractive.

<a id="exactness"></a>
## 5. The conditional exact-closure argument

Suppose the following have been established for the same target, source, and scope:

1. An allowed proper executable policy \(\pi\) has cost at most \(U\).
2. A native-valid lower has value at least \(L\).
3. The lower's proof includes all allowed alternatives, either through exact rows, exact inapplicability, or valid lower-based retirement.
4. State, price, artifact, program/observation, and numerical identities are compatible.

Then

\[
L\le V^*(s_0)\le J_\pi(s_0)\le U.
\]

If the mathematical endpoints agree, the optimum is fixed. If their difference is at most \(\delta\), the result establishes at most that absolute gap under the endpoint guarantees. Relative guarantees need a defined denominator and, commonly, a positive lower.

The implementation can name a policy `exact` after its complete strict proof and named numerical reconciliation pass. That is an implementation contract. A mathematical document must state whether that contract proves exact symbolic equality, an enclosure, or combinatorial closure with a numerical tolerance. It cannot infer one from the other by spelling the status in capital letters. [CLM-0024](../claims.md#clm-0024).

This does **not** require materializing the entire native state space. An independently valid action lower can discharge an unmaterialized alternative. It does require that every omitted part has an actual proof owner or remains visibly unresolved.

The reviewed strict-closure contract requires a proper selected policy, complete action accounting, competitive alternatives certified or carrier-wide dominated at the current generation, closed frontiers/envelopes, and a compiled evaluation that reconciles with the strict value. [Strict Closure](../strict-closure.md).

### Why policy-reachable inequalities alone are insufficient

Checking the selected policy's own equations shows its cost. It does not rule out an alternative that leaves its reachable domain and becomes cheap elsewhere. To prove optimality around the policy, that alternative must also be covered by exact successor values or an independently admissible boundary lower sufficient for its inequality.

Conversely, a full valid root lower can meet a root policy upper without certifying that the policy is locally optimal at every physically possible off-policy state. The proof is rooted in the requested problem, not an unnecessary requirement to optimize every item the engine could represent.

<a id="classification"></a>
## 6. Termination and policy quality must stay separate

A requested bounded finish, memory stop, watchdog, absent incumbent, numerical refusal, and exact closure are different events. A finite internal estimate does not establish an incumbent. A coarse graph completing does not automatically establish strict optimality. A policy returned after a cap can still be a valid bounded upper.

Publication's documented duty is to classify compatible evidence, compile the chosen policy, and evaluate the artifact actually returned. [Publication and Evaluation](../publication.md).

The reader should be able to recover the following from a result:

| Question | Evidence needed |
|---|---|
| Is there an executable policy? | Verified candidate/artifact and entry-specific evaluation |
| Is the reported lower native-valid? | Named proof family, scope, and accepted relation |
| Why did work stop? | Actual termination owner and resource/request context |
| Is optimality established? | Complete target-scope proof plus compatible upper and numerical contract |
| Can it be compared to another run? | Matching target and appropriate experiment controls |

A display field is a projection of these facts, not a substitute for them.

<a id="reference"></a>
## 7. Independent references check particular layers

A small independent rational solver can check a finite model's result. Storm or another model checker can provide a second algorithm and explicit property semantics. Neither independently verifies a native export if both consume the same erroneous exported model.

Reference records should identify:

* the target or auxiliary model and its semantic version;
* the property, including nontermination conventions;
* raw/native/normalized coefficient interpretation;
* the tool or derivation and actual invocation;
* whether the value is an optimum, a policy upper, a lower, or an auxiliary ceiling;
* any challenge or discrepancy.

The [completed backbone reference slice](../../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) records 33 exact synthetic examples and eight saved native-micro coefficient/reference checks. Its normalized reference and raw mass defect remain separately labelled. These checks are implemented evidence, not a new native solve, universal theorem verification, or proof that a shared export represents the native process.

A result that disagrees with a reference should preserve both pieces of evidence until the difference is attributed. Editing an expected number to make a test pass is not reconciliation.

<a id="workflow"></a>
## 8. What may change freely, and what must be reviewed

Numerical proposal generation can change without changing the proof theorem, if the final checker and semantic relation still establish the same premises. Scoped caches can avoid repeated native support work, but their validity depends on their actual inputs. Faster code that changes the target relation or accepts stale minima changes more than runtime.

A prose claim's `accepted` status never authorizes a runtime bound. The existing native constructors and validators still have to run where required. Conversely, a comment-only change to a claim reference should not trigger a solver simulation solely because the word “proof” appears in the file.

Use [research.md](../research.md) to preserve a new mathematical argument or discrepancy once. Routine implementation work does not need a new ledger entry. Numerical correspondence remains [GAP-05](../research.md#gap-05); the relevant native representation and program obligations remain [GAP-02](../research.md#gap-02) and [GAP-03](../research.md#gap-03). [GAP-06](../research.md#gap-06) records completed integration, not a pending whole-solver proof.
