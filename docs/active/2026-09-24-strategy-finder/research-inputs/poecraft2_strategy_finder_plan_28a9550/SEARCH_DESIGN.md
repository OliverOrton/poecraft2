# Search selection: controller first, lazy native construction

## 1. Recommendation and alternatives

The initial algorithm is **bounded best-first/beam search over finite native controller sketches**, guided by handcrafted scores and evaluator feedback. It is probabilistic program/policy search, with optional local AND/OR reasoning over selected native outcomes. The unit in the beam is a candidate decision structure or partial completion, not a favorable physical trajectory.

This is a recommended initial formulation, not an assertion that it is the globally best algorithm. The implementer must first make one generated root controller executable through the actual compiler; if the smallest native representation cannot express the proposed structures, name that exact limitation instead of implementing a text-only sketch library.

| Alternative | Strength for this project | Reason it is not the initial default |
|---|---|---|
| LAO*/RTDP/BRTDP-style state search | natural cyclic SSP reasoning, bound-guided focus | much of this already exists; starting another explicit all-alternative graph could reproduce the current cost/representation barrier |
| Controller-sketch best-first search | chooses coordinated setup/recovery structure; fixed-policy checker already exists; naturally accepts approximate ranking | grammar can be too narrow and checking expensive; requires genuine native compiler consumer |
| MCTS/PUCT over macros | generative-model use, adaptive search, flexible policy/value guidance | primitive long-horizon rare events are difficult; raw trajectories do not supply an executable complete controller; macro-MCTS remains a later matched challenger |
| Evolutionary/program search | changes topology and crosses local minima | expensive invalid/near-duplicate candidates; structured small mutations are useful within a later portfolio, not a second engine now |
| Offline RL / learned Q | amortized guidance across tasks | suitable strategic labels and deployment distribution are not yet established; one-step data are not Q* labels |
| Expert iteration | combines planner-generated strategies with better amortized guidance | useful second phase once the planner emits sufficiently diverse checked decisions and failures |

PAYNT is primary precedent for sketch-based probabilistic synthesis with checking; its finite design-family/quotient machinery and performance do not transfer automatically. BRTDP is precedent for useful lower/upper guidance; its bound premises are not supplied by a heuristic beam. UCT-style finite-horizon/discounted guarantees are not an undiscounted crafting-SSP certificate. [P01–P05]

## 2. Why policy-first, rather than another action filter

A complete finite conditional program can cover many concrete outcomes through native predicates and loops. Search may propose that program without first optimizing every competing action at every successor. The evaluator then expands/refines the reachable stochastic product of the SELECTED program.

This does not guarantee a small checked graph. Candidate verification can still be the limiting cost. It does move the requirement for full stochastic coverage to the policy acceptance boundary rather than require exact optimization of all alternatives before trying the candidate.

The new lane is not merely the old solver with proof disabled, and not the last s116 repair with a larger shortlist. It must construct from the original root, can retain unsuccessful partial structures, can combine complementary decisions, and may heuristically discard proposals without closing proof obligations.

## 3. Bootstrap is a real first milestone

A standalone finder cannot require an incumbent first produced by a hidden full legacy solve. Generate initial complete control structures from the current native grammar and original request.

Preferred seed adapter: existing native renewal/constructive policy machinery, including a roll-until-native-goal controller where its initiation, complete law, paid resources and properness qualify. A compact ordinary graph can express this without inventing a success probability. Native feasibility alone is not its certificate. A request with no compatible seed should return no checked policy, not use a free restart or a historical answer.

Extract the relevant construction/condition/program-emission helper if it is trapped in legacy Impl; do not instantiate the entire proof solver to reach it. It is acceptable for F1 to support a clearly declared initial subset of requests, proven by two real root fixtures. Do not pretend unsupported starts/actions are in the finder coverage.

Saved graphs are allowed for acceptance regression tests and for an explicitly labelled seeded-repair arm. They are NOT counted as finder discovery. A later public portfolio can combine lanes, but seed time/resources must be charged and its mode named.

## 4. Initial controller grammar

Use a finite proposal grammar built from native capabilities, not case names or saved node IDs. The conceptual constructs are:

- **Native program block:** existing legal primitive or macro binding with native paid setup/internal control/cleanup.
- **Native condition branch:** supported goal-role status, side occupancy or context predicate; all branches/default behavior are explicit.
- **Sequence / staged follow-through:** a small number of program decisions with stage control represented in the graph.
- **Retry / recovery:** a genuine native loop or complete return to a supported continuation; no free reset.

Do not expose a pseudo-action “obtain goal X” with invented probability. A role-bound target only selects a native program or condition specification.

The first search must include at least one coordinated change: e.g. change an acquisition choice AND its paid protection/cleanup/follow-through, or retain a native progress region and complete it differently. A single source-item patch alone does not test this architecture. Existing pair-stop additions previously collapsed to the same kernels; adding more stopping labels alone is not a sufficient pilot.

Keep grammar topology finite and explicit. Macro IDs and original native goal-slot references resolve per request. Candidate-specific conditions may deliberately group different physical states; only native construction/checking can establish legality and complete execution over the reached group. An invalid member yields a counterexample/refusal, not class-wide success inferred from a representative.

## 5. Search controls and scoring

