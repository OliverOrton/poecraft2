# Mathematics of the solver

**Authored September 6, 2026; integrated against `215654f`.** The original research reviewed `f3e7c0f`. Claim IDs are registered in the local ledger; status follows each history. Native/source obligations remain explicit.


This is a reference, not a startup checklist. A local nonsemantic fix does not require reading it. For a correctness change, follow the relevant argument far enough to establish its premises; links are available evidence, not recursive homework.

## Read by question

| Question | Start here |
|---|---|
| What optimum are we trying to certify? | [The mathematical model](../mathematical-model.md) |
| When may carriers or states share a value? | [Representations](representations.md) |
| Why does a compiled cyclic strategy supply an upper? | [Policies](policies.md) |
| What makes a lower valid despite an unexplored frontier? | [Lower bounds](lower-bounds.md) |
| What can be pruned or resumed safely? | [Search and resumption](search-and-resumption.md) |
| What do numerical checks and exact closure establish? | [Numerical closure](numerical-closure.md) |
| What has been challenged, and under what assumptions? | [Claim ledger](../claims.md) |
| Which experiment should change our next decision? | [Research questions and workflow](../research.md) |

## The whole argument in one page

The target is a pinned native problem, not whatever subgraph happens to have been generated. A returned strategy is an upper witness only after its actual routed execution is legal, proper, priced, and independently evaluated. A lower is valid only after its abstraction covers every permitted alternative in the right direction. If those compatible witnesses meet under the declared numerical contract, the request can close.

| Link in the argument | Sufficient mathematical reason | What still has to be established in code |
|---|---|---|
| Native behavior → semantic state | The state retains every future-observable distinction | Exact item/control identity, goal semantics, and observation timing |
| Semantic states → shared representation | Exact equivalence or a stated optimistic relation | All represented members and actions satisfy that relation |
| Fixed policy → upper | Almost-sure absorption and finite expected cost in the evaluated graph | Full routes, positive-probability outcomes, prices, and artifact identity |
| Optimistic relation → lower | Checked Bellman inequalities plus proper-policy stopping argument | Complete native/action coverage and correct coefficients |
| Bounds → pruning | A competitor lower strictly exceeds a compatible executable alternative | Same source, scope, continuation semantics, generations, and tie contract |
| Interrupted work → retained result | A previously certified witness is unchanged by unfinished work | No stale rows, mixed certificates, or unavailable resource reservations |
| Completed proof → exact publication | Compatible lower and executable upper, with complete proof obligations | Classifier uses actual proof/evaluation status, not a formatted zero gap |

The [claim ledger](../claims.md) gives these facts durable names. The current implementation map remains [Solver Internals](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/solver-internals.md). Those are different views: a source map tells us where something happens; this reference explains what must be true for the result to follow.

## How to read the evidence

**Derivation** means an argument reconstructed in this draft under stated assumptions. It is not a claim of novelty or a historical explanation of why an earlier author wrote the code.

**Documented implementation** means the pinned mechanism contract identifies an owner or behavior. It is weaker than an exhaustive source audit or a mechanically verified refinement proof.

**Recorded experiment** means an existing artifact reports a scoped observation. It does not mean the author of this draft reran the native experiment.

**Open obligation** means the bridge from a conditional argument to a particular native use has not been established by this draft. Open obligations name the consumer and the needed fact in [research.md](../research.md#open-obligations).

All provisional ledger entries start `open` for repository integration. That does not imply that elementary mathematics is conjectural; it prevents an external draft from inventing project acceptance or a reviewer. Codex should reconcile IDs, arguments, and native applications before adding genuine acceptance events.

## Where old findings belong

A theorem or counterexample belongs in its argument chapter. Its identity, preconditions, status, and challenges belong in the ledger. A measured result belongs in its original artifact and question-linked interpretation. A historical implementation plan remains historical evidence.

This division is important when a technique is removed. Removing its code does not remove the reason an old projection was unsound, nor does a runtime failure refute its underlying mathematics.

The following recurring distinctions are intentionally explicit:

* fixed-policy value versus optimal value;
* native lower versus coefficient-model feasibility;
* exact equivalence versus optimistic abstraction;
* an admissible value versus a Bellman-consistent local iterate;
* a valid bound versus useful proof progress;
* a finite optimistic-model ceiling versus a native executable upper.

## Review path, not blanket preflight

For a lower change, start with the relevant subsection of [lower-bounds.md](lower-bounds.md), its claim preconditions, and the native producer/consumer. For a compiler change, begin with [policies.md](policies.md#compilation). For a scheduler change, separate retained-witness validity from fairness and performance in [search-and-resumption.md](search-and-resumption.md).

A new result should amend only the affected canonical argument and ledger history. Routine code changes need no new mathematical claim. The complete v2 migration plan is not part of future session startup.
