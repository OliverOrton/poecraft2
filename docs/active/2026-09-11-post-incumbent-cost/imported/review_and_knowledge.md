# Review of edd8ec2: preserve recovery, now improve its cost

**Reviewed source:** `edd8ec25f40b11c8ba4f4aaf157db9405b593ed2`, direct successor to `a5ef8a5b7d6263fdc60f94957b0b76f7978f8f72`.  
**Posture:** read-only GitHub source and retained-evidence review. No repository mutation, native build/test/solve, simulator, queue operation or public action. The accompanying arithmetic is synthetic. Sources R01–R12 and E01–E02 resolve in `source_register.json` and the references below.

## 1. Verdict

This commit delivers a genuine cross-base capability gain: Ring-two and Onyx-three now return independently evaluated controllers under their original controls. Core verified-graph coverage rises from nine to eleven of twelve. The one previously exact core case remains exact. This is not a new exact closure or an affordable-policy breakthrough. It should be neither dismissed as more documentation nor exaggerated as solving the hard cost problem. [R01, R02]

The original policy-selector proposal must stay retired for these cases. The native experiment found no terminal goals in its completed views, and the full existing numerical seed solved the illustrative mutual-retry test. The successful successor work completed native terminal/continuation routes and preserved the first proper candidate. It did not add another generic reachability selector.

## 2. What paid off

### 2.1 Actual terminal completion, not merely increasing goal count

The initial captured views had 360 Ring and 31 Amulet states satisfying all requested modifiers but still containing junk. Native witnesses expose valid cleanup/addition routes. This corrects the earlier overly broad “no goal states means missing acquisition” interpretation. Absence of terminal states can mean missing cleanup under the original predicate. [R02]

The retained seed treats terminal debt and pending continuation work as construction preferences, not a changed goal or certified bound. Its preferences apply specifically before an incumbent. Later expected-cost improvement remains a distinct existing path. [R04]

### 2.2 More reforge rows were useful evidence, but insufficient by themselves

The scheduler had interpreted a frozen automatic-epoch yield as exhaustion. The new no-incumbent re-entry exposed requested Harvest/Fossil/Essence rows. Their native terminal mass was zero in the recorded root witnesses, and each carried hundreds or thousands of missing direct continuations. N2 alone still returned no policy. [R02]

The successful Ring strategy instead uses Harvest Augment Defences, Exalt and Annul; the Amulet uses native temporary blockers, Exalt, Annul and priced crafted-mod removal. The final Ring run need not reproduce all the N2 root reforge coverage: its compact controller can be formed earlier. Open reforge obligations are not thereby proved uncompetitive.

### 2.3 An executable seed is preserved before speculative improvement

After useful selected continuations existed, ordinary improvement could choose a cheaper-looking row whose routing was incomplete, before the first proper candidate was retained. A further diagnostic exposed lower estimates from outside the selected policy domain being used as executable upper frontiers. The new code masks those entries unknown and independently qualifies the first candidate before proceeding. [R02, R05, R06, R07, R11]

`certify_initial_candidate()` reuses `CompiledPolicyAssertionWork`, pauses competing discovery while it borrows the candidate's semantic snapshot, accounts its proof copy/child workspace, and promotes only an executable, proper, cost-complete, zero-off-policy, finite nonnegative result. The later native compiler/evaluator remain authoritative. This review is not a complete memory-safety or whole-engine correctness proof. [R06]

### 2.4 Ordinary improvement already runs

Ring improves from first evaluated cost 938,064.6067502735 to 582,192.807187538; Amulet from 48,896,520.03536156 to 40,215,428.995558396. `begin_incremental_upper_policy_pass()` already temporarily exposes completed unresolved alternative rows and starts the existing focused upper solve. Therefore “add policy improvement” or “let it use alternative rows” is not a sufficiently informed next task. The question is where useful further alternatives fail to obtain complete compatible tails. [R02, R07]

## 3. Results and their limits

| Population | Current result | Qualification |
|---|---|---|
| CB06 Ring two | L 10.169847833384168; U 582,192.807187538; 7.395 s | Recovered; stops on memory |
| CB08 Onyx three | L 174.79327321907147; U 40,215,428.995558396; 240.715 s | Recovered; reforge-work cap |
| CB09 Ring three | No policy; 17.608 s | Remaining admission-memory failure |
| CB01 empty Conquest five | U 85,558.706186; 247.882 s | Previous good upper preserved; not a matched speedup against the old short run |
| CB02 empty Conquest four | U 3,746.131941; 243.587 s | Better recorded upper than old 5,218.04095, but longer clock confounds code attribution |
| CB03 partial Conquest three-to-five | U 794,067.453040 | Historical 80,720.78955245294 compatible controller remains unrecovered |
| CB11 Dire Pelt three | U 1,934.636840 | Better reported cost than previous 2,313.66960 under matching native case controls; no statistical speedup claim |
| CB12 exact Regalia | L = U 65.600361 | Existing exact capability preserved |
| Reserved Shield / Wand | U 2,071.736538 / 3,963.671016 | First exposed after selection; no matched old baseline establishing improvement |

