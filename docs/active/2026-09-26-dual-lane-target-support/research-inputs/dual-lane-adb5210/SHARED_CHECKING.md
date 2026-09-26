# Shared request-bound policy checking

## Purpose

A generic strategy evaluator answers a question about the supplied graph's execution. A solver result answers a stronger question: this graph solves the **original resolved request**. Do not confuse the graph's condition-observer basis with that request.

This plan adds/qualifies the smallest explicit connection, not a second interpreter. Current's assertion owner and Finder's acceptance owner remain separate clients of the common native evaluator and target-assessment helpers. [R06–R10]

## Three concepts that must remain distinct

1. **Requested target**: ordered native goal slots, tier/flag requirements actually supported by the request, rarity, any-k threshold, extra-affix policy, count ranges and native data/session.
2. **Observation universe**: every condition that can affect graph execution, including nonterminal tests and count classes. It can contain more distinctions than the target.
3. **Effective graph semantics**: compiled operation contracts, router conditions/default priority, terminal labels, checkpoint/offer/control state and exact execution.

Example: request A; graph tests B to choose an action and later succeeds when A holds. B belongs in the observer basis but not in the objective. An A-plus-B-or-junk terminal is acceptable for R but not necessarily L. An any-two-of-three goal is not an all-three goal merely because all three are observed in the graph.

## Minimal interface contract (design names, not required new framework)

Use or extend an existing owner-local type with these semantics:

```
RequestedPolicyContext
  immutable original root identity/payload
  resolved requested GoalSpec/terminal identity
  native session/artifact identity
  action and allowed-programme scope identity
  frozen prices/economy identity

RequestGoalCheck
  requested / complete / matched / refused / censored status
  same context identity
  success support checked or compiler entailment checked
  invalid-target success mass/witness when available
  unknown support or observer refusal, never silently zero
```

Actual implementation may reuse existing context/provenance records rather than store copies. Charge every retained/transient allocation to its owner. Hashes are indexes, not authority to omit full semantic equality. Do not make a giant global `ResolvedProblem` rewrite a prerequisite.

## Preserve existing trust boundaries

Finder's preparation already checks the original start, trusted original-goal ingress, default-edge behavior, standalone scope and compiler-owned dependency occurrences. Keep these checks. Current already checks paired certification/product graphs and owns compiler evidence. Do not force Current into the Finder's narrowly shaped ingress grammar if its compiler already proves an equivalent guarded route. [R08–R10]

The shared target-match proof can be supplied by either:

- an existing compiler contract, now explicitly tied to the original requested target and effective success routing; or
- complete native success-absorption checking over an observation-complete represented domain.

Prefer the smallest existing proof path that actually covers both consumers. Do not invent broad SMT implication or full physical enumeration merely to accept the current grammar. Unsupported representations refuse. A new broader graph language is not selected.

For absorption checking, map original goal slots into the evaluator's observer coordinates by full native family/group/tier/flag semantics. Keep the original threshold and rarity, not `model.targets.size()` or the evaluator's default rarity. Extra observer slots do not increase the original satisfied count. Verify required membership uniformity on every represented success class. If the class is not uniform, invoke existing refinement/strict evaluation or refuse; one materialized representative is diagnostic only.

If the requested target is not present in the observer universe, either augment it through the existing observation/layout builder with a proved mapping or fail before claiming a checked policy. Do not replace all graph observers with the request. The established eight-target/overlap limits remain explicit unless a separate redesign is authorized.

## Exact shortcuts are part of the audit

The reforge path may factor goal-progress retry/terminal mass, and the evaluator validates particular local gated-route shapes. All of these remain exact only within their declared observer/routing domain. [R07, R12]

U0 compares applicable optimized and full-native paths on:
- an R graph with dirty successful outcomes;
- an L graph that continues past coverage to cleanup;
- additional nonterminal observed families;
- any-k threshold, a different requested rarity and side-count constraints;
- mandatory programme interiors where an intermediate item temporarily satisfies the target but routing does not yet stop.

A matching cheap fixture is necessary, not a proof for arbitrary graphs. Preserve or strengthen the native guards. Fall back to an existing full native path when that guard cannot prove the optimization; label changed performance profiles. Do not drop positive mass, alter the graph's stopping time or create a free reset.

## Success and acceptance

A complete checked executable policy requires:
- faithful original root and permitted scope;
- complete requested target-match evidence;
- complete pricing and finite nonnegative evaluated expected cost;
- proper completion under the current numerical acceptance contract;
- no illegal-action, missing-edge, off-policy, unhandled-stop or unresolved mass beyond that contract;
- complete native programme initiation/exit validation where the compiler granted dependency-only authority.

Do not relabel tolerance-checked floating-point native evaluation as exact rational certification. A fixed-policy cost interval's lower endpoint is not an MDP-optimum lower.

Generic authored-strategy evaluation stays available without a RequestedPolicyContext and continues to report the author's terminals. Such a result must not carry request-checked solver authority. The new request check must not globally reinterpret success nodes in Strategy Builder.

## Reuse, lifecycle and both clients

Bind cached evaluations and retained graphs to the requested terminal, exact root, scope, prices and data. A byte-identical graph checked for R is not automatically a checked solution for L. A graph can of course be checked again against a compatible stronger target, or carry an independently justified implication certificate; no such automatic transfer is selected here.

Current's `reuse_compiled_policy_assertion_evaluation` and any separate retained candidate path must not bypass target-match evidence. Preserve its paired-default validation and earlier failure clearing. Finder must keep graph/control object/entry validation together, including across suspension.

Admission budgets include parent graph/calculator, candidate graph/control/provenance, parsed strategy, observer mapping, evaluator and check result. On Finish/cancel, keep the earlier compatible winner, release speculative work through existing owners, and never return a candidate whose target check was interrupted.

## Required negatives

False success; default-edge decorated success; wrong root; weakened tier; wrong rarity; wrong any-k threshold; wrong extras policy; wrong side count; unobserved requested family; extra nonterminal observer confused with goal; raw JSON forged programme permission; price/scope mismatch; missing or capped target support; stale goal identity; and root-complete/zero-cost paths. Tests exercise Current and Finder adoption, not just a detached helper.

Source labels refer to the pinned [source index](SOURCES.md).
