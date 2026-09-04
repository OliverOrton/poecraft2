# Free-value Bellman certificates for four/five-goal closure

Research for OliverOrton/poecraft2, 2026-09-04. **Research evidence; no production qualification or active implementation boundary.**

## 1. Current state and measured evidence

The main worktree was read at `c9530ac941b95e68a1e3d890caf00d53c9e6d774`, matching the supplied baseline. It had no tracked or staged changes; protected untracked `0` was never opened or touched. AGENTS, the documentation map, direction, HANDOFF, all three clean-five archives, current source contracts, and original rejection records were read. HANDOFF still says no implementation boundary is active. The research was performed outside the repository; this review copy was subsequently archived here at Oliver's request. No new experiment was run during archival. No push, commit, public action, production edit, broad suite, full alternative census, crafting solve, or Simulator run was performed.

The attachment did **not** include `ssp_certificate_experiments.py`; a scoped filename search in attachments and the repository found no copy. The adjacent [independent reference](free_value_fixtures.py) is new research code, not a claim to have used the missing companion.

The accepted clean-five facts remain:

| Evidence | Result and limitation |
| --- | --- |
| Executable incumbent | Cost `85408.64362148782`; success probability one; off-policy mass zero; 775 nodes / 1,634 edges. This is an evaluated upper, not an optimum. |
| Public lower | `36.48853172876641`; upper-minus-lower `85372.15508975905`. This is a certificate interval width, not a measured error from the unknown optimum. |
| Strategy identity | SHA-256 `EA59F23F770FF2A969774266FA34C58FB23CB16A6B2E46C25B6CAFA1B3C1FF1A`. |
| Retained full census | 27,021 reached exact entries, 671,410 applicable alternatives, zero existing-lower retirements. It was not repeated. |
| Retained bounded r9 | 30 constraints; 10 internal and 20 escaping; 12 generic exact rows, 26,064 transitions; action-complete false, dependency-closed false. |
| Fracture | Nine rows, each with all six outcomes outside the policy-entry domain. Shadow RHS `405.41175`; exact deviation Q unavailable. |
| Latest Scour | Global-start continuation `failure_reachable`; zero candidate entries/SCCs; one refused and zero complete dependency kernels. It did not hit the 256-entry bound. |
| Internal temporary-bench control | RHS `85622.5980428518` versus source J `85408.64362148767`, with five successor contributions. One passing row does not certify the successors' complete action sets. |

Sources: [latest Scour archive](../2026-08-31-clean-five-policy-potential-scc-cegar-v1/README.md), [Bellman census](../2026-08-31-clean-five-verified-policy-bellman-subsolution-deviation-gap-census-v1/README.md), and [verified-policy alternative census](../2026-08-31-clean-five-verified-policy-alternative-proof-rc-consumer-v1/README.md).

The Scour refusal establishes that this incumbent controller cannot provide a proper continuation from that exact item. It establishes neither unsolvability nor the absence of a useful lower bound. No refused continuation was assigned an exact Q-value in this research.

New measurements are deliberately smaller: exact rational fixtures, calls to current native sparse arithmetic and the shadow validator, setup-only lower queries on two source shapes, and a bounded exact micro-kernel export. They answer certificate/ownership uncertainties; they do not requalify the product.

## 2. Ranked recommendation and strongest objection

**The most promising route is a free-value lower model in the existing quotient proof graph, refined from its own cheapest optimistic policy.** Retention/capacity and program-phase refinement are how that model can become useful. Another fixed-J domain expansion is not the default next step.

| Rank | Route | Assessment |
| --- | --- | --- |
| 1 | Add a lower-only consumer to existing `QuotientBellmanGraph`; reuse sparse row/SCC machinery; cover missing actions explicitly; refine one minimum-owning program/retention assumption | Removes the unnecessary incumbent-domain restriction and reuses retained graph ownership. Theory and small native arithmetic support it. Medium/five-goal strength and affordable procedural queries remain unproved. |
| 2 | Projected expectation or small correlated kernel for the minimum-owning lower action | Becomes the first representation task if a complete row needs broad physical expansion. Compute only the statistic the proof consumes, with an explicit pushforward proof. This avoids rebuilding the rejected full shared reforge DAG. |
| 3 | Fixed-J SCC closure on the existing internal control | Useful opportunistic certificate/control if action-complete closure is cheap. It cannot establish feasibility for broad clean-five escapes. Keep it independent of ranks 1–2. |
| 4 | Initialize candidates with J, then repair downward | A numerical tactic within rank 1. Repair may reveal a genuinely cheaper policy. It is not an alternative source of authority and must end in complete inequality validation. |
| 5 | CG-style temporary inactivity/separation | Potential later optimization after dependency/query cost is measured. Keeping a scalar cap is simpler and sound today. Dropping constraints and hoping to reactivate them later cannot publish a lower. |

The strongest objection is concrete: **the available optimistic actions pin the lower before the choice of LP versus SCC matters**. Current relaxations permit favorable source clearing, preservation and recovery, and incomplete automatic families require cheap placeholders. Tightening those may cost the same exact state/transition work rejected previously. The new medium phase query raises one projected native program RHS from **7.1136 to 39.7739**. Its uniform lower/pushforward contract and complete remaining coverage are not yet established; common-floor placeholders still pin the conditional reference model. Rebuilding the donor takes 4.35 seconds despite a 43-microsecond row query. The present research establishes a valid integration point and a specific donor reuse candidate, not practical clean-five closure.

There is therefore a **no-go for broad domain expansion or public lower promotion now**. The implementable next boundary in section 7 is deliberately limited to the missing lower consumer, complete coverage contract, and one evidence-selected refinement. It is a proposal for Oliver, not an activated milestone.

## 3. Theory, preconditions and falsifiers

Let the original objective minimize nonnegative expected cost among policies reaching success almost surely; equivalently, assign infinite cost to non-successful paths. Let R be finite, success have value zero, and x be finite and nonnegative on R. At every modeled state, cover every legal action with one of:

