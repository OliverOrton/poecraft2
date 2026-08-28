# Retention–Capacity Abstract SSP proposal

## Recommendation

The proposed lower-bound family should be a **proof-only portfolio of small Retention–Capacity Abstract SSPs (RCASSPs)**, not one globally exact carrier abstraction.

Its defining features should be:

1. exact identity for a small selected goal subset;
2. actual prefix/suffix placement and capacity;
3. action-relative retention classes for non-goal affixes;
4. source-authoritative blocker, protection, and destructive-survival facts;
5. whole-kernel favorable nondeterminism when probability-relevant variables are dropped;
6. exact, action-local projection only for the operators that can consume the added precision;
7. an action-complete fallback row for every operator not refined by the pattern;
8. independent admissibility at every refinement stage;
9. a retained production budget of at most **24 MiB** under the current 1 GiB solver cap.

This fits the existing proof architecture. `ProofPatternManager` already treats independently admissible patterns as typed proof authorities and combines them by maximum, while unavailable specializations fall back rather than borrowing a scheduler value.

The proposal directly addresses the failure found in the July probability audit: goal-count and goal-mask relaxations could not price the future cost of non-goal blockers or loss of retained progress, allowing very cheap first actions to own the lower envelope.  It also avoids repeating the August same-side result, where transferring the existing coarse lower to all `299,394` strict obligations produced zero noncompetitive obligations.

The successful architectural precedent is the Fracture-local quotient: keep the parent small, recover exact physical identity only at the action boundary, aggregate only branches proven continuation-equivalent, and refuse ambiguous cases. That remained around 927 states rather than globalizing exact junk identity.

This must **not** revive the executable carrier planner as proof or policy authority. That planner was rejected after a mass-conservation failure, and its retained header explicitly carries no proof, pruning, terminal, or execution authority.

---

# 1. Formal definition

Let the configured concrete crafting model be the stochastic shortest-path problem

$$
M=(S,G,A,c,P),
$$

where:

* \(S\) is the exact solver state set;
* \(G\subseteq S\) is the exact terminal set;
* \(A(s)\) is the complete caller-authorized action set at \(s\);
* \(c(s,a)\ge 0\);
* \(P(\cdot\mid s,a)\) is the source-authoritative transition kernel, including the existing timing of observed choices.

The concrete optimal value is