Full precision, all twelve cases, stop classifications and executable/report hashes belong to R02/R08. Eleven graphs does not mean twelve passing cases: CB09 has no policy; CB05 keeps its previous verified graph but still fails the named-stop contract. The native observations were not repeated here.

The first broad re-entry mutation regressed Bow-five dramatically. A no-incumbent guard restored its previous cost and passed the discriminating fixture. That observed regression is fixed, not evidence that the final commit still makes Bow worse. Keep the guard. [R02]

The generic reporter excluded all ten shared old/new pairs for `runtime.configuration` because the older record predates M0 metadata. Independent native-input/deadline comparisons establish preservation, not a generic paired timing result. Use a current baseline for the next code experiment. Do not rewrite old ledgers or disable the mismatch guard. [R02, R08]

WASM normal Ring independently matches the 582,192.807187538 controller; first-candidate abandon and finish checks passed. The original short Conquest-four policy also survives. Direct Amulet WASM parity was not established by the inspected report. Native reserved-family results are not browser timings. The 1,342,373,888-byte Ring WASM heap differs from the one-GiB solver-owned cap. No four-GiB native diagnostic or Simulator was run. [R02]

## 4. Next direction: cost improvements need continuation evidence and useful resources

The recovered graphs create an actual incumbent against which better policies can be tested. They do not prove that all admitted reforges have been explored, or that the cost problem is now only a weak lower.

Three competing explanations remain:

1. The next useful row/continuation cannot complete under current capacity.
2. Available alternatives lack entry-compatible fixed-policy values or a closed candidate, and are continually rejected or deferred.
3. The available complete alternatives really do lose, so a new mechanism or better-guided search is needed.

The next programme distinguishes them. It does not assume one universal defect or commit to a new learned heuristic, optimizer or basin structure. A memory experiment belongs early because Ring terminates after seconds, despite an allowed four-minute run. The historical N2 allocation sum is 1,246,891,066 bytes, or about 1.161 GiB; this is a motivating observation, NOT an attribution of the final Ring refusal or proof that four GiB solves it. [R02]

## 5. Mathematical handoff: policy cost is an upper, not a free boundary

### 5.1 Attribute the cost to the actual controller

For a finite proper fixed controller pi, let P_pi be the substochastic nonterminal transition matrix, c_pi its finite expected immediate costs, and e_r select the entry. Then

```
J_pi = (I - P_pi)^(-1) c_pi
 d_pi^T = e_r^T (I - P_pi)^(-1)
J_pi(r) = sum_s d_pi(s) c_pi(s).
```

Properness on this finite reachable domain makes the matrix transient. The expected visit counts include all retries and destructive returns. Reporting by currency/action can aggregate these contributions after evaluation, but a small compiled graph can still induce a large operation/item product chain. A finite-node graph or a node annotation is not itself this evaluation.

This is elementary finite Markov-chain algebra, not a proposed new numerical backend. Prefer the evaluator's existing action/entry attribution. If only a root scalar is available, do not claim to have a whole-domain value vector.

### 5.2 A compatible policy can price a one-time alternative

Suppose a complete legal action a at semantic entry s reaches only entries where the SAME fixed controller pi has valid continuation values and routes. For ordinary transitions,

```
Q_pi(s,a) = c(s,a) + sum_t P(t|s,a) J_pi(t).
```

This is the expected cost of taking a once and then following pi, provided that controller composition is representable and proper. It is an executable candidate/upper calculation, not a lower on Q*(s,a). If permitted choices occur after an offer, choose at that information boundary and retain those decisions. Do not move the choice before observation.

A tail outside pi's verified domain is UNKNOWN. Replacing its cost by an admissible lower may be good proposal guidance but cannot make Q_pi a verified upper. R11 states this current native entry boundary; the new selected-domain mask fixes an actual misuse of it.

A one-time switch and changing the decision on every revisit are different controllers. Example: baseline finishes for 10. Action a costs 1 and returns to s with probability one-half, finishing otherwise. Taking a once then baseline costs 6. Repeating a costs 2. Both must be interpreted as the controller actually emitted; a one-step calculation is not the second controller's exact value by identity.

### 5.3 Proper policy improvement on a common covered domain

For two proper fixed controllers pi and mu on a common finite represented domain with valid full transitions, define

```
A_mu^pi = c_mu + P_mu J_pi - J_pi.
```

Subtracting their linear equations gives

```
J_mu - J_pi = (I - P_mu)^(-1) A_mu^pi.
```

The inverse is nonnegative. Thus A_mu^pi <= 0 throughout the relevant domain is a sufficient condition for non-increase, with strict root improvement when a negative term is visited under mu. This is sufficient, not necessary: requiring every local advantage to improve can miss compensating controller changes. Preserve native complete candidate evaluation as the ultimate arbiter.