1. An exact row: `x(s) <= c(s,a) + sum_inside P(t|s,a)x(t) + sum_outside P(t|s,a)b(t)`.
2. A scalar optimistic action: `x(s) <= L(s,a)`, with independently proved `L(s,a) <= Q*(s,a)`, including immediate and mandatory program costs.
3. A whole-family scalar: `x(s) <= L_F(s)`, proved no greater than the minimum Q over every action in the identified family.
4. A whole-kernel relaxation with an explicit proof that it is optimistic for the continuation functions and action scope actually used.

Outside b must independently satisfy `b(t) <= V*(t)`. It need not be a continuation of the incumbent or a Bellman-consistent vector outside R. Zero is an available conservative boundary under nonnegative costs. Infinity needs separate qualitative proof; a router refusal provides none.

**First-exit proof.** Fix any proper original policy. Stop at success, first exit from R, or selection of a scalar-covered action. Before stopping, conditionally apply the exact-row inequalities and telescope. At exit, b is bounded by the policy's remaining cost; at a scalar stop, L is bounded by the complete remaining action-plus-continuation cost. Properness makes the stopping time almost surely finite. The unfinished potential is at most `max_R x`, so its expected tail vanishes. Nonnegative cost permits monotone convergence. Thus x is bounded by every proper policy's cost, and hence by V*. Cycles are handled simultaneously; no selected-policy equality is required.

This proves **feasible finite candidates sound**, not that any numerical solver finds the strongest candidate. On a finite relaxed model with finite boundary payoffs and a proper finite-exit policy for all retained relevant states, the bounded LP maximizer equals its proper-policy optimum. A positive sum objective requires all variables bounded; an irrelevant unsolvable state can make that objective unbounded even when the root is finite. A mere graph path to goal is weaker than a proper-policy witness.

| Claim | Verdict / necessary condition |
| --- | --- |
| Boundary lower needs incumbent routing | Refuted. Independent admissibility suffices by first exit. |
| Every finite all-action subsolution is a lower under proper/failure-infinity semantics | Proved under the conditions above. Missing action coverage defeats the proof. |
| Scalar placeholders preserve coverage | Yes: interpret them as explicit optimistic terminal-cost actions, or stop the proof when selected. They may be very weak. |
| A partial LP with missing constraints gives a lower automatically | False. Removing constraints enlarges the feasible region and can increase the objective above V*. Tentative values have no authority until complete separation/checking. |
| Adding states always improves the model lower | False for an admissible but inconsistent boundary. Require a feasible lift of the old certificate or a proved refinement relation. |
| Raising an independently valid scalar cap / outside boundary on a fixed model is monotone | Yes: the corresponding constraint relaxes and the feasible set expands. Replacing a scalar by an exact row is not automatically such a refinement. |
| Existing admissible lower can be imposed as x's coordinatewise floor | Not automatically. It can exceed the optimum of a more optimistic finite model and make that LP infeasible. Keep independent certificates separately if necessary. |
| Pointwise max of independent admissible lowers is safe | Yes as an external lower portfolio. It need not satisfy the new model's Bellman inequalities. Max of subsolutions of the **same** monotone model does preserve subsolution status. |
| Zero-cost cycles invalidate all free-value proof | No. Finite subsolutions remain sound for proper/failure-infinity cost. They need not lower-bound ordinary accumulated cost, and bottom VI can choose a different fixed point. |
| Small Bellman residual is small value error | False without a certified horizon/conditioning bound. Residual amplification follows the fundamental matrix `(I-P)^-1`. |
| Primitive proof summarizes mandatory programs | Yes if setup, phases, complete legal decisions, observation timing, prices, failure and exits are preserved or explicitly relaxed optimistically. A fixed chosen recipe does not cover all available choices. |

Exact fixtures precede the native work. The [33 research records](synthetic-results.json) include independently enumerated proper stationary policies, rational LP vertices, and a separate rational simplex implementation. Policy enumeration and LP share the row encoder; simplex does not share Gaussian elimination. They are useful reference cross-checks, not sufficient engine validation.

| Required falsifier | Exact construction and result |
| --- | --- |
| Proper incumbent 10, cheaper deviation | `r ->goal` costs 10; `r ->t` costs 1; `t ->goal` costs 4. Frozen J=(10,4) fails. Free-value optimum is (5,4). Boundary zero yields only root 1; adding t reaches 5. One Bellman repair takes J to (5,4). |
| Optimal root, no incumbent route at t | Root has direct cost 10 and a cost-1 deviation to t; t's true value is 20. Candidate h(root)=10, h(t)=9 passes. A global incumbent route at t is unnecessary. |
| Proper stochastic two-state cycle | `s=1+(1/2)t`, `t=2+(1/4)s`; remaining mass reaches goal. Values are `16/7,18/7`. Root-only boundary-zero proof yields 1. Native SCC arithmetic agrees. |
| Missing action / duplicate coverage | Terminal actions cost 10 and 5. Keeping the 10 constraint twice permits 10 despite the true optimum 5. Explicit cheap placeholder 2 yields 2; refining it to 5 yields 5. |
| Destructive cleanup/rebuild | Dirty goal carrier: cleanup costs 2 and loses a required property; rebuilding costs 8. True recovery 10; free preservation gives only 2. A second shape with rebuild 11 gives 13. One lost-property state is sufficient in these fixtures. |
| Zero-cost improper loop | A zero-cost self-loop competes with terminal cost 10. Proper-policy optimum/max LP is 10; ordinary accumulated-cost optimum/bottom VI is 0. Greedy ties need not produce a proper executable policy. |
| Long retry horizon | Cost 1, success probability `10^-8`: V=`100000000`. Candidate V+100 violates its inequality by only `10^-6`, smaller than a `10^-12 * value` tolerance. Accepting that tolerance would accept an overstated lower. |
| Admissible, inconsistent lower | `s ->t` costs 1; `t ->goal` costs 5. L=(6,0) is admissible, but 6>1+0. R={s}, boundary t=0 solves to 1; adding x(s)>=6 makes this model infeasible without making the original problem infeasible. |
| Exact domain enlargement counterexample | `r ->t ->u ->goal` costs 0,0,10. Independent b(t)=10,b(u)=0 is admissible. R={r} proves 10; R={r,t} proves only 0 against unchanged b(u). No kernel was weakened. |
| Mandatory setup / observation | Setup 3 plus mandatory operation 2 equals macro cost 5. Two equally likely offers with option costs (0,10) and (10,0) give 0 when choice follows observation but 5 when choice precedes it. Collapsing away that decision can overstate a lower. |
| Self in an observed offer | Cost 1, half goal, half offer {self,t=100}. Correct value is 2. Ignoring self whenever the other option is finite gives 51. This breaks the envelope donor, not the shared sparse evaluator. |

