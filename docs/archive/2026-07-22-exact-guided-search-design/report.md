# Exact Guided Search Design

**Status: archived design analysis and prototype measurements; not an accepted
plan and no current implementation authority.**

Parent: [Archive entry](README.md)

This report is preserved from `codex/exact-search-design` commit `273831f`.
Only the document was carried to `main`; none of that commit's prototype
source is a source donor. The contemporary implementation milestone is
preserved under
[Exact Constructive Policy Search](../2026-07-22-exact-constructive-policy-search/README.md).

The historical report includes an exact two-T1 oracle measurement that
predates the current prohibition. Its presence here is evidence, not
authorization to rerun that oracle.

Claim labels used throughout:

- **Proven** means a correctness argument is given or inherited directly from
  a cited theorem under stated assumptions.
- **Corroborated** means the branch prototype agrees with the exact oracle and
  measured cases but has no general proof beyond its ordering-only role.
- **Unverified** means the note says exactly what would settle the claim.

## Phase 1A - Grounded architecture

For fixed data, prices, start item, goal, and admitted action envelope, the
solver is a finite stochastic shortest path MDP. Costs are non-negative, goal
states are zero-cost absorbing states, and `restart` is an always-available
finite-cost transition to one clean carrier. Exact primitive enumeration makes
each chance kernel explicit. Cycles arise from retry policies and preservation;
locally unusable carriers are not global dead ends because Restart supplies a
proper escape.

The measured empty-rare three-natural-T1 case currently proves
`261.05161071365512 <= V*(start) <= 4104.7066630770487`. The upper is
executable. The lower is the value of a partial exact graph whose unexpanded
frontier receives admissible relaxed terminal values.

### Correspondence to known algorithms

| Current piece | Closest known technique | Precise correspondence | Where it breaks |
| --- | --- | --- | --- |
| Focused partial graph and fringe expansion | LAO* | Generate only states reachable from the start under the current greedy partial solution graph, put an admissible heuristic on tips, perform DP backups, repeat. | poecraft2 expands strict-state batches, solves cycles by policy iteration, and follows both lower and executable-upper policies. |
| Greedy focused backups | RTDP | Improve values by Bellman backups only in a start-relevant region rather than the complete graph. | RTDP samples trajectories; poecraft2 deterministically walks all positive-mass successors of a selected partial policy. |
| Closure checks and SCC optimization | LRTDP / HDP | LRTDP labels greedy-reachable converged states solved; HDP uses Tarjan SCCs for solved cyclic regions. | poecraft2 has no persistent per-state solved labels; closure is re-established per round and exact sparse policy iteration owns cyclic evaluation. |
| Probability mass x local upper/lower gap | BRTDP | BRTDP focuses on states both likely under a trial policy and poorly understood. `collect_focused_fringe` computes that product using exact routed occupancy. | poecraft2 has two independently optimized policies and deterministic occupancy, then adds coarse-class quotas. |
| Price-bound action removal and SCC solve | Focused topological VI | Both use lower/upper information to prove actions suboptimal before topological solution. | poecraft2's envelope is dynamic and its certificate price-scoped; guidance alone never removes an action. |
| Goal-progress lower certificate | Probability-aware PDB / Cartesian abstraction | Project to goal subset, rarity, and affix capacity; solve the small stochastic model exactly. The strict normal/magic extension is instance-specific refinement. | Current rare states collapse junk, blockers, fracture locks, and exact correlated continuation; it is not generic CEGAR or merge-and-shrink. |
| Exact behavioral quotient | Stochastic bisimulation / model minimization | Merge only when observation, action identity, and probability into every successor block agree. | Refinement runs on the generated all-action graph and often merges nothing for broad envelopes. |
| Retry/dead-region SCCs | FRET plus proper-policy evaluation | FRET is the right warning for greedy cycles that cannot reach a goal; the evaluator checks properness and solves SCCs. | Priced Restart gives useful local traps a real escape, so they should normally be valued rather than eliminated. |
| Destructive renewal upper | Proper-policy initialization / regenerative analysis | Repeat a kernel to acceptance and reset rejects: a regenerative proper policy giving an exact upper. | The policy is synthesized by a template search rather than found by the same partial graph. |
| Progressive fracture upper | Multilevel splitting / RESTART analogy | Decompose a rare event into progress levels and preserve a checkpoint at a reached level. | Splitting estimates a fixed rare-event probability; it does not optimize an SSP policy or prove a lower heuristic. Fracture changes the controlled system. |

