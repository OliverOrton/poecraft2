# Mathematical contracts for the selected investigation

These are conditional arguments and exact illustrative examples, not claims of new theorems, native formal verification or measured speedups. The engine's existing numerical reconciliation and finite proper-policy checks remain unchanged.

## 1. Search computation and controller execution are separate systems

Fix a crafting target theta, including native start, goal, data/laws, action/program/observation scope and original prices. For a complete permitted proper controller pi, its exact cost is J_theta(pi). The solver computes a succession of artifacts. At a computational prefix n, let E_n be the compatible **owned** completely checked artifacts. Its available witness value is

    U(n) = min { J_theta(pi) : pi in E_n }.

The feasible-witness inequality V*_theta <= U(n) is independent of why computation stopped. It requires the graph/certificate/context/value bundle, not a historical scalar. Before E_n is nonempty the available upper is unknown/infinite, not the current source estimate. Private root authority does not extend to arbitrary statewise lower/upper claims.

Different execution policies P can both be sound while exposing different E_n. If a common logical trace has truly been established, write U_P(t)=u(n_P(t)): quality along that trace and progress along it are different. Without common trace correspondence, use u_P rather than claiming all runtime differences are throughput. Neither completed-row counts nor equal candidate hashes alone provide that correspondence.

## 2. Sufficient grouping invariance and its limitations

Let tau be a deterministic logical transition on full work state W (including frozen inputs, selection state, partial cursors, evidence and resource debit). A public step with ceiling q is grouping-only if it returns tau applied m(q,W) times, with 1 <= m <= q except a legal terminal/control return, and if public call boundaries do not alter future tau transitions. Then any completed concatenation with the same actual logical prefix yields the same W, by induction on actual tau applications.

The assumptions exclude boundary-triggered reinitialization, different scheduling choices, wall-adaptive internal policy choices, observation side effects and different arithmetic ordering. They do not eliminate intentional Finish/cancellation or resource-boundary effects. Requested q is a ceiling; summing q is not summing actual work. Identical invariants at suspension prove safety, not common trace or equal quality by a wall deadline.

A toy counterexample: building eight required rows costs eight work units, and every host return schedules one extra selection pass. q=1 incurs eight passes, q=8 one. If passes consume budget or alter selected work, regrouping changes the resulting trace even if every individual operation is correct. This is an illustration; the current native call tree must establish whether any analogous decision exists.

## 3. Timing at sampled milestones

Let t_N(r) and t_W(r) be separately observed times at a row milestone. A within-run subtraction t(r2)-t(r1) is well-defined for that clock. Comparing those durations does not require subtracting absolute clocks across processes. However, equal r does not imply equal action/row semantics, candidate state or other work. Thus the C0 interval difference is a locator for a causal experiment, not an identified removable cost. Sampling delays and missing earlier observations remain visible.

Separate T_supervisor from T_child/process and cleanup. In general:

    T_supervisor = T_admission + T_process_and_watchdog + T_cleanup + T_persistence

for a genuinely non-overlapping partition. Native nested timers cannot be summed as independent components. Forwarding a requested deadline correctly does not prove the child was killed/reaped; an adapter-forced status string does not prove `timed_out` or absence of survivors. Conversely, total elapsed exceeding a deadline need not imply the child's timed wait exceeded it. Real evidence must decide which contract failed.

## 4. One controller, one complete stopped-program law

For a selected macro-program, stop at its legitimate control exits, not arbitrary primitive interiors. Retain joint exit probabilities, expected paid costs and primitive rewards through the stopping point. For a one-anchor proper renewal with one-attempt expected cost a and return probability r<1,

    J = a + r J = a / (1-r).

In the native focused product-Fracture example, p=1/4 succeeds each attempt. Its complete expected bill is 4×10 + 3×3 + 3×2 + 3×1 = 58. Miss states inside mandatory paid replacement are not external continuation obligations, but their probabilities, costs, actions and required legal recovery do not disappear. A missing replacement continuation must refuse rather than normalize hits. Multiple return contexts need the existing vector equations, not this scalar shortcut.

The repaired source uses an already-owned product kernel. The proof does not authorize a new action, a free restart, a bigger snapshot or copying an upper between physical states.

## 5. Optional first-disagreement attribution for two fixed controllers

This is a useful corollary of the canonical first-exit equations, not a new evaluation system selected for implementation.

Consider two complete proper finite controllers A and B, including all relevant physical item and controller memory. On a common prefix domain C they apply the same native transition/reward law under a valid alignment. Couple that common execution and stop at a true terminal or the first legitimate point D where their decisions/observations diverge. Let mu(d) be the **first-entry subprobability** of such a boundary; mass that reaches the goal first contributes zero tail difference. With finite expected costs and compatible tails:

    J_A(root) - J_B(root)
      = sum_d mu(d) [ V_A(x_d,k_A) - V_B(x_d,k_B) ].

Proof: each cost is the same expected prefix cost plus its first-exit-weighted tail. Subtraction cancels the prefix. For cyclic C, the finite proper common-prefix process supplies its first-exit law through the existing stopped-chain equations. This does not sum repeated root visit exposure; mu is not the occupancy of D under one completed controller. Misaligned memory or a mandatory-program interior invalidates a purported free decision substitution. Unknown/nonuniform tails remain unknown.

The equation can explain an economic difference after actual comparable graphs exist. It does not identify how their discovery should be scheduled, certify every item in a coarse class, or justify building a potentially huge product graph merely for attribution. This programme selects simple native stage/row comparison first.

## 6. Proposal rejection versus proof

If a candidate is completely checked at C6147.83, it is worse than the compatible C5218.04 incumbent under the same original-cost objective. That rejects this candidate, not every continuation family using one of its actions. An upper estimate of 20 for a true cost-six candidate cannot justify rejecting it against a cost-ten incumbent; the earlier warning remains mathematically valid but lacks a useful counterexample in this programme's completed economic tests.

More action rows do not imply a cheaper u_P: the rejected 137895-row variant supplies actual evidence. Keeping alternatives deferred is different from proving they cannot improve. The 2026 CG-iLAO* source is relevant background for this distinction, not authorization to substitute its explicit finite positive-cost model for native implicit programmes, optional zero-cost routing or the existing proof envelope.

## 7. Native correspondence obligations after a change

Every complete row uses the actual native law and observation timing. Every candidate owns or compatibly borrows complete positive-mass tails, and final acceptance checks original-root properness, complete price, native goal and all failure/off-policy categories. Any newly exposed work boundary preserves staged versus committed evidence, lifetime and cumulative debit. Finite fixed-policy evaluations are not full-scope optimum certificates. The known lower must stay tied to its actual domain, especially after the recent Veiled counterexample.

The offline script tests limited models illustrating these statements. It does not prove all native consumers obey them.
