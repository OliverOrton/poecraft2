# Review of completed F0–F4 work

## 1. Overall assessment

The repository now has a real peer `PolicyFinderWork`, a C lifecycle branch, shared native compilation/evaluation, a Calculator experimental mode, frozen request mode, and recorded matched comparisons. These are useful retained engineering assets. The finder does not call a hidden legacy `SolveWork` to obtain its seed. Current remains the default. [R1–R7]

The search implemented behind that boundary is substantially narrower than the original F2 design. It enumerates a few one/two-primitive controllers, whose only item-dependent choice is whether the **entire final goal** is satisfied. It does not yet search general progress-conditioned native macro controllers. Calling the vector a beam does not supply those missing choices. [R4–R6; P0]

The F4 results reject the present grammar/selection combination as an economically competitive replacement on the exposed cohort. They do not establish that separating heuristic policy discovery from native checking is a bad direction. Conversely, a promising architecture does not excuse the present poor results.

## 2. Recorded results, not new measurements

Source: the current living record. These are native fixed-eight comparisons using the stated shared executable, original problems/capacities, and a documented Current-only retention-reuse diagnostic. They are not a fresh source-matched four-case Calculator comparison. [R2]

| Case | Current cost | Finder cost | Finder availability |
|---|---:|---:|---|
| A3 | 2,085.015 | 5,387,327.209 | first checked about 2.92 s |
| B3 | 79,273.770 | none | four refused, one censored |
| A4 | 5,218.041 | 13,111,122.495 | first checked about 0.023 s |
| A5 | 85,558.706 | 455,464,336.951 | first checked about 0.045 s |

The primary finder produces five sketches, checks four or five and terminates in roughly 1.0–4.9 seconds, using 2–87 MiB. Its eight-check ceiling and 16-entry vector are not what exhaust these primary workloads. Current uses roughly 240–246 seconds. Early availability of an extremely expensive graph is not a strategic-quality win.

The uninformed/id-order ablation reduces A4 to 1,516,093.480 and A5 to 28,555,986.962; these remain about 291 and 334 times the respective matched Current costs. A3 ties the first finder, and B3 still has no policy. Checker time dominates sub-millisecond scoring **within this short workload**; this does not establish that checker performance currently prevents useful use of a four-minute budget. The generator runs out of ideas first. [R2, R4]

The two ranking arms share a grammar and a check ceiling. In source, ranking also controls which root seeds enter the initial four and which children are admitted before a cutoff. They are not automatically an order-only experiment on an identical concrete candidate multiset. Extract candidate identities before assigning a more specific causal interpretation. [R4]

### Control anomalies must stay visible

* Current A3 has a cost-matched executable graph but fails its corpus bounded-proof expectation on an unnamed `other_resource_cap`. It is not a clean acceptance pass.
* Current A4 returns 5,218.041 with first verification around 244.83 seconds, despite an earlier qualified 3,746.1319409485764 reference. This discrepancy is real in the reported endpoints, but its cause is not established. Check exact request, source, activation, runtime, native work, stop and graph identities before calling it a regression or a profile difference. Do not replace the stronger preservation reference silently.
* The HANDOFF still says F4 changes are dirty and should be committed, while commit `8860cf6` and the current follow-up already contain them. Treat main as the source of commit status; repair this stale instruction when touched. [R1, R2, R8]

## 3. Plan correspondence

| Checkpoint | Delivered | Remaining distinction |
|---|---|---|
| F0 request-bound acceptance | Exact parsed root, requested primitive membership, raw goal-ingress check, priced independent evaluator | Effective-default-edge bypass found below; richer macro scope not implemented |
| F1 independent peer | Standalone native seeds, lifecycle, no hidden legacy solve | Only simple supported primitive patterns; already-complete root is not an actual zero-cost finder seed |
| F2 controller search | Bounded candidate vector, simple pending second-stage choices, synthetic Chaos/Annul recovery | No general tests on partial progress, no native macro block consumer, no multigeneration controller search, almost no goal-role features |
| F3 product mode | Shared form/request, mode dispatch, compact worker, result authority, export, latest mode-freezing test | Existing scope is experimental and narrow; rendered visual review not performed |
| F4 comparison | Honest severe losses, no default promotion, ranking ablation | Candidate identity projections absent from compact record; Current A3/A4 controls need explicit disposition |

The original prompt specifically requested native program blocks, real condition branches, unresolved controller decisions, role/context features, and coordinated acquisition/protection/recovery. The implementation fulfills a small part of that construction objective. The previous plan's synthetic-fixture exit criteria were too permissive to establish the broader search capability. Preserve completed infrastructure and label F2's scope precisely rather than either calling all work failed or calling the entire proposal tested. [P0]

