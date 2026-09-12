# Research and mathematical handoff

## Evidence status

Remote main remains `0ca76eb4a9f35a90299ecceeab740e5535804fd9`. This research used pinned source and retained records; it did not run native solves/tests or inspect a local Codex session. The included rational checks are new synthetic work, not PoE performance evidence. The dirty-state/coarsening hypotheses were not measured by this session.

The previous notes explicitly deferred adaptive estimates. The currently inspected joint seed is fixed progress/pending-route ordering; current first-return improvement is a narrow independently checked Exalt proposal. Existing provisional/coarse estimates do not establish that the requested online estimated-completion experiment has already been done.

## 1. The useful architectural split

For the same semantic entry s and target/control scope:

    L(s) <= V*(s) <= J_pi(s)

when L is certified and pi is a proper, fully priced controller from that entry. An adaptive V_hat is outside this inequality unless independently certified. It can nevertheless propose a very useful action. Moving V_hat up/down according to evidence does not require changing L or an accepted policy.

For a completed action without post-action choices, the guide may use

    Q_hat(s,a) = c(s,a) + sum_t P(t|s,a) V_hat(t).

With native observed offers, the correct allowed minimum is inside the expectation only at that observation point. An action chosen before a random outcome cannot be chosen afterward in the estimate's proposed emitted controller. Estimates may use a deliberately approximate model, but the native action choices and final verification are never that model.

A candidate can be found using a private subset of permitted actions. It still needs full selected support to form a policy, and its restricted optimum is not a full-scope lower. Keep every original unresolved action obligation. This preserves the useful distinction from the previous coarsening report without requiring a coarser layout first.

## 2. Why estimated guidance is timely but not magic

The project now reliably retains first proper policies on the target Ring/Amulet cases. That creates an executable fallback while a more aggressive guide tries risky or dirtier continuations. The original whole-program numerical seed already solves the earlier mutual-retry toy; another generic reachability solver is not the selected missing mechanism.

The current first-policy debt for distinct goals is D=max(1,m+k-2g) at nonterminal correct-rarity items. A four-affix two-goal item ties an empty two-goal request even when both targets were acquired. This can be a good 'find something cheap to verify' preference while being a poor economic model. The native counterfactual still needs measurement; the static first-policy rule alone does not prove why a later upper pass misses a cheaper strategy.

The synthetic dirty route here pays 10 to acquire goals with junk, then 1 for cleanup; cleanup succeeds with probability 3/4 and loses the goals/returns to the start otherwise. Its complete expected cost is 44/3, not 11, and beats a direct clean route costing 100. It intentionally keeps destructive failures. Acquiring junk can be economically useful even when immediate debt does not fall.

## 3. Why a scalar multiplier is not enough

An estimate h'=w h may preserve almost the same state ordering when the missing difficulty is common to all states. The actual missing information may be the chance of preserving progress, which cleanup route is available, an exclusion/filter effect, or the completion coverage of a particular candidate. The pilot uses native expected successor estimates and local correction, not simply 'inflate the public lower'.

Two hypothetical alternatives have:
- A: certified lower100, estimate20,000, true cost800;
- B: certified lower150, estimate1,500, true cost1,400.

Trying B first is sensible. Permanently deleting A is wrong. After B is checked, an alternative with certified lower2,000 can be retired; A cannot. The guide can be wrong without invalidating L or the incumbent, provided it only controls revisitable work.

## 4. Learning targets and censoring

A completed-row backup with the current surrogate tails is an inexpensive pseudo-target. It is not optimal ground truth when tails are estimated or some actions are uncomputed. Keep the target's row/action/evidence generation, observed-choice semantics and estimate version.

An independently evaluated candidate gives J_pi, not V*. A simple predictor can learn its own error for that candidate family/entry and reduce its trust after optimistic misses. It must not assume a bad existing policy proves all other policies expensive. Similarly, a cap is evidence about computational requirements or missing coverage, not an enormous crafting-cost sample.

For a repeat with cost1 and success probability1/1000, true expected cost is1000. Truncating every rollout at10 actions and pretending termination gives expected observed spend below10. A sample containing no success does not establish success probability zero. Use explicit censoring or a tagged tail bootstrap, preserve rare outcomes in native verification, and avoid sampling millions of useless full crafts merely to initialize an estimate.

No statistical confidence or uniform approximation guarantee follows from a point estimate, an EWMA or a small calibration table. Prediction uncertainty controls exploration/effort, not mathematical retirement.

## 5. A useful exact identity behind local error correction

