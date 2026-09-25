# Native design and conditional mathematical contracts

## 1. Acceptance is about effective execution

Let G be the original native goal predicate, including rarity, tier/minimum satisfaction and the current clean-explicit-affix rule. Let a candidate graph execute from the exact request root. A sufficient terminal safety condition is that every possible ingress into a Success node is an **executed** guard implying G, evaluated on the terminal item after all preceding effects. A raw JSON field that the parser ignores is not such a guard.

In the current parser a default edge has effective condition Always. Thus raw condition equality does not imply effective guard equality. Repair can remain conservative: forbid default success ingress and require the compiled nondefault condition to represent the original trusted predicate. More permissive implication checking is not required now. Keep mandatory-program termination timing intact; checking a goal before unpaid cleanup is not equivalent to checking afterward.

The initial root requires the same rule. If G(root) is true, a guarded zero-action graph is a valid proper controller with cost zero. Native evaluation still checks the graph. Otherwise zero actions do not establish success.

Complete stochastic acceptance remains separate: correct root/session, admissible program scope, full mass, properness and finite complete original-cost accounting under the existing numerical contract. A prepared graph is not a checked artifact. The generic Strategy Builder's intentional authored-success semantics must not be silently changed by finder-specific validation.

## 2. Grammar bounds search quality

Let Pi be the permitted policy class and Pi_G the policies expressible by a particular controller grammar. Then Pi_G is a subset of Pi. A ranker can change which members of Pi_G are checked; it cannot return a member outside it. Even complete optimization within Pi_G proves no optimality over Pi without further coverage.

This is why improving inference latency or a priority score cannot make the current two-stage language express a conditional protected completion. Syntax-guided synthesis makes the semantic specification and candidate grammar separate explicit inputs. [P1]

### Illustrative progress-retention gap

Consider two required achievements with independent success probabilities p and q. A cost-one attempt that redraws both and only succeeds when both occur has expected cost 1/(p q). In a different permitted controller, two cost-one stage-specific attempts preserve the completed first achievement; the expected cost is 1/p + 1/q. For p=1/10 and q=1/5 these are 50 and 15.

This is an invented control example, not a PoE probability calculation or a claim that arbitrary native goals can be independently preserved. Its purpose is to demonstrate a strategy-structure effect that no reordering of the joint-redraw loop can reproduce.

## 3. Minimal controller representation

Use a finite in-memory graph of immutable native-bound nodes. The ordinary emitted strategy document remains the executable interchange format. Each candidate has an immutable problem binding, a graph/control identity, native operation and predicate bindings, unresolved holes, and lineage. It supplies no authoritative probabilities or values.

A Test uses a native expressible predicate. Relevant early tests include actual goal-family satisfaction, side capacity, rarity and supported persistent/protection state. These are not hardcoded case-name tests. Actual representation and backend observations must preserve any context needed by later operations.

A Run refers to an existing primitive or complete native fixed/automatic program. A program descriptor is not merely a string array: initiation, observable choices, mandatory setup, paid recovery, and exit timing are part of its semantics. The options/SMDP framework supplies the general conceptual distinction, not the native proof for a particular program. [P2]

A native macro adapter should produce compiler-owned expansion provenance and explicit exit ports. The scope checker permits each internal dependency only through that allowed program occurrence. It must refuse moving the dependency outside its mandatory control or promoting it to a standalone action. Preserve independent evaluation of the flattened final graph.

Do not fabricate a legacy SolveResult to invoke the compiler. Do not resurrect the benchmark-only fragment verifier as a public general constructor without matching its exact-entry/exit contract.

## 4. Search ownership and feedback

Maintain separate sets:

- Live unresolved frontier: candidates that may still be expanded.
- Seen identities: exact candidate/problem/control identities used for bounded deduplication.
- Completed candidate receipts: success/failure/censoring of an actual checker invocation.
- Best checked artifact: executable graph plus context and evaluation ownership.