$$
V^*(s)=
\begin{cases}
0 & s\in G,\\
\min_{a\in A(s)}
\left[c(s,a)+\sum_{s'}P(s'\mid s,a)V^*(s')\right] & s\notin G.
\end{cases}
$$

An RCASSP pattern is described by

$$
\Pi=(K,\Gamma,\Omega,\mathcal E,B),
$$

where:

* \(K\) is the selected exact goal subset;
* \(\Gamma\) is its quota/terminal relaxation;
* \(\Omega\) identifies the action families receiving retention-aware treatment;
* \(\mathcal E\) is the current observer/refinement schema;
* \(B\) is the pattern’s retained-byte limit.

It defines an abstraction

$$
\phi_\Pi:S\rightarrow\bar S_\Pi.
$$

The abstract model is deliberately an **optimistic SSP with set-valued action kernels**. For abstract state \(\alpha\) and concrete action identity \(a\), let

$$
\mathcal W_\Pi(\alpha,a)
$$

be a set of complete cost/kernel pairs \((\hat c,q)\), where \(q\) is a normalized distribution over abstract successor states.

The abstract action lower is

$$
L_\Pi(\alpha,a;v)
=
\min_{(\hat c,q)\in\mathcal W_\Pi(\alpha,a)}
\left[
\hat c+\sum_{\beta}q(\beta)v(\beta)
\right].
$$

The abstract Bellman operator is

$$
(\bar T_\Pi v)(\alpha)=
\begin{cases}
0 & \alpha\in\bar G_\Pi,\\
\min_{a\in\bar A_\Pi(\alpha)}
L_\Pi(\alpha,a;v) & \text{otherwise}.
\end{cases}
$$

The inner minimum is favorable nondeterminism: the relaxed controller may select the most favorable hidden signature **before** the random outcome. The entire normalized kernel is selected as one unit. It may not independently select the best probability for each successor.

## Required simulation contract

For every concrete \(s\), every legal \(a\in A(s)\), and \(\alpha=\phi_\Pi(s)\), there must be at least one \((\hat c,q)\in\mathcal W_\Pi(\alpha,a)\) such that:

$$
\hat c\le c(s,a)
$$

and

$$
q(\beta)=
\sum_{s':\,\phi_\Pi(s')=\beta}
P(s'\mid s,a).
$$

An over-approximation may contain additional, more favorable cost/kernel pairs. That weakens the lower bound but remains safe.

The terminal condition must satisfy

$$
s\in G\Longrightarrow\phi_\Pi(s)\in\bar G_\Pi.
$$

Thus the abstract terminal set is allowed to include nonterminal concrete states, but may never exclude the projection of a real terminal state.

---

# 2. Abstraction map

The production abstraction map should be:

$$
\phi_\Pi(s)=
\left(
z_K,\sigma_K,
u_P,u_S,
o_P,o_S,C_P,C_S,
J_\Pi,
B_K,
R_\Pi,
D_\Pi,
M_\Pi
\right).
$$

The selected identities themselves live in the immutable pattern descriptor \(K\), so state keys need only retain their current statuses.

## 2.1 Selected goal core

For every selected goal slot \(i\in K\), retain:

* exact slot identity;
* exact member-class identity whenever the source action distinguishes members;
* current tier/status, using the source state’s complete status domain rather than reducing immediately to a Boolean;
* actual prefix/suffix side;
* crafted-goal status when observed;
* fractured-goal status when observed;
* any selected-goal flag that affects legality, targeting, survival, or projected probability.

This is the minimum identity required to prevent the count abstraction from repeatedly crediting the same cheap goal or confusing two goals with different blockers or action access.

The concrete `AbstractState` already retains goal status, goal identity classes, side occupancy, junk classes, crafted/fractured information, protection, influence/Eldritch/Veiled state, and retry-relevant state. Strict terminality also distinguishes unresolved non-goal occupancy rather than treating every satisfied goal mask as terminal.

## 2.2 Actual side split and capacity

Retain:

* \(o_P,o_S\): current occupied prefix/suffix counts;
* \(C_P,C_S\): source-provided capacities;
* \(u_P,u_S\): satisfied unselected goals by actual side, capped at the remaining quota;
* optionally, missing unselected goals by side when that changes action probability;
* exact selected-goal side assignment.

No fixed numerical capacity is assumed by this design. Capacity comes from the engine-owned state/session contract.

Keeping actual side counts is valuable even when selected identities are omitted, because it prevents a prefix-only opening from being reused as a fictitious suffix opening and exposes the cost of a full side.

## 2.3 Action-relative non-goal retention

A single global “junk count” is insufficient. The classification must be relative to the pattern’s refined action families.

For action family \(f\), retain per side:

* \(D^f_x\): **disposable** occupants—source proof allows the abstraction to erase them or choose their most favorable fate under \(f\);
* \(P^f_x\): **persistent** occupants—source proof says they survive the represented \(f\) transition and continue consuming capacity;
* \(L^f_x\): **locally observed** occupants whose identity or subtype affects legality, victim selection, or probability and therefore requires an action-local signature;
* \(U^f_x\): unresolved occupants.

Here \(x\in\{P,S\}\).

`U` must never be silently classified as persistent. That could make the abstraction harder than the real process and invalidate the lower bound. Its safe defaults are:

1. move it to a favorable disposable class;
2. retain/split the missing source variable;
3. mark that action specialization unavailable and use the parent action lower.

The existing `CarrierFacts`, `CarrierEffectSummary`, and `CarrierSuccessorEnvelope` already provide the natural source facts: exact satisfied/blocked/crafted/fractured masks, protection, junk counts, side counts, all/any successor masks, and min/max successor occupancy.

## 2.4 Blockers

Retain:

* one blocker bit or small blocker class for each selected unsatisfied goal;
* an optional side-level blocker summary for unselected quota progress;
* an action-local blocker signature when the action reads a finer pool distinction.

A blocker bit is retained only if the source says it changes legality, the eligible pool, or outcome probability. Unknown blocker state is relaxed favorably, usually to “not blocked,” unless the variable is split.

## 2.5 Protection and legality flags

Retain only flags observed by the pattern’s represented actions:

* selected-side protection;
* source-defined cannot-roll or metamod effects;
* crafted/fractured distinctions;
* influence, Eldritch, Veiled, Split, Corrupted, or other flags only when a source contract says they affect the represented action.

Dropping an observed protection flag requires either:

* favorable nondeterminism over every complete flag-specific kernel; or
* state splitting.

It is not sound to average protected and unprotected probabilities into a kernel that corresponds to neither.

## 2.6 Destructive survival

For each selected goal and refined destructive family, retain a survival class:

* `MustSurvive`;
* `MayBeVictim`;
* `MustBeDestroyed`;
* `NotObserved`.

For unselected progress, retain enough side/count information to bound how much quota can survive.

The survival profile is action-relative. A goal may be persistent for one action and exposed for another. The state therefore stores a small source-derived profile for the few refined families rather than a universal “retained” bit.

## 2.7 Retry and automatic-program state

For automatic micro-programs, retain:

* program counter or phase;
* saved-carrier projection;
* retry-basin identity;
* any source-observed choice memory needed to make the projected process Markov;
* whether a setup resource has already been paid;
* source-defined reset/recovery mode.

## 2.8 Terminal debt

A refined RC state is abstract-terminal only when all retained facts certify the relaxed terminal condition:

* required selected/residual quota satisfied;
* required rarity condition satisfied if retained;
* no retained persistent non-goal occupancy that violates the source terminal predicate;
* no retained mandatory cleanup or program phase remains.

An unknown debt is relaxed to zero or delegated to the existing `TerminalDebt` pattern. It must not be guessed positive.

The existing `TerminalDebt` value and RCASSP should normally combine by **maximum**, not addition. Adding separately computed debt to RC progress would risk double-counting the same future action.

---

# 3. Pattern portfolio

RCASSP should be a bounded portfolio, not a single state space.

| Pattern                   | Retained information                                                               | Purpose                                  |
| ------------------------- | ---------------------------------------------------------------------------------- | ---------------------------------------- |
| **RC-Count**              | Global satisfied count only                                                        | Control measurement; expected to be weak |
| **RC-SideCapacity**       | Actual satisfied counts by side, occupancy, free capacity, coarse debt             | Production parent                        |
| **RC-Selected(\(K\))**    | Exact selected identities/status/sides plus residual unselected quota              | Identity-sensitive lower                 |
| **RC-Retention(\(K,f\))** | Selected pattern plus \(D/P/L/U\), blockers, protection, survival for family \(f\) | Retention-aware family refinement        |
| **RC-Local(\(K,s,a\))**   | Exact one-step action kernel at a strict source, projected to an RC continuation   | High-precision operator lower            |
| **RC-Auto(\(K,p\))**      | Program phase, saved projection, retry basin, complete option kernel               | Automatic-program lower                  |

Every state-level pattern must remain action-complete. Actions not receiving specialized treatment get a fallback action row from:

1. the current independently admissible operator lower;
2. first-step price plus universal continuation;
3. zero, if no stronger certified action lower is available.

It is unsound to omit unrefined actions from an abstract state merely because the pattern is intended to study one family.

## Recommended initial four-goal PDR portfolio

Start with no more than eight patterns:

* one `RC-SideCapacity` parent;
* four exact singleton patterns;
* one or two exact selected pairs chosen from same-side/capacity conflicts;
* at most one action-local refinement initially.

Do not begin with an exact four-goal carrier or globally exact junk identity. The previous four-goal investigations already found that global exact identity multiplies the state space, whereas action-relative “retained core plus disposable shell” is tractable.

The carrier MDP should remain available as the fallback parent. Its current strength comes from exact goal progress while deliberately granting free cleanup, perfect preservation, and favorable carrier shapes; RCASSP is a monotone precision layer above it, not a replacement.

---

# 4. Abstract action and probability construction

## 4.1 Proof signatures

For source state \(s\) and action \(a\), define the local observer signature

$$
\lambda_a(s)=
\left(
\text{legality observations},
\text{selected identities/statuses},
\text{sides/capacity},
D/P/L,
\text{blockers},
\text{protection},
\text{survival profile},
\text{source flags},
\text{retry phase}
\right).
$$

The signature contains only values that the source action reads and that are not already fixed by \(\phi_\Pi(s)\).

For an abstract state \(\alpha\), let

$$
\Lambda(\alpha,a)
=
\{\lambda_a(s):\phi_\Pi(s)=\alpha,\ a\in A(s)\}.
$$

An implementation may use a proved over-approximation

$$
\widehat\Lambda(\alpha,a)\supseteq\Lambda(\alpha,a).
$$

Every element must represent a complete joint source configuration or a normalized set of complete kernels. It may not be a collection of independently selected marginal facts.

## 4.2 Three row-construction modes

### A. Reusable abstract row

Use this only when the source contract proves that the retained state plus signature determines the projected kernel.

For each \(\lambda\in\widehat\Lambda(\alpha,a)\):

1. obtain the exact or source-certified optimistic kernel;
2. project every successor through \(\phi_\Pi\);
3. aggregate equal abstract successors;
4. retain the exact stored probabilities;
5. verify total probability mass;
6. pair its cost with that same kernel.

The abstract action picks the cheapest signature-specific complete row.

### B. Exact action-local row

For a concrete strict obligation \((s,a)\):

1. obtain `CalcContext::outcomes(s,a,...)`;
2. project each exact successor to the selected RC pattern;
3. use the validated RC lower at each successor;
4. compute

$$
L^{\mathrm{local}}_\Pi(s,a)
=
c(s,a)+
\sum_{s'}P(s'\mid s,a)h_\Pi(s');
$$

5. expose this through the existing typed `OperatorLower` path.

This is the preferred mode for identity-observing cleanup, victim selection, or complicated destructive actions. It adds exact precision at the consumer without forcing exact identity into every RC state.

### C. Scalar fallback row

When a reusable signature cover cannot be proved and local exact projection is not budgeted, use an independently admissible action lower as a terminal escape row.

This preserves action completeness and avoids turning “unsupported” into “action absent.”

## 4.3 Dropped probability variables

When a dropped variable changes projected probabilities, exactly one of the following is allowed:

1. **Favorable complete-kernel nondeterminism**
   Add every possible complete normalized kernel and take the minimum.

2. **State splitting**
   Move the variable into the abstract state, producing separate child states.

3. **Normalized uncertainty polytope**
   Use a convex set containing every possible pushed-forward kernel and minimize expected continuation over that set.

The following is forbidden:

* independently choosing the minimum probability of every bad successor;
* independently choosing the maximum probability of every good successor;
* combining the immediate cost from one hidden signature with the transition kernel from another unless that cross-product itself is intentionally admitted as an additional relaxation;
* dropping missing probability mass.

The rejected executable carrier planner demonstrated why an attractive scalar projection is not enough: a projected candidate may fail to conserve the exact strict mass.

## 4.4 Source authorities

Primitive outcome probability must come from the exact calculator or an already-proved action refinement contract. `OutcomeDistribution` provides the ordinary probability distribution, while `OptionKernel` carries expected resources, exits, retry and continuation states, observed choices, almost-sure termination, and automatic completeness evidence.

Action-family labels may be used for budgeting and telemetry, but not to infer mechanics. The source code deliberately separates family classification from legality and transition semantics.

## 4.5 Action-local exact junk identity

Exact junk identity is introduced only when an action observes it.

For a local action row:

* enumerate exact physical branches that affect a selected goal, retention, capacity, or continuation;
* aggregate branches only when they project to the same RC successor;
* alternatively aggregate branches proven to enter an identical restart/dead continuation;
* refuse the specialization when equivalence is not proved.

This is the same proof shape as the qualified Fracture-local operator, not an assumption that the Fracture mechanics generalize to other actions.

## 4.6 On-demand transitions

Use a structural cache keyed by:

```text
pattern_version
goal_identity
caller_scope_identity
action_vocabulary_identity
source-mechanics identity
abstract_state_key
operator identity
local_signature
```

Price-dependent values additionally key on the economy identity.

Rows should be generated only when:

* the state is queried by `completion_proof_lower_value`;
* an operator lower is queried for a live strict obligation;
* the abstract Bellman solution reaches the state;
* CEGAR selects the state/action for refinement.

Cold exact local rows are reconstructible and may be evicted. The retained certificate is the validated pattern value/subsolution and its structural identities, not an unversioned row pointer.

---

# 5. Admissibility proof sketch

## Theorem

Assume for pattern \(\Pi\):

1. costs are nonnegative;
2. every concrete goal projects to an abstract goal;
3. every concrete legal action has an abstract row or scalar fallback satisfying the cost/kernel inclusion contract;
4. every abstract row is a complete normalized kernel;
5. \(v_\Pi\ge0\), is zero on \(\bar G_\Pi\), and satisfies

$$
v_\Pi\le\bar T_\Pi v_\Pi.
$$

Then

$$
h_\Pi(s)=v_\Pi(\phi_\Pi(s))
$$

is an admissible lower bound:

$$
h_\Pi(s)\le V^*(s).
$$

## Proof

Fix a nonterminal concrete state \(s\), let \(\alpha=\phi_\Pi(s)\), and choose any concrete legal action \(a\).

By the action simulation contract, the abstract uncertainty set contains a pair \((\hat c,q)\) such that

$$
\hat c\le c(s,a)
$$

and \(q\) is the pushed-forward concrete kernel. Therefore

$$
L_\Pi(\alpha,a;v_\Pi)
\le
\hat c+
\sum_\beta q(\beta)v_\Pi(\beta)
$$

and hence

$$
L_\Pi(\alpha,a;v_\Pi)
\le
c(s,a)+
\sum_{s'}P(s'\mid s,a)h_\Pi(s').
$$

Because \(v_\Pi\le\bar T_\Pi v_\Pi\),

$$
h_\Pi(s)
\le
L_\Pi(\alpha,a;v_\Pi)
$$

for every legal \(a\). Thus

$$
h_\Pi(s)
\le
\min_{a\in A(s)}
\left[
c(s,a)+
\sum_{s'}P(s'\mid s,a)h_\Pi(s')
\right].
$$

So \(h_\Pi\) is a concrete Bellman subsolution.

For any proper concrete policy, iterating this inequality to its goal-hitting time and using nonnegative costs and bounded \(h_\Pi\) gives

$$
h_\Pi(s)\le
\mathbb E_\pi[\text{total cost from }s].
$$

Taking the infimum over proper policies gives \(h_\Pi\le V^*\). If no proper policy exists, \(V^*=\infty\) and the finite lower remains valid.

## Refinement preservation

A refinement may:

* split an abstract state;
* retain another goal identity;
* retain a side or capacity field;
* move a dropped observer into the local signature;
* replace an uncertainty set by a smaller set that still contains every concrete kernel;
* remove false abstract terminals while still containing all concrete terminal projections.

Each refined pattern independently satisfies the theorem.

Mathematically, reducing favorable nondeterminism should not lower the abstract optimum. Operationally, the production value should still be published as

$$
h_{\text{published}}(s)=
\max\{h_{\text{parent}}(s),h_{\text{child}}(s)\},
$$

so numerical or incomplete-child issues can never weaken the existing lower.

The maximum of any set of independently admissible lower bounds is admissible:

$$
\max_i h_i(s)\le V^*(s).
$$

No RC patterns should be summed.

---

# 6. Automatic programs and retry basins

Automatic options require a separate micro-SSP treatment.

## 6.1 Micro-state

An automatic abstract state is

$$
(\alpha,\mathrm{pc},b,r,m),
$$

where:

* \(\alpha\) is the retained carrier projection;
* `pc` is the program phase;
* \(b\) is the saved-base or saved-carrier projection;
* \(r\) is the retry-basin class;
* \(m\) is source-observed choice memory.

Every mandatory primitive step is charged when traversed.

## 6.2 Macro-row eligibility

An automatic program may collapse to one abstract macro row only when the source proves:

* the option kernel is complete;
* expected resource consumption is exact for the represented choice policy;
* exits and their probabilities are complete;
* all retry/continuation states are accounted for;
* termination is almost sure;
* the retained exit projection is Markov;
* observed choices retain their actual timing;
* the kernel identities match the current mechanics, action vocabulary, caller scope, and economy.

Then the macro cost is

$$
\hat c=
\sum_r \operatorname{price}(r)
\,\mathbb E[\operatorname{quantity}(r)]
$$

and the continuation is the projected exit distribution.

Linearity permits expected resource cost and exit continuation to be added, but choice-dependent resource and exit variants must remain paired.

## 6.3 Retry basins

Retry basins should be solved as recurrent SSP components.

A geometric shortcut such as \(c/p\) is allowed only when the source contract proves:

* identical retry state;
* memorylessness;
* exact success probability;
* exact reset behavior;
* no hidden accumulated resource or observation;
* almost-sure termination.

Otherwise the retry states remain explicit micro-states.

For zero-cost or potentially improper cycles, publish only the least nonnegative subsolution obtained from zero or fall back to the parent lower. Do not select an arbitrary positive fixed point.

The existing automatic-program work is already budget-sensitive, and the Imprint follow-up found that such work should remain opt-in until it demonstrates consumers.

---

# 7. Any-\(k\)-of-\(n\) admissibility

Let the concrete goal require at least \(r\) distinct goal slots from a set \(N\), where \(|N|=n\).

## 7.1 Global count pattern

A global count abstraction retains

$$
x(s)=
\min\left(r,\#\{\text{satisfied goal slots in }s\}\right).
$$

Its abstract terminal set is \(x=r\).

Because every concrete terminal has at least \(r\) satisfied slots, its projection is abstract-terminal. If all concrete action kernels are included through exact projection or favorable complete-kernel nondeterminism, the general theorem proves admissibility.

Identity-free transitions may be dramatically optimistic, but optimism affects strength, not soundness.

## 7.2 Exact selected subset

Choose \(K\subseteq N\), with \(|K|=m\). The \(n-m\) omitted goals are granted for free.

The minimum number of selected goals that every concrete terminal must contain is

$$
\kappa(K)=
\max(0,r-(n-m)).
$$

Define the selected-pattern terminal as

$$
\#\{\text{satisfied selected goals}\}\ge\kappa(K).
$$

For any concrete terminal, at most \(n-m\) of its \(r\) satisfied goals can lie outside \(K\). Therefore it must contain at least \(r-(n-m)\) satisfied selected goals. Hence every concrete terminal maps to an abstract terminal.

This proves the selected-subset relaxation admissible.

Consequences:

* for an all-\(n\)-of-\(n\) goal, \(\kappa(K)=|K|\), so every selected goal is required;
* for permissive \(r\)-of-\(n\) goals, a small \(K\) may have \(\kappa=0\), producing the valid but useless zero pattern;
* retaining unselected progress counts can strengthen the pattern beyond the free-omission formula.

Each \(K\)-pattern is independently admissible, so the portfolio may safely take

$$
\max_K h_K.
$$

## 7.3 Side-aware quota lattice

Let \(x_P,x_S\) be satisfied counts by actual side.

For a globally specified \(r\)-of-\(n\) goal, the abstract terminal must be the disjunction

$$
x_P+x_S\ge r,
$$

together with any side minima that are logically implied by the known goal-side totals.

It must **not** commit to one arbitrary fixed pair \((r_P,r_S)\) unless that pair is required by the concrete goal.

Even taking the minimum of independently solved fixed-quota SSPs is not generally safe. The value of reaching a union of side outcomes can be lower than the value of committing in advance to either outcome.

Generic counterexample:

* goal is any one of one prefix goal and one suffix goal;
* an action costs 1 and produces either one with probability \(1/2\);
* the union goal is reached in one action, value 1;
* a fixed-prefix target or fixed-suffix target has retry value 2.

Thus both fixed-quota values, and their minimum, exceed the real union-goal value. The side-aware RC pattern must retain the disjunctive terminal in one SSP.

---

# 8. Unbounded strength-ratio counterexample

This is a generic SSP construction, not a claim about a Path of Exile mechanic.

For any \(M>0\), consider:

* two capacity slots on one side;
* terminal requires selected goals \(g_1\) and \(g_2\);
* the initial state contains \(g_1\) and persistent junk \(j\), so capacity is full;
* `add` costs 1:

  * with a free slot, it adds \(g_2\);
  * when full, it self-loops;
* `clean` costs \(M\), removes \(j\), and preserves \(g_1\).

The concrete optimal value is

$$
V^*=M+1.
$$

A retention–capacity abstraction distinguishes the full state with persistent junk and also obtains \(M+1\).

A global count abstraction maps the initial state to “one goal satisfied.” That same abstract cell also contains a hypothetical one-goal state with a free slot. Favorable nondeterminism selects that hidden witness and permits `add` to reach the two-goal terminal for cost 1.

Therefore

$$
h_{\text{count}}=1
$$

and

$$
\frac{h_{\text{RC}}}{h_{\text{count}}}
=
M+1.
$$

The ratio is unbounded as \(M\rightarrow\infty\).

Identity can cause the same phenomenon even without junk: a count abstraction may repeatedly credit one cheap goal-producing action by switching between hidden one-goal witnesses, while an exact selected subset recognizes that a second identity requires an arbitrarily expensive action.

---

# 9. Comparison

| Property              | Global \(k\)-of-\(n\)    | Side-aware quota/capacity                   | Exact selected subset                                      |   |                                      |
| --------------------- | ------------------------ | ------------------------------------------- | ---------------------------------------------------------- | - | ------------------------------------ |
| Goal identity         | None                     | None or coarse side class                   | Exact for \(K\)                                            |   |                                      |
| Actual side split     | Dropped                  | Retained                                    | Retained                                                   |   |                                      |
| Capacity              | Usually favorable/global | Exact per side                              | Exact per side                                             |   |                                      |
| Persistent junk       | Usually dropped          | Side counts possible                        | Selected/action-relative                                   |   |                                      |
| Blockers              | Favorably omitted        | Side-level                                  | Selected exact/local                                       |   |                                      |
| Probability precision | Count transition         | Side-conditioned                            | Identity/signature-conditioned                             |   |                                      |
| Safe terminal         | Total count              | Disjunctive side lattice                    | Residual quota \(\kappa(K)\)                               |   |                                      |
| Typical state scale   | \(O(r)\)                 | \(O(r_P r_S C_P C_S)\) before debt fields   | Roughly exponential in (                                   | K | ), then multiplied by carrier fields |
| Main strength         | Cheap control bound      | Prevents cross-side/capacity hallucinations | Prevents repeated-goal and blocker/survival hallucinations |   |                                      |
| Main weakness         | Unboundedly weak         | Still merges identities                     | State growth                                               |   |                                      |
| Recommended role      | Baseline/control         | Production parent                           | Targeted CEGAR children                                    |   |                                      |

The production hierarchy should therefore be:

$$
\text{Global Count}
\;\preceq\;
\text{Side/Capacity}
\;\preceq\;
\text{Selected Identity}
\;\preceq\;
\text{Selected Retention/Local}.
$$

Not every member must be a literal partition refinement of the previous one, but each is independently admissible and the portfolio maximum is safe.

---

# 10. CEGAR/refinement algorithm

## 10.1 Seed

Build:

* one side/capacity parent;
* selected singleton patterns with \(\kappa(K)>0\);
* selected pairs that share a side or appear in blocked/retention-heavy strict obligations;
* no automatic macro patterns initially.

The current telemetry already groups work by satisfied mask, side capacity, blocked mask, protection, fracture shape, unrelated occupancy, operator family, lower margins, and pruning outcomes. That is sufficient to seed the first portfolio without inventing a new sampling system.

## 10.2 Solve lazily

For each pattern:

1. start value iteration from zero;
2. create rows only for reached/query states;
3. use acyclic dynamic programming where the pattern structure proves acyclicity;
4. otherwise compute a monotone Bellman subsolution;
5. validate the residual before exposing the typed proof value.

## 10.3 Counterexample witnesses

A refinement witness is not a soundness violation. It is evidence that optimistic merging is consuming too much lower-bound strength.

Collect:

* an abstract argmin using a hidden signature incompatible with the queried strict state;
* repeated visits to one abstract state that choose mutually incompatible retention profiles;
* a count action credited to different selected identities on successive visits;
* a full-side state using a free-capacity kernel;
* a blocker-free kernel selected from a blocked strict witness;
* a destructive action selecting a survivor pattern not possible for the source;
* an uncertainty-polytope optimum using a correlation not represented by any complete source kernel;
* a strict obligation still competitive solely because its continuation falls back to the coarse carrier lower;
* a high-byte/high-fanout action family with many near-incumbent obligations.

## 10.4 Candidate splits

Candidate refinement dimensions are:

1. selected goal identity;
2. current selected-goal status;
3. actual selected side;
4. occupied/free capacity by side;
5. disposable versus persistent boundary;
6. one selected blocker bit;
7. one protection state;
8. one selected destructive-survival class;
9. action-local junk observer class;
10. program phase or retry mode;
11. source-observed choice memory.

## 10.5 Selection score

Rank refinement candidates using an explicitly consumer-oriented score such as

$$
\operatorname{score}(e)=
\frac{
\operatorname{reachWeight}(e)
\cdot
\operatorname{predictedRetirements}(e)
\cdot
\operatorname{lowerMarginGain}(e)
}{
\operatorname{retainedBytes}(e)
+\lambda_t\operatorname{buildTime}(e)
+\lambda_s\operatorname{newStrictStates}(e)
}.
$$

`predictedRetirements` should be estimated from currently unresolved ledger obligations and their verified-upper margins, not merely from root-value gain.

## 10.6 Validate before publication

Every child must pass:

* concrete-to-abstract map coverage;
* goal-terminal inclusion;
* action completeness;
* cost lower-bound checks;
* exact pushed-kernel inclusion;
* probability mass conservation;
* source choice-timing checks;
* Bellman subsolution and residual checks;
* identity/version checks;
* retained-byte reservation;
* parent/child shadow comparison.

Then publish

$$
\max(h_{\text{parent}},h_{\text{child}}).
$$

## 10.7 Refinement stop

Stop refining a pattern when:

* its next split would exceed its byte reservation;
* its signature cover cannot be proved;
* exact local rows have poor reuse and begin reproducing the strict graph;
* two representative ordinary PDR runs show no direct obligation retirement;
* a cheaper action still pins the same Bellman minimum after all candidate local refinements;
* retained RC bytes exceed the strict proof bytes they save.

The envelope proof already has a compatible operational shape: refine the currently minimizing action on demand while retaining an independent common lower when rows remain incomplete. RC CEGAR should extend that mechanism rather than introduce a separate proof lifecycle.

---

# 11. Explicit memory budget

The current ordinary PDR baseline already retains approximately `846,846,750` bytes in proof-store and quotient structures before completing, under a 1 GiB solver cap. Replay is not yet a behaviorally faithful acceptance authority.

Define the retained RC budget as

$$
B_{\mathrm{RC}}
=
\min(24\text{ MiB},\,0.025\,B_{\mathrm{solver}}).
$$

At a 1 GiB configured solver cap this is 24 MiB.

Use 16 MiB for the first shadow experiment. Promotion to 24 MiB requires demonstrated consumers.

## Proposed 24 MiB allocation

| Component                                              | Budget |
| ------------------------------------------------------ | -----: |
| Packed abstract keys, values, row spans, hash index    |  5 MiB |
| Row headers and packed successor arcs                  |  7 MiB |
| Exact probability table and local-signature dictionary |  3 MiB |
| Bellman/SCC working state retained between slices      |  3 MiB |
| CEGAR witnesses and refinement metadata                |  2 MiB |
| Reconstructible local-row LRU                          |  2 MiB |
| Audit/certificate telemetry                            |  1 MiB |
| Allocator/headroom                                     |  1 MiB |

## Hard secondary caps

* at most 8 active patterns;
* \(|K|\le3\) by default;
* \(|K|=4\) only after smaller patterns demonstrate direct retirements;
* at most 100,000 retained abstract states across the portfolio;
* at most 250,000 retained rows;
* at most 500,000 packed arcs;
* one pattern may retain at most 8 MiB;
* all other individual patterns at most 4 MiB;
* transient exact-projection scratch at most 8 MiB and charged to the solver’s existing peak-work ledger.

## Pressure behavior

* **80%:** stop speculative CEGAR splits;
* **90%:** evict reconstructible cold local rows;
* **100%:** abandon the newest unfinished child and retain the validated parent;
* never discard the only proof data behind a published scalar unless deterministic reconstruction and revalidation are guaranteed.

RCASSP should reserve bytes before generating exact local successors. A row generator may not materialize strict states first and ask the byte ledger afterward.

---

# 12. Exhaustive oracle tests

## 12.1 Generic finite-SSP oracle

Create a mechanics-independent test model with:

* up to three selected goal identities;
* two sides;
* capacities 0–2;
* disposable and persistent occupancy 0–2;
* blocker and protection bits;
* deterministic and stochastic destructive-survival maps;
* rational probabilities from a small set such as
  \(\{0,\frac14,\frac12,\frac34,1\}\);
* costs from \(\{0,1,2,4\}\);
* one- and two-state retry basins.

Exhaustively enumerate every valid state and every action-template instantiation within that domain.

Solve the concrete SSP by exact policy enumeration or rational linear equations and solve every RC pattern/refinement.

For every state assert:

$$
0\le h_{\Pi}(s)\le V^*(s).
$$

Also assert:

* concrete goals map to abstract goals;
* every concrete action/kernel has a covering abstract pair;
* every abstract probability row sums to one;
* paired costs are no greater than concrete costs;
* the abstract Bellman subsolution inequality holds;
* the portfolio maximum remains below the exact value;
* every completed refinement is at least as strong as its parent where the schemas form a true refinement;
* publication by `max(parent,child)` never decreases.

## 12.2 Exhaustive any-\(r\)-of-\(n\)

For every:

* \(1\le n\le5\);
* \(0\le r\le n\);
* subset \(K\subseteq N\);
* assignment of every goal to either side;
* selected-goal status combination;

verify:

$$
\kappa(K)=\max(0,r-(n-|K|))
$$

and prove by enumeration that every concrete terminal satisfies the selected abstract terminal.

Add negative tests that reject:

* \(\kappa\) one larger than the formula;
* arbitrary fixed side quotas;
* taking maximum or minimum over harder fixed-quota targets as a substitute for one disjunctive terminal SSP.

## 12.3 Kernel differential tests

For every reachable state/action in small engine fixtures:

1. obtain the exact `OutcomeDistribution`;
2. push it through the RC map;
3. independently build the RC action row;
4. compare exact successor mass by abstract state;
5. verify the actual pushed kernel is present in the favorable uncertainty set.

Cover:

* selected identity changes;
* side occupancy;
* full-side behavior;
* blockers;
* protection;
* crafted/fractured selected goals;
* persistent/disposable junk;
* destructive survival;
* choice groups;
* restart scope;
* replacement-recovery scope;
* unsupported action fallback.

## 12.4 Automatic-program equivalence

For every small automatic fixture:

* expand the exact micro-program;
* solve its retry basin;
* build the proposed macro `OptionKernel` row;
* compare expected resource cost and projected exits;
* verify choice timing;
* verify almost-sure termination;
* verify that incomplete evidence refuses macro collapse.

## 12.5 Failure-injection tests

The pattern manager must reject or disable a child with:

* probability mass \(<1\) or \(>1\);
* missing actual signature;
* cost greater than the concrete mandatory cost;
* false exclusion of a concrete terminal;
* stale economy or action-vocabulary identity;
* an unpriced mandatory automatic step;
* an unresolved retry continuation;
* a claimed local aggregation whose exact projected successors differ.

## 12.6 Historical regression fixtures

Retain fixtures corresponding to:

* the clean deterministic lower cases from the probability audit;
* the qualified Fracture-local quotient;
* the accepted carrier lower;
* fresh Normal Restart behavior;
* Eldritch/current source-guard fallbacks;
* same-side strict-obligation shadow evaluation;
* automatic-program refusal paths.

The relevant test surface already includes solver, abstraction, exact-calculation, refinement, and quotient-proof suites.

Suggested placement:

* `engine/tests/test_solver_abstract.cpp`: map and terminal tests;
* `engine/tests/test_solver_calc_exact.cpp`: exact pushforward and probability coverage;
* `engine/tests/test_solver_refinement.cpp`: CEGAR/refinement monotonicity;
* `engine/tests/test_solver_quotient_proof.cpp`: proof-manager and ledger authority;
* `engine/tests/test_solver_solve.cpp`: operator consumers and PDR-level regressions;
* optionally, a new mechanics-independent `test_solver_retention_capacity.cpp`.

---

# 13. Production PDR acceptance metrics

Use ordinary fresh runs at the pinned configuration as authority, not current development-checkpoint replay.

Baseline:

* certified lower: `21.7724592843617`;
* verified incumbent: `460678.970156889`;
* proof-store plus quotient bytes: `846,846,750`;
* same-side strict obligations examined: `299,394`.

## Gate RC-0: soundness

Required:

* zero exhaustive-oracle violations;
* zero kernel-inclusion failures;
* zero mass failures;
* zero terminal-inclusion failures;
* zero untyped RC scalar reaching pruning;
* all exposed values pass the existing proof-pattern Bellman/residual contract;
* RC disabled automatically on any source-generation mismatch.

Any violation is a hard stop.

## Gate RC-1: proof-only shadow

Run with RC calculated but not allowed to retire or prune.

Required:

* identical caller action scope;
* identical verified upper and compiled policy;
* identical incumbent provenance;
* root lower unchanged or increased;
* no more than 16 MiB RC retained;
* total wall overhead no greater than 5% on a no-consumer control run;
* no unexpected strict-state materialization caused solely by RC.

## Gate RC-2: consumer liveness

Root-value improvement alone does not pass.

At minimum, before further investment:

* RC must be the selected lower owner on reachable states and operators;
* at least one strict obligation must be directly retired under the existing independent-lower authority;
* at least two distinct carrier/action shapes must consume the bound, avoiding a single-fixture accident.

For production promotion, require either:

1. at least **1%** of the previous `299,394` strict obligations—approximately **2,994 obligations**—to become noncompetitive before materialization; or
2. at least **10% fewer** retained strict rows or proof-store/quotient bytes at the same action scope and no weaker certified lower.

## Gate RC-3: net memory benefit

Required:

* RC retained bytes no greater than 24 MiB;
* strict proof bytes avoided at least twice the retained RC bytes; or
* the PDR witness closes under the existing 1 GiB cap;
* no earlier resource stop;
* no more than 5% additional exact strict states generated by local RC projection;
* action-local row reuse is demonstrated rather than one retained row per strict source.

## Gate RC-4: end-to-end acceptance

Accept for production when all correctness gates pass and at least one of these is achieved:

1. exact PDR closure under the current resource cap;
2. root certified lower at least 1.5 times the baseline, with no increase in proof-store/quotient-plus-RC bytes;
3. at least 10% proof-store/quotient reduction at an equal or stronger lower;
4. at least 20% reduction in strict proof work with wall time no worse than 10%.

An RC run with a higher displayed root lower but zero direct retirees should be stopped, matching the lesson from the previous descriptor and same-side experiments.

## Required telemetry

Per pattern and per operator family record:

* states, rows, arcs, SCCs;
* retained and transient bytes;
* row-build requests and cache hits;
* exact-local versus reusable rows;
* signature-set cardinality;
* lower-owner calls;
* state and operator margin over the previous pattern;
* strict obligations retired;
* rows/materializations avoided;
* fallback reasons;
* refinement dimension and estimated/actual gain;
* probability-inclusion checks;
* terminal-debt classifications;
* new exact strict states caused by RC.

The existing action envelope ledger already has the correct retirement authority and evidence categories, including carrier facts, effect summaries, successor envelopes, refinement contracts, and exact option kernels.

---

# 14. Likely failure modes and stop gates

| Failure                                                     | Consequence                                   | Required response                                            |   |                              |
| ----------------------------------------------------------- | --------------------------------------------- | ------------------------------------------------------------ | - | ---------------------------- |
| Cheap action still pins the minimum                         | No Bellman or pruning effect                  | Stop after two representative runs with zero direct retirees |   |                              |
| Unknown junk classified as persistent                       | Potential overestimate                        | Hard refusal; default unknown to favorable/disposable        |   |                              |
| Whole-kernel correlations replaced by marginals             | Unsound probability relaxation                | Hard stop                                                    |   |                              |
| Action coverage incomplete                                  | Pattern can solve a restricted harder problem | Add scalar fallback or disable pattern                       |   |                              |
| Fixed side quota used for global \(r\)-of-\(n\)             | Can exclude valid terminal routes             | Reject in oracle tests                                       |   |                              |
| Exact selected subset grows globally                        | Recreates strict state explosion              | Enforce (                                                    | K | ), state, row, and byte caps |
| Local rows keyed effectively by every strict state          | No abstraction reuse; memory regression       | Stop family or use one-step nonretained evaluation           |   |                              |
| `calc.outcomes` creates excessive strict successors         | RC consumes the memory it was meant to save   | Reserve budget and cap RC-attributable state growth          |   |                              |
| Action-local junk aggregation lacks equivalence proof       | Wrong projected transition                    | Keep branches separate or refuse                             |   |                              |
| Protection/blocker field omitted despite source observation | Actual kernel may be missing                  | Split or include all complete kernels                        |   |                              |
| Automatic retry not Markov under retained fields            | Invalid macro row                             | Expand micro-state or fall back                              |   |                              |
| Incomplete `OptionKernel` evidence                          | Missing costs or exits                        | Refuse macro collapse                                        |   |                              |
| Zero-cost recurrent component                               | Arbitrary positive fixed point possible       | Least subsolution from zero or fallback                      |   |                              |
| Stale price/scope/vocabulary cache                          | Wrong cost or action coverage                 | Generation-keyed invalidation                                |   |                              |
| False abstract terminal with retained mandatory debt        | Lower can incorrectly terminate               | Terminal oracle hard stop                                    |   |                              |
| RC value sent to executable planner                         | Repeats rejected upper-policy projection      | Proof-only typed path                                        |   |                              |
| Root lower rises but no consumer                            | Cosmetic improvement                          | Do not ship                                                  |   |                              |
| RC bytes exceed avoided proof bytes                         | Net-negative memory                           | Stop refinement and retain parent                            |   |                              |
| Replay differs from ordinary run                            | Invalid acceptance comparison                 | Use fresh ordinary runs only                                 |   |                              |

---

# 15. Exact source integration points

No code changes are proposed as part of this review. The smallest implementation seam would be:

| Existing source                                                                 | Integration                                                                                                                                                                                                                                              |
| ------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `engine/src/solver_proof_pattern_manager.hpp`                                   | Add one aggregate `ProofPatternKind::RetentionCapacity`. The internal RC portfolio selects its own strongest pattern; only one typed `ProofLowerValue` crosses into pruning. Action-local RC rows may continue using `OperatorLower` with RC provenance. |
| `engine/src/solver_solve_types.hpp`                                             | Add packed RC state, row, signature, portfolio, CEGAR work, byte-ledger, and diagnostic structures. Reuse `CarrierFacts`, `CarrierEffectSummary`, and `CarrierSuccessorEnvelope`.                                                                        |
| `engine/src/solver_solve_bounds.cpp::prepare_goal_cover_cost()`                 | Seed the initial RC portfolio after the existing universal/clean/carrier sources are available. Reuse the current goal-reach, survival, pricing, and caller-scope authorities. Do not globally materialize RC rows here.                                 |
| `engine/src/solver_solve_carrier_pattern.cpp::completion_proof_lower_value()`   | Query the aggregate RC state lower and include its typed value in the existing maximum. Local unavailability falls back to current universal/clean/carrier/strict values.                                                                                |
| `engine/src/solver_solve_operator_proof.cpp::operator_proof_lower_value()`      | Add the exact action-local RC pushforward and take the maximum with the current operator lower. Keep `retire_unmaterialized_by_operator_proof()` as the consumer.                                                                                        |
| `engine/src/solver_action_envelope_ledger.hpp`                                  | Reuse `IndependentGlobalLowerVsVerifiedUpper`; do not add a parallel retirement state machine. Add RC evidence/provenance only if needed for diagnostics.                                                                                                |
| `engine/src/solver_solve_envelope_proof.cpp`                                    | Reuse the existing shallow minimizing-action refinement workflow to schedule RC action-local CEGAR work and preserve the independent fallback on incomplete rows.                                                                                        |
| `engine/src/solver_calc_types.hpp`                                              | Treat `OutcomeDistribution` and `OptionKernel` as probability/resource authorities. RC owns projection, not mechanics.                                                                                                                                   |
| `engine/src/solver_action_family_contract.hpp`                                  | Obtain observer/refinement dimensions from exact source contracts. Family labels remain telemetry keys only.                                                                                                                                             |
| `engine/src/solver_options_automatic.cpp` and `solver_options_semantics.cpp`    | Supply automatic-program definitions and complete option kernels; RC does not rederive their rules.                                                                                                                                                      |
| `engine/src/solver_solve_strict_pattern.cpp::prepare_strict_clean_goal_cover()` | Use as implementation precedent for exact kernel retrieval, pushforward, shared rows, unsupported-shape refusal, and fallback.                                                                                                                           |
| `engine/src/solver_solve_telemetry.cpp` and `solver_solve_telemetry_json.cpp`   | Add pattern ownership, bytes, signatures, build/cache, refinement, fallback, and direct-consumer telemetry.                                                                                                                                              |
| `engine/src/solver_solve_audit.cpp`                                             | Add PDR shadow comparison and strict-obligation consumer audit.                                                                                                                                                                                          |
| `engine/src/solver_solve_finish.cpp` and current owned-byte accounting          | Charge all RC retained and transient bytes to the existing solver cap. Do not create an unaccounted auxiliary cache.                                                                                                                                     |
| `engine/src/solver_anytime_scheduler.hpp`                                       | Schedule RC preparation/refinement as proof work. RC values may affect proof urgency only after validation; they do not become executable policy authority.                                                                                              |
| `engine/src/solver_executable_carrier_planner.hpp`                              | **No proof integration.** Do not convert its descriptors into a scalar lower or policy. Any generally useful immutable projection descriptor should first be moved to a neutral proof/source type.                                                       |

The current lower call path already provides the right two seams: preparation in `prepare_goal_cover_cost()` and typed selection in `completion_proof_lower_value()`.

The current operator path already prices primitive or automatic first steps, projects survival/reachability, compares against the verified incumbent, and retires ledger obligations. RC should strengthen that path rather than duplicate it.

---

# 16. Mechanics decisions for Oliver

No rule below should be answered from outside research or inference. The project instructions make the engine and Oliver’s decisions authoritative for mechanic ambiguity.

| ID      | Oliver decision required                                                                                                                 | Safe default until decided                                                              |
| ------- | ---------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------- |
| **O1**  | Exact source of prefix/suffix capacities and every condition that changes them                                                           | Retain source capacity; assume no numerical constant                                    |
| **O2**  | Whether each goal slot has a fixed side or can have state-dependent side identity                                                        | Retain actual source side; refuse side-invariant reuse                                  |
| **O3**  | For each action family, which non-goal occupants are guaranteed disposable, guaranteed persistent, or identity-observed                  | Unknown becomes favorable/disposable or specialization unavailable                      |
| **O4**  | Exact blocker definition and which blockers alter legality or projected probabilities                                                    | Retain source blocker bit where available; otherwise favorable unblocked kernel         |
| **O5**  | Exact protection effects on legality, victim selection, survival, and probability                                                        | Split by source flag or include every complete flag-specific kernel                     |
| **O6**  | Joint destructive-survival distribution for selected goals and junk                                                                      | Use exact outcome kernel only; no independent survival assumptions                      |
| **O7**  | Which actions observe physical affix identity rather than only class/count                                                               | Exact local branches or refusal                                                         |
| **O8**  | Whether two member identities in one current member class are interchangeable for each action                                            | Treat as distinct when source proof is absent                                           |
| **O9**  | Exact automatic-program step order and which costs are mandatory versus contingent                                                       | Micro-expand or use complete `OptionKernel` only                                        |
| **O10** | Retry reset state, saved-state reuse, accumulated observations, and Markov sufficiency                                                   | Retain retry state explicitly; no geometric shortcut                                    |
| **O11** | Timing of observed choices relative to random outcomes                                                                                   | Preserve source timing; never move an unobserved chance event into a real policy choice |
| **O12** | Exact distinction between economic Restart and mechanic-owned replacement recovery                                                       | Use only the caller-authorized economic action as an ordinary action                    |
| **O13** | Complete strict terminal requirements for junk, crafted occupancy, protection, rarity, and other debt                                    | Delegate to source terminal predicate; unknown fields relaxed                           |
| **O14** | Which influence/Eldritch/Veiled/Split/Corrupted/metamod fields each refined action observes                                              | Retain or split only when the source contract says they matter                          |
| **O15** | Whether any action probability depends on a dropped pool variable not represented by the proposed signature                              | Enlarge signature or use a complete favorable uncertainty set                           |
| **O16** | Whether `OptionKernel` completeness and almost-sure-termination evidence is sufficient for RC macro projection in every automatic family | Family remains unavailable until explicitly accepted                                    |

For every unanswered item, the implementation has only three sound choices:

1. retain/split the variable;
2. grant a demonstrably favorable normalized relaxation;
3. mark the RC specialization unavailable and fall back.

It must not assign a Path of Exile rule.

---

# Proposed first experiment

For the fixed four-goal PDR witness:

1. enable a 16 MiB proof-only shadow portfolio;
2. build one side/capacity parent;
3. build four exact selected singletons and the two most relevant same-side pairs;
4. retain only source-certified persistent/disposable counts, blocker bits, and protection;
5. disable automatic macro programs;
6. perform exact local pushforward only for strict obligations whose current lower is near the verified incumbent;
7. record would-retire counts without activating pruning;
8. stop immediately if no direct strict-obligation consumer appears or RC-attributable strict-state growth exceeds 5%;
9. promote to a 24 MiB pruning experiment only after kernel-oracle soundness and consumer-liveness gates pass.

That experiment directly tests the missing hypothesis: whether **retention and capacity at the strict obligation**, rather than another transfer of the current coarse lower, can make expensive broad rows noncompetitive before they consume the PDR proof budget.

No repository files were modified.
