# Review: dirty progress, useful abstractions, and native headroom

**Reviewed main:** `0ca76eb4a9f35a90299ecceeab740e5535804fd9` (September 11, 2026).  
**Evidence posture:** read-only connected GitHub inspection; critical intake of two owner-supplied reports; selected primary literature; new exact synthetic examples. No repository mutation, native build, solver run, simulator run, or local Codex-thread access. All native numbers below are retained repository measurements. Source IDs resolve to pinned URLs in `source_register.json`.

## Decision

The next push should make useful dirty intermediate items and broad acquisition actions competitive, not simply tighten the old clean-first controller. Three issues are separable:

1. a real first-policy ordering preference against dirty transitions;
2. expensive native continuations whose values remain unknown because the checker caps out;
3. representation distinctions maintained for actions that a private policy-search phase need not use.

Their proposed remedies should share existing solver owners. Headroom is the first experiment, action-scoped coarsening is the first representation experiment, and preserving valuable partial progress is the substantive controller change. Symbolic family/query work is a conditional fourth step, not a wholesale rewrite.

## 1. Main has moved beyond the supplied reports

The reports reviewed `84f02ee3...`; main now contains the subsequent first-return improvement. The current ordinary Ring controller is **149,977.25092497544**, a 26.7557408612% reduction from the preceding stronger 4-GiB result **204,763.14825000268**. Amulet remains **40,215,428.995558396**. No new exact closure or two-family win is reported. [R01, R02]

The successful change was a current-run Exalt excursion plus a paid native return, independently checked before repetition and again as a complete emitted strategy. It is not a guessed probability or a source-level shortcut. The first-root old value was 938,064.6067502735; that is different from the later stronger capacity reference. [R02, R05]

The broader gated Chaos candidates reached **200,000 checker states** and were removed. Their complete one-shot Q and first-exit reward/probability laws are unknown. Their 4,596/5,862 constructed bridge rows cannot be substituted for whole-policy evaluation. This is not evidence that broader reforges are bad. [R02]

The original Ring result has a quality/resource tradeoff: native time rose about 47.44%, peak solver-owned estimate about 56.12%, while its policy became much cheaper. Extra interrupted strict work contributed to that footprint. These are recorded observations, not a matched causal allocation proof. [R02]

The new Ring has expected action count about 26,175.42. Its 1,000 original-limit simulation trials produced 978 goals and 22 action-limit stops, not 1,000 successes. All failures remain in the denominator. Independent properness and a finite expected cost do not guarantee completion within a finite operational action cap. [R02]

## 2. Junk aversion: a precise source-level result, not a blanket diagnosis

### 2.1 Deriving the current score

`select_joint_policy_seed_row()` uses a first-policy preference based on

`missing goals + extra explicit affixes + rarity mismatch`.

For the distinct physical goal modifiers in the current Ring/Amulet fixtures, write:

- m: number of required goals;
- g: number of satisfying goals present, with g<=m;
- k: total explicit affixes;
- correct requested rarity held fixed.

For a nonterminal item the score is

\[
D(m,g,k)=\max(1,(m-g)+(k-g))=\max(1,m+k-2g).
\]

A true native goal is handled separately as D=0. The current empty Rare has D=m, and a nonterminal redraw successor strictly improves D only when **2g>k**. [R04; derivation here]

| Request/output | Debt at empty source | Successor debt | Counted as strict progress? |
|---|---:|---:|---|
| Two goals; both present among four affixes | 2 | 2 | No |
| Two goals; both present among five/six affixes | 2 | 3/4 | No |
| Three goals; two present among four affixes | 3 | 3 | No |
| Three goals; all three among four/five affixes | 3 | 1/2 | Yes |
| Three goals; all three among six affixes | 3 | 3 | No |

This is a structural bias in the first-policy score. Under the native ordinary rare redraw count of four to six, the Ring's two-goal primitive redraw cannot strictly improve this debt in one step, even when it rolls both desired mods. [R04, R12]

The seed's lexicographic key also places the number of missing successor routes before attempt cost. One incomplete route can win over two even when the former action is vastly more expensive. `selection_values` is unused by this seed function. These observations explain why it is good at producing a small easy-to-complete first policy, not why that first policy should be economical. [R04]

### 2.2 What this does NOT prove

