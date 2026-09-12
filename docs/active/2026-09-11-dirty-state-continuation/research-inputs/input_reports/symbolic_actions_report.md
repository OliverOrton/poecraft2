**Yes—I think this is worth exploring, and I would push it further than simply putting actions into categories.** The interesting version is: **represent a whole family of real actions symbolically, leave some parameters unresolved, and answer only the questions needed to decide which possibilities deserve more work.**

Your concern about exploration is real, but it does **not** mean every action must be fully evaluated before exploring states. The crucial distinction is between **postponing the calculation of a choice** and **pretending that the choice does not matter**.

I reviewed `main` at `84f02ee3b603fe3879ac2c4a885d1b81ba67772f`. These are proposed research directions, not implemented or measured improvements.

## 1. Represent actions as parameterized operations, not just named categories

The current registry describes a flat list of action instances—one per essence, fossil, Harvest tag, bench mod, influence, and so on. It also already has broader action-family categories. **Adding another category enum would not, by itself, change the computational problem.**

What I would investigate instead is a representation along these lines:

```text
Action family:
    native applicability conditions
    affected part of the item
    preservation/removal rule
    pool restrictions and weight changes
    guaranteed effects
    random draw process
    observation and choice points
    required setup/cleanup
    price expression
```

A particular action would be an assignment to some of these parameters. A family would be a **constrained set of assignments**.

For example, an illustrative—not proposed native API—description could be:

```text
Redraw(
    preserved_features = ?,
    pool_profile        = ?,
    guaranteed_effect   = ?,
    setup_program       = ?,
    cleanup_program     = ?
)
```

The question marks do not mean “anything is allowed.” A native legality relation would determine which combinations correspond to actual permitted actions or programs.

This is important because otherwise the representation invents combinations such as the price of one action, the preservation behavior of another, and the outcome distribution of a third.

### Categorize by computational behavior

I would use broad labels such as **redraw, add, remove, change capabilities, and conditional program** for orientation, but base sharing on more specific effects.

Two actions called “Harvest” may have little useful computation in common. Conversely, actions from different named systems may share a preserved-item calculation, pool-construction stage, or continuation query.

There is already a narrow precedent in your code: temporary-bench effect classes group variants using the follow-up action, goal slot, blocker side, pool-blocking behavior, and exact conflict mask. That is much closer to useful abstraction than grouping by currency name.

**The extension would be to make those unresolved choices part of the object the solver reasons about**, rather than resolving them into a flat action list before most useful work begins.