## 4. Why the current candidate set is so small

`PolicyFinderWork` sorts priced registry primitives, seeds Chaos when root-legal, and adds up to four root-legal singleton controllers. It queues a Chaos→Annul recovery and limited Transmute/Alchemy/Regal rarity-setup holes. The main four cases start empty Rare, so the rarity-setup route does not create a broad new family there. [R4]

`compile_finder_candidate_json` rejects more than two actions. It emits only:

```
one action: repeat until the entire native goal is true;
two actions: first, then repeat second until the entire goal is true;
             or alternate first/second on every miss.
```

There is no branch for “A attained but B still absent,” “all requested families present but extra affixes remain,” “protected side acquired,” or “recover after loss of one selected role.” A controller may happen to retain progress as a consequence of its primitive, but it cannot deliberately choose a different next operation based on that progress. [R6]

The `Sketch` is an action-index vector plus a return-topology boolean. A processed sketch remains in the same capped vector, and successful checking does not generate mutations or deeper sketches. `FinderScoreFeatures` contains prices, total goal-slot count and renewal/recovery booleans, not a heterogeneous role-state representation. The current result is closer to a bounded template enumerator than the intended general best-first controller search. [R4, R5]

## 5. High-priority correctness finding: raw decoration versus executable default

`prepare_finder_candidate` checks raw JSON edges to a success node and requires their `condition` to equal the trusted goal JSON. It does not exclude `is_default:true`. [R4]

The ordinary compiler has a different, intentional interpretation: for a default edge, it sets the compiled condition to `Always` and does not compile the supplied condition as a guard. The runtime follows a default as fallback, regardless of that raw condition. [R9]

Therefore a graph with a start node, a success terminal and this single edge passes the raw goal-ingress predicate while its effective routing is unconditional:

```
start → success
is_default: true
condition: <the exact native goal object>
```

An empty non-goal root can still match the requested root, and the primitive-scope loop has no operation to reject. The parser requires a real Start node, so the counterexample uses one; it does not rely on a terminal start ID.

**Evidence level:** the preparation/parser mismatch is source-confirmed. The supplied regression has not been executed against the native engine during this review. Native full acceptance behavior must be checked explicitly. The current trusted finder emitter does not emit default edges to success, and there is no evidence that the existing F4 output graphs exploit this defect.

Nevertheless this violates the stated untrusted-candidate preparation guarantee and must be closed before expanding controller generation. Validate effective compiled routes or a trusted assembler-owned terminal contract, not merely JSON decoration. Keep the general Strategy Builder's default-edge semantics unchanged.

## 6. Two additional lifecycle issues to fix at the boundary

### A. Already-complete root

The current test accepts a hand-authored zero-action graph for a completed root. The actual finder constructor does not install such a seed; its emitter starts by executing a primitive. With no priced seed it can return no policy even though zero actions suffice. Add a real `PolicyFinderWork`/public-mode fixture for an already-complete root, including an empty priced action set. This is a source-level coverage gap, not an executed reproduction here. [R4, R6, R10]

### B. Winning-graph transfer accounting

On acceptance, the finder copies `checking_graph_` to a new accepted string before replacing `best_`. The active checker, parsed graph and prior best can still be live, but `update_peak()` runs after checker/graph release. The visible selected ledger does not capture/reserve that transient copy interval. Current emitted graphs are tiny, so no actual cap overrun is established. Broader graphs make this worth fixing now, by move/ownership transfer or correctly reserved overlap. Do not replace the issue with an indiscriminate sum of noncoincident historical peaks. [R4]

## 7. Other work worth preserving

The closed-component evaluator repair handles improper seeds by retaining unresolved mass instead of treating a finite entry snapshot as recurrent occupancy. The synthetic regression rejects the singleton and accepts the complete paid recovery. This is a valuable rejection path for a proposer that will generate bad programs. It is not permission to skip internal probability mass on a proper controller. [R2, R11]

Mode freezing is tested at the actual Calculator submission boundary, including edits during asynchronous preparation. The last commit is a test-only follow-up, not a new algorithm. [R7]

The experimental lane should stay independent and available for development. Its losses are not a reason to move its heuristic state into ProofStore or hide a call to the legacy solver.

## 8. Recommended decision

Complete the missing conditional-controller capability behind the existing lane after a small hard acceptance gate. Do not start ML, increase beam width as a substitute for vocabulary, optimize sub-millisecond scores, or make full checking approximate. Do not claim a new global optimum from exhaustion of a finite grammar.

Use the next implementation to demonstrate real progress-conditioned branching, one compiler-owned native program family, a deeper generative lineage and request-bound complete evaluation. Then compare useful strategy quality, not just mode plumbing.
