# Current mathematical handoff: useful estimation without false authority

This supplements the earlier argument retained unchanged under `supporting/mathematical_handoff.md`. That older document is research context at 0ca7, not the current implementation state. Here the private dirty-policy evaluator and cost-improvement loop already exist. The next consumer is choosing additional action/continuation work, not replacing those equations.

## 1. Availability precedes ranking

Let A(s) be the caller's allowed actions and C(s) the actions whose candidates the private builder presents. Minimizing even a perfect score over C(s) cannot select an element of A(s)\C(s). A better controller in the larger set is irrelevant to the ranking until its candidate, complete setup/cleanup and legal observations are actually represented.

A restricted proper policy can be a full-request upper after native checking, because it still uses allowed actions. Its optimized value cannot be promoted to a full-request lower. Expanding private candidates can also add observer distinctions and make the former layout insufficient. Neither legality nor representation equality follows from a promising estimate.

Current source supplies a concrete example: registered native action vocabulary is broader than the tail row builder's Exalt/Annul/conditional-Scour choices. This is the most important refinement to the previous estimate plan. It is not proof that an omitted protection action is applicable or cheap.

## 2. Fixed-controller value, pseudo-target and optimum

For a fixed finite proper controller pi with non-goal transition matrix P_pi and nonnegative one-step cost c_pi,

    J_pi = c_pi + P_pi J_pi.

The private coarse calculation approximates that particular controller on its private model. A native independent evaluation qualifies its actual emitted graph. Both differ from V*, the optimum over the whole original scope.

For an action with a complete native distribution, an estimated first-action cost is

    Q_hat(s,a) = c(s,a) + E[V_hat(S')].

The expectation uses all outcomes and the legal information order. An estimated frontier is allowed only in guidance. A fixed-policy checkpoint can label prediction error for the same policy and entry; it does not establish the optimal-value label. A minimum over only completed actions is a restricted pseudo-target. A timeout supplies no observed crafting cost at all.

Static-versus-adaptive ablation must retain common underlying numerical progress. 'Static' means learned correction/trust is off, not that newly completed real evidence is hidden from it.

## 3. The exact policy-difference identity and a useful trap

Assume both old pi and new pi' are finite and proper on all states reachable under pi', or valid compatible boundaries close the equations. Let

    A_pi,pi' = c_pi' + P_pi' J_pi - J_pi.

Subtract the fixed-policy equations:

    (I-P_pi') (J_pi' - J_pi) = A_pi,pi'.

Therefore, at root distribution mu,

    mu^T(J_pi' - J_pi) = d_pi'^T A_pi,pi',
    d_pi'^T = mu^T(I-P_pi')^(-1).

This uses the NEW policy's occupancy. Old occupancy d_pi can rank an experiment, but substituting it does not generally give exact savings. No dense inverse is needed; the formula describes sparse solves.

Synthetic single-state example (not PoE): old action costs 1 and returns with probability .99, so J_old=100. New action costs 2 and returns with probability .9, so J_new=20. The one-step advantage against the old tail is 2+.9*100-100=-8. Old expected visits 100 would predict -800, which is wrong. New expected visits 10 give -80, the actual improvement.

This matters when ranking Annul-heavy entries. High occupancy identifies leverage, but the proposal changes repeat frequencies and lost-goal recovery. A root-only certificate also need not supply J_pi at newly reached states. Missing values invalidate the exact formula as a native certificate; an approximate version may remain guidance with its uncertainty explicit.

## 4. Costly cleanup can rationally be replaced by a more expensive step

Another synthetic model: a cleanup attempt costs 1; succeeds with probability .01; otherwise returns to the same decision. Its expected cost is 100. A legal protection/cleanup alternative costing 2 and succeeding with probability .1 costs 20. The more expensive immediate step is cheaper overall. Conversely a price-20 alternative with the same .1 success would cost 200 and lose.

This does not assert any particular PoE protection mechanic. Native applicability, preserved affixes, weights, setup/removal, observations and failure recovery determine the actual law. It also does not imply that 96% Annul spend is removable; the alternative can change the distribution of visits and all other costs.

## 5. Improvement and deferred validation remain separate

A coarse candidate exceeding U is currently deferred, not proved inferior. Even after adaptive correction, a finite prediction cannot retire it permanently. Preserve an uncertainty/age route and revisit only on meaningful changed evidence, not every step.

A sampled or truncated return trajectory cannot replace the complete law. Rare retries amplify errors. For a proper finite policy,

    J_pi - v = (I-P_pi)^(-1) (c_pi + P_pi v - v).

Small residuals may coexist with large value errors when expected absorption time is large. The new rank-one occupancy preparation addresses numerical work under original acceptance; it is not permission to claim a uniform estimator error bound.

A first-return expression r/p is valid as a policy value only with complete cost, return/goal mass, and the corresponding proper repeated controller. One-shot r+(1-p)J_old and repeated r/p rank different policies. Existing native graph checking remains decisive.

## 6. Tail interpretation

The reported native policies are eventually proper but have mean execution counts far above 100k. Finite-action truncation is a practical outcome, not an illegal edge or proof of nontermination. Keep all stopped trials. Their spent-to-cap average is not an estimate of the unlimited expected cost.

Keep expected Chaos as the objective. Report expected actions and original-limit completion alongside it. Any future risk/constrained-action objective needs an explicit product decision and new target identity; this programme does not silently choose it.

## 7. Evidence and research status

Current native correspondence is supported by the pinned return/dirty owner, resource contract and completed living record in `source_register.md`. The policy-difference algebra above is a direct derivation under the stated finite properness assumptions. It is not a novelty claim or a performance theorem for the proposed estimator.

The preceding package's 34 synthetic checks and references are retained in `supporting/`. This revision adds a small rational check of action availability, occupancy weighting and source scope. Passing it is not native correctness or evidence that adaptive learning improves the real cases. The required evidence is a changed useful decision and an independently verified matched outcome.