The rule is conditional on having no incumbent. Afterward, ordinary quantitative improvement exists, and the seed uses different progress criteria. It is not a hard global removal of dirty states, a legality restriction, or an admissible lower. Compound programmes can also improve the terminal condition over multiple native actions. Therefore the source derivation alone does not prove that replacing the score improves a native run. [R04, R03]

The actual distinction is between **fast first-policy construction** and **later optimal-cost discovery**. Preserve the first verified graph. Trace whether the useful dirty continuation is never serviced, is serviced but cannot be completed, or is completed and genuinely loses.

### 2.3 Exact synthetic counterexample

The companion script contains this invented finite SSP (unit costs on the dirty branch):

| State/action | Outcomes |
|---|---|
| Root dirty acquisition, cost 1 | d with probability 1 |
| d cleanup attempt, cost 1 | e with 1/2; f with 1/2 |
| e cleanup attempt, cost 1 | goal with 1/3; h with 2/3 |
| f paid recovery, cost 1 | d with probability 1 |
| h paid recovery, cost 1 | d with probability 1 |
| Root clean alternative, cost 100 | goal with probability 1 |

Assign d two goals/two junk, e two goals/one junk, and the losing branches fewer goals. All failure probability and recovery cost are retained. The equations are

\[
J_f=J_h=1+J_d,\quad J_e=1+\tfrac23J_h,\quad
J_d=1+\tfrac12J_e+\tfrac12J_f.
\]

They give **J_d=14, J_root=15**, versus 100 for the clean alternative. The dirty controller is proper, yet its first acquisition ties the root's terminal-debt score. This refutes a general inference from “does not reduce junk/debt now” to “cannot be economically useful.” It is not a PoE recipe or measured native result.

### 2.4 When skipping premature cleanup really is safe

If cleanup is paid, cannot itself terminate successfully, and leaves the subsequent redraw's legality, cost, persistent state, observation timing and complete conditional law identical, cleanup followed by that redraw is weakly dominated by the redraw alone. Couple the redraw outcomes: both continuations see the same law, but cleanup paid an additional nonnegative cost.

The premises matter. Removing a blocker, changing capacity, preserving a lock, or introducing a usable observation can change that law. A junk label does not establish irrelevance. Test the actual native predicates rather than asserting “never clean before reforge.” Final success remains the original junk-free target.

## 3. The checker bottleneck is distinct from search RAM

Current `evaluate_return_graph()` sets evaluator state capacity from the ordinary search state's limit, uses corresponding row/transition limits, and applies a hard 1-GiB evaluator ceiling. It charges reforge work cumulatively and retains cooperative checkpoints. [R05]

A 4- or 8-GiB main solver with the same 200,000-state/1-GiB candidate checker may therefore reject the same broad graph. The new permission should reach the actual checker, with independent typed controls and aggregate remaining-budget enforcement. It must not weaken the checks or allocate a hidden second memory budget.

A prior observed host had approximately 63.7 GiB physical memory and 41.1 GiB available. That motivates an 8-GiB native research profile and a targeted 16-GiB follow-through after fresh host admission; it does not establish today's free capacity. [R03]

Resource curves measure achievable work, not a change in asymptotic complexity. Examine completed candidate/row targets, checker states/pairs, phase cost and peak scratch. More time cannot improve a fixed policy after its exact expected cost is established; it can only allow a different or previously unfinished candidate to be evaluated.

## 4. Report 1: targeted coarsening is worth testing, but the gains are unmeasured

The report's strongest contribution is a specific observer dependency, not an argument for deleting whole action families. Current registry source confirms that targeted Harvest reforge/augment tags choose their roll pools and are not declared persistent state discriminator tags. Resistance conversion declares source, target and resistance, while some bench dependencies add other distinctions. [R06]

The supplied report attributes CB08's four persistent tags to two admitted conversions into lightning. It reports a 31-candidate/21-junk-class starting point, but explicitly did not execute the leave-one-out rebuilds. The actual changed class counts, row support and speedup are unknown. Preserve this limitation. [I01]

Both single deletions and the pair matter because dependencies overlap. In the supplied new synthetic example, four invented modifier signatures remain four classes after either single deletion but merge to one after the pair. That is a demonstration of nonadditivity, not a prediction for native CB08.

Current `CalcContext` includes fixed and conditional programme dependencies in the layout action set. Automatic mode also retains every explicit-affix modifier in the reachable universe. Removing a non-discriminating candidate need not remove its modifier identities. [R07, R08]

