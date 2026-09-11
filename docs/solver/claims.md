# Claim ledger

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


This ledger gives durable identities to propositions, not to attempted techniques. The mathematical chapters own the full arguments. A failed implementation does not automatically refute a theorem; a successful finite test does not prove a universal native relation.

**Integration convention.** `CLM-0001` through `CLM-0025` are registered without collision in the local ledger. The unchanged authored packet is preserved in the [completed programme archive](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md); original history events remain intact. Initial draft events are `open`, and later events record the actual responsible review. Read the final event of each claim for its status. Conditional mathematical self-review does not establish every native application, and this ledger is not a machine-checked proof catalogue.

An actual acceptance event must name the responsible author/reviewer and its basis: a mathematical derivation, native correspondence under a pinned domain, or scoped empirical evidence. Conditional mathematics and the validity of one native application are separate questions.

Statements and preconditions must not be silently narrowed after a counterexample. Material changes receive a new claim and explicit supersession; editorial link/typo repairs may be recorded without inventing a new proposition. The last history event is the status; there is no parallel handwritten current-status table. Valid statuses are `open`, `accepted`, `refuted`, `superseded`, and `withdrawn`.

Logical dependencies below are justification dependencies. Evidence, motivation, and supersession links are not silently treated as premises. A stochastic SCC is not a cycle of unsupported claims. A conditional theorem can remain valid when a proposed native use fails its preconditions.

