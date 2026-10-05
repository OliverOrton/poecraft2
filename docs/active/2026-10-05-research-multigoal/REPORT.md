# Multi-goal crafting for profit per invested currency
## Research and bounded implementation proposal — 2026-10-05

**Decision:** build a distinct, native-checked multi-goal economic objective atop the existing Calculator goal tabs. Use fully paid repeated production as the planning default. Optimize the ratio of expected net profit to expected invested currency, allow the strategy to choose when to sell, and value each item once. Reuse the native mechanics, request envelope, compiled-controller checker and occupancy accounting; do not promote terminal Calculator projections into continuation states or single-goal cost certificates into multi-goal optimality.

This is research and planning only. No engine or product code, canonical documentation, frozen data, prices or protected content was changed. No build, test, solver, Simulator or dependency-install command ran. The mathematical examples below were derived by hand and are synthetic. Implementation, native correspondence, economic qualification and WASM activation are all pending. Repeated-production/default acquisition treatment was confirmed by the coordinating parent during this investigation; an already-owned-input objective remains a deliberate alternative.

## 1. Evidence scope and revision ledger

The connected GitHub commit endpoint resolved **remote main to 7252027c80856628ed16734583bfc9d6e166458b** at startup. The normal checkout was **7eb16ac3d63834fd5d3256ab42483f47d264b764**, while its cached origin/main was 7252027c. These were distinguished. Direct shell remote access failed through the sandbox proxy; the connected GitHub read supplied the live result instead. The report's released-source observations use the exact 7252027c objects, not the older checkout's files.

The current causal worktree's local committed HEAD was **fb59476f54601f81dc18a8bf2457d8eb7d3eb36b** at inspection. Its canonical HANDOFF, status, claims, policies and living-record files had unrelated in-progress changes, with an untracked joint-service check directory. None was edited. Its connected remote branch still resolved to **23dcc5aea8828dc535466613479493bc379d4e7b**. Therefore later local receipts are explicitly local evidence; this report does not pretend their owner branch had published them, or that the current eligibility extension was built or qualified.

Read: AGENTS.md; research-standards.md; current-status.md; HANDOFF.md; research.md sections 4/5 and relevant GAP-01/GAP-02; the solver-research-handoff template; relevant mathematical, request/publication, source and recombination contracts. The local .agents directory was empty; the inspected tracked directories had no nested AGENTS.md. Relevant prior imported research was reused selectively, particularly complete-candidate-validation.md section 6.1/6.2, rather than redoing the four-report programme or reading archives recursively.

Evidence classes throughout:

| Label | Meaning |
|---|---|
| **Observed source** | Implemented behavior inspected at a named source revision; no fresh execution inferred |
| **Measured receipt** | A pre-existing native result with its own source/build/request scope; this task ran nothing |
| **Derived mathematics** | Conditional argument or hand calculation; does not by itself establish native correspondence |
| **Hypothesis** | Plausible benefit or implementation route requiring falsification |
| **Proposal** | Future work requiring separate authorization and qualification |

The standalone report is the original research input. Shared canonical owners receive only the integration proposals in section 13, after coordination.

## 2. What already exists, and the precise missing capability

### 2.1 Multiple Calculator goals are implemented

**Observed source, 7252027c.** [calculator-goal-set.ts](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/src/app/calculator-goal-set.ts) defines one to eight goal drafts, stable unique IDs, per-goal rarity/slots/tier thresholds/extra-modifier behavior and implicit/influence/corruption observations. It sorts IDs for the native request and keeps the active goal as a presentation choice. Its comment explicitly separates odds identity from presentation and future values.

[workspace/persistence.ts](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/src/app/workspace/persistence.ts) already persists a versioned Calculator goal list and restores legacy scalar drafts. Calculator drafts are not Stash resources. Strategy revisions and feeder snapshots have separate identity/lifecycle contracts.