The class key includes the previous parent's junk-class ID when `refinement_parent_layout` is supplied. That enforces refinement, not coarsening. A private coarser context needs a new namespace and valid initial/entry projection. It cannot reinterpret live state IDs or incumbent vectors. [R08]

### Upper search and full-scope proof have opposite requirements

For a legal action subset A' of A,

\[
V_A^*\le V_{A'}^*.
\]

A proper controller found under A' can still be evaluated as a legal controller of the original request. Its evaluated cost is an upper on V_A*. But its restricted optimum/lower is not generally a lower on V_A*. Excluded actions remain in the original proof obligations. Local retirement at one source does not justify deleting a discriminator everywhere, especially after classes are merged.

A complete mapping of old class members and projected probabilities can measure how many successor entries merge. It is not a proof that the remaining native conditional draw computation is smaller, nor that the emitted graph's independent evaluator will visit fewer physical states. Measure state storage, successor support, recurrence work and checker work separately.

## 5. First-return improvement should not require erasing useful progress

The current Ring bridge is legitimate but restrictive. Its uncovered entry is a desired FireResist8 suffix; the paid old-controller route deletes it to return to the empty Rare. The new Exalt excursion is then repeated. On Amulet the same Exalt at an already-Exalt root just reproduces the old controller. [R02, R05]

This supports exploring new continuations, not declaring those native Annul operations incorrect. We should consider keeping useful dirty or partial entries instead of automatically forcing the old policy's domain. A boundary needs actual observable state/control identity and an executable continuation; root-only evaluation does not provide one everywhere.

### 5.1 Complete local response

For a fixed finite controller on internal R and absorbing boundary B, assume almost-sure exit and finite expected internal cost. Then P_RR is transient and

\[
g=(I-P_{RR})^{-1}c,\qquad H=(I-P_{RR})^{-1}P_{RB},\qquad
J(v_B)=g+Hv_B.
\]

The derivation is the fixed-policy equation `J=c+P_RR J+P_RB v_B`. Solve the sparse systems without constructing a dense inverse. Rows of H have total exit mass one when B covers every eventual exit. Changed boundary values can reuse a genuinely unchanged g/H; changed internal decisions cannot.

For two internal states the supplied exact example gives

\[
g=(24/7,20/7),\qquad
H=\begin{pmatrix}4/7&3/7\\1/7&6/7\end{pmatrix}.
\]

Boundary values (3,9) yield entry values (9,11), exactly matching the direct system. Replacing the response by a context-free cleanup cost loses this dependency.

### 5.2 One-shot ranking is not final-controller ranking

For a single return with goal probability p>0, expected excursion cost r and old continuation cost J,

\[
Q(J)=r+(1-p)J,\quad J_{\mathrm{repeat}}=r/p,\quad
J-Q(J)=p(J-J_{\mathrm{repeat}}).
\]

The native Ring result already demonstrates strong amplification. The implementation should not rediscover this identity as a new theorem. [R02]

There is another ordering consequence. At old J=10, a candidate with p=.9 and repeated cost 8 has a one-shot gain 1.8. A candidate with p=.01 and repeated cost 1 has one-shot gain only .09, but yields the much cheaper repeated controller. Ordering only by raw one-shot improvement can prefer the weaker final policy. This is an exact synthetic illustration, not a prescription to ignore computation cost or tail risk.

If several partial entries are revisited under new decisions, their boundary system must be solved jointly. Two locally exiting components can cycle forever when composed. Include the actual control phase in the Markov state; a private return STOP is not a public success terminal.

## 6. Report 2: use symbolic ideas to avoid work, not to add another representation layer

The report correctly preserves correlation between action parameters and the native time at which choices are made. For a fixed finite continuation v,

\[
F(v)=\min_{a\in A_F}(c_a+p_a\cdot v).
\]

The minimum over the convex hull of the actual correlated (c,p) pairs is the same: a convex combination's objective is a weighted average and cannot beat the least endpoint, while every endpoint remains feasible. This is elementary linearity, not a claim that the hull is cheap to construct. A loose outer envelope can be optimistic; it is not an executable action. [I02; proof here]

The best family member can change with continuation values. The synthetic pair `1+.9x` and `5+.1x` switches at x=5. Combining price 1 with failure probability .1 invents an unavailable action. Likewise, choosing an action after seeing its future random outcome can produce a fictitious cost 1 instead of the valid precommitted expectation 51.

A more useful interface may be a frozen expectation query rather than a full successor list. At a complete stopping line of the native draw process,

\[
E[v(Y)]=\sum_h P(h)E[v(Y)\mid h].
\]

If the conditional continuation is uniformly known, unnecessary suffix work and interning can be skipped. Uniform lower information gives a lower query; it does not give an exact fixed-policy kernel. The native exclusion/remaining-weight law, guarantee phase and observations must still be preserved. Merely compressing outputs after the full recurrence is not the proposed saving.

### Primary literature and limits

**Factored action SDP — Raghavan et al., AAAI 2012.** The publisher abstract explicitly supports exploiting both state and action variables and warns of extra memory, addressed through recursive conditioning. Use this as motivation for constrained, unresolved action choices; it does not establish an SSP-specific guarantee or native speedup. The publisher's migration timestamp is 2021, but the conference issue is 2012. [L01; abstract/metadata inspected]

**Pareto caching — Watanabe et al., CAV 2024.** The inspected overview shows why one component's preferred controller changes with boundary weights, and uses demand-driven component queries. Its supplied compositional reachability model and sound stopping results are not automatically expected-cost policy synthesis for an implicit crafting graph. Section 2.4 explicitly separates sound stopping from guaranteed finite termination. We should find a small useful native interface, not import a component library first. [L02; publisher HTML introduction/overview inspected]

**Dice — Holtzen et al., OOPSLA 2020.** The inspected sections explain factored discrete inference, conditional interfaces and local structure. Section 6.2 gives favourable composition under suitable dependencies; general BDD composition can still multiply representation sizes, and Appendix D highlights ordering sensitivity. Its bounded/nonrecursive language does not directly solve cyclic crafting policies. A native query pilot must skip actual work and earn its memory cost. [L03; HTML sections 1–3, 6.2 and Appendix D inspected]

These are adaptation leads, not novelty claims or reasons to install new dependencies. No published benchmark speedup is attributed to poecraft2.

## 7. What should change in the development decision

The old question “can we verify this within the unchanged 200k cap?” has been answered negatively for the broad candidate. Oliver now permits changing that premise. Finish its evaluation under a realistic native envelope first, while cheaply checking whether removing two unused observer actions makes the search representation materially smaller.

If a complete broad candidate is still expensive, stop treating that as a verification problem. Improve its actual dirty-state decisions and partial-entry response. If it is cheap but too expensive to verify routinely, the corresponding exact grouping/query is the selected computational problem. If the pair coarsening helps search but not the independent checker, report those as separate outcomes rather than promising both.

This yields a falsifiable programme rather than a list of attractive architectures. Preserve old useful policies and the true final goal throughout. Do not demand every intermediate item be clean, and do not pretend every dirty item is harmless.

## 8. Authored knowledge delta

| Proposition | Basis | Destination / disposition |
|---|---|---|
| First-policy debt only rewards empty-source primitive progress when 2g>k under stated premises | Current function and algebra; dirty SSP counterexample | Search/resumption mathematics and actual scheduling source map; ordering, not lower authority |
| Candidate checker headroom is not the same as main solver RAM | Current evaluator assignments and capped candidate receipts | Resources/options/tooling; typed control and aggregate accounting |
| Pair observer deletion can coarsen a private policy search without proving the pair globally irrelevant | Current layout/dependency source, report hypothesis, subset inequality | Representations and upper policy scope; no automatic all-action lower |
| A nonempty progress entry may be a better continuation boundary than forced empty return | Actual Ring erased desired suffix; complete local response argument | Policies/return bridges; requires native observable coverage and proper global composition |
| One-shot advantage and repeated-controller improvement have different scale | First-return identity and ranking counterexample | Existing return-bridge argument; do not assign a new ID merely for algebra |
| Fixed query equivalence is weaker than universal behavioural equality | Conditional expectation; changed-value and observer counterexamples | Representation/lower events only if a real consumer is built |

The companion `synthetic_checks.py` runs **38 exact rational/algebraic checks**. They reproduce the examples, not general native proofs, performance or the unexecuted layout ablation. No native current leave-one-out result is claimed. The successful Ring and capped broad policies in this review are read from repository evidence; they were not rerun here.