Let a fixed finite proper policy have nonterminal transition matrix P, nonnegative cost c and zero goal value. Its true value satisfies `(I-P)J=c`. For an arbitrary finite v, put e=c+Pv-v. Algebra gives

    J-v = (I-P)^(-1)e.

The inverse is shorthand for a sparse linear solve. Because `(I-P)^(-1)=sum_{k>=0}P^k`, its entries are expected transient visit counts. If |e|<=epsilon entrywise and tau=(I-P)^(-1)1, then

    |J(s)-v(s)| <= epsilon * tau(s).

This proves a property of a fixed proper controller and an actual residual. It does not certify an estimated model whose missing tails, controller or native state abstraction are unknown. As guidance, a local error correction can use an approximate visit/return count, but it remains a prediction.

The implication for this project is concrete: a near-one retry probability can magnify a tiny one-step error or gain. A model should not look 'accurate' merely because its local residual is small. Also, fitting one global error multiplier across unrelated pool/control families is unlikely to supply the missing structural information.

## 6. One-shot versus repeated policy ranking

For one complete excursion returning to the exact entry with probability q and otherwise reaching the true goal, let r be its expected cost. With old continuation value J:

    Q_once = r + q J,
    J_repeat = r/(1-q),
    J-J_repeat = (J-Q_once)/(1-q).

The repeated expression requires q<1 and a complete transient excursion. A return STOP is an internal interface, not a published crafting success. Unknown exits or other boundaries cannot be normalized into q.

Synthetic example, old J=1000:
- A: r=1,q=.99 => Q_once991, J_repeat100.
- B: r=100,q=.8 => Q_once900, J_repeat500.

One-shot gain ranks B first; repeated economic value ranks A first. Both are proper. This is directly relevant after the native Ring's small one-shot gain amplified into its retained149,977 result. A guidance mechanism estimating only immediate c/progress or one-step advantage can miss the better recurrent controller.

For several actual continuation entries B, a fixed local controller has response g+H v_B after eliminating its transient internal block. If entries can return to one another, solve their complete system and check properness. Do not force a scalar empty return or build a permanent basin library without an actual multi-entry consumer.

## 7. Error checks and precise nonclaims

The included script checks34 named synthetic cases/identities. It confirms the illustrations, source-score arithmetic for distinct-goal examples, correct full-mass expectations, changed-policy identity, censoring, and a simple ordinary-service reserve. It is not a proof of native policy improvement, fairness of the actual scheduler, no-poll behaviour of Codex, or a convergence theorem for the proposed learner.

A practical learner can be defeated by biased completed-row coverage, insufficient useful dirty examples, selection bias toward cheap-to-check policies, a expensive native evaluator, or a guidance overhead larger than the saving. S versus A at equal resources tests whether adaptation itself pays. A new cost scalar without changed candidate delivery is not success.

## 8. Research used and boundaries of transfer

**Thayer, Dionne and Ruml (ICAPS2011), Learning Inadmissible Heuristics During Search.** The inspected learning/one-step-error sections and their PDF pages2–3 concern online guidance and correction using search experience. They assume deterministic search-child relations in that derivation; do not replace a stochastic row by its best outcome, or copy their correction denominator as an SSP theorem. Readability/performance of a learned heuristic is an empirical issue, not just accuracy on labels. PDF text was available; web screenshots failed, so no chart/table-derived claim is used here.

**Thayer and Ruml (IJCAI2011), Bounded Suboptimal Search: A Direct Approach Using Inadmissible Estimates.** The official abstract separates cost-quality certification from potentially inadmissible search ordering and distinguishes solution length from cost. Its graph-search bound is not imported into this cyclic native SSP. Abstract only for this report.

**Hansson and Wahlberg (May2026 preprint), Performance Bounds for Rollout Policies in Stochastic Shortest Path Problems.** The inspected HTML assumptions, performance-difference derivation and Theorem2 condition a bound on a proper policy and uniform surrogate error; expected hitting time amplifies that error. We have neither a uniform error bound to V* nor a general transience guarantee for arbitrary proposals, so the theorem does not certify the new guide. It reinforces the need for native acceptance and horizon-aware evaluation, not another theorem-based product claim.

**Official Codex configuration and source.** Empty-input terminal waits have a configurable maximum and are distinct from initial interactive yields and code-mode outer waits. Repository worker polling is deterministic. The exact installed client controls and actual parent-model usage remain unobserved here. Use the short local capability probe instead of assuming current upstream source solves the user's installation.

Portable URLs and the retrieved source scope are in `sources.md`. No novelty claim, external PoE-mechanic research, or new numerical authority is introduced.