A checked candidate can still be a valid parent for a structural mutation. An unresolved candidate cannot become an executable result. A no-longer-live candidate must not consume every beam slot forever. Any retained identity history has a real memory budget; a visited set is not free.

The search may prune heuristically and may ignore some legal alternatives. Such omissions affect search coverage, not native probability mass or proof scope. Generation of children must not depend on an assumed value at a missing continuation. Counterexamples and typed refusal can guide which branch is refined, but an evaluator exception is not a high-cost label. PAYNT is a precedent for a generator/checker interaction over sketches, not a dependency or a performance promise. [P3]

A ranker comparison over different admitted seed sets measures selection and ordering together. To isolate checking order, freeze candidate identities and graphs in advance and change only their order under the same checking/time/work policy.

## 5. Unequal role bindings

A role-feature view may deliberately alias native states for ranking. Preserve the exact native source and candidate identity separately. A heterogeneous binding changes its actual family members, weights, tiers, applicable programs and costs. It can therefore change the best branch and policy value.

Permutation of goal record order with consistent predicate/index remapping is not the same as replacing Fire with another differently weighted family. No common feature key permits sharing native transitions, selected values, checked artifacts or observed legality. The first implementation uses role-relative construction; no claim of cross-binding computational reuse is required.

## 6. Properness and control memory

For a complete finite proper policy with transient nonterminal matrix Q and expected immediate cost c, V satisfies V=c+QV. A graph with a recurrent positive-input non-goal closed component has no finite absorbing occupancy. A finite entry snapshot is not its occupation vector.

The current shared-row repair preserves unresolved mass for such a component rather than pretending its finite snapshot is a completed flow. Retain that distinction. Do not remove a loop's chance outcomes to get a solvable controller.

Staging is part of semantic state. A controller that executes a preparatory program once and then a continuation may differ from a controller that repeats preparation on every revisit. Represent this difference in nodes/control state. Evaluate recurrent patches as recurrent. A search-node depth bound is not a bound on the returned policy's primitive execution length.

## 7. Memory and acceptance transfer

At every instant include simultaneously owned beam/control data, problem/economy storage, compiled graph(s), checker state, retained incumbent and transfer scratch under the existing budget.

For example, outer ownership 100, old best 20, active graph 30 and checker 40 total 190. Copying the active graph creates a transient 220, even if the final state later shrinks to 130. A cap of 205 is violated during the transfer; observing only the final state would miss it. These are illustrative byte units, not measured native allocations.

Prefer a validated move of graph ownership, or reserve the overlap before allocating. Certificate/context/result fields transfer with the same graph. The new best is adopted only after valid completed checking and compared with the current best, not a stale pre-check cost. Failure and cancellation preserve the old checked artifact where the lifecycle promises it. Releasing scratch does not refund cumulative native work.

Do not compute a purported real peak by adding independently timed historical peaks. Maintain current ownership and audit the changed overlap. Parser/emitter scratch is also real ownership when candidate size grows.

## 8. Specific implementation boundaries

| Boundary | Current file | Recommended change |
|---|---|---|
| External problem gate | solver_finder.cpp / prepare_finder_candidate | Effective compiled success semantics; request-bound macro dependencies |
| Finder control representation | solver_finder.hpp/.cpp | Replace action-list-only sketch with the minimal internal control view and real live frontier |
| Emission | solver_compile.cpp / compile_finder_candidate_json | Native Test/Run/stage graph, trusted terminal guard and one exact program adapter |
| Macro behavior | Existing solver_options_* and native compiler owners | Reuse supported descriptions and actual expansion, not a duplicate mechanics implementation |
| Independent stochastic checking | StrategyEvalWork / existing evaluator | Preserve complete checking and rejection; changes only for a demonstrated missing interface |
| Mode/lifecycle | solver_api.cpp + current web mode path | Preserve peer dispatch, finish/cancel, no proof authority and frozen mode |
| Evidence | Current benchmark/corpus/report owners | Small per-candidate records and declared comparison identities |

All paths under engine/src unless explicitly indicated. New helper names are design proposals, not claims that they already exist.