For a small fix, this ledger need not change. For a mathematical change, follow the relevant claim and argument. See [research workflow](research.md#handoff) for importing external findings without rewriting the same report twice.

<a id="clm-0001"></a>
## CLM-0001 — Proper-policy cost and raw total cost are different targets

**Kind:** mathematics.

**Statement:** A zero-cost non-goal cycle can make unrestricted accumulated-cost minimization cheaper than minimization over proper finite-cost policies. A Bellman fixed point alone need not select the latter value.

**Preconditions:** The target policy class and nontermination convention are explicit. Finite-chain transience claims apply only to a finite fixed-policy graph.

**Canonical argument:** [Read the derivation](mathematical-model.md#properness).

**Logical dependencies:** None beyond the definitions stated in the argument.

**Attempted falsification:** A zero-cost self-loop plus a five-cost finish has raw optimum zero, proper optimum five, and multiple Bellman fixed points. This is an exact example, not a native mechanic.

**Evidence and implementation correspondence:** [Bertsekas](https://arxiv.org/abs/1711.10129); [PRISM property definitions](https://www.prismmodelchecker.org/manual/PropertySpecification/Reward-basedProperties). Native reconciliation is GAP-01.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the explicit zero-cost counterexample and distinction of policy classes. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.

<a id="clm-0002"></a>
## CLM-0002 — A proper evaluated fixed policy supplies an entry-scoped upper

**Kind:** mathematics.

**Statement:** For a finite transient nonterminal matrix Q with finite expected immediate costs r, J=(I-Q)^(-1)r is the fixed policy cost and upper-bounds the optimum from every certified entry.

**Preconditions:** Complete native policy graph, allowed actions/observations, almost-sure goal absorption from the entry, finite costs, matching prices and numerical interpretation. Properness from one entry is not universal graph properness.

**Canonical argument:** [Read the derivation](mathematics/policies.md#fixed-policy).

**Logical dependencies:** [CLM-0001](#clm-0001)

**Attempted falsification:** A root that immediately succeeds can coexist with a root-unreachable improper self-loop. The root upper remains valid; certifying the loop entry fails.

**Evidence and implementation correspondence:** [Publication/evaluation contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md). Candidate construction and numerical endpoints still need their own evidence.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the finite transient Neumann-series and feasible-policy subset argument. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.
- 2026-09-07 — `accepted` — Codex, scoped native accounting application: completed transient raw/shared occupancy replaces preliminary quotient flow; positive-input closed components still refuse. The [flow argument](mathematics/numerical-closure.md#flow-accounting), old-code counterexample and [verified development policy](../archive/2026-09-07-continuous-bounds/README.md) retain the existing numerical contract. No native optimality claim.
- 2026-09-10 — `accepted` — Codex, responsible integration self-review: the [complete-row safe-progress argument](mathematics/policies.md#proper-seed) constructs a proper candidate under explicit finite-view and observation preconditions; statement and upper-publication requirements are unchanged. Actual native helpers reproduce the strict-rank/finite-Q limitation, while their complete numerical caller solves the mutual-retry example. The [bounded Ring/Amulet views](../active/2026-09-10-proper-policy-recovery/README.md) lack goal closure and falsify direct selector applicability there. No new native upper, exactness status or general native completeness claim.
- 2026-09-10 — `accepted` — Codex, scoped native delivery application: the [entry-domain argument](mathematics/policies.md#properness) excludes unrelated lower estimates from upper-policy frontier authority. The [goal-reaching continuation](../active/2026-09-10-goal-reaching-row-delivery/README.md) compiles and independently evaluates Ring/Amulet policies at the original memory limit, retaining the first verified candidate before improvement. Full goal masks are not terminals; complete native failures and observations remain required. Statement, preconditions and exactness status are unchanged; control and WASM qualification are recorded separately.

<a id="clm-0003"></a>
## CLM-0003 — Observed-choice timing changes the value

**Kind:** mathematics.

**Statement:** Choosing after a random observation can be cheaper than committing before it; evaluating a fixed policy must retain its actual observation-dependent decision rule.

**Preconditions:** The available information and allowed actions at each choice are part of the semantic state. Any extra information granted by a lower is explicitly optimistic, not executable authority.

**Canonical argument:** [Read the derivation](mathematics/policies.md#choices).

**Logical dependencies:** None beyond the definitions stated in the argument.

**Attempted falsification:** With equiprobable outcomes and costs A=(0,10), B=(10,0), post-observation choice costs zero and pre-observation choice costs five.

**Evidence and implementation correspondence:** [Evaluator product and choice state](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md); program correspondence remains GAP-03.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the observation-conditioned minimum and two-outcome counterexample. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.

<a id="clm-0004"></a>
## CLM-0004 — Program summaries need both local exit completeness and global properness

**Kind:** mathematics.

**Statement:** A mandatory program with almost-sure finite exit and finite expected internal cost composes by internal cost plus exit-weighted continuation values. Proper local options alone do not prove proper global composition.

**Preconditions:** No skipped caller decision, correct checkpoint/choice memory, full exit probability mass, all mandatory costs, compatible continuations, and proper composed controller.

**Canonical argument:** [Read the derivation](mathematics/policies.md#programs).

**Logical dependencies:** [CLM-0001](#clm-0001), [CLM-0002](#clm-0002), [CLM-0003](#clm-0003)

**Attempted falsification:** Two one-step options s→t and t→s each exit but their composition never reaches the goal. Renormalizing a 0.9-success/0.1-trap program also changes the problem.

**Evidence and implementation correspondence:** [Compiler/evaluator mechanism](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md); current program grammar coverage is an implementation obligation.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

**Scoped implementation observation (2026-09-09):** Bounded missing-continuation
discovery preserves this obligation: an incomplete selected/publication walk
cannot publish, and ordinary completion still goes through the existing
properness/compiler/evaluator checks. The [C3 empty-five result](../active/2026-09-09-empty-start-partial-continuation/README.md)
improves the verified upper while keeping the lower fixed. This application does
not promote the general claim's open status or assert native optimality.

<a id="clm-0005"></a>
## CLM-0005 — A behavioral quotient needs action-wise class-transition equality

**Kind:** mathematics.

**Statement:** In the stated finite SSP, a partition preserving terminal truth, corresponding actions and observation semantics, expected costs, and probability into every class supports an exact behavioral reduction.

**Preconditions:** The policy-class mapping is preserved as well as the one-step equations. This is an equality relation, not merely similarity of displayed goals or one representative row.

**Canonical argument:** [Read the derivation](mathematics/representations.md#equivalence).

**Logical dependencies:** [CLM-0001](#clm-0001), [CLM-0003](#clm-0003)

**Attempted falsification:** Two members with the same mask/counts but different goal-reaching probabilities under an action cannot share an exact quotient row.

**Evidence and implementation correspondence:** [States](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md); [strict carrier-wide rows](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md). GAP-02 separates the theorem from native coverage.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0006"></a>
## CLM-0006 — Uniform lower and upper aggregation require complete member coverage

**Kind:** mathematics.

**Statement:** The infimum of valid member-specific lowers is a lower for every represented member. A supremum of proper member-policy costs gives scalar upper bounds; one executable class continuation additionally needs an implementable selector or common policy.

**Preconditions:** Every member of the claimed class is covered, all values have the same target/scope, and policy selection respects actual observations. A sample or representative is not a complete member domain.

**Canonical argument:** [Read the derivation](mathematics/representations.md#members).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0003](#clm-0003)

**Attempted falsification:** For member optima 2 and 100, broadcasting lower 100 fails. Different policies attaining the two values do not automatically create an executable common router.

**Evidence and implementation correspondence:** [Native retention member checks](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); [representation scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/states-carriers.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-09 — `accepted` — Codex, scoped member-domain application: the full state projection includes known metamod flags. An absent flag excludes its necessarily flagged members from a mixed modifier mask; unknown roles and veiled templates remain refused. Shared native projection and positive/negative mixed-class fixtures support this limited [preimage argument](mathematics/lower-bounds.md#certificate-domain). No representative-based broadcast or retry-carrier extension is accepted.

<a id="clm-0007"></a>
## CLM-0007 — A bounded Bellman subsolution lower-bounds every proper solution

**Kind:** mathematics.

**Statement:** A bounded nonnegative potential, zero at true goals, satisfying every allowed action inequality is below the cost of every allowed proper finite-cost policy.

**Preconditions:** Nonnegative costs; complete action/state relation; correct conditioning; the bounded tail vanishes under almost-sure termination. Unbounded potentials require a separate transversality argument.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#subsolution).

**Logical dependencies:** [CLM-0001](#clm-0001), [CLM-0003](#clm-0003)

**Attempted falsification:** A zero-cost loop shows that the lower can remain weak despite zero residual. The proof does not assert greatest-fixed-point convergence or infeasibility.

**Evidence and implementation correspondence:** [Lower/checker authority](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md). The chapter supplies the finite-horizon telescoping proof rather than inferring it from tests.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the bounded stopped-process telescoping, vanishing proper-policy tail and monotone convergence. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.
- 2026-09-07 — `accepted` — Codex, scoped early-endpoint application: a useful intermediate native vector passes complete coverage and final-vector minimization before ordinary maximum consumption. The [explicit subsolution audit](../archive/2026-09-07-continuous-bounds/README.md) does not require auxiliary tightness; full-refinement audit remains unchanged. No convergence or optimality claim.

<a id="clm-0008"></a>
## CLM-0008 — Omitted action computation must retain complete proof coverage

**Kind:** mathematics.

**Statement:** An unmaterialized allowed action must remain represented by a sound optimistic relation, an independently valid scalar/family lower, or a complete inapplicability proof.

**Preconditions:** Coverage is the complete canonical set or proved family partition for the actual source. A family floor quantifies over all unresolved members; equal row counts do not establish membership.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#coverage).

**Logical dependencies:** [CLM-0007](#clm-0007)

**Attempted falsification:** Dropping a one-cost finish while retaining a ten-cost finish creates a false lower. Duplicating the expensive action cannot repair the missing cheap action.

**Evidence and implementation correspondence:** [Request scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/request-action-scope.md); [canonical lower-only coverage](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the all-action quantifier and independent-boundary stopping argument. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.

<a id="clm-0009"></a>
## CLM-0009 — Action restriction and optimistic relaxation have opposite directions

**Kind:** mathematics.

**Statement:** Restricting the admissible proper policy set cannot decrease its infimum cost. Expanding it cannot increase that infimum when the same target costs/goal meaning are retained. More general abstract changes need their own domination relation.

**Preconditions:** The compared policy sets and cost semantics are genuinely related by inclusion. An algorithmic cap is not a policy restriction unless explicitly made part of the target.

**Canonical argument:** [Read the derivation](mathematical-model.md#scope).

**Logical dependencies:** [CLM-0001](#clm-0001)

**Attempted falsification:** The ten-cost-only restricted optimum is not a lower when the original scope contains a one-cost finish. Extra conservative Restart may weaken a lower without becoming a native action.

**Evidence and implementation correspondence:** [Request/policy restrictions](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/request-action-scope.md); [lower relaxation scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the inclusion of permitted proper-policy sets. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.

<a id="clm-0010"></a>
## CLM-0010 — Independent lowers compose by maximum; sums need a cost argument

**Kind:** mathematics.

**Statement:** The pointwise maximum of independently valid same-target lowers is valid. Summing requires an additional decomposition such as nonnegative action-wise cost partitioning. Common-operator subsolutions are also closed under maximum.

**Preconditions:** Compatible target/source and independently valid component claims. Feasibility in a different truncated model is not inferred. Cost partitioning must account for mandatory setup and program steps.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#composition).

**Logical dependencies:** [CLM-0007](#clm-0007)

**Attempted falsification:** One five-cost action satisfies two subgoals, each independently lower-bounded by five. Summing gives an invalid ten-cost claim.

**Evidence and implementation correspondence:** [Maximum-only proof manager contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); no additive native implementation is authorized by this entry.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the pointwise maximum, monotone-operator argument and action-wise cost partition premise. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.

<a id="clm-0011"></a>
## CLM-0011 — A whole-state lower supplies every same-scope action and family floor

**Kind:** mathematics.

**Statement:** If L(s) lower-bounds the full proper-policy optimum, it lower-bounds the infimum cost over policies beginning with any specified legal action, and over any subset of those actions.

**Preconditions:** The lower covers the same original caller scope and semantic state. A program-specific or restricted-action value does not establish this premise.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#composition).

**Logical dependencies:** [CLM-0001](#clm-0001), [CLM-0009](#clm-0009)

**Attempted falsification:** Apply a ten-cost restricted optimum to a scope containing a one-cost action: the missing full-scope premise is exposed immediately.

**Evidence and implementation correspondence:** [Whole-scope source-floor issuer and ordinary consumption](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the first-action policies form a subset of the same full proper-policy class. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.

<a id="clm-0012"></a>
## CLM-0012 — A lower-only finite region may stop at independent boundary bounds

**Kind:** mathematics.

**Statement:** Complete interior Bellman inequalities with independently valid boundary values and scalar action placeholders establish a lower without an incumbent continuation outside the region. Coupled regions may be checked simultaneously.

**Preconditions:** Bounded interior values, nonnegative costs, compatible finite/integrable boundary evidence, proper-policy stopping, and complete native action coverage. Boundary evidence is not a provisional circular claim.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#frontier).

**Logical dependencies:** [CLM-0001](#clm-0001), [CLM-0007](#clm-0007), [CLM-0008](#clm-0008)

**Attempted falsification:** A root finishes for 10 or pays 1 to reach an outside state with true cost 20. Boundary lower 9 closes root value 10 even when the incumbent router has no outside route.

**Evidence and implementation correspondence:** [Lower-only quotient](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); coupled equations here are mathematical examples, not a new native run.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the bounded stopped-region proof with independent finite boundaries and complete simultaneous inequalities. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.
- 2026-09-09 — `accepted` — Codex, scoped constructor-domain application: a separate certificate frame admits an unchanged unfractured request into the already checked coupled region. All-member guards, independent first exits and same-final-goal semantics remain necessary. [Argument and implementation correspondence](mathematics/lower-bounds.md#retention); [current evidence](../active/2026-09-09-empty-start-partial-continuation/README.md). Component correctness does not establish campaign completion or native exact closure.
- 2026-09-09 — `accepted` — Review integration: complete physical event expectations are source/action bounds, not uniform retry-marker values. The [transport argument and counterexamples](mathematics/lower-bounds.md#composition) retain control-domain identity, full mass, final-vector minimization and competing families. Native retry transport remains unimplemented; the current empty model's unchanged Scour/Alchemy/Fracture route defeats a Chaos-only refinement.

<a id="clm-0013"></a>
## CLM-0013 — Admissibility does not imply feasibility in a changed local relaxation

**Kind:** mathematics.

**Statement:** A valid native lower can violate a more optimistic truncated model inequality. It must not be forced into that model as a feasible iterate merely because it was previously certified.

**Preconditions:** The local model or boundary relation changed. Common-operator subsolution closure is a different statement. Independent certificates can still be preserved at compatible maximum consumption.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#initialization).

**Logical dependencies:** [CLM-0007](#clm-0007), [CLM-0010](#clm-0010), [CLM-0012](#clm-0012)

**Attempted falsification:** Native s→t costs 1 and t→goal costs 9. Native lower at s is 10; truncating at t with boundary zero imposes x(s)≤1.

**Evidence and implementation correspondence:** [Separation of prior evidence and changed-model feasibility](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-07 — `open` — Codex self-review, selected numerical-reuse application only: current-query identity, changed-query refusal, final native re-minimization and complete exact inequalities retained; [focused fixtures and matched evidence](../archive/2026-09-07-checked-numerical-reuse-v1/README.md). No broader closure or convergence claim.

<a id="clm-0014"></a>
## CLM-0014 — A minimum-expectation witness is value-specific

**Kind:** mathematics.

**Statement:** A native-covering distribution set supplies a lower expectation only through its minimum or a certified lower on that minimum. A feasible or formerly minimizing distribution can overstate it for the current potential.

**Preconditions:** Complete normalized event partition; native distribution contained in the allowed set; safe event values; correct current-value minimization and arithmetic direction.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#events).

**Logical dependencies:** [CLM-0007](#clm-0007)

**Attempted falsification:** Capacities (3/5,3/5), values (0,10) and then (10,0): the minimum is 4 in both cases, but replaying the old allocation gives 6.

**Evidence and implementation correspondence:** [Current final-vector checking and support reuse](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); [applied-reforge reuse evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-06 — `accepted` — Codex, responsible integration self-review: conditional mathematical argument accepted on the normalized native-covering probability box, exchange minimization and explicit stale-allocation counterexample. Statement and preconditions unchanged; no independent reviewer or blanket native correspondence is claimed. Native applications still require the named GAP obligations.
- 2026-09-07 — `accepted` — Codex self-review, selected numerical-reuse application only: current-query identity, changed-query refusal, final native re-minimization and complete exact inequalities retained; [focused fixtures and matched evidence](../archive/2026-09-07-checked-numerical-reuse-v1/README.md). No broader closure or convergence claim.

<a id="clm-0015"></a>
## CLM-0015 — Joint-goal products require conditional-history bounds

**Kind:** mathematics.

**Statement:** Products of uniform conditional bounds upper-bound an ordered conjunction. Summing over complete distinct-position assignments bounds a multi-goal event without assuming independence.

**Preconditions:** Condition on all relevant earlier history. Distinct goals really require distinct draws; retained/forced goals, overlaps, draw limits, filters, guaranteed pools and cross-side blockers are handled.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#conditional).

**Logical dependencies:** None beyond the definitions stated in the argument.

**Attempted falsification:** Perfectly correlated goal events each have marginal q and joint probability q, not q^k. A single modifier satisfying multiple slots also invalidates an injective-draw assumption.

**Evidence and implementation correspondence:** [Native joint-goal proof contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md). Numerical native weight bounds require their own scope evidence.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-07 — `accepted` — Codex responsible mathematical self-review: repeated conditioning and the complete assignment union establish the conditional statement. The [retained-slot application](mathematics/lower-bounds.md#conditional) counts one physical fracture slot, includes guaranteed draws and excludes retained/forced goals. Exact integer cap and all 26,586 final relation checks are retained in the [campaign evidence](../archive/2026-09-07-continuous-bounds/README.md); no general native correspondence or empty-start lower is asserted.

<a id="clm-0016"></a>
## CLM-0016 — Acquisition and completion potentials are different table roles

**Kind:** mathematics.

**Statement:** In a compatible acquisition relaxation, remaining cost from mask M uses a minimum over acquisition sets S that complete M under the requested any-k predicate, not the acquisition value A(M).

**Preconditions:** The acquisition relaxation is itself valid for the continuation domain; native overlap and goal-count semantics are respected. Coordinate conversion alone proves no native admissibility.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#coordinates).

**Logical dependencies:** [CLM-0009](#clm-0009)

**Attempted falsification:** Acquisition [0,3,5,8] versus completion [8,5,3,0]. A complement-only formula also loses the any-k union semantics.

**Evidence and implementation correspondence:** [Documented proposal-role repair](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); native caller mapping remains separately reviewable.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0017"></a>
## CLM-0017 — Applied postconditions and minimum refill occupancy need separate evidence

**Kind:** native relation.

**Statement:** The archived legal Alchemy path establishes applied Rare rarity independently of refill completion. A minimum occupancy additionally requires every relevant pre-target history to retain an insertable eligible modifier.

**Preconditions:** Only the named native action/domain and its documented application contract. Conditional pool/exclusion and insertion premises must hold; a nonempty initial pool is insufficient.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#retention).

**Logical dependencies:** None beyond the definitions stated in the argument.

**Attempted falsification:** Empty or singleton pools can stop filling early while leaving the applied rarity. A blocker picked first can exhaust a pool that was nonempty at the start.

**Evidence and implementation correspondence:** [Applied-reforge native evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md). This draft did not rerun its native fixtures.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0018"></a>
## CLM-0018 — A proper optimistic-model policy can prove that relaxation has a ceiling

**Kind:** mathematics.

**Statement:** A permitted proper policy of an auxiliary optimistic model upper-bounds that model optimum. It does not supply a native executable upper. An inexpensive abstract finish can cap every feasible lower of that model.

**Preconditions:** The ceiling policy is complete and proper in the specifically identified auxiliary model. Target, auxiliary and coefficient semantics remain labelled.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#ceilings).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0009](#clm-0009)

**Attempted falsification:** A deterministic abstract jump to goal costing ε enforces h≤ε, regardless of solver iterations. A 212-cost abstract metamod exit is not a native completion policy.

**Evidence and implementation correspondence:** [Documented support-only and later ceiling history](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); [21-state optimistic policy evidence](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-09-05-native-applied-reforge-preparation-v1/README.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0019"></a>
## CLM-0019 — Tied constraints and computational floors can mask useful refinement

**Kind:** mathematics.

**Statement:** Raising one of several equal minimizing constraints need not raise the model value. An immediate-price computational placeholder is not necessarily a permanent semantic ceiling.

**Preconditions:** All unchanged constraints remain present. Distinguish independent outside evidence from a deliberately unexpanded continuation; rechecking an old vector is not a solve of the refined model.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#ceilings).

**Logical dependencies:** [CLM-0008](#clm-0008), [CLM-0012](#clm-0012), [CLM-0018](#clm-0018)

**Attempted falsification:** Two floors at 10: raising either alone to 50 leaves 10. A ten-cost action with a ninety-cost continuation remains artificially pinned if its temporary floor is never expanded.

**Evidence and implementation correspondence:** [Candidate-price reactivation contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md).

**Scoped implementation observation (2026-09-09):** Joint retention now builds
supported native relations eagerly under the same proof cap, leaving genuine
outside-domain escapes intact. On the empty-five model, this removes repeated
temporary-price rounds (54 → 5) with the identical checked root lower. The
anchored compact control preserves both checked source values. This computational
result supports [early construction](mathematics/lower-bounds.md#ceilings), not
removal of action coverage or promotion of an auxiliary optimum to a native upper.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0020"></a>
## CLM-0020 — Strict compatible local dominance permits retirement

**Kind:** mathematics.

**Statement:** If a valid action lower at s strictly exceeds a compatible proper executable continuation upper at s, that action cannot be an optimal first action for the same target.

**Preconditions:** Uniform coverage for the retired source/class, exact scope/price compatibility, valid upper and lower endpoints, and preservation of the source tie/numerical contract. A root upper is not automatically a local upper.

**Canonical argument:** [Read the derivation](mathematics/search-and-resumption.md#pruning).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0006](#clm-0006), [CLM-0011](#clm-0011)

**Attempted falsification:** Using a cheap root upper for an uncovered expensive successor miscompares different questions. Equality can leave multiple actions and does not justify removing every proper witness.

**Evidence and implementation correspondence:** [Lower pruning](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); [strict alternatives](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0021"></a>
## CLM-0021 — Cached or resumed evidence is reusable only under its semantic dependencies

**Kind:** mathematics.

**Statement:** A fixed candidate need not remain greedy to remain executable, but cached values, rows, and proof outcomes cannot silently survive changes to the semantic inputs on which they depend.

**Preconditions:** Exact relevant state/member/policy/action/price/control identities and generation behavior are established. Append-only compatible growth may be safe; hashes and a lifecycle label alone are not the argument.

**Canonical argument:** [Read the derivation](mathematics/search-and-resumption.md#snapshots).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0006](#clm-0006), [CLM-0014](#clm-0014)

**Attempted falsification:** Changing a selected row while keeping its old evaluated value invalidates the cache. A released enum can coexist with retained payload and suppressed ordinary work; the archive records that implementation failure.

**Evidence and implementation correspondence:** [Reclamation archive](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md); [replay scope](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
- 2026-09-09 — `open` — Codex, scoped continuation experiment: exact structural membership beyond a saved policy does not establish an owned decision. The [new-controller argument](mathematics/policies.md#properness) requires fresh rows, properness and evaluation. A focused out-of-snapshot completion fixture passes, but both ordinary variants fail the 90-second watchdog and are removed; [failure evidence](../active/2026-09-09-empty-start-partial-continuation/fresh-comparison.json) grants no native acceptance or exact closure.

<a id="clm-0022"></a>
## CLM-0022 — Validity, eventual completion, and bounded performance are separate claims

**Kind:** mathematics.

**Statement:** Correct checking does not imply eventual search completion, and fair finite-work completion does not imply useful bounded runtime. A resource-stopped run does not alone refute a conditional completeness theorem.

**Preconditions:** Any eventual-closure assertion must identify finiteness, finite row work, finite refinement, numerical termination and fair persistent-obligation service. Current source satisfaction is not assumed.

**Canonical argument:** [Read the derivation](mathematics/search-and-resumption.md#progress).

**Logical dependencies:** [CLM-0001](#clm-0001), [CLM-0008](#clm-0008)

**Attempted falsification:** A sound lower can remain zero forever under a starving schedule. A fair enumeration can require more work than the chosen cap. Both can publish only valid values.

**Evidence and implementation correspondence:** [Scheduler contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/scheduling-bellman.md); [resource contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/resources-resume-replay.md). GAP-04 remains open.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0023"></a>
## CLM-0023 — Stored-coefficient feasibility and small residuals are not native error guarantees

**Kind:** mathematics.

**Statement:** Exact checking against represented coefficients proves those inequalities, not native coefficient provenance. A small residual can produce a larger value error through the transient fundamental matrix.

**Preconditions:** Specify target/coefficient interpretation and directed error bounds. A residual-to-error claim needs a bound on (I−Q)^(-1) or an equivalent condition; nontransient cases are excluded.

**Canonical argument:** [Read the derivation](mathematics/numerical-closure.md#residual).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0007](#clm-0007), [CLM-0014](#clm-0014)

**Attempted falsification:** A retry success probability 10^-6 amplifies a residual of 10^-6 into a value error of 1. Dropping a self-choice changes x=1+0.5 min(x,100) from solution 2 to a different expression yielding 51.

**Evidence and implementation correspondence:** [Exact coefficient checking contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/lower-pruning.md); numerical publication correspondence is GAP-05.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0024"></a>
## CLM-0024 — Exact native closure requires compatible proof and executable evidence

**Kind:** mathematics.

**Statement:** A native-valid lower L and proper executable upper U for the same target sandwich its optimum. Equality of guaranteed mathematical endpoints proves the optimum; tolerance or formatting alone does not.

**Preconditions:** Complete allowed-action proof or valid retirement, full state/price/scope identity, correct numerical endpoint interpretation, properness and evaluation of the actual returned artifact.

**Canonical argument:** [Read the derivation](mathematics/numerical-closure.md#exactness).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0007](#clm-0007), [CLM-0008](#clm-0008), [CLM-0020](#clm-0020), [CLM-0023](#clm-0023)

**Attempted falsification:** A rounded zero gap can hide unequal endpoints. An anchored lower paired with an unrelated empty-start upper is not a gap for either target. A policy equation lacks alternative proof.

**Evidence and implementation correspondence:** [Strict closure](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/strict-closure.md); [publication](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/publication.md); [benchmark semantics](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md).

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.

<a id="clm-0025"></a>
## CLM-0025 — Dropping a useful context or evaluating one cleanup policy does not create a lower

**Kind:** mathematics.

**Statement:** Removing a pool-improving filter before lookup need not preserve lower validity. The cost of one cleanup-and-finish policy is an upper on optimal continuation, not a lower on every alternative.

**Preconditions:** The native context affects allowed transitions or costs. A transfer requires an explicit domination relation covering all represented members and permitted actions.

**Canonical argument:** [Read the derivation](mathematics/lower-bounds.md#retention).

**Logical dependencies:** [CLM-0002](#clm-0002), [CLM-0006](#clm-0006), [CLM-0009](#clm-0009)

**Attempted falsification:** Unfiltered retry cost 100 versus filtered retry cost 2 shows that an unfiltered lower cannot be broadcast. Adding a chosen two-cost cleanup makes a four-cost policy, still not a universal lower.

**Evidence and implementation correspondence:** [Metamod/pool contract](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/mechanics/bench-and-metamods.md); future filtered-domain proof must use native evidence, not this toy as a mechanic ruling.

**History:**
- 2026-09-06 — `open` — Drafted by ChatGPT as source-linked research input. Explicit argument/counterexample supplied; local correspondence and repository acceptance remain to be reviewed. No independent reviewer or native rerun is claimed.