Suggested calibration starting point (not a theorem or hidden product budget): beam/frontier width 16, at most 8 new complete candidate checks per 240-second invocation, one live checker, and a small configurable outer control-node budget. Freeze the actual outer-node default after the first compiler fixture shows how macros are represented. Native compiled-node/edge/output and all existing work/memory caps remain authoritative. These knobs can be changed on development cases with explicit treatment identity; do not run an automatic grid.

A descriptor-stage score can combine native action costs, compatible goal roles, protected progress, available capacity, estimated progress/loss, recovery structure, current known continuation and expected checking complexity. The weights are heuristic. Do not choose only immediate progress actions: a paid blocker or capacity preparation can enable a much cheaper recurring controller.

Use at least two semantic search biases in one small frontier, such as low acquisition cost and progress preservation/recovery. This is diversity inside one search algorithm, not two simultaneously running solvers. Maintain deterministic tie-breaking. Beam eviction is a heuristic omission. No token called `retired` or `proved_bad` is emitted from it.

Model/score interface: batch feature views + candidate IDs → finite untrusted ranking values, optionally uncertainty. NaN/invalid results use a deterministic fallback ranking and diagnostic reason. This interface cannot construct a checked artifact. No model is trained initially.

Feature availability matters: raw native descriptors can rank before expansion; exact progress probabilities computed by a huge row cannot be presented as cheap pre-expansion features. Charge them to post-expansion scoring when actually computed.

## 6. Concrete algorithm skeleton

```text
resolve immutable original problem and finder treatment
create peer finder state and original shared budget
obtain native generated seed sketches; no saved-answer import
best_checked = none
frontier = bounded diversity-aware queue of seed/partial controller specs
check_queue = bounded queue; at most one active native checker

while budget allows and neither Finish nor Cancel is requested:
    service the active checker cooperatively, if present
    if its result is ready:
        complete → compare actual original-root cost; retain the best bundle
        semantic failure → record exact counterexample and context for repair
        resource stop → mark censored; preserve best; release owned scratch
        record label, stage/work costs and immutable identities

    select a frontier item with an unfinished expansion cursor
    if it is structurally complete and not already queued/checked:
        assemble through the native request-bound compiler
        apply cheap request/grammar/shape checks
        enqueue it for native checking, including the first complete seed
    if the same candidate/context already has a valid receipt:
        reuse that receipt for scoring; do not repeat its native check
        still service any distinct, unfinished structural-expansion obligation

    enumerate a bounded batch of native-supported hole bindings / structured edits
    rank cheap descriptions before costly native materialization
    retain selected partial or complete children in the bounded frontier
    do not demand that each partial edit independently improves the root
    record the expansion cursor so unchanged work does not run again

    if the checker is idle and a complete candidate is queued:
        start its complete request-bound native check
    if frontier, checking queue and active checker are all empty:
        stop as search-exhausted-in-this-grammar, not optimality-proved

Finish: stop speculation; deliver only best_checked or truthful unavailable
Cancel: release actual children/contexts; return cancelled under existing rules
```

A complete candidate can contain a failure default that the checker proves unreachable. A reached failure default prevents acceptance. Search must not delete or renormalize rare bad outcomes to make the candidate look complete.

For local lookahead, a frontier may use a verified compatible continuation or a heuristic score. Only the former supplies executable tail evidence. The latter is useful for ranking an unfinished proposal, not publishing it. A candidate may acquire new states not covered by the old controller; it must define their behavior itself or be rejected/deferred.

## 7. Exactness that remains useful inside the finder

Keep cheap native legality, scoped bounds and exact selected rows where they reduce uncertainty economically. A valid full-candidate lower can reject an expensive candidate. An entry lower cannot be compared directly with an unrelated root upper. A candidate upper greater than the incumbent does not prove the candidate cannot improve after further optimization.

Heuristic or learned pruning is explicitly permitted in the finder and needs no full-scope fairness theorem. Search exhaustion under this grammar/beam is not optimality. Approximate statistics and native complete laws remain separately typed. A learned value is not an automatic transposition-table value for every physical state sharing its features.

## 8. Falsifiers and allowed pivots

Stop the current algorithm treatment when it cannot construct a new complete root controller, every useful grammar choice reproduces old generated controllers, verification of all viable candidates hits the existing budget, or held-out quality is persistently worse with no compensating anytime benefit. Report which boundary failed.

A bounded pivot to macro-level MCTS or a different structured mutation ordering is a subsequent experiment using the same interfaces, not permission to build all search engines in F0–F4. Full symbolic symmetry, response caching or more exact lower work is not an automatic response to poor heuristic quality.

## 9. First implementation handoff details

The algorithm skeleton describes responsibilities, not concurrent threads. Native checks and search batches advance serially within the existing bounded-step contract. When no checked policy exists, service a complete generated seed before spending the whole budget on speculative edits. Keep candidate checking status separate from expansion status: a checked candidate may still be a useful parent, while a fully expanded duplicate should not generate the same children again.

Choose one actual native two-phase fixture before expanding the grammar. Record its original root, native program specifications, expressible branch predicates, explicit recovery edges, and all terminal rules in the living record. The corresponding native graph is the acceptance test for the proposed interface. A diagram or pseudocode sketch without that emitted graph does not pass F1/F2.