There is a particularly close paper here: **“Planning in Factored Action Spaces with Symbolic Dynamic Programming.”** It represents action variables alongside state variables and performs symbolic optimization over them. Its memory-bounded variant splits action-variable assignments when the intermediate representation becomes too large. That supports investigating a hybrid—not choosing between “enumerate everything” and “one enormous symbolic object.” ([AAAI Publications](https://ojs.aaai.org/index.php/AAAI/article/view/8364/8223 "Planning in Factored Action Spaces with Symbolic Dynamic Programming"))

## 2. Your “superset” idea has a useful geometric interpretation

This is probably the most direct formalization of your idea.

For a fixed source state, an action can be viewed as:

```math
(\text{immediate cost},\ \text{probability distribution over successors}).
```

Given continuation values `v`, its total one-step value is:

```math
q_a(v)=c_a+p_a\cdot v.
```

A family is therefore not one scalar score. It is a collection of these cost–probability pairs, with value:

```math
F(v)=\min_{a\in\mathcal A_F} \left(c_a+p_a\cdot v\right).
```

**The best action within the family depends on the continuation values.** It is not generally something that can be determined once from price or success probability.

### A small example

Suppose two hypothetical actions reach success or a failure state whose remaining cost is `x`:

| ActionImmediate costSuccess probabilityTotal expected cost |   |     |          |
| ---------------------------------------------------------- | - | --- | -------- |
| Cheap attempt                                              | 1 | 10% | `1+0.9x` |
| More reliable attempt                                      | 5 | 90% | `5+0.1x` |

When `x=2`, the cheap action wins: **2.8 versus 5.2**.

When `x=20`, the reliable action wins: **7 versus 19**.

They switch at `x=5`.

So instead of remembering “the best redraw action,” the solver could remember a **response function describing which action becomes best under which continuation costs**.

### A superset can even preserve the exact answer

Here is a useful mathematical observation.

Let:

```math
H_F=\operatorname{conv}\{(c_a,p_a):a\in\mathcal A_F\}
```

be the convex hull of the family’s cost–probability pairs. Then, for a fixed finite continuation vector:

```math
\min_{(c,p)\in H_F}(c+p\cdot v) = \min_{a\in\mathcal A_F}(c_a+p_a\cdot v).
```

Why? A linear objective evaluated on a convex combination is just a weighted average of the original action values. It cannot beat the smallest original value, and every original point remains available.

**That gives you a superset representation without automatically losing the one-step optimum.**

The practical research question becomes whether a family has a compact description through constraints, shared expressions, or a small set of relevant boundary points. Computing a giant hull *after enumerating every action and successor* would miss the point.

There are two useful versions:

**Exact family geometry.** Preserve the true cost–probability relationship. Optimize it for the current continuation query, then recover a concrete native action that attains the result.

**Optimistic outer approximation.** Use a larger, cheaper-to-describe region containing every real action. Its minimum can supply a lower bound, but a point selected from that larger region is not necessarily executable.

This addresses the dangerous version of “superset”: **retain the correlations between attractive properties**, rather than independently taking the cheapest price, highest success probability, and best preservation behavior.

An incomplete collection of real action candidates has the opposite limitation: it can find a good candidate, but cannot establish that an unseen member is worse.

## 3. The biggest opportunity may be representing transitions as queries instead of successor lists

Action abstraction alone may not remove the expensive part.

Your July automatic-action deferral experiment already demonstrated this: temporary-bench kernels could be deferred, but the next ordinary broad reforge recreated the same state explosion. That rejects *deferral alone*, not the broader representation idea.

The more ambitious change is to stop treating this as the mandatory interface:

```text
For this action, construct every successor and its probability.
```

Instead, allow:

```text
For this action family and this continuation function,
calculate—or bound—the expected continuation cost.
```

Those are different computational requests.

### A huge distribution can sometimes have a cheap expectation

Imagine a partial random draw with probability mass `w`. Suppose every possible completion beneath that partial draw has the same continuation value, 12.

For that particular query, the entire remaining subtree contributes:

```math
12w.
```

There is no reason to construct all its completed items just to rediscover that they all contribute 12.

If the continuation value is only known to be at least 12, the same expression supplies a lower contribution rather than an exact one.

The useful shortcut happens **before completing the draw process and before interning the successor states**. Merely compressing the final output list would leave much of the work intact.

Your previous abstraction audit already proposed a related frozen-value, stopping-line query. The further extension I would investigate here is **keeping action parameters unresolved inside that same expectation calculation**. That would combine action factoring and successor avoidance rather than treating them as separate optimizations.

### What the symbolic object would contain

Conceptually, it would be a shared expression graph containing:

- unresolved action parameters;
- conditional random draws and their exact dependencies;
- tests of item properties relevant to the continuation;
- numeric continuation values or certified bounds.

The solver could simplify parts of this graph without constructing every complete state–action outcome.

However, the native reforge evaluator already uses a grouped frontier calculation. It tracks goal and junk buckets, exclusion effects, per-bucket picks, and remaining weights. This is **not** a naïve independent-draw implementation waiting for an obvious compression. A replacement must preserve those conditional dependencies or deliberately bound them in a valid direction.

That means I would **not** start from “prefixes and suffixes are independent.” I would start from the actual conditional draw program and ask which variables can be eliminated for the particular query.

The Dice work—**“Scaling Exact Inference for Discrete Probabilistic Programs”**—is relevant because it compiles discrete probabilistic programs into structured weighted-counting problems and separates logical structure from probability parameters. It shows a route for exploiting program structure, not a guarantee that your exclusion-heavy draw process will compile compactly. ([arXiv](https://arxiv.org/abs/2005.09089 "\[2005.09089] Scaling Exact Inference for Discrete Probabilistic Programs"))

**My hypothesis:** the worthwhile representation is not a smaller complete transition graph. It is an object that can sometimes answer the solver’s question without constructing that graph at all.

## 4. Use different state views for different questions

I would also challenge the assumption that there must be one globally “best” abstraction of an item.

Consider three questions:

> Can this family beat a cost threshold?

> Which concrete action should this controller execute?

> Do these two items have identical behavior under every admitted action?

Those questions need different amounts of information.

For an exact, universally reusable state merge, you need matching terminal behavior, action availability, observations, costs, and transition probabilities into the relevant successor classes. Your current representation contract correctly requires much more than equal goal masks.

But a particular expectation query may require fewer distinctions.

### Proposed representation: item predicates plus query-specific dependencies

Instead of immediately replacing several items with one permanent solver state, represent a set through a predicate such as:

```text
these required properties are present
this occupancy pattern holds
this protection state holds
remaining identities satisfy these constraints
```

Then attach the question being asked.

A redraw query may be able to forget a distinction that a later removal or filtering decision needs. A fixed cleanup controller may distinguish items differently from an all-actions optimality proof.

The implementation could progressively request more information:

> “This distinction does not affect the current expectation.”

or:

> “These members disagree about legality or continuation cost; split here.”

The critical difference from ordinary state merging is that the certificate would say:

> **These members are interchangeable for this declared query.**

Not:

> **These items are interchangeable forever.**

A useful internal identity would therefore include the item domain, action-parameter domain, continuation query, and applicable semantic generations.

This could avoid carrying every distinction through every computation. It also creates a serious engineering risk: too many specialized views can cost more than the universal representation. I would want shared immutable descriptions and explicit memory accounting, not a second graph for each question.

## 5. Represent cleanup and retry regions by how they respond to future costs

This is the direction most directly connected to the latest missing-continuation problem.

The current post-incumbent report records root Chaos kernels that were already legal and complete, but lacked compatible continuations: **2,478 missing direct rows for Ring and 3,361 for Amulet** at the reported first-upper snapshots. Choosing an action family more cheaply does not, by itself, solve those missing tails.

Instead of storing a summary such as:

> “Cleaning up this region costs 30.”

I would investigate summaries that say:

> “This local controller incurs these expected costs and exits through these boundary conditions with these probabilities.”

For a fixed local policy with transient internal transitions `P_{RR}`, define:

```math
g=(I-P_{RR})^{-1}c, \qquad H=(I-P_{RR})^{-1}P_{RB}.
```

Here `g` is accumulated internal cost, and `H` maps entry states to exit probabilities. Given boundary continuation values `v_B`:

```math
J(v_B)=g+Hv_B.
```

This is a **continuation-response operator**, not one context-free cleanup price.

Different local policies provide different response functions. The best one can change as the values outside the region improve—just like the two-action example earlier.

The creative opportunity would be to represent a cleanup rule over a symbolic set of entry items, so one verified rule covers many exits of a broad reforge. That is a hypothesis about finding a compact rule, not permission to assume every dirty item has the same cheap cleanup.

The existing terminal contract matters here: goal coverage alone is insufficient; unwanted explicit affixes and temporary modifiers cannot simply be treated as successful completion.

**“Compositional Value Iteration with Pareto Caching”** is relevant to reusable component responses under changing boundary queries. Its setting is compositional reachability, however; applying that idea to your expected-cost objective would need the appropriate cost and termination arguments. ([arXiv](https://arxiv.org/abs/2405.10099 "\[2405.10099] Compositional Value Iteration with Pareto Caching"))

I would also preserve a hard distinction between local and global termination. Two individually terminating components can still be assembled into an endless cycle. Complete exit mass, entry coverage, actual routing, and global properness remain necessary.

## 6. Your exploration concern: three different failure modes

### A family representative can hide the useful states

Suppose a family contains an inexpensive destructive action and an expensive preservation action.

Exploring only the inexpensive member’s successors can miss the entire region that makes preservation worthwhile. This is not solved by giving the representative a broad family label.

**My proposed remedy:** leave the other members represented as unresolved alternatives with valid bounds. A representative can guide work, but it cannot stand in for complete family coverage.

Your lower-only machinery already has the relevant contract: explicit actions and residual families form a complete partition, and refining one member must not silently remove the rest. That is infrastructure to extend, not replace.

### An optimistic family can attract endless work

An outer approximation may look excellent precisely because it combines possibilities that no real action achieves.

That can be a sound lower bound and still be terrible search guidance.

I would have refinement identify **which artificial advantage makes the family competitive**. Is it ignoring a preservation cost? Combining incompatible guarantees? Giving too much freedom to clean up afterward?

Then split or strengthen that part of the representation, rather than repeatedly expanding states under the same unhelpful optimism.

Sometimes the right answer is not another state expansion. It is:

> “Resolve this action parameter first, because it determines whether this whole branch is real.”

### Delayed calculation must not become hindsight

This is the easiest way to turn the concept into an incorrect optimizer.

In general:

```math
\min_a \mathbb E[Z_a] \ne \mathbb E[\min_a Z_a].
```

For a toy example, two actions each cost 1. A fair random outcome then incurs additional costs:

- action A: either 0 or 100;
- action B: either 100 or 0.

Choose the action before the outcome, and either choice costs **51 in expectation**.

Illegally choose after seeing the outcome, and the apparent cost is **1**.

**The planner may postpone resolving an action parameter computationally. The emitted strategy must still make that choice at the native decision point.**

Real observed choices can legitimately optimize after observation. The symbolic expression must preserve that ordering rather than moving every minimum inside every expectation.

### A different unit of exploration

I would therefore consider a pending work item shaped more like:

```text
(source domain, unresolved action family, continuation query)
```

Servicing it might mean refining an action parameter, integrating another part of a random draw, or solving a continuation state.

That is more expressive than assuming that every useful next step is “expand another state.”

For correctness, a family can be ruled out only when its valid lower bound shows that no member can improve a compatible upper at the **same entry state**, following the existing numerical and tie rules. Otherwise it remains unresolved. Constraint-generation work on stochastic shortest-path problems supports this selective-action perspective, but its algorithmic assumptions cannot simply replace your native properness and numerical contracts. ([arXiv](https://arxiv.org/abs/2604.01855 "\[2604.01855] Efficient Constraint Generation for Stochastic Shortest Path Problems"))

For performance, there is no guarantee this ordering wins under a fixed budget. Unresolved competitive families need service, and the best verified controller should survive failed refinement attempts. **Sound abstraction and good exploration are separate questions.**

## 7. What I would actually investigate first

My first choice would be **a combination of parameterized action families and query-directed expectation evaluation**, with the geometric interpretation providing a disciplined way to construct family bounds.

I would not begin with a whole-engine symbolic rewrite.

A useful pilot would take one existing broad-reforge workload and ask:

> **Can we compare a meaningful family against a continuation-cost threshold while avoiding both most individual action evaluation and most successor construction?**

The comparison should include ordinary evaluation, family deferral alone, and the combined symbolic query. Otherwise a favorable scheduling change could be mistaken for a representation improvement.

The decisive measurements would be native draw work actually skipped, successor states never created, total peak memory including symbolic preparation and checking, and effects on returned controller quality or useful bounds. Fewer descriptors or a smaller final graph would not be enough.

In parallel with interpreting that result—not necessarily building another subsystem—I would ask whether the resulting positive-mass exits admit a compact executable continuation rule. The latest Ring/Amulet evidence makes that a necessary check against optimizing the wrong stage.

A minimal set of adversarial examples should force the representation to handle a best-action switch after continuation values change, hidden blockers, pre-/post-observation choice, a residual family with an overlooked member, and a rare exit lacking a valid continuation. Those are more discriminating than immediately running a large benchmark sweep.

### My ranking

| DirectionWhy I would pursue itMain way it could disappoint |                                                             |                                                                    |
| ---------------------------------------------------------- | ----------------------------------------------------------- | ------------------------------------------------------------------ |
| **Parameterized family + expectation query**               | Could avoid action work and successor construction together | Conditional probability computation remains large                  |
| **Cost–probability envelopes**                             | Formalizes your superset idea while preserving correlations | Constructing a useful envelope costs as much as enumeration        |
| **Query-specific state views**                             | Avoids making every computation carry every distinction     | Specialized views and invalidation become expensive                |
| **Symbolic continuation-response regions**                 | Could address thousands of missing cleanup tails            | No compact, uniformly executable rule exists for the needed domain |

**The strongest version of your idea is not “explore using generic actions, then substitute the best currency afterward.” It is “keep the action choices, stochastic outcomes, and continuation dependencies symbolic together, and resolve only the distinctions that affect the decision.”**

That is the representation direction I would prioritize. It offers a way to avoid generating work, rather than merely doing the existing work in a different order.