Candidate repair may decrease values and must invalidate/check incoming dependencies. A uniform epsilon subtraction is not a universal repair: closed edges and long retry horizons require structural/quantitative treatment. Preserve an old certified lower as a separate portfolio member if a new model is weaker; do not claim the maximum vector is the new model's feasible solution.

### What the papers transfer—and what they do not

| Primary work | Transfer and limit |
| --- | --- |
| Schmalz & Trevizan, [AAAI 2024](https://ojs.aaai.org/index.php/AAAI/article/download/30005/31764), superseded by [AIJ 2026 / arXiv 2604.01855](https://arxiv.org/pdf/2604.01855), DOI 10.1016/j.artint.2026.104505 | AIJ Sections 2–4, Algorithm 2, Definition 13 and Theorem 3 distinguish admissibility/consistency and repair temporarily violated inactive constraints. Returned partial-SSP values need not be full-SSP consistent; epsilon accuracy involves expected steps. The transferable idea is lazy separation. The repository must supply procedural successor/predecessor bookkeeping; action-count reductions do not establish cheap native row construction. Use the superseding version. |
| Chatterjee et al., [TACAS 2025](https://link.springer.com/chapter/10.1007/978-3-031-90653-4_7), DOI 10.1007/978-3-031-90653-4_7 | Section 5, Lemma 6 and Proposition 7 address expected rewards with nonreachability assigned infinity, including necessary finiteness/qualitative conditions. Lower certificates need not equal a policy value. Its finite-MDP checker does not prove this engine's abstraction, action coverage, or program kernels. |
| Klößner, Seipp & Steinmetz, [ECAI 2023 Cartesian CEGAR](https://journals.sagepub.com/doi/pdf/10.3233/FAIA230405), DOI 10.3233/FAIA230405 | Sections 3.1–3.5 refine flaws in an optimal abstract policy: false goal, applicability, or transition. This supports lower-policy-selected retention/capacity distinctions. Efficient Cartesian rewiring and eventual identity refinement do not guarantee tractable procedural physical kernels. Zero costs receive dedicated treatment. |
| [SSP cost partitioning, ICAPS 2022](https://ojs.aaai.org/index.php/ICAPS/article/download/19802/19561/23815), DOI 10.1609/icaps.v32i1.19802 | Compatible cost allocations can add abstraction bounds without double charging. Arbitrarily adding existing independent lowers is invalid; maximum is the existing safe operation. Negative residual costs from saturated partitions introduce assumptions absent from the nonnegative proof above. Occupation constraints can still omit important sequencing interactions. |
| [Probabilistic merge-and-shrink with prune transformations, ECAI 2024](https://ai.dmi.unibas.ch/papers/kloessner-et-al-ecai2024.pdf), DOI 10.3233/FAIA240618 | Section 3/Definition 1/Theorem 1 require an appropriate alive-state preservation relation. An off-incumbent item is not dead or irrelevant merely because the incumbent router refuses it. Factored composition hypotheses need an actual native representation proof. |
| Hartmanns, [TACAS 2022 floating-point arithmetic](https://ris.utwente.nl/ws/portalfiles/portal/295755740/978_3_030_99527_0_3.pdf) | Sections 3–4 motivate conservative probability conversion and rounded Bellman operations throughout the computation. A small final tolerance is not an enclosure. Compiler/runtime arithmetic guarantees and representational fixed-point limits must be checked locally. |
| Hartmanns et al., [STTT 2026 revised practitioner's guide](https://publications.rwth-aachen.de/record/1032364/files/1032364.pdf), DOI 10.1007/s10009-026-00848-y | Sections 2–3 distinguish undiscounted algorithms and LP; its ordinary accumulated-reward objective needs adaptation here. No solver dominates every explicit model. An LP is a matched reference, not evidence that model-construction cost disappears. |
| [LRTDP](https://cdn.aaai.org/ICAPS/2003/ICAPS03-002.pdf), [i-dual](https://cdn.aaai.org/ojs/13768/13768-40-17286-1-2-20201228.pdf), [occupation-measure heuristics](https://users.cecs.anu.edu.au/~thiebaux/papers/icaps17.pdf) | Greedy-envelope labeling, heuristic artificial frontier terminals, positive-flow expansion and relaxed occupation constraints are established closest prior art. Their assumptions, epsilon stopping and representation costs do not confer exact native certificate authority. No new feature or novelty is claimed from combining them. |

The possible repository contribution is engineering: engine-owned expectation queries, exact scope identity, program composition and persistent lower-only reuse. The Bellman/LP/CEGAR theory is standard. Detailed primary-source pinpoints are in the supporting [literature review](papers.md).

## 4. Source reuse map and smallest changed invariant

| Existing owner | What already exists | Actual missing consumer or guard |
| --- | --- | --- |
| [Focused lower SolveWork](../../../engine/src/solver_solve_focused.cpp#L9) | Free values, independent frontier proof values, retained values and sparse Bellman/SCC work. | The admitted restricted graph is not the full action envelope. Lines 62–65 explicitly withhold its global authority while coverage is open. Do not invent another numerical algorithm for this. |
| [StrictClean](../../../engine/src/solver_solve_strict_pattern.cpp#L8) | Finite free-value relaxation with exact clean states, projected rare variables and optimistic exits. | Narrow eligibility, no arbitrary canonical lower-only domain; Eldritch guard names incomplete option coverage. Useful precedent, not the complete proposed engine. |
| [QuotientBellmanGraph](../../../engine/src/solver_quotient_bellman.hpp#L35) | Persistent stable cells/rows, proof sidecar, generations, reverse dependencies, memory. Uncertified rows explicitly remain lower-relaxation alternatives. | Its [lower vector](../../../engine/src/solver_quotient_bellman.cpp#L706) is only the minimum across immediate row costs and scalar alternative lowers, with no downstream reader. The later solve is gated by a certified executable terminal attractor. Add a lower-only solve consumer of this retained graph. |
| [ProofPatternManager](../../../engine/src/solver_proof_pattern_manager.hpp) | Typed admissible pattern results, projections and authority separation. | Own the validated lower result/model provenance; an ordering score or candidate vector is not a new proof type. |
| [ProofStore](../../../engine/src/solver_quotient_proof.cpp#L1431) and [action ledger](../../../engine/src/solver_action_envelope_ledger.hpp#L23) | LowerOnly/open/partial/certified/stale lifecycle and identity-aware retirement. Source optimistic minima retain conditionally noncompetitive obligations. | Numerical inactivity must remain distinct from admissible retirement or scheduler completion. Need a canonical set/family coverage witness at the lower consumer. |
| [Strict oracle](../../../engine/src/solver_policy_oracle_setup.inc#L349) / CalcContext | Canonical exact item import and exact action query independent of incumbent global routing. | Existing verified-entry wrappers carry unnecessary controller/coarse-parent assumptions for lower-only use. Preserve native caller admission, exact applicability and whole-choice semantics when exposing the query. |
| [Shared sparse evaluator](../../../engine/src/solver_sparse_policy.cpp#L226) | Correct self-choice fixed point; SCC evaluation; deterministic policy arithmetic. | Reuse with proper/failure semantics, explicit frontier/sink meaning, all choices, and a conservative final checker. Do not copy the envelope's separate choice formula. |
| [Program proof owner](../../../engine/src/solver_solve_operator_proof.cpp#L88) | Mandatory path minimum, exact priced options, declared execution paths and survival/destruction contracts. | Compose phase-sensitive optimistic programs into lower constraints. A fixed selected observation recipe cannot substitute for all legal choice policies. |

`QuotientBellmanGraph::install_cells` needs identity/generation, not upper support. `append_row` accepts uncertified rows with known targets, finite nonnegative price and complete mass. Thus lower-only physical states need not be forced into an incumbent graph. Current `solve()` nevertheless gates its executable path through certified-terminal support. **The smallest missing consumer is a separate lower solve over these existing cells/rows and explicit optimistic sinks**, returning a typed lower result rather than an executable-policy result. Its current input lacks choice groups, so either adapt the shared sparse choice representation or retain explicit mandatory phase/offer states.

The current envelope has only refresh-local dependency ownership. Its proof CalcContext, depth cache and active-recursion set are local to a refresh; it excludes its own contribution from successor calculations. `kSuccessorRefinementDepth=0` is [diagnostic scratch behavior](../../../engine/src/solver_solve_envelope_proof.cpp#L635), not a theorem against deeper lower reasoning. The [original Gate 6](../2026-08-25-solver-anytime-proof-realignment/gate6-evidence.md) rejected a non-tightening Harvest partition and missing Eldritch option coverage; no depth-specific mathematical or performance rejection was found. Its final best value does not consume the scratch refined minimum. Raising the constant establishes neither persistence nor new authority. Missing persistent ownership is the current integration diagnosis, not a proved historical reason for choosing zero.

**Small changed invariant:** a persistent lower cell is admitted by canonical original decision-state and caller scope, without requiring an incumbent entry. Every legal action and internal choice is covered by an exact/proved optimistic constraint or a named family placeholder. Outside mass binds independent lower proofs. Free candidate values acquire authority only after complete set-level and conservative simultaneous validation.

### Original failures that constrain this proposal

| Original record | Measured rejection / retained lesson |
| --- | --- |
| [Full strict reconstruction then merge](../2026-07-31-policy-guided-exact-refinement/report.md#L103) | 183,062 carriers, 423,756 transitions, 10,466 kernels, 1,089,111,449 owned bytes, 467.3 s, zero partition rounds. Keep only a small exact oracle; do not reconstruct first. |
| [Early alternative materialization](../2026-08-01-proof-carrying-quotient-refinement/report.md#L114) | Four carriers/two kernels already generated 345,192 transitions, exhausted 20M work and produced zero classes/rows. Lazy alternative obligations remain necessary. |
| [Generic descriptors](../2026-08-24-carrier-aware-proof-bounds/evidence/gate4-result.md#L25) | 22,729/202,735 descriptors; zero separations; minima 0.577/0.54205 unchanged. More descriptors without controlling-lower gain are not progress. |
| [Full shared reforge DAG](../2026-07-28-harvest-shared-reforge-frontier/report.md#L70) | 226,586 nodes, about 1.22M edges, about 49.5 MB extra; exact parity but time +26.3%/+32.9% and unchanged cap stop. Action-specific propagation stayed additive. |
| [Retention consumer success](../2026-08-25-solver-anytime-proof-realignment/gate7-evidence.md#L35) | 79,799 separations among 96,105 eligible obligations; envelope 109,100 -> 29,339; proper upper unchanged. Action-specific retention plus a real local upper mattered. |
| [Same-side success](../2026-08-26-exact-same-side-closure/result.md#L12) | Exact three-prefix 1,618.2138946963837 / three-suffix 1,101.15648683309. A transfer to 299,394 older PDR obligations separated zero and was removed. Current PDR closure cannot be reused as evidence of an unresolved consumer. |

The [source/archaeology audit](ownership.md) gives the additional rejected/retained owners and line references. These archives are evidence, not sequencing commands.

## 5. Complete certification contract

| Surface | Required invariant before authority |
| --- | --- |
| Model objective | Nonnegative costs, exact success predicate, non-success-as-infinity/proper-policy convention, finite candidate values. Zero-cost end components and absent proper exits are explicit. Infinite lower claims need separate qualitative proof. |
| State identity | Full collision-checked canonical item/projection key plus all sufficient original process memory. Keep base/data, goal, terminal semantics and caller scope in model identity. Dense IDs and digests are lookup aids. |
| Markov versus controller fields | Physical crafted/fractured/goal/junk/occupancy/influence/Eldritch/Veiled/protection facts remain when observed by legality, transition or goal. Checkpoint, offer and phase remain when enabled programs depend on them. Compiled node, chosen entry, strategy SHA and representative routing class are controller provenance, not compulsory lower-variable identity. The retry-basin marker is a virtual action restriction, so removing it changes scope unless that restriction is deliberately and validly relaxed. |
| Action set | Compare full canonical sets, not counts. Every legal action is accounted for; any selected action supplied for a fixed-J/control comparison is included exactly once. Free-value admission needs no selected action. Exact inapplicabilities name actions; fixed-program dependencies are not silently independent actions. Family membership/exclusion/completion has native authority. Duplicates cannot replace a missing member. |
| Open families | Retained primitive/delayed action vocabulary can be enumerated under caller admission. Unfinished state-local automatic synthesis and requested but incomplete Imprint grammar remain open. Use a whole-family conservative bound until complete generation or a whole-grammar optimistic proof closes it. A bound on the retained vocabulary is not a bound on an unexhausted family. |
| Exact applicability | Use caller admission **and** native legality. Raw `CalcContext::outcomes` is not a stand-alone legal-action enumerator: ordinary illegal primitives can return no-op distributions. Preserve explicit inapplicability evidence before using a row. |
| Choices | Preserve offers, the time of observation, legal options and self choices. A single fixed choice recipe is upper-policy evidence. Lower authority requires all choices, correct minimization, or a proved optimistic replacement. |
| Programs | Preserve paid setup, mandatory phase transitions, conditional branches, checkpoint/restore resources, phase-specific prices and first external decision exits. Do not grant setup for free while calling the macro exact; do not remove an optional decision while proving a lower. |
| Kernel | Bind canonical source/action/program semantics, cost identity, complete probability mass and every positive outcome/choice. No support truncation, silent renormalization or tiny-probability deletion. Outside targets carry independent b identities; aggregated targets need a pushforward/expectation proof. |
| Numeric | Use exact rational or outward-safe input/accumulation/checking relative to the declared probability/price model. Verify lower inequalities in the safe direction, success zero and finiteness. Small residuals and relative epsilon do not prove lower authority. If uncertainty prevents a directed inequality, repair/refuse rather than promote. |
| Persistence | Bind data/artifact, request, goal, economy, caller/action/program vocabulary, source/target generations, projection, boundary and numeric schema. Revalidate on changes. Graph-local handles or controller reuse alone are insufficient. |
| Refinement | Demonstrate a feasible lift/dominating refinement if monotone model values are claimed. Otherwise retain previous independent certificates in a separate maximum portfolio. LP infeasibility from imposed old floors is not a dead-state proof. |
| Inactivity | Temporary numerical inactivity has no lifecycle retirement authority. Keep a sound scalar cap or perform complete separation before publication. Track source-value and successor-value invalidations, prices, families and unknown procedural dependencies. |
| Publication/retirement | A certified lower may be compared only with a compatible evaluated proper upper. No lower-only policy is automatically executable. A discovered better feasible policy follows the existing compiler/evaluator path; use 1,000 Simulator runs only when changed-strategy verification is genuinely needed. |

Two current interfaces fail parts of this promotion contract. The [SCC validator](../../../engine/src/solver_policy_refinement.cpp#L631) compares `caller_authorized_actions` with `constraints.size()+1`; a current-source native fixture certifies duplicated alternatives. The [envelope choice helper](../../../engine/src/solver_solve_envelope_proof.cpp#L79) ignores a self option when a finite alternative exists. The shared sparse owner correctly solves that fixture. These are bounded research findings; no live artifact's lower was newly invalidated by a demonstrated reachable crafting row.

For procedural constraint generation, count more than selected rows. Materialized rows cost O(support) to recompute and need incoming dependency indexes. Unbuilt kernels have no free reverse-dependency list: candidate/domain changes may require a query, a conservative dependency overapproximation or a complete invalidation scan. An immutable scalar cap depends only on its source and bound identity, which is cheap but weak. Building all omitted-action dependencies in advance would repeat the rejected expansion.

An exact expectation query can be smaller than a full physical kernel. If a potential is additive, exact marginal expectations suffice by linearity; no independence assumption is needed. Interactions require joint statistics. A projected Markov kernel requires stronger sufficiency for future actions than one `E[potential]` query. Current reforge generation interns projected leaves before aggregating outcomes; no general no-interning expectation consumer was found. The representation alternative is a direct weighted statistic/pushforward query with native parity evidence, not reconstructing a shared full DAG first.

## 6. Minimal matched experiments

### Current-source native arithmetic and validator probes

A standalone mechanics-independent helper directly called the shared sparse row evaluator, SCC component solver and policy-potential validator. To pin the tested owners, only current `solver_sparse_policy.cpp` and `solver_policy_refinement.cpp` were compiled to scratch objects and linked before the existing dependency archive. No production source/object/library changed during the research or archival.

| Probe | Measured native result |
| --- | --- |
| Cost-1 half-success/half-self-choice | `2`; stored nonself count 1, `has_self=1`. |
| Two-state proper SCC | `2.2857142857142856, 2.5714285714285716`, matching `16/7,18/7`. |
| Selected action plus duplicated alternative | Validator certified one entry with three claimed actions but only one unique alternative action. This proves the set-coverage consumer gap; it does not show a production artifact contained duplicates. |

The [source-tied output](ownership_current_source_probe_result.txt) and [helper](ownership_probe.cpp) are retained. Source-tied binary SHA-256 is `75E13B3BE80B1FE8278F39F77EB72F4F915D28680DBBC41FB600BE62919375CA`. Other dependencies use library SHA-256 `A00317CB1CFD9D1FDEB94AB4252B591E17C62E33BE53C2F669A0E94AC951B5FE`; its timestamps are consistent with the checkpoint, but it has no embedded source SHA. The crafting-query probes below are measurements of that hashed library, not a claimed reproducible current-source build.

### Exact native microproblem: one-mod reset model

Selected exact archive: [final acceptance, case `oracle-real-one-mod` (retained excerpt)](native-selected-artifacts.json). Vaal Regalia level 86, empty Normal to sole magic T1 `LocalIncreasedEnergyShield11`; caller action set `{transmute, alteration, restart}`, prices 0.05/0.1/base 5, economic Restart enabled, automatic programs and goal-progress gating disabled. Archived exact optimum/evaluated cost is `23.78999999999971`, with success one and off-policy zero. Its independent envelope lower `2.240771812080537` was an earlier pattern contribution, not the final exact lower.

The new [native export](native-micro.json) contains eight calculator states, seven nonterminal states, all 21 state/action obligations, seven explicit native inapplicabilities, 14 legal kernels and 56 transitions. It exports full canonical representative keys. These are sufficient calculator-model representatives under this reset-only scope, not every physical modifier item. Set equality was checked at each nonterminal state. Every failure state's complete native action/row signature is identical, permitting the known three-cell behavioral quotient: start, failure, goal. No goal-plus-junk state is made terminal.

The native row query took `0.3747 ms`; total load/setup/query `265.4278 ms`; accounted owner bytes increased `644195 -> 651843` (+7,648). These are measured snapshots, not process peak/transient memory certification. Fourteen unique legal rows were built **once**; all reference variants reuse that export. The first harness refusal was diagnosed and corrected at the native-legality seam, with the original 8-state/24-obligation/56-legal-transition limits unchanged. No SolveWork step or finish ran.

There is an important numerical limit. Exact addition of the exported binary doubles gives mass `1 + 21/2^60` on the seven-outcome distribution, although floating addition displays 1. The reference therefore records two different models explicitly: raw stored-coefficient algebra, and a separate exact rational model obtained by dividing each row by its exact stored-coefficient sum. That normalization is an **explicit research model transformation**, not silent repair or certification of the underlying probability derivation. The normalized reference optimum is `23.790000000000003`; the raw coefficient solution is `23.790000000000106`, differing by `1.03e-13`. Both agree numerically with the historical exact oracle. No new production lower authority is claimed from that agreement.

The [matched comparison](native-model-comparison.json) uses the same action scope, prices, native rows and fresh lower donor. All reported gaps in the following table are to the **explicit normalized rational reference oracle**. Rational simplex and exhaustive proper-policy evaluation agree; residuals are computed exactly, not accepted under epsilon.

| Variant | Root lower/reference value | Gain over fresh L=2.15 | Reference oracle gap | Nonterminal variables / consumed rows / coefficients |
| --- | ---: | ---: | ---: | --- |
| Existing lower only, explicit scalar coverage | 2.15 | 0 | 21.64 | 7 / 14 scalar / 14 |
| Root exact rows, all successors at existing boundary | 2.2235586973264385 | 0.0735586973264385 | 21.566441302673564 | 1 / 2 / 8 |
| One goal-plus-junk recovery refinement | 2.2415219923456653 | 0.0915219923456653 | 21.548478007654335 | 2 / 4 / 16 |
| Complete free-value finite model | 23.790000000000003 | 21.64 | 0 | 7 / 14 / 56; exactly reducible to 3 total cells |
| Frozen optimal J, root-only existing boundary | Fails its Transmute inequality | No certificate | Not applicable | Same root model as row 2 |
| Frozen J on the complete exact reference domain | Passes all inequalities | Same as complete free-value | 0 | Same complete model; mathematical check, not a new compiled-entry certificate |

Root Transmute owns the minimum. State 7 has the requested goal prefix **and a junk suffix**, so it is not terminal; its fresh lower is only 0.1. Adding that exact recovery state raises the root lower by `0.0179632950192268` over the one-state model. Its minimum is Alteration; the complete model makes every failure-state value approximately 23.84. This is a concrete small example of why cheap cleanup/progress assumptions miss reconstruction cost. It does not rely on inventing any game rule: goal flags and kernels come from the native export. The exported fresh lower vector happens to be Bellman-consistent; the required inconsistent-base falsifier remains the separate synthetic example.

Allowing J to decrease repairs the root-only and two-state models to their free optima. This repairs a certificate against weak boundary values; it does not discover a cheaper real micro policy than the known exact optimum. No strategy was changed or reverified.

Reference arithmetic retained/peak allocations were 888/3,376 bytes for the one-variable model, 1,512/6,032 for the recovery refinement and 6,696/84,008 for the complete model. They measure Python arithmetic only, after input construction, and include the exhaustive policy comparison; they are not native memory savings or scalability evidence. Simplex used 1, 2 and 7 pivots respectively. All variants used the same exported kernel; no cross-source native kernel-cache speedup was measured.

### Medium two-sided witness with live alternatives

Selected [last-mile operator-proof artifact (retained excerpt)](native-selected-artifacts.json): Conquest Lamellar / Allflame five-goal request, starting with three requested T1 prefixes and fractured requested spell suppression, occupancy 3/1, source goal mask 23, fracture mask 16. Requested Physical Damage Reduction remains missing. This is a bounded two-sided proof witness with live alternatives, not terminal PDR.

| Archived native baseline | Value |
| --- | ---: |
| Existing lower / evaluated proper upper | `36.42861718910441` / `2698.8747960143623` |
| Upper-minus-lower | `2662.446178825258`; true optimum/oracle gap unknown |
| States / rows / sparse transitions | 6,820 / 40,551 / 119,625 |
| Raw outcome entries | 74,514,364; distinct from retained sparse transitions |
| Ledger entries / unresolved obligations | 193,360 / 22,976 |
| Prior incumbent-dominated obligations | 79,938 |
| Native live / peak ownership | 62,809,343 / 99,834,010 bytes |

Its root trace has ten admitted, finite cached rows. The independent fixed-identity clean model contributes `36.42861718910441`. Known exact scratch compositions are Harvest Physical `38.068615060362319` and Scour `36.809254018621417`. The reported smallest materialized operator is `option:eldritch_side_intent:suffix:eldritch_exalt:eldritch_ichor:1`.

**The report does not give that operator's numeric RHS.** The source publishes `max(common, exact_operator_envelope)` but reports the minimizing operator even when common dominates. Inferring that its Q lower equals the public lower would be another count/annotation-to-authority error. Complete ranked canonical root coverage is likewise not serialized. Thus Scour refinement is not justified by occupancy here, and an LP on these partial report fields has no automatic full-scope authority.

The identified program is the mandatory paid sequence Ichor tier 1 then Eldritch Exalt, cost `0.08234 + 3.59 = 3.67234`. The artifact reports three root outcomes; its 69 total rows/209 outcomes used zero reforge work. The exact macro already pays setup. A specific source-based candidate refinement is to retain fixed affix/fracture and occupancy facts across the changed Eater tier by constructing an independent lower owner anchored at the post-Ichor carrier. The current fixed-identity donor requires unchanged Eater/Exarch tiers and therefore cannot simply be reused across that phase. The bounded phase probe measured this hypothesis without deleting the guard or charging setup twice.

The [new medium native probe](native-medium.json) queried only native Ichor then Exalt. Setup deterministically changes Eater tier 0 -> 1. It yields three complete native **modeled outcome classes**: probabilities approximately 0.0359066427 and 0.9551166966 retain mask 23/fractured mask 16 with occupancy 3/2; probability 0.00897666068 reaches exact goal mask 31. These classes are not three literal physical modifier outcomes. The consumed lower depends on the recorded retained facts, not on a selected incumbent continuation; promotion still requires the strict-oracle pushforward/coverage contract for all merged members.

| Matched projected phase query | Original donor | Reanchored existing donor |
| --- | ---: | ---: |
| Post-Ichor identity eligibility | false | true |
| Nonterminal successor lower | 3.4724500000000003 | 36.428617189104408 |
| Paid macro RHS | 7.1136189946140025 | 39.773949853475088 |
| Projected RHS gain | — | **32.660330858861085** |
| Certified global/pruning gain | 0 | **0** |

Native work: two primitive rows/four transition entries; five modeled states from one initial state; 43.4 microseconds row time; accounted rows add 1,216 bytes (`1238571 -> 1239787`). New donor preparation takes **4.3467238 seconds**, total probe 8.9600234 seconds. The reanchored owner reports 1,239,778 bytes including the shared calculator; do not add the two owners' totals as if the shared calculator were duplicated. Peak transient memory and a cheap shared-table cache were not measured. This is reuse of the existing lower implementation across a second source identity, with its preparation cost disclosed, not proof that cache reuse is already cheap.

Conditional on the uniform lower/pushforward contract, this native projection instantiates the admissible-versus-consistent distinction. The original root lower is 36.4286, while composing the recorded outside donor values across the projected macro gives only 7.1136. Therefore those entries are not a Bellman-consistent vector for the declared finite reference model. The original root lower remains in the independent maximum portfolio. Reanchoring makes this reference inequality pass because 39.7739 exceeds 36.4286; it does not close all root actions or independently certify all physical members of the outcome classes.

The [conditional rational comparison](medium-model-comparison.json) keeps exactly that program row plus an explicit common-floor placeholder covering every other caller action. It records the raw probability mass defect and explicitly normalizes only its separate reference model. Baseline free model = 7.1136, refined free model = 36.4286; the public independent maximum remains 36.4286 in both, so global gain is zero. Frozen root J=2698.8748 cannot satisfy either model; repairing it means accepting the lower free value, not claiming a cheaper executable policy. Even if the generic family cap were strengthened, the already-known untouched Scour lower constraint would cap this particular refinement at 36.809254 unless it too were refined. That is a model ceiling, not Scour's true Q or a whole-medium exact bound.

The directly measured defect is **loss of eligibility across a mandatory identity change**, not missing setup cost in the native macro. After validating the reanchored donor's composition contract, any further refinement must follow the next actual minimum. Source-visible additional optimism includes failure-occupancy normalization, optimistic progress preservation and favorable target-side success probability in the clean model; none was independently removed in this run. This keeps the experiment to one evidence-selected phase refinement.

For this medium scope, the existing-lower baseline is measured; a full fixed-J action-complete attempt is unavailable; the conditional refined free model using common scalar placeholders is capped at the common lower by construction. Candidate repair or an LP cannot evade that cap. The projected phase query is a matched local refinement candidate, not a whole-medium optimum or full free-model result. Full reactivation work, a stronger action-complete medium lower and cross-shape native proof-cache reuse remain unmeasured.

### Clean-five placeholder ceiling and reuse limits

The [setup-only native probe](native-probe.json) inspected the explicit 28-action request and the empty rare/Normal source shapes without solving. Primitive legality and caller admission remain separate: the printed Restart registry entry is excluded by this request's no-economic-Restart scope. All still-unmaterialized automatic families retain a conservative common-floor placeholder.

| Root scalar before exact row refinement | Value |
| --- | ---: |
| Harvest Physical | 36.4286171890906 |
| Scour | 36.48853172876641 |
| Chaos | 38.76116050095102 |
| Harvest Defences | 40.49615117725027 |

Fresh scalar coverage is pinned by Harvest Physical and any open common-floor family. The archived r9 had already refined Physical to `38.909043109189682`, which explains why Scour then owned its root envelope. Repeating Scour or selecting it only from incumbent occupancy would skip this model-stage distinction.

One deterministic Scour query cost `22.1 microseconds`, returned one transition/no choices, and added 616 accounted calculator bytes, with states 2 -> 2 because both source shapes were already interned. Its successor lower is `36.11443172876641`, giving the known `0.3741 + 36.11443172876641 = 36.48853172876641`. Total load/proof preparation/query was 4.7099 s, live proof ownership 1,250,224 bytes. This query establishes router-independent lower access and its local cost, not a useful new gain. Querying Normal uses the same proof owner but is not measured reusable exact-row proof across a second shape.

The internal bench sample has five contributions but only three distinct **reported item/entry digest pairs**. Full archived canonical vectors are unavailable, so neither five distinct cells nor exactly three collision-safe cells is established. An opportunistic SCC must reconstruct/compare full identities and complete each successor action set.

### What was and was not measured across variants

The synthetic cleanup refinement raises 2 -> 10 and 2 -> 13 on two source shapes with three rows each. It establishes a representation distinction, not implemented cache reuse. Temporary-inactivity analysis demonstrates one omitted cheap constraint reactivated and a final two-action check; those scan counts are analytical bookkeeping for a toy, not procedural-kernel performance. Current native probes perform zero retirement/lifecycle mutations and no dynamic inactivity scheduling.

The old Fracture retention shadow's `.31175` action-lower gain and zero retirements remain historical evidence on one semantic source shape. The r9 generic row work retained 24,584,584 bytes and required 89,465,656 transient bytes for 26,064 transitions; the Scour evaluator refusal alone used 131,714,112 live / 213,481,561 peak bytes. These are different operators/scopes and cannot be presented as matched speedups from the new micro query.

No useful full-medium/five-goal gain, true medium oracle gap, procedural reactivation saving or second-shape native proof-cache reuse has been established. That is the structural stopping result—not a fabricated performance pass. The exact micro supports the free-value model; the medium query supplies a measured phase-donor improvement and shows setup amortization, complete program coverage and remaining placeholders as the next constraints. Raw sources, provenance and archived extracts are in [native evidence](native-evidence.md) and the [hash manifest](native-provenance.json).

## 7. One implementable next boundary

**Proposed boundary: a covered lower-only quotient consumer, with one controlling program/retention refinement.** Oliver has not activated it.

1. Add a lower-only entry point to the existing quotient sparse/proof owner. Reuse its persistent cells, rows, generations, memory and shared SCC arithmetic. Explicit boundary/scalar sinks must carry lower provenance; they must not need a certified executable terminal attractor. Keep ProofPatternManager as result authority. Do not add a second graph reconstruction engine, focused scheduler or production optimization dependency.
2. Make canonical action/choice/family coverage an input certificate checked by set equality. Use the existing shared self-choice evaluator. Add conservative final arithmetic validation and an explicit proper/failure convention before any authority flag.
3. Reproduce the exact micro reference on the same complete native kernel and current source. Then retain the measured medium post-Ichor donor under its full phase identity and compose the existing paid program row. Prove its uniform lower/pushforward contract for all modeled outcome members. Reuse the donor on a second compatible source shape without rerunning identical setup; measure the preparation and invalidation cost rather than assuming table equality. Keep every other action explicitly covered and expose the next minimum/remaining placeholder ceiling before further exact queries. The measured local target is 7.1136 -> 39.7739, with no assumed global gain.
4. Compare the unchanged lower portfolio, fixed-J attempt where a valid entry exists, free-value solution and repaired J on identical rows. A missing entry remains unavailable, not infinity. Measure minimum-owning constraints, row/query counts, state/transition growth, retained/transient bytes, invalidation scans and reuse on a second source shape.
5. Stop if the new bound remains pinned by unrefined families without a specific affordable refinement, if query work becomes equivalent to rejected strict expansion, or if coverage/program semantics cannot be certified. In the row-dominated case the result is a precise projected-expectation contract. Do not add scheduler instrumentation as a substitute. If an optimistic policy becomes concretely cheaper, switch to the existing upper construction/compile/evaluate route instead of forcing proof of the old policy.

The small internal fixed-J row can remain an opportunistic control, but complete all its canonical successor action sets before claiming an SCC. No performance pass threshold is invented here. Observed matched baseline, checked lower gain and measured consumer/reuse will determine whether this boundary merits promotion. HANDOFF and production remain unchanged until Oliver selects work.

## 8. Prompt for the implementation session

> Read current AGENTS, docs map/direction, HANDOFF and the 2026-09-04 free-value research report; re-pin local HEAD and preserve protected `0`. If Oliver has selected this boundary, add the missing lower-only solve consumer to existing QuotientBellmanGraph/ProofStore and publish only a typed ProofPatternManager certificate. Reuse sparse Bellman/SCC and correct self-choice arithmetic; do not change successor depth or reconstruct a full exact graph. Require canonical action/choice/family set coverage, independent boundary/scalar provenance, explicit proper/failure-infinity semantics, and conservative final inequalities. Reproduce the exact micro, then retain and compositionally certify the measured medium post-Ichor donor: local RHS 7.1136 -> 39.7739, no assumed global gain. Prove the projection and measure donor reuse across a second compatible source; expose the next minimum before another exact query. Compare fixed J, free values and repaired J on identical kernels. Stop at a concrete projected-expectation contract if expansion dominates. No production LP dependency, full census, automatic successor milestone, broad intermediate tests, push, or proof-only Simulator. Keep a cheaper concrete policy on the existing upper compiler/evaluator path. Run selected final acceptance once and update HANDOFF honestly.
