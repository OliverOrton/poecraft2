# Research handoff: partial-state knowledge must reach the original root

**Baseline:** `06d4c41d0cef62ec528bf22f25b122de230c913f`.  
**Evidence:** current repository source/contracts plus the newly derived finite examples below. No native engine run or current user-request reproduction was performed.  
**Disposition:** candidate research for Codex to reconcile and integrate. Do not automatically register it as accepted native correspondence. Reuse the existing ledger IDs where the proposition is already present.

## P1 — The user's partial-to-root intuition is correct

For a fixed original final goal, action scope and semantic state space, let `V*(s)` be the optimal remaining cost from `s`. All past spending is sunk. If future permission depends on a checkpoint, observed offer, program position or retry condition, that information is part of `s`.

A valid partial-state lower `L(t) <= V*(t)` contributes through a complete action row:

\[
q_L(s,a)=c(s,a)+\sum_t P(t\mid s,a)L(t)\le Q^*(s,a).
\]

This is just expected continuation cost; no monotone acquisition assumption is required. Destructive successors, retries and setbacks remain ordinary successors. Cycles require the existing simultaneous Bellman/stopping argument rather than an acyclic recipe.

If an action has probability `p` of reaching a partial state whose lower improves by `delta`, its fixed-row expression improves by `p*delta`, with other successor values held fixed. This is not a guarantee about the root minimum: another action can remain cheaper.

**Exact example.** At `s`, pay 1; with probability 1/2 reach `t`, otherwise return to `s`. At `t`, completion costs 12. The root recurrence is

\[
x\le 1+\tfrac12 L(t)+\tfrac12x,
\]

so it permits `x <= 2+L(t)`. Improving the partial lower from 4 to 12 raises this root action bound from 6 to 14. If a competing finish costs 8, the complete root lower stops at 8. Continuing to improve only `t` cannot raise it beyond that competing action.

**Native consequence.** Trace certificate -> exact/class query -> complete predecessor relation -> current minimum -> root proof. A prepared table or high lookup count alone does not show that chain. Do not build a second backward solver if the existing envelope/quotient owner can consume it.

**Existing claims:** CLM-0007/0008/0011/0012; verify exact current statements before adding annotations.

## P2 — The same partial policy can help the root, but scalar stitching is not enough

Suppose a fixed prefix controller reaches a boundary at a stopping time with complete exit probabilities. Each exit is assigned a permitted continuation, with the correct state and control memory. If the combined controller is proper and all costs are finite, its cost is prefix expected cost plus the exit-weighted continuation costs.

In the example above, a verified continuation costing 12 gives a proper root policy costing 14. This is an executable upper if the implementation actually routes every outcome correctly, not merely if the arithmetic is written down.

Two locally terminating options `s -> t` and `t -> s` nevertheless form an improper combined controller when they are the only selected operations. Thus local option termination does not establish global goal absorption. Retrying until a favorable branch also needs all failure probabilities and costs: a program costing 1 with a 0.1 chance of requiring a 50-cost continuation has expected cost 6, not 1.

**Native consequence.** Preserve candidate decisions and their dependency snapshot through restoration, compilation and strict lift. A new state lying outside the saved vector cannot be repaired by copying whatever greedy action currently exists. Distinguish evidence lost from the same candidate from genuinely new evidence.

**Existing claims:** CLM-0002/0003/0004/0021.

## P3 — A construction anchor is not automatically the theorem's query domain

A proof may be constructed using an anchored item but establish a function over a larger domain `D`, including unfractured states. Conversely, a constructor can accept many inputs while establishing only a narrow result. The useful question is what is quantified over by the accepted native relation.

A sufficient transfer argument is: a bounded nonnegative potential is zero at true goals; it satisfies complete native-valid action inequalities on `D`; each exit uses an independently valid lower under the same continuation semantics. Then the proper-policy stopping argument establishes the lower for every covered entry in `D`, independently of which auxiliary anchor helped generate the candidate.

If a native fracture outcome can leave `D`, it must have an explicit safe boundary. A proof restricted to one predetermined fracture policy is not a lower for a caller free to use other fractures or no fracture at all. Also, a numerical estimate at a coupled fresh cell is not sufficient without the complete native bridge.

**Native consequence.** Inspect `PreparedPhasePotential` and the native producer's actual frame. A safe minimal integration may expose already proved unfractured queries; another may prepare a run-local view after an eligible partial state is reached. Neither justifies deleting the constructor's fracture guard without a replacement contract. Reachability is useful for deciding where to spend work, but is not itself the source of lower validity.

**Existing claims:** CLM-0006/0007/0009/0012/0013. The exact native application remains to be established.

## P4 — A smaller goal with a junk-free terminal is not automatically a relaxation

Let the original target require both A and B and permit terminal item `{A,B}`. A naively truncated target requiring only A while forbidding every occupied non-A affix rejects `{A,B}`. Its terminal set is not a superset of the original target's terminal set.

Therefore one cannot conclude that the separately solved 'A only' cost is a lower for 'A and B' by counting fewer requirements. A safe goal relaxation must prove that every original terminal is accepted by the relaxed terminal and that actions/costs/policy restrictions have the appropriate direction. It may need to allow the omitted original goals as nonpenalizing content.

An executable policy that merely reaches A also does not provide a complete upper for A and B. It needs final-goal continuation, with any subsequent destruction accounted for.

**Native consequence.** The next plan uses the original five-goal continuation problem at partial items. A goal-subset model may still be useful for proof or proposal generation, but only with its explicitly established semantics.

**Existing claims:** CLM-0005/0007/0009; a distinct terminal-inclusion claim may be worth adding if not already present. Do not change a historical claim silently.

## P5 — Empty-start applicability and backward consumption are separate engineering obligations

Current source sets `native_retention_attempted` before checking whether the requested start has exactly one fracture. The existing ordinary entry path therefore does not prepare a later query just because that query is a partial state encountered in an empty-start solve.

Current source also has completion-lower lookups and predecessor row expressions. The missing work is to determine which exact preparation/admission, identity, generation or complete-envelope contract prevents those owners from carrying new partial evidence to the root. It is not established that a new global solver is needed.

**Falsification target.** A baseline trace may show no relevant state in the existing donor's domain, or a competing path unaffected by it. In that event, dynamic anchoring alone is the wrong optimization. The plan authorizes a minimal unanchored relation or the actual blocking upper/proof repair, not forcing the craft to visit an anchor.

**Sources:** `solver_solve_bounds.cpp`, `solver_solve_envelope_proof.cpp`, `scheduling-bellman.md`, and the latest campaign at the pinned baseline. See the full implementation plan for portable source URLs.

## Required knowledge receipt

Codex should map P1–P5 to existing arguments, new native correspondence, open obligations or counterexamples. Keep the original report once. Incorporate the useful explanation into the canonical mathematical chapter rather than preserving only implementation instructions. Record actual fixture/native checks separately; the companion exact examples are synthetic and make no performance claim.