[solver_api.cpp, pc_calc_create_goal](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_api.cpp#L2066) parses calculator_goal_set_v1, accepts up to eight goals, constructs an inspection-only holder and refuses goal members that independently change the shared action scope. Automatic candidates and automatic Dominance are disabled in that Calculator construction. It is not a multi-goal search constructor.

[calculator_currency.cpp](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/calculator_currency.cpp#L54) performs one native action and records joint matched_goal_ids and per-goal observations. It validates each eight-slot goal independently and caps aggregate slots at 64. Cross-goal overlaps are represented as count observations, rather than concatenated solver goal slots. Union success probability and separate goal probabilities are both available.

The Calculator explicitly refuses multi-goal observed-choice actions without a common policy. It also refuses unsupported information/resource states. This matters: a goal-specific choice cannot be made independently for each sale quote after a single physical offer.

### 2.2 Solve remains selected-goal only

**Observed source.** [pc-calculator.tsx](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/src/app/components/pc-calculator.tsx#L2077) states that Strategy finder and Solver Lab export use only the selected goal. The source exposes existing Current/Finder controls, separate economic-Restart and Imprint choices, priced action scope and checked-strategy presentation. No economic multi-goal objective follows from the goal tabs.

[GoalSpec and GoalAssessment](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_model.hpp#L343) and [assess_terminal_goal](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_calc.cpp#L1401) retain one resolved goal's slot statuses, rarity, minimum coverage and explicit occupancy constraints. Clean and extras-allowed targets differ. Generally equal mod counts do not imply equivalent goals, equal mechanics or equal sale value.

The [strategy evaluator result](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_types.hpp#L667) retains convergence/properness-related diagnostics, terminal success/failure/stop mass, exact occupancy and resource/cost totals. It does not yet retain a native-certified sale-revenue reward for each physical terminal item. [Terminal mass aggregation](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval.cpp#L8994) is currently by terminal node/kind. Adding a price to a presentation label would not establish terminal item valuation.

### 2.3 The terminal Calculator carrier is not a search carrier

**Observed source.** calculator_currency.cpp says that there is no continuation after its observation, excludes future search actions and merges final junk configurations only when they have equal goal/count/flag observations. Its refill DP retains complete physical exclusion groups through the action. This is an appropriate terminal optimization, with a deliberately narrower contract.

**Minimal synthetic counterexample.** Two physical items x and y have the same currently observed goal vector, counts and flags. Neither matches a sale goal. A permitted future action Add has the law x -> valuable item with probability 1, y -> worthless item with probability 1, at the same paid cost. Terminal observation legitimately merges x and y when nothing follows. Continuation-value preservation cannot merge them: expected future revenue differs. An exact continuation abstraction must preserve every permitted action's probabilities into equivalence classes, immediate rewards, observations and legality, not just today's sale truth.

**Native correspondence lead, not an instantiated native witness.** Native refill uses concrete exclusion/conflict groups and retained modifier identities. Different unrelated affixes can affect later pools even if terminal goal observations coincide. No frozen-data search was performed to claim particular PoE modifiers instantiate x/y. The source's own terminal-only comment is already enough to block an unproved promotion. A future native witness should use existing native descriptors/full items, not a base-name dispatch.

**Existing native-scoped counterexample reused.** The released [representation chapter's bounded authored Dominance section](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/representations.md#equivalence) records elemental- and physical-reflection prefixes with equal side/tags/required level/native groups/flags/goal status but different elevated-elemental successor probabilities (1/2 versus 0). Its solution is a scoped singleton explicit-identity carrier and complete native pair reconstruction, with unsupported contexts refused. This is an existing documented native correspondence counterexample, not a new test by this task. It strengthens the x/y warning: even an apparently rich current signature can fail to determine a future upgrade law.


A membership bitmask is useful for valuation and display. It is not a replacement for a sufficient item/control state.


### 2.4 Shared action scope and goal-progress gating are new obligations

The existing product goal-progress restriction is part of its policy class, not a cosmetic search heuristic. “Progress toward any goal,” “progress toward every goal,” and “progress toward the currently selected goal” are different constraints. Changing a selected goal after an outcome can also require explicit controller memory. A scalar union mask does not establish equivalence.

**Proposal for the first finite checker/proposer:** declare a common finite native action/programme scope explicitly and record the economic request's gating rule. For the synthetic witnesses, use their complete explicitly listed actions without a hidden single-goal gate. For native candidate-only stages, a union of independently qualified per-goal proposal families can be used as a restricted grammar, but it is neither a complete action envelope nor an automatic authorization for dependency-only primitives. Preserve exact programme-bound occurrence admission and full positive physical entries.

Before product activation, choose and document the actual multi-goal gating rule, including after paid restart and at observed-choice boundaries. The unrestricted economic target can permit every caller-admitted native action; a progress-gated product target is a named restriction. A proposal generator may order candidates using vector goal progress without making that heuristic a legal-action filter or proof retirement.

Do not concatenate up to 64 cross-goal slots into the existing eight-slot solver GoalSpec. A bounded full explicit-identity item/control carrier with native per-goal terminal matching is the safest first correspondence witness. Later compress it only after proving classwise action/observation and reward preservation. Existing single-action count observations can guide the predicate owner, but their terminal-only merging remains insufficient for search.

## 3. Objective, quotes and overlapping goals

### 3.1 Explicit default: fully paid repeated production

**Proposal, parent-confirmed planning default.** A production invocation begins with acquisition of the configured base/input, follows one finite proper controller, and ends in exactly one sale, discard, destruction or declared terminal liquidation. Every replacement base and paid recovery is charged. Repeated independent invocations use the same pinned request and quotes; the default has no carried inventory.

Let C_pi be expected invested currency per invocation, R_pi expected realized proceeds per invocation, and P_pi = R_pi - C_pi expected net profit. The objective is

    rho_pi = P_pi / C_pi = R_pi / C_pi - 1,    C_pi > 0.

Maximize q_pi = R_pi/C_pi and report ROI = q_pi - 1. Report C, R and P together with the ratio so a tiny profitable opportunity is not presented as high total income. The underlying policy class includes state-dependent branch choices and stopping decisions; it is not merely a menu of fixed recipes for each goal.

This is a ratio of expectations. It is not expected per-path ROI, and it is not profit per crafting action, per solver second or per real-world hour. It is conditional on the supplied valuation assumptions. No price refresh or sale-liquidity estimate occurred.

For an owned input, historical paid spend and current opportunity value require an explicit alternative objective. Never silently replace the paid acquisition cost with zero. At minimum distinguish:
- repeat-production ROI, with repeatable acquisition/replacement paid;
- incremental proceeds/net profit from a currently owned input;
- a capital/wealth comparison against selling that input now.

Those can prefer different policies. The UI/objective choice must bind certificates and saved results.

### 3.2 Sale quotes and fees

**Proposal.** Each goal has a stable ID, exact native predicate, a user-supplied sale quote, quote currency/economy identity and a declared proceeds convention. Default: the quote is expected **net realized proceeds** after any user-declared withholding/haircut. Invested currency contains actual base/crafting/recovery purchases and separately paid selling expenses, if explicitly declared. No game fee is invented.

If the user enters a gross sale price, preserve gross revenue, the declared deduction and net proceeds as separate fields. Charge each expense once. Withheld deductions already netted from R must not also be charged to C in the default convention. An alternative “gross revenue divided by all cash expenses” ROI convention is a distinct objective, not an equivalent display conversion.

If sale actions have separately paid currency costs f_i, the q-subproblem chooses a matching sale maximizing v_i - q f_i. Merely choosing the largest gross quote can be wrong. The simple best-quote formula below assumes no additional paid sale action cost, or a shared identical one. Sale delays and probabilities of finding a buyer are absent from the default point-quote model; unknown liquidity is not silently probability one in a claimed real-market certificate.

### 3.3 Overlap values an item once

Let M(s) = {i : native predicate G_i(s) is true}. In the simple default,

    v(s) = max({v_i : i in M(s)} union {declared salvage(s)}).

The salvage value is an explicitly declared assumption; use zero liquidation proceeds conservatively when no salvage quote is supplied, and disclose that scope. Destruction is not a live item with an invented salvage price. One physical item cannot collect the sum of all matching quotes.

For stable reporting, retain all matched IDs and the chosen quote ID; ties use a deterministic stable-ID rule. Do not use tab order as economic authority. Net quote provenance and goal predicates remain separate so renaming/reordering tabs does not change the problem.

**Synthetic overlap witness.** Prices are A=10 and B=20. Model X produces an item matching both with probability 1/2 and neither with probability 1/2. Model Y produces A-only or B-only with probability 1/2 each. Both have per-goal marginal probabilities 1/2. Their correct expected revenues are 10 and 15, respectively. The sum of marginal price products is 15 in both, and overvalues X. Joint native matching is required.

Goal records can overlap even with the same number of requested mods, through tier alternatives, minimum-satisfied thresholds or extras-allowed settings. Different single-goal “clean” predicates also cannot be collapsed into a union slot list without rechecking occupancy/assignment semantics. Empty slots, crafted cleanup, below-tier affixes, influences, implicits and corruption constraints must retain the existing native resolver's meaning.


### 3.4 ROI, expected profit and profit per time can rank differently

**Synthetic comparison.** A cycle costing 1 and returning 2 has ROI 100% and profit 1. A cycle costing 100 and returning 150 has ROI 50% and profit 50. The first maximizes return on spending; the second earns more per completed cycle. If their declared player durations are 100 and 10 time units, respectively, their profit rates are 0.01 and 5 per time unit. All three objectives can favor different controllers.

With finite regenerative cycles and positive expected duration T_pi, profit per time is (R_pi-C_pi)/T_pi. A parametric time-rate subproblem uses R-C-lambda T, not R-qC. Native expected primitive executions are a useful separate reward, but are not automatically duration: acquisition, decision, execution and selling can have different durations. Solver wall time measures computation, not player crafting time. A time objective would require declared semi-Markov duration laws at the actual actions and terminals, its own certificates and a separate request identity.

A very low-volume high-ROI craft also need not reinvest profits quickly or satisfy an income target. Bankroll, capacity, sale throughput, downside risk and pathwise insolvency constraints are absent from this expectation-ratio default. If later requested, they need explicit model/state/policy changes rather than an undocumented ranking penalty.

## 4. Selling, stopping, paid restart and ownership

**Proposal.** Matching a sale goal makes a Sell action available; it does not automatically make the state absorbing. A matched item can be sold or continued toward another goal. Absorption occurs when the controller actually sells/discards or reaches a physical terminal outcome. This differs from the existing cost-to-one-goal SSP and needs its own request and compiler/checker semantics.

A first-hit controller is a valid restricted candidate. It cannot establish unrestricted multi-goal optimality when further upgrades are permitted. Sell cannot interrupt a mandatory programme, an unrevealed offer or required cleanup. The native decision boundary determines when a choice is available.

Separate:
1. Sell: transfer the live item's ownership, collect one declared payment, and end the invocation.
2. Discard: relinquish the item for declared proceeds (normally zero) and end.
3. Physical destruction: preserve the mechanic's outcome and zero/specified terminal reward.
4. Paid economic restart: relinquish the current item and acquire an exact replacement, including its price and complete acquisition law.
5. Mechanic-owned recovery: preserve its mandatory paid programme and native exits.

Default production repetition begins a new invocation **after** a terminal sale/discard; it is not a free in-graph Restart operation. Existing product voluntary Restart controls remain explicit. A plan may include paid in-invocation restart if authorized and priced, but this changes the action scope and the properness obligations.

Ownership must be irreversible at sale. “Sell the same item again” cannot remain a legal transition. A sale must not both collect proceeds and leave the sold input in inventory. Expected quoted child-output cost is not an ownership token.

### 4.1 Root acquisition and replacement accounting

If the start distribution is alpha and acquisition is a deterministic known charge b, write C_pi = b + expected subsequent paid costs. Pay b once per invocation, including at invocations that fail to sell. Replacements are subsequent costs each time they occur. A stochastic acquisition law must be modeled as a paid action with its complete output distribution; “condition on the desirable start” cannot erase rejected acquisitions.

If an initial owned input is designated sunk in an explicitly separate incremental mode, that designation belongs in the request and output. Include its opportunity comparison separately. A free root with immediate valuable sale makes R/C undefined; inserting a tiny epsilon base price would change the target and is forbidden.

### 4.2 Sale-in-one-cycle versus retained inventory

For iid regenerative production with finite expected cost and duration, the long-run proceeds-to-spend ratio is E[R_cycle]/E[C_cycle]. Correlation between a cycle's proceeds and its own cost does not justify E[R_cycle/C_cycle]. This follows directly by applying the law of large numbers to cumulative proceeds and cumulative costs.

The default cycle ends on every declared terminal outcome, including unsold failures. If a different cycle is defined “until the first sale,” retries, failed-item spend and recovery must remain inside that cycle. Equivalent iid repetition scales both revenue and costs consistently; dividing only cost or only one goal's revenue by its success probability is invalid.

The [existing success-normalized report](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_report.cpp#L530) explicitly describes independent whole-strategy retries. It is not a joint multi-goal valuation or a conditional successful-path expectation. For profit accounting, report the unconditioned invocation first.

Retained donors, leftovers, unsold stock and recycling break the single-item iid-cycle premise unless the entire inventory/control state returns to a declared regeneration state. Do not extend the scalar cycle formula to them by naming a feeder.

## 5. Complete cost and feeder obligations

The released [recombination-solver contract](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/engine/recombination-solver.md#checked-single-item-feeder-and-builder-bridge) already distinguishes purchased sources, completed-feeder quotations and native-checked child laws. Its restricted integration pays actual successful single-item child starts/actions, preserves full outputs/rolls/flags and refuses nested inventory or failed-child recovery outside its contract. General inventory evaluation remains held.

**Derived composition.** A stopped child programme with full exit law K(s,t), finite expected paid cost k(s), and compatible continuation satisfies

    C(s) = k(s) + sum_t K(s,t) C_tail(t),
    R(s) = r_internal(s) + sum_t K(s,t) R_tail(t).

Here r_internal is actual revenue if the declared programme genuinely transfers resources; for a single-item acquisition child it is normally zero. All positive exits, setup/cleanup, failed attempts, replacement inputs and consumed inventory must be retained. Summarizing expected cost is sufficient for the unconstrained additive expectation only after the complete exit distribution is available. Capital constraints, nonlinear prices or inventory-dependent decisions can require the joint cost/output law and larger state.

**Proposal, narrow first feeder support.** Start with no inventory and no feeders. After a single-item economic checker is qualified, accept only the existing checked single-current-item child contract, with embedded immutable strategy/revision/output contract, complete start/action prices and all positive output branches. Default refuse unknown failed-child recovery, nested resource acquisition, quote-only “exact” optimization and advanced model-sensitive recombination. A completed-feeder quote can be a declared exogenous purchase model, clearly labeled, but it is not a certified native child law.

Use one acquisition accounting mode per source:
- declared exogenous purchase: pay its contractual price once;
- native child execution: pay actual child input/material/action costs;
- sunk owned input: only under the explicit owned-input objective.

Do not charge both an expected child quotation and the child's actual crafting bill. Do not omit feeder failures because the output contract names only successful items. Match any sale quote against the actual returned full item, not a goal mask or averaged output.

**Reuse disposition.** Prior complete-candidate-validation research section 6.2 supplies the relevant positive-entry obligation:

    for every reached entry with positive occupancy:
        exact binding AND native programme admission AND complete paid outcomes.

It is incorporated here as a premise, not as a new theorem or experiment. An entry's occupancy can exceed one because of retries; it is not a probability to threshold away.

**Synthetic rare-tail witness.** A positive exit of probability epsilon has paid continuation cost 1/epsilon^2. Omitting it removes expected cost 1/epsilon, however small epsilon is. Replacing it by an unvalued recurrent class destroys properness. A probability-display cutoff cannot remove the obligation.

## 6. Fixed-policy evaluation with two reward vectors

**Derived mathematics.** Fix a finite native-correct compiled controller, including item, operation, choice/programme memory and all authorized ownership facts. Let Q be its nonterminal matrix, c its expected invested-currency vector, and r its expected sale-proceeds vector. A sale action has revenue before entering the absorbing terminal; ordinary crafting has zero revenue in the narrow default.

Premises: complete positive support; legal in-scope operations and observations; native-correct item transitions; complete prices; finite c/r; original-root reachable Q transient. Properness is termination at a declared economic terminal, not necessarily one-goal success.

Then

    N = (I-Q)^(-1) = sum_{k>=0} Q^k,
    C_vector = N c,
    R_vector = N r,
    C_pi = b + alpha^T C_vector,
    R_pi = alpha^T R_vector.

Proof: transience gives the Neumann series; its kth term accounts for the reward at the kth nonterminal visit. Linearity gives each expected total. The same occupancy vector alpha^T N independently reconciles resource spend and per-sale-category proceeds. A payment appears exactly once at the ownership-changing transition.

The actual checked policy supplies a **feasible lower bound on maximized ROI**. Existing native language “checked upper” refers to a feasible cost upper for cost minimization; its direction cannot be copied onto a revenue/cost maximization result. Preserve the artifact's checked-cost authority while naming the new economic bound correctly.

An independent evaluator must resolve terminal quote validity from exact native terminal items before aggregating their valuation classes. A terminal-node annotation or truncated class sample is not enough. Match success/stop/failure/destruction categories explicitly; unresolved mass is not sale mass.

A root-only scalar pair (C,R), even if independently checked, supplies no statewise vector or admission certificate. Compatible statewise values require the same graph, item/control entry, valuation and cost conventions. The currently isolated representation/eligibility owner must finish its own programme; this lane neither modifies that owner nor depends on copying its active candidate table.

### 6.1 Numerical certificates

Source floating-point acceptance remains the existing [numerical contract](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/numerical-closure.md). Its accepted decimal is not automatically a rigorous rational value or outward-rounded interval.

For a rigorous policy ROI interval, obtain verified C in [C_lo,C_hi], R in [R_lo,R_hi], with C_lo>0 and R_lo>=0. Then

    R_lo/C_hi - 1 <= ROI_pi <= R_hi/C_lo - 1.

If C_lo<=0 or revenue is signed under a different convention, do the corresponding interval division case analysis and refuse an unsupported finite endpoint. A rounded displayed zero does not establish zero investment. Small residuals need conditioning/transience information: for reward-vector error e, value error is N e. Nearly recurrent controllers amplify tiny residuals.

Without verified enclosures, label the result a native numerically checked policy estimate under the established contract. Do not display a rigorous ROI certificate solely because the existing cost checker passed. No numerical tolerance is loosened in this proposal.

## 7. Fractional objective as a proper SSP

### 7.1 Root-specific parametric objective

Let Pi be the declared proper finite-cost policy class, with C_pi >= delta > 0, nonnegative bounded terminal proceeds and finite C_pi. Define

    q* = sup_pi R_pi/C_pi,
    F(q) = sup_pi (R_pi - q C_pi),   q >= 0.

For each policy,

    R_pi - q C_pi = C_pi (R_pi/C_pi - q).

Therefore F(q)>0 exactly when some permitted proper policy beats q. If all ratios are <=q, F(q)<=0. At an attained optimum q*, F(q*)=0. Do not claim existence/attainment or a unique zero without the required policy-class and denominator premises. On a finite, fully represented uniformly transient scope with finite actions, bounded expected occupancy and positive denominator, compactness/attainment can be established; merely having a finite state space with possible retry loops is not that premise.

This is the fractional/parametric relationship associated with [Dinkelbach's original 1967 paper](https://pubsonline.informs.org/doi/10.1287/mnsc.13.7.492). The algebra here is derived for the declared policy class. It does not import an algorithmic convergence guarantee into incomplete native Current/Finder search.

A checked feasible policy gives q_L. At q_L, a positive transformed value from another fully checked policy proves improvement. Failure to find one in bounded search does not prove optimality. Different q iterations must retain the best independently checked ratio artifact, rather than replacing it solely because it was the active candidate for one subproblem.

### 7.2 Preserve nonnegative SSP costs with a terminal shift

In the one-sale model, let K be a finite bound on all net terminal sale quotes/salvage values. For q>=0, minimize

    H(q) = inf_pi [ q C_pi + K - R_pi ].

The internal paid cost is q times actual invested currency; the final sale/discard cost is K-v(s). If selling has a separately paid charge f, its terminal cost is q f + K-v(s). These are nonnegative under the declared premises. Include q b at original acquisition. Since exactly one terminal reward is realized,

    H(q) = K - F(q).

This gives a nonnegative proper-SSP formulation, but the terminal shift is an **optimization weight**, not a fictional consumed currency. Native paid-resource totals must still report c, not q c or K-v. A compiler/evaluator needs explicit typed reward semantics.

The shift is invalid if multiple revenues are collected within the same modeled invocation, K does not bound actual proceeds, or a policy can terminate without exactly one corresponding terminal accounting event. A general resource system may instead require signed transformed rewards and a different proof contract.

The current [TargetNeutralZero capability](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_solve_contracts.hpp#L28) withholds positive lower/retirement/exact closure outside its qualified ordinary-clean target. It does not automatically certify H(q). Ordinary clean lower tables cannot be rebound by changing a price scalar: goal absorption, sale decisions, programme coverage and reward meaning have changed.

**Safe initial authority:** checked candidate economics, no global ratio-optimality label. A separate proof owner must certify all competitor actions/programmes and the transformed target before any closure field becomes available.

### 7.3 State-dependent decisions use one root exchange rate

At a nonterminal state, the transformed reward equation uses one fixed root q:

    W_q(s) = max_allowed [
        r(s,a) - q c(s,a) + sum_t P(t|s,a) W_q(t)
    ],

with zero value at the absorbing economic terminal and proper-policy semantics explicit. Sell competes with continuation; choices inside observed offers stay after their actual observation.

Do not maximize a separate remaining-revenue/remaining-cost ratio at every state. That omits investment already paid and can make a zero-additional-cost sale look infinitely good. The scalar q incorporates the root objective consistently.

**Synthetic sunk-cost witness.** After 9 paid currency, sell now for 10 gives root ratio 10/9. Spend 1 more and sell for 12 gives 12/10, which is better. The local “sell for 10 at cost zero” ratio is undefined, not a reason to prefer selling. At the root-optimal q=1.2, sale has transformed value 10, upgrade has 12-1.2=10.8, while the already-paid root charge is accounted separately.

If a hard bankroll or pathwise loss constraint is later requested, remaining capital/accrued expenditure can become necessary state. This report's expectation objective supplies no solvency or drawdown guarantee.

### 7.4 Global ratio upper certificate

**Derived sufficient certificate.** On a complete finite represented domain, choose bounded w with w(terminal)=0 such that, for every allowed action and every supported state,

    w(s) >= r(s,a) - q_U c(s,a) + sum_t P(t|s,a) w(t).

Check initial acquisition consistently:

    alpha^T w <= q_U b.

For every permitted proper policy, telescope the inequalities up to terminal absorption. Bounded w and proper finite termination make the final expected w vanish. Thus R_pi - q_U C_pi <= 0 and R_pi/C_pi <=q_U. A compatible checked policy lower plus this global upper brackets q*. Exact equality needs exact/verified numerical inequalities and complete native correspondence.

The inequalities must cover omitted/generated actions, every physical member represented by an abstraction, offers, mandatory recovery and restart. Checking only the chosen controller's reachable inequalities proves nothing about cheaper/better competitors. A restricted finite model certificate remains restricted unless transferred by a proved native relation.

With R<=K and C>=b>0, the cheap bound q*<=K/b is already valid in this narrow one-sale model. It may be loose, but gives a meaningful initial ceiling. A positive paid base provides denominator separation without inventing epsilon costs.

If a certified lower L_H(q) on the minimized transformed SSP exists, then F(q)<=K-L_H(q). When L_H(q)>=K, q is a ratio upper. When K-L_H(q)=epsilon>=0 and C>=delta, q*<=q+epsilon/delta. A bounded solve's feasible H value is an upper on minimized H, hence a **lower** on maximized F; it cannot supply this global ratio upper.

### 7.5 Dinkelbach-style iteration and stop rules

**Proposal after model/checker qualification.** Begin with an independently checked proper economic fallback. At q_k = R_k/C_k, seek a proper candidate maximizing R-q_k C. Independently evaluate its actual R/C; retain only a genuine checked improvement. In a fully solved finite attained problem, an exact subproblem optimizer updates q monotonically toward q*, with finite policy alternatives providing a finite termination argument under an appropriate deterministic-policy completeness proof.

In practical Current/Finder bounded search, describe this as parametric candidate search. Unavailable subproblem proof, open envelope, cap or no candidate means unresolved optimization, not “ROI exact.” Preserve the best checked ratio and its source/request identity when a later q attempt fails.

Do not add an arbitrary stopping epsilon to prices, discount future cost, truncate positive outcomes or reset cumulative checker budgets per q. A near-zero transformed residual is not by itself an economic gap certificate.


### 7.6 A precise finite-policy completeness premise

**Derived sufficient case, not a current native assumption.** Suppose the complete finite economic MDP has finitely many actions per state and admits a common finite nonnegative h, zero at the absorbing terminal, satisfying

    h(s) >= 1 + sum_t P(t|s,a) h(t)

for **every** allowed nonterminal state/action. This implies uniform finite expected termination under any allowed history-dependent policy by telescoping the stopped inequality. Let h_max=max_s h(s)>=1. For weighted sup norm with weights h(s), every nonterminal transition contracts by at most beta=1-1/h_max<1. The transformed Bellman operator is a contraction because bounded immediate rewards do not affect differences. It has a unique bounded fixed point, and selecting a maximizing action at each state yields a deterministic stationary optimum for each q.

With a positive base charge and bounded one-sale revenue, every such policy has a positive bounded denominator and the finite set of deterministic stationary policies contains a ratio optimum: solve at the optimal ratio and choose its maximizing policy; transformed value zero supplies the optimum's witness. Thus exact parametric iteration over strictly improving ratios cannot visit the same deterministic policy twice. A crude iteration ceiling is the number of deterministic policies, at most the product of allowed action counts over states, which is exponential in state count. This is a completeness argument, not a practical latency estimate.

The premise fails if any permitted action is a deterministic nonterminal self-loop, including an unconstrained retry/restoration loop. Failure of this **sufficient** h premise does not imply the proper-policy optimum is unavailable or no good policy exists. It means this contraction/deterministic-completeness proof cannot be used. A selected-policy h certificate proves only that policy's termination; it cannot silently remove legal competitor actions. The general proper-policy case still needs its own finite-model policy-class theorem or the all-action ratio-upper certificate of section 7.4.

### 7.7 Quote representation and transformed identity

**Proposal.** Preserve user-entered decimal quotes in a canonical finite-decimal representation with their explicit currency conversion convention, rather than treating formatted JavaScript numbers as exact financial inputs. Use the existing request/economy identity machinery to bind goal predicates, proceeds convention, acquisition, replacements, scope and selected q/K coefficients; do not add a parallel hashing database.

Exact arithmetic on the rational representation of stored binary coefficients can certify that coefficient model. It does not prove those stored coefficients are exact physical probabilities. Conversely, a user quote may be an exact declared decimal input while still being a conditional market assumption. The result should say which of these meanings is certified. Shared transition topology can remain reusable across quote changes only when the complete native law/observation contract is unchanged; reward vectors, policy ranking and bound certificates must be refreshed.

## 8. Occupation-measure linear-fractional formulation

**Derived alternative for a declared complete finite model.** Let x_sa be expected preterminal visits to state/action from the root. With nonterminal transition matrix P and initial alpha,

    sum_a x_sa - sum_{t,a} P(s|t,a) x_ta = alpha_s,
    x_sa >= 0.

Let r_sa be expected immediate terminal-sale proceeds and c_sa invested cost. Then R=r^T x and C=b+c^T x. Set tau=1/C>0 and y=tau x. The transformed linear constraints are

    flow(y) = tau alpha,
    b tau + c^T y = 1,
    y>=0, tau>0,

and maximize r^T y. Subtract 1 to obtain ROI. User-declared expected-cost/action bounds, if part of the semantic target, can also be transformed linearly, for example sum y <= H tau. A computational cap is not such a constraint.

This is the algebra behind a positive-denominator Charnes–Cooper transformation. The [original 1962 paper](https://iiif.library.cmu.edu/file/Cooper_box00010_fld00009_bdl0001_doc0001/Cooper_box00010_fld00009_bdl0001_doc0001.pdf) explicitly uses a bounded feasible-set premise to establish positive scale in its regular case. Native occupancy with arbitrary retry/circulation is not automatically bounded.

For finite feasible x, extracting pi(a|s)=x_sa/sum_a x_sa on occupied states gives a stationary randomized controller. A root-reachable closed nonterminal class would receive positive initial/incoming flow but have no outgoing terminal flow, contradicting the summed finite balance equation. Unreachable circulations can still exist; remove them and independently recompute the extracted controller's true root occupancy before reporting economics. Only a qualified policy-class mapping permits this extraction in the native programme/observation context.

Critical limits:
- A solver accepting tau=0 can return a recession/circulation solution without an executable production invocation. Require a positive-scale witnessed extraction; do not declare it a policy.
- Zero-cost circulations can make the feasible occupancy set unbounded and leave infinitely many equivalent descriptions. Do not cite a bounded-polytope theorem without proving its premise.
- A rational LP optimum is exact for its stated coefficients. Stored native doubles, physical laws and native-to-model equivalence remain separate.
- Randomized extracted actions need an executable observation/sampling contract. If product policies are deterministic, prove an appropriate deterministic optimality result for the declared finite transient class or restrict the claim.
- Multi-item inventory, history-dependent ownership, nonlinear sale schedules and resource constraints need their actual state and coefficients. They are not solved by attaching a second reward vector.

**Recommendation:** use this as a small finite independent oracle and possible certificate design after approval, not a new dependency or production LP engine now. The native proper-SSP route better aligns with existing source owners, but its new objective still needs proof work.

## 9. Loopholes and explicit negative evidence

### 9.1 Zero denominator, infinite activity and free resale

**Synthetic counterexamples, not native mechanics.**
- Valuable owned item with C=0: positive proceeds divided by zero is undefined/unbounded under the selected accounting. An epsilon “fix” is a new problem.
- Policies with R=1 and C=1/n: ratios tend to infinity unless a positive-denominator separation or another declared constraint exists.
- A zero-cost self-loop and a paid finite finish: Bellman equations alone can select looping forever. Root properness is still required after the terminal-shift transformation.
- Selling without consuming ownership, followed by costless restart: repeated sale prints unbounded revenue from one item.
- Refunding/resetting the spend ledger at Restart: deletes failed production costs and overstates ROI.
- Conditioning feeder quotation on success, while discarding failed purchase/craft costs: overstates the output economics.
- Crediting unsold inventory at a guaranteed sale quote while also carrying it forward for later sale: double-counts asset value.

The project [proper-policy argument](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematical-model.md#properness) and [Bertsekas's primary paper](https://arxiv.org/abs/1711.10129) explain why raw accumulated-cost and proper-policy objectives can differ. This report uses finite transient controllers, not a universal infinite-state Bellman-convergence theorem. The transformed nonnegative-cost formulation does not remove zero-loop or native correspondence obligations.

Missing prices, unsupported kernels and unresolved branches remain explicit unavailable evidence. They must not become free actions, zero revenue at a supposedly resolved sale, or evidence of infeasibility.

### 9.2 Existing causal programme: preserve its actual result

**Measured receipts, inspected at local fb59476f.** The Conquest matched control/treatment both cost **101311.35474896732**, success one, complete prices, zero off-policy mass, requested bounded Finish and cap mask zero. The treatment receipt reports 33 upper requests, zero starts, 33 rejections, last reason seed_rejected_statewise_values_without_focused_fallback, and zero joint attempts. The 6,557-byte strategy is byte-identical to control with SHA-256 d65787d11ad895d5925ce9dd4d0aefde483e53bc25df3a80a6a9dfe7a0d7e053. Both root slots are spent.

Implementation source/build belongs to **5a74619339b5283a22074b79026ea3c0177cc0ca**, engine tree **056f257007efd976d9fd48420d6d85d9dac797f8**, Benchmark SHA-256 **57ae44df5212fd2becfef122329e96b8de9524160178574af18008a260bbd0f7**. The summary's final publication winner is distinct from its active expensive primitive candidate. No root scalar is statewise authority.

The 179-check/three-case service fixture at that source passes a conditional internal 13-to-3 adoption and preserves 13 in missing-successor/resource negatives. Its normal constructor is inactive/closed; it is not Conquest recovery or a multi-goal implementation. No new run reproduced those receipts in this task.

The historical 85970.67 graph is a reprice with unresolved product-envelope admission, not a matched old-source recovery target. The private roughly 210k candidate remains inferior. The representation-independent extension currently in progress has its own owner. This report borrows none of its unpublished activation, statewise or performance qualification.

These negatives inform the multi-goal architecture: retain best checked artifacts independently, distinguish active search candidates from publication winners, require complete checker admission before claiming economic improvement, and do not assume a successful local fixture reaches a real consumer.

The current [published causal record at 23dcc5ae](https://github.com/OliverOrton/poecraft2/blob/23dcc5aea8828dc535466613479493bc379d4e7b/docs/active/2026-10-04-sol61-armour-recovery/README.md) is an earlier checkpoint; it does not attest the later local treatment receipt. The compact receipt facts above are retained in this new report so that the source/qualification distinction remains durable.


### 9.3 Profitability acceptance and abstention

**Final-review safeguard.** A best independently checked revenue/cost ratio below 1 is a search result, not a global impossibility certificate. Display **“No profitable checked policy found”** and retain the best checked candidate, its economic request identity and the still-open search scope. A checked ratio equal to 1 is break-even and likewise establishes no positive-profit checked candidate; a numerical interval crossing 1 leaves that candidate's profitability unresolved. Unknown/incomplete prices or unresolved mass cannot supply either conclusion.

Only a certified **GLOBAL** ratio upper at or below 1, covering the entire declared permitted action/programme scope with matching valuation, acquisition, properness and numerical premises, supports **“No positive-profit policy in this scope.”** A restricted model upper applies only to that explicitly named model/scope. Failure to find a profitable policy, a bounded Finish, a resource cap, a single-policy check or an optimistic-looking search score does not supply this certificate.

Abstention is a separate product decision outside the positive-investment ratio policy class. It has **no ratio value**: never encode it as 0/0, an epsilon-cost policy or an “exact” free-start crafting strategy. If no checked positive-profit policy is available, the interface can recommend abstention while distinguishing “search remains open” from “positive profit is globally ruled out.” Continue to expose conditional quote assumptions; this is a model-conditional profitability result, not a guarantee of realized market profit.

**Minimal acceptance witnesses:** a checked candidate with ratio 4/5 and an unsearched permitted candidate at 6/5 must show only the checked-search message; a valid all-action ratio upper 1 supports the global scoped message; a break-even candidate is not positive profit; abstention keeps its ratio absent and cannot become the fallback's fake denominator. Add these to the existing finite falsification packet without weakening any native or arithmetic gate.

## 10. Synthetic decision witnesses and minimal falsification packet

### 10.1 A branching policy beats tab-wise blanket behavior

**Hand-derived finite example.** Acquire a base for 2, then pay 2 for a roll producing A or B with probability 1/2 each. A can sell for 8; B for 4. A's optional deterministic upgrade costs 3 and sells for 9. B's optional deterministic upgrade costs 1 and sells for 7. Each upgrade is one-use, and all paths sell one item. These are economic placeholders, not PoE actions.

| Controller | Expected proceeds R | Expected spend C | R/C | ROI |
|---|---:|---:|---:|---:|
| Sell A and B immediately | 6 | 4 | 3/2 | 1/2 |
| Upgrade A only | 13/2 | 11/2 | 13/11 | 2/11 |
| Upgrade B only | 15/2 | 9/2 | 5/3 | 2/3 |
| Upgrade both | 8 | 6 | 4/3 | 1/3 |

At q=5/3, A's incremental transformed gain is 1-3q=-4, while B's is 3-q=4/3. The policy therefore sells A and upgrades B. Its root transformed value R-qC is zero. Full enumeration of these four declared controllers establishes the synthetic optimum. It does not close a native programme grammar.

This is the intended state-dependent product behavior. Optimizing every tab in isolation and taking the cheapest/most expensive tab's result cannot represent the same branching decision.

### 10.2 Ratios of expectations and expected ratios disagree

A controller has outcomes (R,C)=(10,1) or (0,9), equally likely. Its expected per-path ROI is (9-1)/2=4, while its production ROI is E[R]/E[C]-1=5/5-1=0. A deterministic (R,C)=(3,2) controller has production ROI 1/2 and should rank above it under the selected objective. The ranking reversal is a required objective test.

### 10.3 Smallest useful future falsification experiment

**Proposal only; no allowance created or consumed.** Before any real-data economics, use the existing native finite fixture/checker owners to encode a tiny declared economic controller set. Target at most 32 semantic states, 64 state/action entries, 256 positive transitions and eight goals; retain every row. These are proposed fixture-size bounds, not native semantic truncations.

One deterministic supervised batch, after separate implementation/build authorization, should answer these causal questions:
1. Overlap: same marginal goal probabilities, different joint matching, correct single sale value.
2. Continuation carrier: the x/y terminal-equivalence counterexample refuses a merged continuation claim.
3. Branching: independent exhaustive finite calculation and native compiled evaluation reproduce the four-controller table.
4. Paid entry/restart: acquisition is charged in failures and every replacement; zero-cost owned-entry mode is labeled separately.
5. Ownership: a second sale of the same resource refuses.
6. Support: add a tiny positive expensive exit, a missing successor and a positive recurrent class; none can publish a complete economic value.
7. Feeder accounting: actual child costs versus exogenous quote are mutually exclusive; unsupported failed-child recovery refuses.
8. Rewards: native paid consumption reconciles to C, exact terminal valuation to R, and q/K optimization weights never appear as spent currency.
9. Certificate direction: a feasible policy is an ROI lower; a selected-policy Bellman check cannot publish a global upper.
10. Mutation/lifetime: changing quote/predicate/root/scope invalidates economic results, while renaming/reordering tabs leaves predicates and odds unchanged.

Use exact small rational expected values as the independent synthetic oracle where possible. Native-double comparisons retain existing tolerances and mass/properness checks. An oracle result is not native physical-law exactness. If any support, ownership, terminal predicate or accounting witness fails, stop dependent economics work; continue only independent documentation/source diagnosis.

The existing native supervision, fixed-policy checker, limits and compact receipts own this work. Do not add a second run ledger/service, renew Conquest allowances, or use large real cases as the first witness. No user-approved real-data run budget exists for this proposed lane.

## 11. Complexity, resource costs and bounded milestones

### 11.1 Costs that are known structurally

For n finite represented nonterminal states, m state/action entries and e positive transitions:
- Graph and complete support storage is O(n+m+e), before state payloads, physical-member certificates, evaluator scratch and compiled output.
- Joint membership for G<=8 can be represented in eight bits; materializing every membership category costs up to 2^G=256 labels, but the full continuation state can be vastly larger.
- Per-state sale valuation is O(G) predicate/value work if the exact state is already available. Native slot resolution and observation classes have their own costs.
- Shared exact kernels can serve several quotes/goals where native law is unchanged. Adding more required distinctions may enlarge the carrier and transition enumeration; “one pass instead of eight” is not a measured speedup.
- Two reward right-hand sides can share a fixed policy's factorization. Dense worst-case factorization is O(n^3) time/O(n^2) space, with O(n^2) per additional right-hand side. Sparse fill may still approach that worst case.
- SCC/support properness analysis is O(n+e); wide numerical solves, semantic admission and physical-member checks remain additional work.
- Parametric search multiplies subproblem work by the number of q iterations. It must share a single aggregate resource allowance and keep one checker at a time where the owner requires it.
- LP variables are O(m) plus the scale, with O(n) balance constraints; coefficient bit complexity and sparse fill are material. No universal latency guarantee follows.

Do not allocate one independent 1 GiB solve/checker per goal or per q. A bounded real economic policy can still be expensive to check. Reuse transition topology only under complete probability/observation identity; revalue/reoptimize under changed quote/cost data and retain source identity.

**Initial supported shape proposal:** one configured base/session and original physical root; one to eight native-resolved explicit-affix sale predicates, with common shared action scope and complete quote/acquisition prices. Clean/coverage and tier/count semantics must remain exact per goal. Admit implicit, corruption, offer/memory or feeder cases only after the corresponding continuation carrier is qualified. This is a capability-derived restriction, not a special recipe keyed to a base name. Changing only the goal tabs cannot enlarge an unsupported solver carrier.


### 11.2 Stage gates

| Stage | Bounded deliverable | Gate and stop condition |
|---|---|---|
| M0 — economic contract | Versioned request schema and native accounting specification; repeat-production default; quote/fee convention; ownership/terminal rules | Resolve supported goal shapes and initial-input mode; no gameplay changes |
| M1 — independent authored evaluation | One finite single-item controller, exact terminal valuation and C/R rewards through existing evaluator; no search/feeder inventory | All section 10 finite falsification witnesses pass with unchanged native law/checker thresholds |
| M2 — Finder proposal path | State-dependent sell/continue routing plus bounded q candidate ordering; retain best independently checked ratio | Native original-root, all-positive-entry and complete-price evidence; no global optimum claim |
| M3 — Current candidate path | New objective capability with native sufficient carrier and transformed nonnegative SSP; existing action scope remains explicit | Qualified request/compiler/evaluator mapping; no reuse of clean lower tables or premature closure |
| M4 — ratio proof | Complete finite transformed bounds or verified w certificate, denominator separation and policy-class transfer | Every allowed/generated action accounted; numerical inequalities verified; scope-specific closure only |
| M5 — restricted feeder extension | Existing checked single-item child law with exact paid acquisition/output/revision | No quote-only native certificate, nested inventory or unqualified failed-child recovery |
| M6 — product/WASM | Calculator values/objective controls, result presentation, worker transport, export/replay and cancellation | Matching final native/WASM bytes and exact economic request; visual review by Oliver if requested |

Stages are candidates for a new programme, not implicit authorization. M1 should prove accounting before M2 tries to improve policies. M4 can be deferred indefinitely while useful checked bounded policies ship with honest status. Full inventory/time/risk optimization is outside these stages.

A later real comparison must pin identical roots, all goal predicates/quotes, terminal rules, economy/law/data, acquisition/fees, action/programme scope, native source/build, q/candidate grammar, checker and aggregate caps, host and runtime. Compare best checked ROI by budget plus C/R/P and periods with no policy. A single-goal cost baseline is not a same-objective multi-goal comparator.

## 12. Current, Finder, authored and WASM applicability

| Consumer | Present fact | Proposed applicability | Qualification still required |
|---|---|---|---|
| Calculator one-action odds | Joint per-outcome goal matching implemented | Add quote valuation/display only after quote contract; keep native probabilities | Native overlap/fee/source identity; no continuation claim |
| Authored exact evaluator | Complete single-item cost/occupancy and terminal categories implemented; general inventory exactness held | First C/R economic checker on supported full item/control states | Sale predicate, ownership, terminal reward, properness and numerical correspondence |
| Authored Simulator | Resource/feeder execution has separate native support | Later economic trace display and ownership assertions | Sampled execution is not exact expectation/proof; no Simulator work in this task |
| Finder | Checked executable candidates, no global lower/optimum authority | Multi-goal branch and q-guided candidate proposals | Actual original-root and all positive programme entries checked; private then explicitly activated |
| Current | One-goal cost solve; profile-specific proof capability | New proper economic target, candidate search then separately certified ratio bounds | Carrier, objective/reward mapping, complete action envelope, properness and native numerical gates |
| WASM/worker/UI | Existing Calculator and selected-goal solve transport | Versioned economic request/result, quote edit invalidation, worker/replay lifecycle | Matching final source/artifact, real worker functional acceptance and cancellation; no inherited timing or economics |

Same native helpers do not establish a hybrid Current/Finder mode. A compatible checked-artifact handoff must carry exact economic request identity and rewards. An active candidate's rejected statewise table cannot replace a separately retained publication winner. The isolated causal owner remains protected.

**Recommended result surface:** expected invested currency; expected sale proceeds; expected net profit; ROI; sale-goal distribution with overlap-safe chosen labels; failure/discard/stop/unresolved mass; expected primitive/resource use; input/quote/economy identities; evidence status; restricted action/grammar scope; stop/cap reason; native/WASM qualification. Show “best checked policy” independently from “global ratio upper/closed,” and keep unknown totals unknown.

Price edits can reuse genuinely price-independent native transitions if all request/cached-law premises still match. They invalidate quote-dependent ranking, ROI values, transformed proofs and any retired-action decision. Goal edits can change required carrier distinctions and invalidate more than the valuation table. Existing request lifetimes provide a useful UI owner, but must bind the entire new request.

## 13. Exact canonical integration map and doc-ready propositions

No canonical file was edited. The later integration owner should retain this report once, reconcile it against then-current main/local changes, and use the following precise insertion map. Do not hand-edit generated research-state.md or create a second status database.

| Canonical owner | Insertion/reconciliation target | Proposed content |
|---|---|---|
| docs/solver/mathematical-model.md | After #cost and alongside #properness/#goal | Define economic invocation, C/R/P/ROI, paid production root, optional sale absorption and proper economic terminal; distinguish owned-input mode |
| docs/solver/mathematics/policies.md | After #fixed-policy/#programs | Two-reward transient evaluation; complete terminal quote matching; feeder exit/cost composition and ownership; ratio feasible-bound direction |
| docs/solver/mathematics/numerical-closure.md | #direction, #residual, #exactness | Positive-denominator interval division; verified w/SSP ratio-upper certificate; transformed coefficient/native contract; residual conditioning |
| docs/solver/mathematics/representations.md | #equivalence and terminal-observation discussion | Preserve sale reward plus every continuation law/observation; terminal projection x/y counterexample; union goal truth insufficient |
| docs/solver/request-action-scope.md | Inputs And Outputs; Product and diagnostic scope | Versioned common action scope; enabled sale/discard/restart; repeat-production versus owned entry; per-goal predicates and price identity |
| docs/solver/publication.md | Evaluation Contract and Classification | Actual compiled economic controller, terminal ownership transfer, separate C/R accounting, “best checked ROI” versus global bound, complete support |
| docs/solver/upper-authority.md | Recovery and exact terminal success | Keep existing cost-upper semantics; identify corresponding feasible ratio lower; no root-to-statewise authority promotion |
| docs/engine/recombination-solver.md | Checked single-item feeder and Builder bridge | Economic consumer must reuse paid immutable child law; quote-only, nested inventory and failed-child holds remain |
| docs/foundation/solver-internals.md | Phase map/Retained Authorities | Name actual new request/reward/certificate consumers when implemented; no speculative new service inventory |
| docs/solver/claims.md | Allocate fresh IDs only after coordinated review | New conditional multi-reward/fractional/certificate propositions; retain CLM-0001/0002/0003/0004/0005 preconditions/history; do not close their general GAPs |
| docs/solver/research.md | #handoff/#knowledge and relevant GAP-01/02/03/05 | Disposition of this packet, native terminal/representation/grammar/numerical transfer still open |
| docs/solver/current-status.md | Goal, authority and lane map | Only after actual qualification: separate Calculator quote valuation, authored economic check, Finder/Current consumer and WASM status |
| HANDOFF.md | Selected sequencing only | A short approved programme pointer with source, gates and spent allowance; recommendations do not select implementation |
| Calculator/UI owner docs, located when implementing | Existing goal-list/workspace and engine protocol owners | Stable value quotes, explicit objective mode, edit invalidation, no frontend crafting-rule duplication |

**Doc-ready conditional propositions, with no assigned ledger IDs:**

MG-P1 — On a finite native-correct transient compiled economic controller with complete paid rewards and one ownership-changing terminal payment, expected C and R are obtained from the same fundamental matrix and occupancy. A feasible positive-cost controller lower-bounds maximum ROI. Counterexamples: positive recurrent class; sale twice; missing positive paid exit.

MG-P2 — For a declared proper policy class with positive expected investment, ratio comparison at q is equivalent to comparison of R-qC. Bounded one-sale proceeds permit a nonnegative terminal-shift SSP. This is an algebraic relation, not native closure. Counterexamples: C=0; multiple sale rewards under one K; unqualified improper loops.

MG-P3 — A bounded native-valid superpotential satisfying all action reward inequalities and the original acquisition inequality proves a global ratio upper at q. Policy-reachable checks alone are insufficient. Counterexample: omitted competitor with larger R/C.

MG-P4 — Exact behavioral reduction for economic continuation must preserve legal actions, observations, class transitions and both rewards/terminal ownership. Same current sale membership is insufficient. Counterexample: x/y terminal projection witness.

MG-P5 — Independent repeated cycles justify ratio of aggregate expected proceeds/spend under the stated finite iid regeneration premises; expected per-path ROI can rank controllers differently. Counterexample: section 10.2. Retained inventory requires a different regeneration/state proof.

**Disposition of material inputs:** existing properness/occupancy/choice/programme/representation arguments are incorporated conditionally; Calculator joint matching is reusable observed source; terminal-carrier promotion is contradicted without additional equivalence; current causal matched result is a scoped negative; historical graph recovery and representation-independent service activation remain open with their owners; general inventory, time/risk and ambiguous mechanics are out of this implementation proposal.

## 14. Decisions, limits and next handoff

1. Use fully paid repeated-production ROI as the explicit default; parent confirmed this choice. No further user question is needed to finish planning.
2. Treat sale as an optional native-checked decision and value overlaps once. Common goal mod count is presentation context, not a mathematical shortcut.
3. Build accounting and an authored finite checker before Finder/Current search. Keep terminal Calculator probabilities reusable only within their terminal contract.
4. Use parametric proper SSPs for candidate search; exact fractional/LP/global-certificate arguments are conditional and need native transfer. Do not market bounded candidates as exact maximum ROI.
5. Preserve complete positive support, mandatory payments, ownership and all existing admission/numerical gates. No new approximation, special-base recipe, weakened test or recycled allowance is proposed.
6. Keep feeders narrow until their existing native child contract is sufficient. General inventory and robust recombination remain held.
7. Separate ROI from expected profit and profit/time. Time optimization needs declared player execution/acquisition/sale-duration laws, not solver CPU receipts.
8. Stage-specific qualification belongs to each consumer. Main's full functional acceptance and the isolated causal fixture do not qualify this new objective.
9. A sub-break-even checked candidate means no profitable checked policy was found; only a complete global upper at or below one rules out positive profit in scope. Abstention remains outside ratio division.

**Remaining blockers before implementation:** new economic terminal/reward/ownership contract; sufficient continuation carrier for all selected goals; complete checker mapping; denominator/properness handling; any complete global ratio-proof authority; final-byte WASM/worker acceptance. These are concrete stage gates, not failures of this research delivery.

**Actual validation:** primary source inspection; explicit source/receipt separation; hand proof/counterexample checks; report privacy and byte/line preservation checks at publication. Native checks executed by this task: zero. Existing run allowances spent by this task: zero. No solver process was started or left running. No deployment, main merge or market-data refresh occurred. The designated gaming windows were not entered during this research; no new local execution permission follows from this packet.

## 15. Primary source index

Released source links all bind main 7252027c80856628ed16734583bfc9d6e166458b:
- [Research standards](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/research-standards.md), [research import/knowledge](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/research.md#handoff), [current status](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/current-status.md).
- [Calculator goal-list source](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/src/app/calculator-goal-set.ts), [native joint terminal calculation](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/calculator_currency.cpp), [Calculator API construction](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_api.cpp#L2066), [selected-goal product solve](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/src/app/components/pc-calculator.tsx#L2077).
- [Native terminal assessment](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_calc.cpp#L1401), [evaluator result](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_types.hpp#L667), [success-normalized retry semantics](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_report.cpp#L530).
- [Executable policies](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/policies.md), [representations](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/representations.md), [numerical closure](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/numerical-closure.md), [publication contract](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/publication.md).
- [Request/action scope](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/request-action-scope.md), [restricted paid feeder bridge](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/engine/recombination-solver.md), [claim ledger](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/claims.md).

Historical/local evidence used once:
- Published causal branch snapshot: [23dcc5aea8828dc535466613479493bc379d4e7b](https://github.com/OliverOrton/poecraft2/commit/23dcc5aea8828dc535466613479493bc379d4e7b).
- Later local fb59476f contains checks/root-treatment-20261005/summary.json and checks/root-only-joint-conditional-native-20261005/summary.json under the selected armour-recovery programme. Their reviewed fields and implementation source/build identity are preserved in section 9.2. No private machine paths or raw trace were copied.
- Later local research-inputs/complete-candidate-validation.md sections 6.1/6.2: reused full-entry/fixed-policy argument, not a new native result. Original owned Library ID recorded by its import owner: libfile_f0d21fa811788191acf82f3642e02c37. This identifier is an evidence locator, not this report's new Library identity.

External primary literature, inspected online:
- Werner Dinkelbach (1967), [On Nonlinear Fractional Programming](https://pubsonline.informs.org/doi/10.1287/mnsc.13.7.492), Management Science 13(7), 492–498. Original parametric/fractional method; native iteration claims are derived conditionally here.
- A. Charnes and W. W. Cooper (1962), [Programming with Linear Fractional Functionals](https://iiif.library.cmu.edu/file/Cooper_box00010_fld00009_bdl0001_doc0001/Cooper_box00010_fld00009_bdl0001_doc0001.pdf), Naval Research Logistics Quarterly 9, 181–186; [publisher DOI](https://onlinelibrary.wiley.com/doi/10.1002/nav.3800090303). Original transformation and bounded regular-case premise.
- Dimitri P. Bertsekas, [Proper Policies in Infinite-State Stochastic Shortest Path Problems](https://arxiv.org/abs/1711.10129), with [author text](https://arxiv.org/html/1711.10129v2). Proper versus unrestricted objective warning; no blanket application of its nonnegative infinite-state theorem to signed economic rewards is claimed.

## Publication verification

Before publication, connected GitHub again resolved remote main to 7252027c80856628ed16734583bfc9d6e166458b. The selected causal local HEAD remained fb59476f54601f81dc18a8bf2457d8eb7d3eb36b. The unique branch name dot/research-20261005-multigoal was absent on the connected remote. Report source and examples were reviewed for private paths, credentials and unrelated personal data; no raw private machine path or large trace is included. The local report is the canonical UTF-8 text copied without line conversion to the isolated documentation worktree and uploaded as one new owned Library report.

The exact resulting branch SHA, final remote-main check, UTF-8 byte/line/hash receipt and Library ID are delivery metadata returned separately. Neither new identity can be embedded before its own creation without a circular commit. The coordinating parent's completed final review explicitly requested the section 9.3 acceptance safeguard; that narrow amendment updates this same report, research branch and owned Library identity while retaining history. No sharing operation is authorized or performed.