**Two limits:** properness is an explicit premise, and the new controller's visitation weights—not the old ones—give the exact total change. A zero-cost self loop has zero Bellman advantage against a 10-cost finish yet never reaches the goal. A cheap-looking move into an uncovered entry with lower zero and true continuation 100 can appear to cost 1 while actually costing 101.

The reference script checks a two-state pair with J_pi=(12,10), J_mu=(6,5), and A=(-1,-5). Old visits (2,1) give weighted advantage -7, while new visits (1,1) correctly give root change -6. Old cost attribution is a useful search priority, not a certified amount of future gain.

### 5.4 Certification and search guidance remain separate

The project already has candidate generation, fixed-policy evaluation, full-action lower checking, and entry-scoped upper evidence. The algebra here does not authorize omitted outcomes, hidden observations, a new policy subset being called exact, or an untrusted estimate becoming a lower. Stronger incumbents can make existing certified pruning useful; exact closure still requires the original complete competitor coverage.

Bertsekas' proper-policy SSP analysis explicitly distinguishes proper-policy values and initialization-dependent Bellman solutions under nonnegative costs; it supports not treating a stable numerical value as termination evidence. Chatterjee et al. provide independently checkable finite-MDP fixed-point certificates; this supports preserving the native checker boundary, not a claim that their proofs establish poecraft2's abstraction. Only the primary abstract/metadata pages were consulted here, not a new full-paper review. [E01, E02]

## 6. Small actual cleanup findings

The current HANDOFF describes its completion commit as local-only; it is now visible on remote main. Update current status at the next checkpoint, preserving old receipts.

The retained Ring artifact has the description “Exact within the zero-progress-reroll policy restriction” despite being a bounded nonoptimal controller. That text is misleading if interpreted as optimality. The reported L/U are correctly far apart; no invalid exact certificate is alleged. Correct the current producer's future bounded wording and keep explicit scope. The same graph has per-node `expected_cost` fields different from the independently evaluated root: inspect their entry/estimate semantics before using them or renaming them. Do not rewrite historical artifacts or flatten every field to the root value. [R09]

The older audit's recommendation to preserve the existing workbench, independent checkers and scoped reuse still applies; its build/lint/M0 tasks are already done. No repeated tooling audit or broad purge belongs before cost work.

## 7. Disposition for Codex

| Finding | Status | Destination |
|---|---|---|
| First-policy recovery now reaches native and Ring WASM delivery | Source plus recorded native evidence; not rerun here | Existing living record and capability view |
| Queue breadth alone did not deliver the policies | N2 counterexample, followed by selected continuation/retention repair | Existing scheduling/policy knowledge |
| Per-entry upper values cannot be replaced by unrelated lower values | Existing claim; new native counterexample/repair | Upper/policies/selected-domain test |
| Current costs and caps justify post-incumbent investigation | Proposed next decision; root bottleneck must be captured | Next living record, existing source owners |
| Occupancy-weighted policy difference requires new-policy visitation and properness | Conditional elementary derivation, 16 synthetic checks | Existing policies chapter if useful; no automatic accepted claim ID |
| More RAM may improve native capability | Untested capacity hypothesis; N2 is not final allocation evidence | Distinct capacity profile/results |
| Bounded strategy description can imply optimality | Direct artifact text; numerical endpoints remain bounded | Current compiler/metadata owner and focused presentation fixture |

No native source change, runtime result, hardware speedup or new exact closure is claimed by this review. The implementation plan selects the next work, supplies bounded experiments, and makes existing first-policy delivery a non-regression requirement.

## References

R01: https://github.com/OliverOrton/poecraft2/commit/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2

R02: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/docs/active/2026-09-10-goal-reaching-row-delivery/README.md

R03: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/HANDOFF.md

R04: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/engine/src/solver_solve_constructive.cpp#L4095-L4295

R05: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/engine/src/solver_solve_incremental.cpp#L1458-L1540

R06: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/engine/src/solver_solve_finish.cpp#L6038-L6205

R07: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/engine/src/solver_solve_incremental.cpp#L695-L757

R08: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/docs/active/2026-09-10-goal-reaching-row-delivery/results.json

R09: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/docs/active/2026-09-10-goal-reaching-row-delivery/experiments/strategies/cb06-cross-base-product8-long240.strategy.json

R10: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/AGENTS.md

R11: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/docs/solver/upper-authority.md

R12: https://github.com/OliverOrton/poecraft2/blob/edd8ec25f40b11c8ba4f4aaf157db9405b593ed2/docs/foundation/tooling.md

E01: Dimitri P. Bertsekas, *Proper Policies in Infinite-State Stochastic Shortest Path Problems*. arXiv:1711.10129v2 (2020 revision). https://arxiv.org/abs/1711.10129v2

E02: Krishnendu Chatterjee, Tim Quatmann, Maximilian Schaeffeler, Maximilian Weininger, Tobias Winkler and Daniel Zilken, *Fixed Point Certificates for Reachability and Expected Rewards in MDPs*. Extended TACAS 2025 paper, arXiv:2501.11467v1. https://arxiv.org/abs/2501.11467v1
