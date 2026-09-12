# What the newly pushed main changes

**Current pin:** `23e03ba9d3fef2f67f2bf1597f69c2c36422e5a7`, direct successor of `0ca76eb4a9f35a90299ecceeab740e5535804fd9`.
**Commit:** Add native dirty-state continuation search and checker headroom.
**Evidence:** pinned source and the final sections of the dirty-state living record, not fresh native runs in this review. Search indexing still returned older commits during the review, so search absence was not used to prove a feature absent.

## 1. Real end-to-end progress

| Case | Matched same-capacity control U | New independently evaluated U | Meaning |
|---|---:|---:|---|
| CB08 Onyx Amulet three | 48,896,520.03536156 | 1,973,847.6160810417 | 95.9632% reduction; also beats original-profile 40,215,428.995558396 |
| CB07 Amethyst Ring four | 3,725,358,919.4984236 | 10,251,920.398984343 | 99.7248% reduction; not the Ring-two request |

The new mode is a **public native opt-in**. The browser qualification preserves its previous default mode, not proof that the new large-resource mode runs identically in the browser. Both native policies have complete independent expected-cost accounting, reported success probability one and zero off-policy mass. Neither has new exact closure. L remains 174.79327321907147 and 201.85524142944053 respectively.

Original-profile qualification reports 16 strict pairs (12 core, two exposed, two wide primaries), no exclusions or reported regressions in the final receipt. Twelve original graphs remain identical, CB03 is cheaper at 536,407.1454418747, and CB09 remains absent. CB12 preserves exact 65.60036144971359. CB05's stop-contract failure remains visible. Those classifications matter more than a generic 'all passed'.

The final profile is **600-second finish / 840-second native watchdog / 870-second outer safeguard**, 8-GiB aggregate solver, 4-GiB candidate and final checker ceilings, 2M checker states, 10M pairs, 40M transitions, 200M logical work and 14-GiB fresh host admission. Ordinary search still has its separately specified 200k limits.

Source: [living record](https://github.com/OliverOrton/poecraft2/blob/23e03ba9d3fef2f67f2bf1597f69c2c36422e5a7/docs/active/2026-09-11-dirty-state-continuation/README.md), especially final v14 and qualification sections.

## 2. Work now DONE

- Actual candidate state/pair/transition/memory controls, including size-guarded native API fields.
- Fresh private action-scoped layout and the conversion-pair diagnostic. CB08 classes **21 to 6**, Chaos support **4160 to 85**; paired native row work/time improved. Do not repeat the 'unmeasured 21-class' hypothesis.
- Full private dirty-controller construction, sparse policy iteration and independently checked nonempty continuation composition.
- Root Exalt/Chaos proposals and a genuine satisfying-tier Essence proposal.
- Paid redraw at actual dirty zero-goal physical states instead of mandatory cleaning back to empty. Virtual retry control still constrains legal actions.
- Exact duplicate redraw-span sharing through existing sparse ownership.
- Earlier certification of an already completed primitive-renewal fallback in dirty mode.
- Cooperative identity-replay materialization instead of a fixed 4096-pair refusal.
- Rank-one preparation of large occupancy solves, followed by checks against the original equations at unchanged tolerance.

The last two were essential. A large checker graph fitting memory did not mean its numerical solve would finish. More time alone stalled; the numerical change allowed both final controllers to be checked. No M4 query-family subsystem or new mathematical authority was needed.

## 3. Current weakness is not simply 'we need cost estimates'

`try_dirty_continuation_candidates()` already constructs and evaluates private controllers and iterates policy selection with full continuation costs. It already defers a proposal whose private estimate looks worse than the verified upper. Those are heuristic comparisons, not global action retirement.

The remaining candidate vocabulary is narrow: root Exalt/Chaos plus one Essence chosen by price per forced satisfying goal; tails primarily Exalt/Annul, paid redraw at zero goals, and Scour on the nonempty branch. A private context registering many native operators does not mean they all receive candidate rows.

The source inspected does not implement the proposed run-local online correction/trust model. But a new predictor cannot choose a protection/cleanup operator that the candidate builder never presents. The new plan therefore couples a small **candidate-coverage extension** to an **actual static-versus-adaptive guide test**, keeping both arms on identical candidate eligibility.

Source: [return/dirty owner](https://github.com/OliverOrton/poecraft2/blob/23e03ba9d3fef2f67f2bf1597f69c2c36422e5a7/engine/src/solver_solve_return_bridge.cpp#L650-L1460).

## 4. A specific economic target

Amulet expects 283,397.630 actions. Annul accounts for about **96.42%** of cost: 1,903,235.026 of 1,973,847.6160810417. Ring-four expects 1,476,470.424 actions; Annul costs 9,881,200.843, about **96.38%** of its 10,251,920.398984343 total.

These are native execution-accounting figures, not search-work percentages. High spend identifies where a better continuation may matter; it does not prove Annul is locally wrong or that removing it will save that amount. A protection action may cost more per visit yet reduce repeated acquisition/loss enough to win. It may also be inapplicable or worse. Use native rows to decide.

At the original 100k-action limit, 1000 trials produced 330 Amulet successes and 55 Ring-four successes. The remaining 670 and 945 trials hit that action cap, with no other execution failures. This is not a contradiction of eventual properness. It is a serious practical limitation and must stay visible. We do not change the objective to risk or silently increase simulation caps to obtain an all-success receipt.

## 5. Important entry-ownership issue for the next implementation

New dirty graphs are retained as **root-only compiled artifacts**; their parent `policy_decision_bindings` are cleared. Their private state IDs do not belong in the parent's state namespace. The existing nonempty selection route is geared toward a prior graph containing a fracture and compatible bindings. Do not assume it directly enumerates the new Annul-heavy dirty graph's entries.

Prefer the already-live child rows/entry observations where suitable, or request exact physical/control entries through the existing native evaluator. A small entry-extraction extension is allowed when needed. Preserve the root-only certificate; independently qualify any new tail. Never repopulate bindings with unrelated parent IDs, copy the root upper into all states, or make a representative stand for a whole class.

## 6. Old experiments not to repeat

- Ring-two's wide baseline already reaches 1,620.317363896656 with the old search. The old gated excursion's inferred 22,890.56 cannot beat it; finishing that unchanged check is not the next task.
- Gated Amulet's old one-shot remained unknown at 600 seconds. A new cheaper Essence dirty controller now exists; do not insist on finishing the old experiment first.
- Bow/Pelt compositions were completed and worse than their actual incumbents. More memory does not improve their fixed values.
- Do not repeat the full layout leave-one-out matrix, old selector/B1/B2/T1/T2 failures, M0, first-policy recovery, or the 4096/retry numerical repairs.

## 7. Tooling status

Current AGENTS and tooling map still lack the explicit no-routine-LLM-polling and deterministic-batching rules requested after the push. Add that bounded amendment. Existing native supervision remains useful; eliminate repeated **model re-entry**, not internal cancellation checks.

One fresh waste example is documented: integer `315` versus float `315.0` in a qualification adapter caused strict metadata exclusion after fourteen runs had already completed. Preflight the resolved typed identity before launching an expensive batch. Preserve raw historical reports and exclusions. Fixing or explicitly deriving a versioned semantic comparison does not require rerunning an unchanged native computation solely to change JSON number spelling.