Primary anchors: [LAO*](https://aaai.org/papers/0058-aaai98-058-heuristic-search-in-cyclic-and-or-graphs/),
[RTDP](https://doi.org/10.1016/0004-3702(94)00011-O),
[LRTDP](https://rakaposhi.eas.asu.edu/lrtdp.pdf),
[HDP](https://www.ijcai.org/Proceedings/03/Papers/176.pdf),
[BRTDP](https://www.cs.cmu.edu/~ggordon/mcmahan-likhachev-gordon.brtdp.pdf),
[FRET](https://doi.org/10.1609/icaps.v21i1.13452),
[model minimization](https://cs.brown.edu/people/tdean/publications/archive/GivanetalAIJ-03.pdf),
and [splitting/RESTART](https://www.informs-sim.org/wsc06papers/014.pdf).

### Candidate admissible heuristics

Let `T` be the concrete Bellman operator and `J_M*` the exact optimal
expected cost in relaxation `M`. All values below use the real 23-action
empty-rare Dire Pelt case and its pinned economy.

#### H0 - deterministic reachable-set cover

Define `R(a)` as the goal slots any primitive in operator `a` could produce.
The relaxed state is only a satisfied-slot mask. Applying `a` costs `c(a)`
and may deterministically add any subset of `R(a)` while preserving all prior
progress:

`h_cover(s) = min sum_i c(a_i)`

subject to the union of granted and already-satisfied slots reaching the
required cardinality.

**Proven admissible.** Every real policy pays the action costs, real actions
cannot produce outside `R(a)`, and the relaxation replaces stochastic
production and possible progress loss with deterministic, perfectly selected
production and perfect preservation.

Real start value: **`1.0`**, because one relaxed Chaos macro may grant all
three reachable slots. This is safe but nearly uninformative inside the
`[261.0516, 4104.7067]` bracket.

#### H1 - probability-aware clean goal-progress PDB

Use abstract state

`z = (rarity, satisfied_goal_mask, prefix_count, suffix_count)`.

Destructive rolls replace the unfractured goal subset; additive actions add a
slot; reset/scour return through normal rarity. For each action/subset, the
transition probability is an optimistic upper bound computed from exact goal
draw data, clairvoyant outcome identity, union bounds, and free best blockers
where allowed. Solve the finite cyclic abstraction exactly:

`h_clean(s) = J_clean*(phi_clean(s))`.

**Proven admissible under the implemented relaxation argument.** Every
abstract action has no greater cost, no lower target-production probability,
no worse observation choice, and no additional progress loss. Bellman
monotonicity gives `h_clean <= V*`. This is the existing
`clean_goal_progress_mdp`, not a new claim.

Real start value: **`198.84849900205941`**. It captures about 76% of the
currently proved lower bound but does not distinguish a permanently fractured
target from an ordinary currently satisfied target.

#### H2 - exact partial-graph Bellman lift

For any generated set `E`, keep every concrete action row exact within `E`
and make transitions leaving `E` terminate at `h_clean(s')`. Let the exact
optimal value of that finite partial SSP be `h_E`. This is closure of exact
Bellman backups over `E` with `h_clean` on the fringe.

**Proven admissible.** Replace every omitted continuation `V*(s')` with
`h_clean(s') <= V*(s')`, then solve the optimistic SSP exactly. Equivalently,
Bellman monotonicity proves every finite lift `T^k h_clean <= V*`.

Real start value after the existing three-state bootstrap expansion:
**`260.93830695419985`**. The unchanged-cap deeper graph reaches
**`261.05161071365512`** after 544 expansions. Selecting the right `E`, not
repeating cheap Bellman algebra, is the dominant question.

#### H3 - lock-aware strong-simulation PDB

Use

`phi_lock(s) = (rarity, satisfied_mask, fractured_goal_mask,
prefix_count, suffix_count, explicit_count, blocker_effect_class)`.

For each abstract block, include the aggregate of every concrete exact kernel
from every representative in that block as an abstract action. The abstract
controller may therefore choose the most favorable representative-specific
kernel every time it visits the block. Let `J_lock*` be the exact solution of
that action-union abstraction and define

`h_lock(s) = max(h_E(s), J_lock*(phi_lock(s)))`.

**Proven admissible as a specification.** The action-union quotient is a strong
simulation: concrete kernel `(s,a)` is present at `phi_lock(s)`, while the
abstract policy receives extra representative-switching choices. Therefore
`J_lock*(phi_lock(s)) <= V*(s)`. The maximum of two lower bounds remains a
lower bound. This is a standard stochastic-abstraction construction, not a new
theorem.

Real start value for the conservative first prototype contract:
**`260.93830695419985`**. With no fractured goal, the prototype uses `h_E`
unchanged; a new table may only strengthen locked states. The full action-union
table has not been built, so any sampled/discovered-row approximation is
**Unverified for pruning** and may be used only for ordering. Pruning requires
complete block action sets or a direct strong-simulation witness for every row.

### Recommended prototypes

1. **Lock-preserving BRTDP/HDP scheduling.** Keep `h_E` and every admitted
   action unchanged. Refine only the coarse fringe scheduling signature and
   quotas with `fractured_goal_mask`, lock/gateway level, and relevant blocker
   effect. Rank inside a stratum by exact occupancy mass x bound gap and
   relaxed-value progress. This is exactness-safe because only strict-state
   expansion order changes.
2. **Lock-aware PDB after ordering evidence.** Build H3 lazily but completely
   for the small lock lattice using the exact aggregate kernels and action
   union required by the proof. First use it only in fringe priority. Enable
   pruning only after the strong-simulation witness and adversarial synthetic
   cases pass.

Do not prototype a new hand-authored fracture policy. The test is whether the
ordinary exact graph reaches fracture-prep and post-lock continuation states
earlier under generic guidance.

### Adopt versus build

| Stuck component | Adopt | Build locally | Decision |
| --- | --- | --- | --- |
| Cyclic heuristic-search control | LAO*/LRTDP/HDP invariants | Existing exact sparse policy iteration and cooperative state machine | Adopt theory; do not replace the core. |
| Lower/upper focus | BRTDP probability x gap | Deterministic occupancy flow, balanced two-policy quotas, reset-backed upper | Existing approach is specialized BRTDP; refine its schedule. |
| Abstract lower bounds | Probability-aware PDB, Cartesian CEGAR, merge-and-shrink, saturated cost partitioning | Engine-native projection and exact correlated row aggregation | Adopt proof framework; build only the adapter and factors. |
| Dead ends/traps | FRET and proper-policy tests | Treat priced Restart as a real escape and retain salvage choices | Adopt diagnostics; no FRET rewrite while reset is universal. |
| Exact state reduction | Stochastic bisimulation refinement | Collision-checked action/observation signatures and strict fallback | Already adopted correctly. |
| Rare-event route discovery | Splitting/RESTART levels as analogy | Exact ordering-only lock/gateway strata inside BRTDP batches | Build a narrow scheduler prototype; do not import sampling estimators. |
| Lower-bound optimization | Occupation-measure LP and probabilistic cost partitioning | A possible small LP/PDB backend later | Defer unless H3 construction dominates or is weak. |

Additional primary work: [occupation-measure heuristics](https://ai.dmi.unibas.ch/research/reading_group/trevizan-et-al-icaps2017.pdf),
[probabilistic cost partitioning](https://doi.org/10.1609/icaps.v32i1.19802),
[PDB pattern selection](https://ojs.aaai.org/index.php/ICAPS/article/download/19801/19560/23814),
[probabilistic merge-and-shrink](https://ojs.aaai.org/index.php/ICAPS/article/download/27196/26969/31265),
and [Cartesian CEGAR](https://fai.cs.uni-saarland.de/kloessner/papers/kloessner-etal-ecai23.pdf).

## Phase 1B - Novelty hunt

The complete intersection is unusual, but almost every individual technique is
known. Novelty claims below concern only a combination not found in this
search, never a claim that no such paper exists.

| Structural feature | Known technique that exploits it | Candidate here | Classification and verification path |
| --- | --- | --- | --- |
| Exact bisimulation-minimized SSP | Stochastic bisimulation; probabilistic merge-and-shrink | Reuse exact quotient blocks as safe cache/solution states | **Reinvented.** The implementation is domain-adapted, not theoretically novel. Verify all-action signatures and quotient/full value parity. |
| Start-focused cyclic policy graph | LAO*, RTDP, LRTDP, HDP | Expand only greedy-reachable partial policy and solve its SCCs | **Reinvented.** Verify against exact full-graph controls. |
| Lower and executable upper with likely x gap focus | BRTDP | Exact occupancy-weighted lower/upper fringe | **Reinvented.** Deterministic occupancy instead of sampled trials is an implementation choice. |
| Config-independent priced reset | SSP proper-policy theory; BRTDP upper initialization; regenerative/restart analysis | Universal finite fallback and renewal witnesses | **Reinvented in principle.** Exact engine-specific renewal synthesis is local work. Verify properness, renewal equation, and compilation. |
| Goal-subset lattice | Probability-aware PDBs, Cartesian abstraction, merge-and-shrink, occupation measures, cost partitioning | `satisfied_mask` and subset-pattern lower tables | **Reinvented.** Use their strong-simulation proof obligations. |
| Exact correlated-mod enumeration | Chance nodes preserve arbitrary finite correlations; probabilistic merge-and-shrink retains task-level outcome identity | Aggregate native kernels by goal/lock features without factorizing probabilities | **Reinvented algorithmically.** The engine enumerator is domain-specific. Verify projected probability mass and row parity. |
| Irreversible lockable progress | Splitting/RESTART levels; monotone abstraction factors | H3 lock factor and gateway strata | **Reinvented as an abstraction factor.** A lock bit is a retained state variable. Verify H3 by strong simulation. |
| BRTDP batches stratified by irreversible lock/gateway level while all actions remain admitted | BRTDP handles gap/reach; splitting handles levels; PDBs retain factors, but searched sources do not combine them as this ordering rule | Reserve quota for reachable `(locked subset, lockable subset, gateway readiness)` strata; rank inside by occupancy x gap; retain exhaustive fallback | **Possibly novel; unverified.** Search ICAPS/AAAI/IJCAI, probabilistic model checking, rare-event MDP optimization, and constraint-generation SSP work for milestone- or splitting-stratified BRTDP. A matching method moves it to reinvented. Empirically require oracle parity and earlier ordinary-policy fracture/prep reachability on multiple cases. |
| Exact action generation ordered by lock-aware relaxed reduced cost without pruning | CG-iLAO* already defers SSP action evaluation using constraint generation; LAO*/BRTDP order states | Generate every row eventually, but schedule kernel work by H3 reduced cost and checkpoints | **Reinvented/adjacent.** A fair no-pruning variant is an engine adaptation, not a novelty claim. Its proof must show omitted rows remain represented by a valid lower envelope until generated. |

The Phase 1 novelty position is modest: abstraction, gap focus, reset, and
splitting ingredients are known. Only the precise ordering composition remains
possibly novel, pending Phase 3 refutation.

## Phase 2 - Prototype measurements

### Scope and protocol

The prototype is intentionally narrow and reversible. It changes internal
telemetry, focused-expansion ordering, and reuse of an already executable
constructive incumbent. It does not change actions, legality, costs,
transition probabilities, exact aggregation, caps, Bellman updates, pruning,
compilation, or the C ABI.

All real-target runs used the selected
`solver-scaling-dire-pelt-three-t1-from-scratch-production-caps` case, its 23
actions, and unchanged production caps. Runs used an external wall-clock
watchdog, five-second liveness checks, process termination at the deadline,
and a post-run survivor check. The longest cooperative solver step remained
about 10.7 seconds, below the existing 20-second worker slice setting.

### A. Lock-aware scheduling signature

Candidate A adds exact fracture facts to `focused_schedule_signature`:
`fractured_goal_mask`, fractured metamod flags, fractured junk totals, and the
fractured item flag. These are scheduling classes only; they do not merge
states or remove work.

**Correctness: Proven.** With unbounded resources, changing only the order in
which strict states are expanded cannot change the closed graph, Bellman
operator, or exact solution. The exhaustive fallback is unchanged.

On the real case Candidate A reproduced the control exactly. This is a useful
negative result: none of the newly distinguished fractured states was on the
lower- or upper-policy fringe, so a finer class key had nothing to select.

| Metric | Telemetry control | Candidate A |
| --- | ---: | ---: |
| status / cap | `refused_resource_cap` / reforge | same |
| proved bracket | `[261.05161071365512, 4104.7066630770487]` | identical |
| focused rounds / expanded | 6 / 544 | 6 / 544 |
| discovered / frontier | 127,661 / 127,117 | identical |
| action rows / transitions | 9,631 / 430,232 | identical |
| reforge work | 11,000,000 | identical |
| solver selected live / peak bytes | 89,274,807 / 155,223,656 | identical |
| solve wall time | 50,557.7 ms | 51,199.5 ms |
| maximum cooperative step | 10,641.4 ms | 10,713.4 ms |
| expanded gateway / fractured states | 222 / 0 | 222 / 0 |

The 642 ms wall-time difference is not evidence of a regression or gain from
one run. Topology and bound parity are exact; utility is **Corroborated
negative** on this case.

**Exact 2-T1 oracle gate: Corroborated.** The final binary was run on an
oracle-equivalent case whose only change was `solve_step_work_items: 4096 ->
1`. It converged to **`230.26738656962243`** with zero gap, matching the known
oracle within `1e-7`. It expanded 57,182 states, discovered 116,041, built
738,139 action rows and 1,165,840 transitions, and charged 8,535,132 reforge
work. Peak selected solver memory was 990,276,669 bytes; solver time was
1,023,439.4 ms and the longest cooperative step was 8,190.6 ms. The watchdog
completed normally and found no surviving process. The optional 10,000-run
policy simulation was skipped because this gate was the exact solve, compile,
and case-expectation check rather than compiled-strategy verification.

### B. Reserved post-lock batch quota

Candidate B additionally scanned already-discovered, unexpanded fractured-goal
states and reserved up to one quarter of a focused batch (capped by the lower
batch size) for them. The remaining batch and exhaustive fallback were
unchanged, so this was still ordering-only and did not prune.

The first selected fractured state was enough to exhaust the unchanged
11,000,000 reforge-work cap. It reached one fractured-plus-additional-progress
state, but the run stopped after 258 expansions instead of the control's 544.
Its bounds are deliberately withheld: the same build did not re-green the
exact oracle within the five-minute gate (4,201 expansions at the deadline).
The candidate is rejected, not merely inconclusive.

| Metric | Telemetry control | Candidate B |
| --- | ---: | ---: |
| real-case status / cap | `refused_resource_cap` / reforge | same, earlier |
| focused rounds / expanded | 6 / 544 | 4 / 258 |
| discovered / frontier | 127,661 / 127,117 | 126,948 / 126,690 |
| action rows / transitions | 9,631 / 430,232 | 4,472 / 366,002 |
| solver selected live / peak bytes | 89,274,807 / 155,223,656 | 50,424,514 / 91,997,333 |
| solve wall time to cap | 50,557.7 ms | 28,032.4 ms |
| expanded fractured / post-lock-progress states | 0 / 0 | 1 / 1 |

The lower memory and time are artifacts of stopping earlier, not improvements.
This exposes a second bottleneck: once a lock state is selected, exact
all-action expansion immediately requests enormous post-lock reforge kernels.
State-level guidance alone cannot make that work incremental.

### C. Reuse the first executable constructive incumbent

Candidate C addresses the measured cooperative bottleneck without changing the
search space. Once `focused_fallback()` has produced an executable policy, later
focused rounds retain it instead of rebuilding the same renewal witness after
every graph extension. The strict graph is append-only and its costs and rows
are unchanged, so the cached policy remains executable. It may become stale
and therefore loose, but it remains a valid upper bound; lower-bound
calculation, action admission, and exhaustive closure are untouched.

**Correctness: Proven for the current append-only focused solver.** Reuse cannot
make the retained executable policy cheaper than its evaluated cost or remove a
concrete action. The exact oracle corroborates that the same optimum is reached.
This is not a theorem that indefinite stale-incumbent reuse is useful on every
case; the Phase 3 counterexample separates bound validity from guidance quality.

| Metric | Candidate A | Candidate A + C |
| --- | ---: | ---: |
| real-case proved bracket | `[261.05161071365512, 4104.7066630770487]` | identical |
| real rounds / expanded / discovered | 6 / 544 / 127,661 | identical |
| real rows / transitions / reforge work | 9,631 / 430,232 / 11,000,000 | identical |
| real solve wall time | 51,199.5 ms | 19,868.9 ms |
| real constructive time / anchor checks | 41,539.0 ms / 6 | 10,325.6 ms / 3 |
| real maximum cooperative step | 10,713.4 ms | 10,542.0 ms |
| cooperative oracle total wall time | 3,977,985.5 ms | 1,030,819.2 ms |
| cooperative oracle constructive time | 2,946,382.8 ms | 8,025.5 ms |
| cooperative oracle maximum step | 23,802.5 ms | 8,190.6 ms |
| oracle value / expanded / discovered | `230.26738656962243` / 57,182 / 116,041 | identical |

The real run was 61% faster than Candidate A and the cooperative oracle was 74%
faster than the pre-cache run. Those percentages are single matched runs, so
they establish this case's bottleneck and candidate utility, not a general
speedup distribution. Candidate C also brought the oracle's maximum step below
the 10-second prototype guardrail.

### Measured diagnosis and decision

- **Corroborated:** exact occupancy x gap reaches 222 legal fracture gateways
  but assigns no expansion to their fractured successors on this budget.
- **Corroborated:** a signature refinement cannot help when those successors
  are absent from the selected policy fringe.
- **Corroborated:** blindly injecting even one post-lock state spends the
  reforge cap before the solver can exploit the milestone.
- **Corroborated:** constructive-upper synthesis, not Bellman optimization,
  dominates the control run: 41,040.8 ms versus 1,410.4 ms for expansion.
- **Corroborated:** retaining an executable incumbent removes most repeated
  constructive work while preserving the real bracket/topology and exact
  oracle value.
- **Decision:** adopt no source change from this review branch. Candidate A is
  exact but inert here; Candidate B is rejected; Candidate C is promising
  review evidence but needs a refresh policy and broader cases before
  adoption. The deep-route architecture still needs safely incremental action
  rows or a stronger admissible lower model before forcing post-lock states.

## Phase 3 - Adversarial review

### Attempts to falsify the architecture

1. **“Lock progress is monotone.” False for the full MDP.** Unfractured goal
   progress is destroyable. Fractured progress survives many actions but
   `restart` intentionally discards it. Lock level is a useful feature, not a
   global partial order or a license for topological solution.
2. **“A sampled lock PDB is admissible.” False.** H3 is safe only when every
   representative kernel is present in the abstract action union (or has a
   direct simulation witness). Omitting a favorable representative removes an
   abstract choice and can raise the abstract value above the concrete value.
   Sampled/discovered-only H3 is ordering-only.
3. **“Occupancy x gap finds every useful detour.” False.** A state behind an
   action not chosen by either current partial policy has zero routed
   occupancy. The measured 222-to-0 gateway/locked split is a concrete
   counterexample. BRTDP focus needs exploration or a stronger heuristic; it
   is not a completeness proof by itself.
4. **“Milestone quotas are cheap.” False.** Candidate B shows one post-lock
   expansion can consume the entire reforge-work allowance. Rare-event
   starvation and rare-event overspending are dual failure modes.
5. **“Restart makes every configured model a proper SSP.” Too broad.** The
   argument additionally needs Restart to be legal and finite from every
   relevant state and the reset carrier to have a finite positive-probability
   route to the goal. Those conditions hold for the selected case, not by
   syntax for every future configuration.
6. **“An executable upper plus a lower heuristic proves a pruning rule.”
   False.** Pruning still requires a row-specific certificate such as
   `c(a) + E[h(s')] >= U(s)` under a proved admissible `h`, with numeric error
   accounted for. Ordering evidence alone proves nothing about removal.
7. **“A valid cached incumbent remains good guidance.” False.** A later graph
   extension could expose a much cheaper fallback or a policy with very
   different occupancy. Reusing the old executable policy still gives a valid
   upper and cannot alter the exact optimum, but it can keep a loose bracket or
   steer capped search poorly. Candidate C's general guidance utility is
   **Unverified**; broader cases need either periodic refresh or a proved
   trigger tied to incumbent improvement potential.

### Fresh literature challenge

The second search found no primary source that directly combines exact BRTDP
occupancy-gap batches with quotas indexed by irreversible controlled
milestones while retaining an exhaustive exact fallback. That is weak negative
evidence, not novelty proof. Short-sighted SSP decomposition varies an
execution/replanning horizon, and rare-event splitting allocates simulations
across importance levels; neither is this exact state scheduler.

The action-level idea is not novel. [CG-iLAO*](https://ojs.aaai.org/index.php/AAAI/article/view/30005)
already uses constraint generation to avoid evaluating SSP actions until
needed. Its certificates may inspire a safe lower envelope, but this project
cannot claim lazy action generation as new. [Short-sighted SSPs](https://ojs.aaai.org/index.php/ICAPS/article/view/13527)
and [landmark progression](https://ojs.aaai.org/index.php/ICAPS/article/download/27180/26953)
are adjacent decomposition and milestone-ordering work, while the earlier
BRTDP and splitting anchors cover the two main ingredients separately.

### Final claim ledger

| Claim | Final label | What would change it |
| --- | --- | --- |
| H0, H1, and H2 are admissible under their stated relaxations | **Proven** | A violated cost/probability dominance premise or Bellman counterexample. |
| Complete action-union H3 is admissible | **Proven as a specification** | A concrete kernel absent at its abstract image or a stricter abstract goal. |
| Candidate A preserves exactness | **Proven** | Any changed row, pruning decision, cap, or non-exhaustive fallback. None exists. |
| Candidate A improves this target | **Corroborated false** | Repeated matched runs showing topology or bracket progress; the first run is exactly identical. |
| Candidate B usefully exposes post-lock work | **Corroborated** | The telemetry observed one locked-plus-progress expansion. |
| Candidate B is viable | **Corroborated false** | A reworked incremental action-row design that re-greens the oracle and improves matched budgets. |
| Candidate C preserves bound validity and exactness in the append-only solver | **Proven** | A mutation that invalidates a cached row/policy, or an oracle mismatch. |
| Candidate C improves these two measured cases | **Corroborated** | Matched reruns contradicting the identical topology/value and timing reduction. |
| Candidate C is generally useful guidance | **Unverified** | Multi-case evidence plus a defensible refresh rule. |
| Exact occupancy x gap is specialized BRTDP | **Corroborated by source correspondence** | A materially different published definition or implementation invariant. |
| Lazy SSP action generation is novel | **Reinvented/adjacent** | CG-iLAO* is a direct prior method; only a narrower new theorem could revive a claim. |
| Lock/gateway-stratified exact BRTDP scheduling is novel | **Possibly novel; unverified** | A matching prior paper moves it to reinvented. A claim would also need a general result and multi-domain evidence, not this single negative prototype. |

### Review recommendation

Do not adopt any source prototype directly from this review branch. Preserve it
as evidence for three findings: policy occupancy does not enter the lock
branch; atomic all-action expansion makes entering it too expensive; and
repeated constructive-incumbent synthesis is a separable cooperative
bottleneck. Candidate C is separately worth review because it preserved exact
topology/value and materially reduced both measured runtimes, but adoption
should include a refresh policy and broader cases.

The most defensible next research chunk is a proof-first, action-incremental
partial Bellman model informed by CG-iLAO*, with H3 used only as an admissible
lower envelope until complete. That is a new proposal for Oliver to accept or
reject, not implementation authority from this note.
