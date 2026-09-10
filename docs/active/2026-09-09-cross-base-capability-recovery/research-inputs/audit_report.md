# Cross-base solver capability audit

**Repository:** OliverOrton/poecraft2  
**Pinned latest main:** `be553608ecabde28a3dec07856255911459ec97d`  
**Original audit checkpoint:** `317438392c88c0151f41e48b4b0323b06b1d0037`; updated for its one direct child commit.  
**Review date:** September 9, 2026  
**Posture:** read-only, falsification-first; no repository edits, commits, issues, pushes, or other public actions.

## Executive finding

The solver has not simply stopped improving. It has repeatedly lost useful policy-construction capability, repaired that loss, and improved the reliability and cost of its proof machinery. But the recorded five-goal headline is now close to a result already achieved in August, and recent end-to-end qualification is concentrated on Conquest Lamellar. Broader capability has not been demonstrated by the recent work.

The principal failure is **a narrow optimization/evaluation feedback loop combined with fragile delivery of completed continuations and expensive action/state work**. Weak lower bounds matter, but stronger bounds cannot cure a run that exhausts memory before finding a policy, loses candidate-owned decisions, or cannot complete a demanded frontier. The new native-only diagnostic supplies real proof time and consumes the prepared heuristic, yet still leaves root bounds unchanged. The question has narrowed from missing proof opportunity to useful completion cost and proof-memory demand. Those problems need not occur in the same proportions on other bases. `[R27, R28]`

The next major direction should be **cross-base, resource-aware continuation search with reusable certified endgames inside the existing ladder**. Start by running a small fixed cohort that exposes the already-known non-Conquest failure classes. Do not commit to a new basin, thread-pool, or learned architecture before those runs identify its consumer. The diagnostic cohort is assembled in the accompanying JSON/CSV; the measurement system is already adequate and should not be rebuilt.

### Evidence boundary

This audit reads current source/contracts and retained experiment records. It does not contain a newly executed native A/B campaign. The 12-case cohort was selected and mapped to repository evidence, not freshly solved here. In particular, current Ring/Bow/Amulet performance cannot be inferred from the newer Conquest results. Their historical failures are reasons to measure them now, not evidence that every failure persists unchanged.

Source references `[Rxx]` below resolve to pinned repository URLs in `source_register.json`. Literature references `[Lxx]` also resolve there. The accompanying arithmetic file contains only calculations on recorded numbers, not benchmark measurements. Literature references are carried forward from the original audit; this update does not claim a new literature search. The new findings come from current repository source and retained measurements. No native/WASM tests or simulations were rerun here.

### Latest-main disposition

One new commit, `be55360`, implements the earlier proof-time proposal far enough to falsify **allocation alone** as a sufficient remedy on the tested Conquest cases. It adds a benchmark-only stable-candidate handoff, complete demanded continuations beyond the old selected table, deferred option-kernel certification, and a whole-cell prepared-heuristic consumer. The current ordinary short policies remain unchanged. `[R27–R33]`

Four-goal F7 gets 178.287 seconds of strict work and strengthens 74,015 local obligations, but retires zero as noncompetitive and does not improve the root interval. Five-goal F8 hits the replay-backed partition-memory cap before the 240-second finish. Neither is a new exact closure or a material root gain. No new non-Conquest measurement was added. These findings strengthen the case for a diverse capability baseline while removing already-implemented tasks from the programme. See [the complete delta](main_update.md). `[R28]`

## 1. What improved, what regressed, and what is not comparable

### 1.1 The five-goal trajectory is mostly recovery

The August 22 carrier-ladder result, completed August 23, produced an independently evaluated five-goal Conquest policy costing **87,361.1690420501** with a lower of **36.4286171891**. It used 35 carrier epochs and 15 joint-policy checkpoints. The report's 615 goal-subset observations are activity across the run, not 615 distinct subsets of five goals. The requested finish was 60 seconds; the solve itself took 71.88 seconds before subsequent qualification. Imprint was enabled in that request but absent from the selected strategy. `[R02]`

The August 26 Calculator-profile matrix returned **14,454,067.43** on the familiar clean-five case. This was a real, proper, evaluated policy, not merely a displayed estimate. Changing work slices from eight to 1,024 did not change the finalized five-goal value in that matrix. Thus work-slice drift alone did not explain that quality cliff. `[R03]`

By August 30, a matched clean-five control was already at **85,408.64362148782**. An attempted resumable-candidate integration regressed to a **470,485,191.44** fallback. The cause was concrete: a refused candidate retained a large traversal payload and continued suppressing ordinary joint-policy work. Reclamation and nonexclusive refusal restored the 85,408.64 result, while PDR remained exactly solved at 3,758.1244 under its own identity. `[R05]`

The current ordinary short qualification still records **405.3694021063399–85,558.70618560436** for empty five-goal Conquest. Its nominal upper is only **2.063% below** the original 87,361 figure. The later August 30 result is slightly better numerically, but its 120-second profile and other identities prevent calling that difference a matched regression. Likewise, the 2.063% comparison is a historical headline comparison, not a controlled speedup or clean algorithmic improvement. `[R01, R02, R05, R06]`

The correct conclusion is not “nothing worked.” It is **“we have recovered roughly the same useful five-goal policy class after intervening losses, with stronger checking and a stronger lower, but have not demonstrated a new broad exact-closure frontier.”**

### 1.2 A potentially unrecovered partial-case regression

The August 26 quality matrix reports **80,720.79** for Conquest partial three-to-five. The last retained pre-update evidence reports **794,067.4530398862** for the corresponding named partial control (not freshly requalified by `be55360`), a nominal **9.84-fold higher cost**. The latest campaign compared against an intervening 470-million fallback and correctly reported a large improvement over that control. That does not answer whether the older 80.7k capability was lost. `[R03, R06]`

This is a **regression candidate**, not a final causal finding. Recover the exact historical request, artifact, prices, terminal/action semantics and strategy, independently evaluate it under the intended comparable target, and perform a matched replay if appropriate. A historically valid policy can be a reference oracle without being injected into new search. If semantics changed, record a model-version difference instead of a solver regression.

This is the first historical comparison I would resolve. It tests whether the programme's headline progress is being measured against recent low-water marks rather than the best compatible known behavior.

### 1.3 A demonstrated cross-base scheduling failure

An August 24 ordering experiment improved clean-five from roughly 14.45 million to **1.56 million**, while a non-armour partial-five Bow control changed from a proper **6,026,985,788.49** policy returned in 58.81 seconds to a **127.33-second watchdog with no policy**. Warlord and tri-element Bow controls stayed stable. The ordering was removed. `[R04]`

This is strong evidence that base/mechanic diversity can falsify an apparently promising local optimization. It is not a claim that the removed ordering remains in main. It also shows why one “non-armour control” is insufficient: two Bow-like workloads can react differently.

### 1.4 Current genuine delivery and preparation gains

The September continuation changes found missing siblings in selected and publication prefixes, serviced them through the existing owner, and completed one useful joint candidate. C1 serviced 142 missing entries with no policy improvement; C2 serviced 252 and slightly worsened the upper; the combined C3 treatment serviced 367, completed a candidate, and reduced the upper from **16,997,812.20 to 85,558.71**. This is an actual root-policy gain, not just a counter increase. `[R06]`

Supported retention relations were then built eagerly rather than gradually behind temporary price floors. On the empty-five workload, model rounds fell from 54 to five, and ordinary setup fell from **27.353 to 8.278 seconds** with the same checked lower and policy. The useful scheduling direction here was more eager construction of an already justified small model, not a blanket rule that all work should be lazy. `[R06]`

The preceding `3174383` fix repaired bounded-finish latching and stopped optional strict work once the cheapest compatible verified artifact could be returned. Its four-goal observation returned the same 5,218.04 strategy in **62.203 rather than 86.477 seconds**, before strict rows started. The new commit retains that delivery behavior while adding an optional pre-finish proof attempt; it passes the old table-extent boundary but not the remaining frontier/memory barriers. `[R01, R06, R28, R29]`

### 1.5 New longer-run evidence: more time is not automatically more capability

The previous-source ordinary 240-second four-goal control returns **8,500.234970856602**, worse than the known short-run **5,218.040949685988**; both ordinary long controls report zero strict rows. This is a delivery/quality-vs-budget warning, not proof that a cheaper independently verified incumbent was discarded within the long run. Its first trajectory upper appears only at final evaluation. `[R28, R31]`

The working handoff recovers the 5,218 policy and gives strict work a real window. F4 constructs complete new-candidate rows beyond the old table. F4b then attributes a state cap to full option-kernel construction during descriptor admission at an **in-table** parent. F5 defers that exact work until demanded certification, passes that admission obstruction, and encounters a different missing-parent boundary. The retained code covers that branch, with the careful fixture-versus-real-run limitations in the living record. `[R28, R29]`

Final F7 uses 178.287 seconds of strict work, 33,032 kernels, and 9,694 beyond-table rows, but returns the unchanged four-goal interval. F8 spends 117.089 seconds on five-goal strict work before partition memory caps; its previous verified interval survives. Final ordinary short four/five native and WASM graphs, plus the existing exact three-suffix anchor, remain unchanged. These are completed intermediate mechanisms and specific negative capability results—not a reason to rerun the same settings loop. `[R28, R33]`

## 2. Why stronger proofs did not consistently improve search

### 2.1 The public lower and the useful search decisions have different consumers

The retention component now gives a real full-scope lower for its checked domain, including a supported fresh region. The previous fresh-source refusal was repaired. However, preparing the same component without consuming it produced the identical 16.998-million strategy in the early September causal control. Expanding compatible partial consumers to 4,049 of 4,055 expanded states also left the root interval unchanged. Availability, consumption and useful action elimination are distinct. `[R06]`

At the recorded C0 point, the operator-lower pruner performed zero evaluations because there was no finite incumbent available to compare against. A stronger lower cannot do incumbent-based retirement before a suitable upper exists. That is a lifecycle issue, not an argument that admissible heuristics are useless. `[R06]`

Conversely, earlier lower work did retire **79,799 of 109,100** unmaterialized obligations on the fractured four-to-five witness. This directly falsifies the claim that the lower infrastructure has never avoided real work. Its end-to-end value and domain still have to be measured. `[R16]`

### 2.2 The current relaxation has a cheap complete route

The saved complete lower model permits Scour, Alchemy, then a four-affix Fracture exit costing 405.1 with zero outside-domain continuation. Exact evaluation of that optimistic policy gives approximately **405.3694021469**, almost equal to the checked lower. The selected route has no Chaos action. `[R06]`

More iterations of that unchanged model cannot raise the result materially, and a Chaos-only refinement cannot remove that competing route. The number is an **auxiliary-model ceiling**, not a native upper. A follow-up must remove an actual optimistic escape or change a useful consumer, not improve a nonlimiting diagnostic.

### 2.3 Proof opportunity and proof validity are different

A safely evaluated policy does not make its coarse value estimates exact. The preceding five-goal evidence records a coarse estimate around 358 million while the compiled graph evaluates near 85.6k. Only the latter is upper authority. New demanded-row construction now passes the old four-goal selected-table boundary without pretending the old snapshot owned those decisions. Do not repeat that diagnosis as the current blocker. `[R06, R27–R29]`

The allocation question now has measured answers. Ordinary longer controls did not obtain strict work; the native-only handoff did. But approximately three minutes of four-goal strict work did not complete the remaining frontier, and the five-goal attempt stopped on partition memory. Adding time or recreating the handoff is not the next intervention without a changed premise. This does not prove a universal architectural limit; it identifies the tested path's current cost owners. `[R28, R30]`

### 2.4 The new strict consumer is valid but weakly action-discriminating

For a complete strict source cell C, the new code installs an alternative lower of the form

```text
B(C,a) = max(price_lower(a), min(h(s) for every covered member s of C)).
```

The complete-cell minimum makes the statewise lower uniform across the cell. The maximum, not a sum, combines independent floors. Under the existing native bridge, h(s) <= V*(s) <= Q*(s,a); a nonnegative continuation separately justifies the price floor. This is a source-state floor shared by the cell's alternatives, not an action-conditioned successor expectation c(s,a) + E[h(s')]. Its correctness does not promise useful discrimination. `[R27, R32]`

F7 actually strengthens **74,015** distinct obligations in **1.654 ms** of lookup time, but records **zero noncompetitive retirements** and the same root gap. F8 strengthens 191,505 obligations without a root gain. Avoid optimizing this already small lookup timer or saying the consumer is absent. Inspect the still-partial demanded frontier and binding complete-model constraints. A stronger, valid action-specific or boundary relation remains a separate hypothesis; these results do not refute all admissible heuristic research. `[R28]`

## 3. Overfitting: the evidence is stronger than “we used one fixture often”

### 3.1 Test availability is not effective test coverage

There are 146 seeded natural-T1 cases: 14 smoke, 120 full-short, and 12 deep. But their generator draws from only six fixed bases, all at item level 86, with empty Rare starts and natural T1 goals. That generator covers goals one through four. Its development/validation/frozen-test split is by **stratum**, not held-out base or modifier-pool family. `[R07, R08, R09]`

The newer quality ladder contains 18 primary cases across only three bases: nine Conquest, five Spine Bow and four Amethyst Ring. The qualification-1,024 copies are alternative runtime settings, not additional semantic diversity. The Lab's seven entries include four Conquest cases, one Bow, and a duplicate control/shadow pair for one renewal problem; the Lab manifest is not a broad capability gate. `[R11, R12]`

The preceding finish qualification's four populations were all Conquest. The new commit again qualifies ordinary short Conquest four/five and its exact three-suffix anchor; its long treatments are also Conquest. These are useful controls of the changed behavior, but add no cross-base evidence. The partial three-to-five high-water discrepancy remains open rather than being silently requalified. `[R06, R28, R33]`

### 3.2 The lower is parameterized, not hardcoded to a base name—but is structurally narrow

The phase lower binds actual base, item level, goal masks, prices, artifact and action descriptors. I did not find evidence that it returns a memorized Conquest value. Its native evidence depends on the session's real weights. `[R13, R14]`

Nevertheless, the probability producer explicitly refuses more than five goal slots and a rare side capacity other than three. Its frame excludes generic influence, veiled state and other unsupported item flags, needs a natural goal fracture frame, rejects unsupported Imprint memory, and rejects overlapping/duplicate satisfying categories that need a richer removal model. The consumer's guarded fallback can be correct while providing no new strength on those requests. `[R14]`

This is **structural specialization and evaluation overfitting**, not evidence of deliberately invalid hardcoding. The same distinction applies to first-frame selection: choosing a deterministic eligible goal frame is sound only under its domain proof, but can make heuristic strength depend on goal order or frame choice. A goal-permutation experiment should measure that sensitivity without claiming every bounded policy must be identical.

### 3.3 Diversity must cover the effective transition problem

Base name, goal count, and natural pool size are not enough. Current metadata should additionally characterize:

- prefix/suffix weight asymmetry and target-weight skew;
- goal-member multiplicity, overlap and exclusion connectivity;
- actual useful/available mechanic families and generated-program envelope size;
- retained affixes, crafted/filter/veiled/influence state and terminal cleanup debt;
- direct/guaranteed additions versus probabilistic joint acquisition;
- observed kernel support/fanout, bytes per completed row and time to first verified policy.

These are proposed diagnostic descriptors derived from native metadata or existing telemetry, not new proof assumptions. They guide which existing test to run. A similar descriptor vector does not license sharing transition probabilities or certificates across bases.

## 4. The heterogeneous failure families already visible in the repository

| Family | Retained evidence | What it falsifies |
|---|---|---|
| Conquest empty five | Ordinary policy near 85.6k; long diagnostic proof caps during replay-backed partition construction | More proof time/heuristic hits alone do not remove proof-memory demand |
| Spine Bow four/five | August 26 reached 200k-state cap; uppers 223,349 and 17.073 billion | Same goal count and finish budget do not create Conquest-like search behavior |
| Amethyst Ring two/three | August 26 memory cap after 7.5/12.8 seconds, no policy | “Only a few mods” does not mean easy; longer wall time alone does not fix memory exhaustion |
| Amethyst Ring four | Memory cap around 23.5 seconds, upper 3.725 billion | A completed bounded result may still be practically useless |
| Onyx Amulet three suffixes | Historical complete Chaos support 3,204,323 with >1 GiB peak | A same-side goal can still have enormous transition work |
| Rare-goal Amethyst PSS | Historical support 712,877, target single-draw weight as low as 50 | Pool shape and probability skew differ from familiar armour targets |
| Jewelled Foil PPSS | Goal weight 25 in a 153-mod pool; historical first-expansion cap | Balanced sides and rare weapon targets are a distinct decomposition test |
| Dire Pelt PPS | Historical policy obtained, then repeated constructive/properness and memory work | Not all bases fail before a policy; the right optimization stage differs |

The first four non-Conquest runtime numbers come from the August 26 matrix. The support counts come from a July complete-row census under its diagnostic semantics and resource settings. They are not claimed as current product runs. The diverse cohort turns these known failure hypotheses into current questions. `[R03, R10, R18]`

One particularly instructive comparison: the Onyx three-goal row has **14.4 times** the successor support of the Spine Bow four-goal row. Both historical supports are almost dense products of the side projections. Generic meet-in-the-middle or prefix/suffix factorization does not automatically avoid the joint probability work. `[R10]`

## 5. What paid off, and what mostly increased cost or complexity

### Retain and exploit

**Continuation completion and lifecycle repairs.** These changed returned policies, prevented refusal from suppressing ordinary search, and preserved completed useful work. They have the strongest recent causal evidence. `[R05, R06]`

**Eager construction of the small checked retention model.** This removed dozens of redundant refinement rounds and improved browser delivery without altering the target or discarding actions. `[R06]`

**Native checking, properness, identity and bounded artifact retention.** These prevented attractive but invalid values from becoming public results. They also make safe reuse possible. This is necessary infrastructure, but its construction is not itself a capability benchmark.

**Existing benchmark/corpus/reporting/Lab tools.** The corpus already distinguishes many kinds of work and keeps evidence. The missing ingredient is a required diverse decision gate, not another measurement platform. `[R07, R11, R12, R22]`

### Do not confuse local success with payoff

**The approximately elevenfold root-lower gain.** It is real, but the five-goal interval still spans a factor of about 211 in upper/lower. The causal controls did not establish that this lower created the recovered policy. A better heuristic can remain useful in other local comparisons; root ratio alone does not prove uselessness. `[R06]`

**Many extra rows or resumed requests.** C1/C2 substantially increased services without completing a useful candidate. The useful unit was a completed executable continuation, not service count. `[R06]`

**Demanded continuations and deferred option kernels.** The old short-watchdog experiments were rejected; the new longer-window attempt now passes that table boundary with complete rows and retains verified fallback delivery. It also removes a specific eager-admission cap. It still fails the root-improvement gate at a later frontier/memory obstruction. Count it as qualified intermediate machinery, not exact closure or broad speedup. `[R06, R28, R29]`

### Do not revive rejected designs without changed premises

**Immediate resume at every ready dependency:** hundreds of attempts, worse policy; removed. `[R06]`

**Full shared destructive-reforge DAG replay:** exact parity but roughly 49 MB extra storage, no avoided successor allocation in the measured cases, and 26.3%/32.9% slower total time. Replaying the same dense probability work through a shared graph is not the desired reuse. `[R17]`

**A second coarse option planner:** attractive estimates rejected by complete-mass evaluation; historical recommendation later shifted to verified fragments. Preserve the failure lesson without rebuilding its machinery under a basin name. `[R19]`

## 6. Are endgames being rediscovered instead of reused?

The answer is **partly, but the solver does not lack all reuse**.

The ordinary ladder already retains candidate-local continuations. Kernel/graph reuse and persistent strict proof ownership also exist. The July acceptance even recorded constructive synthesis and hundreds of reuses. Therefore “add memoization” or “introduce a persistent quotient” would be an inaccurate recommendation. `[R15, R25, R26]`

The missing boundary is reuse of a completed, correctly scoped continuation when another candidate or exact entry needs it. A root-evaluated strategy does not automatically provide a certified scalar for arbitrary entries, and a selected snapshot does not grow merely because the calculator gains states. Those are the places where compatible work can be discarded, inaccessible, or reconstructed. `[R06, R25]`

Measure reuse in layers: identical kernel requests; identical selected-row completion; repeated properness/evaluation of the same controller/domain; repeated lower preparation; and new entries requiring genuinely new information. Millions of lookups are not millions of reusable independent results.

I recommend run-local reusable completion evidence first. Reuse the existing owners and immutable handles. Cross-request persistence should be a later experiment requiring measured repetition and full identity reconciliation. Across bases, reuse a proposal template or feature-aware construction rule; do not copy a policy value or probability law merely because both items have two good prefixes.

## 7. Ranked next directions

### 1. Cross-base resource-aware continuation search and proof allocation

**Highest confidence.** First-policy starvation, candidate suppression, uncompleted siblings and finalization delays are measured causes. Preserve the existing ladder, but make the cost of useful completed work visible across the cohort. Service complete named dependency batches rather than every “promising” state or every newly ready dependency. Preserve the cheapest verified artifact and reuse the now-existing diagnostic proof window rather than rebuilding it. The leading measured implementation hypotheses are bounded frontier completion and partition-memory/rebuild reduction, conditional on their appearance across the cohort. `[R28, R29]`

The implementation should follow the first failing stage of each family: initial automatic admission, transition construction, policy assembly, numerical evaluation, or alternative proof. A single fixed global ratio tuned on Conquest is not the goal. A cheap resource/fanout estimate can order work without receiving pruning authority. All deferred native actions remain represented as obligations.

**Falsification:** a continuation/partition repair helps only the familiar case, or earlier proof service displaces enough discovery that another class loses its prior policy or exact answer. Stop the scheduling variant on that evidence; do not rescue it by deleting its bad control.

### 2. Reusable certified goal-side/endgame basins inside the existing ladder

**Most promising structural research bet, conditional on measured repeated completion work.** A basin is an actual entry domain with a controller reaching the original goal, or with complete typed exits into compatible continuations. It is not just a goal mask. Preserve junk, capacity, blockers, filter state, price/scope identity, observations and destructive exits.

For a forward policy that reaches a covered basin or the goal almost surely at a well-defined stopping time, its expected prefix cost plus the compatible basin continuation cost yields an executable upper after the ordinary global checks. If several local controllers return to one another, local termination is insufficient; the composition must still be proper globally.

Lower values remain a separate product: an exact optimum within a restricted endgame policy library is not a lower on the unrestricted product problem. Full native alternatives must still be covered by lower proof.

The appropriate “bidirectional” version is backward construction of useful completion domains plus **forward stochastic boundary connection**, not reversing stochastic edges. A predecessor action with one favorable connection and many destructive exits has not met the endgame merely because one branch did. Options/SMDP research supports explicit temporal policies and termination, and transfer research warns that changed dynamics can invalidate an apparently similar option. Neither solves the native certificate or performance problem automatically. `[L04, L05]`

**Falsification:** few repeated compatible entries, almost no same-run basin hits, expensive certification outweighing saved work, or a second whole graph built before any useful policy. Stop rather than creating a permanent hand-authored fragment catalogue.

### 3. Pool-aware lower/transition refinement driven by the limiting optimistic behavior

Keep admissible probability/retention machinery, but specialize/refine according to actual native pool structure and the currently binding constraint. For some classes the next useful result may be a cheap expected-potential query rather than an explicit successor list. For others it may be a better complete descriptor/family lower, or a capacity/exclusion distinction.

The old dense-frontier experiment shows why sharing a large representation is not enough: the work itself must be avoided or bounded. The current Scour/Alchemy/Fracture optimistic route shows why strengthening an unrelated Chaos expression does not help. `[R06, R17]`

Probability-aware Cartesian CEGAR provides a relevant principle: refine the actual flaw in an optimistic solution while retaining native coverage. Its theorem is for its stated formal abstraction, not automatic validation of this procedural implementation. CG-iLAO* supplies another relevant lesson—avoid repeated expensive action-value calculations—but this repository already has delayed actions and lower placeholders. Reuse them rather than porting another search engine. `[L03, L07]`

**Falsification:** preparation overwhelms search, scope refusal dominates new bases, or another unchanged family fixes the complete lower. Require actual consumer effect on at least two different pool families before broad promotion.

### 4. Native parallelism on a measured independent workload

Native multithreading deserves a bounded experiment; it should not be dismissed merely because the browser historically constrained execution. The current engine is single-threaded; its data/session contract allows immutable sharing, while mutable action contexts require worker-local ownership. `[R21, R23]`

First profile the diverse cohort. Good initial candidates are immutable-row numerical evaluations, independent native query batches, or separate automatic-program evaluations with deterministic coordinator commit. Do not concurrently mutate a shared calculator state arena, action catalogue, cache or proof ledger. Preserve all mass, canonical results, cancellation and a global reservation that includes worker scratch.

Memory-limited Ring cases may get worse with duplicated worker state. A first test should compare one, two and four workers at the **same total memory and wall-clock contract**, then separately label any resource-scaling study. Parallel corpus execution is already supported and is not within-solve acceleration.

**Falsification:** no substantial parallel fraction, memory duplication moves failures earlier, or kernel speedup yields no better end-to-end policy/proof result. A faster inner loop alone is not a release gate.

### 5. Learned ordering or proposal guidance, after a cross-base evaluation baseline

A learned model may rank actions, continuation obligations or abstraction refinements, or propose controllers that are independently checked. It must not author transition probabilities, state equivalence, admissible bounds, or permanent pruning decisions without their ordinary proof.

ASNets is relevant because it learns relational policies across problem instances, rather than memorizing one fixed action vocabulary. Its results do not guarantee optimality or transfer across different PoE pool families. For this project, compare a small model against simple deterministic feature-based ranking and hold out whole base/pool/mechanic families. `[L06]`

Do not train on a handful of successful Conquest traces or use incomplete coarse values as optimal labels. Include failures and censoring. An ordering policy can preserve safety while starving the work required for eventual proof, so fairness and fallback need testing.

**Falsification:** no benefit on held-out families, gains disappear after inference/training cost, or the model simply learns the familiar base/action IDs. Do not expand model size to hide that failure.

## 8. Proposed diagnostic and regression suite

The attached selection contains **12 cases, seven bases, six item classes**, goal counts one through five, several side distributions, both empty and partial starts, easy and rare goals, and different recorded failure phases. The exact file references are in `diagnostic_cohort.json` and `.csv`.

| ID | Case | Why it belongs |
|---|---|---|
| CB01 | Conquest empty 5 | Familiar reference retained, not allowed to dominate the whole programme |
| CB02 | Conquest empty 4 | Now-measured demanded-frontier cost after passing the old table/finish barriers |
| CB03 | Conquest partial 3→5 | Potential lost 80.7k capability hidden by a recent weaker baseline |
| CB04 | Spine Bow empty 4 | Weapon state-cap and non-armour control |
| CB05 | Spine Bow empty 5 | Same cardinality as headline case, different usable mechanics/pool |
| CB06 | Amethyst Ring empty 2 | Early memory/no-policy failure despite few goals |
| CB07 | Amethyst Ring empty 4 | Hard jewellery failure and balanced goal content |
| CB08 | Onyx Amulet SSS 3 | Huge joint support despite same-side target |
| CB09 | Amethyst rare PSS 3 | Probability skew and mixed-side failure distinct from CB06/07 |
| CB10 | Jewelled Foil PPSS 4 | Rare 25-weight target and another weapon pool |
| CB11 | Dire Pelt PPS 3 | Different armour base; historically reached policy then repeated costly checks |
| CB12 | Vaal Regalia single suffix | Fixed preparation tax and easy-task non-regression on another pool |

Most are deliberately already-exposed regression cases, **not a fresh held-out test set**. Preserve their original development/validation/frozen-test metadata and add actual exposure information. Do not silently rewrite old roles. Before an implementation trial, reserve two supported base/pool families outside this core—one caster-weapon-like and one shield/belt-like family if supported—and select their goals with the existing native generator. Do not claim unsupported base domains as solver regressions. `[R08, R09]`

Retain the current same-side exact and PDR references as short sentinels with their own exact identities. Add rotating native-validated scope probes for influence, relaxed tier thresholds, initial implicits, any-k/overlapping goals, crafted/veiled partials, and six-goal ordinary capacity where applicable. These broaden coverage beyond all-natural T1 at level 86 without forcing every ordinary test to exercise every feature.

### Profile and comparability

Keep the existing historical cases unchanged. For current development, derive separately identified cases through the current tooling, using the selected Allflame economy, current product scope, a **240-second requested finish / 300-second native watchdog / 315-second outer cleanup**, and initially **1 GiB**. All other caps, flags, step sizes, artifact and clock origins must be explicit and identical between A/B arms. Legacy Mirage cases translated into that profile are new benchmark targets, not directly comparable old costs.

The ordinary cross-base baseline leaves `--proof-handoff-seconds` disabled. Any handoff probe is a separately identified treatment, not a hidden default at 60 seconds. Reuse the existing long-profile definitions without overwriting their historical outputs. Use one long trajectory with real observations near 60/120/180/240 seconds. Do not retrospectively call a final evaluated policy a verified incumbent at an earlier time. An actual short stop changes finalization and may select another policy; a sampled 60-second point of a long run does not replace the short user-path regression.

A state/memory cap reached before a useful policy is not cured by waiting longer. A separate resource-scaling diagnostic may vary one binding cap with matching controls and host-safe reservations. Report it as resource scaling, not an algorithmic win.

### Metrics and gate

Primary: valid exact closures over all planned eligible cases, stratified by base/pool family. Secondary: verified-policy coverage, per-case certified gaps and predeclared time-to-policy/proof targets. Also report first-policy time, setup, admission, kernel, assembly, strict/evaluation time, memory, state/work caps, repeated semantic queries and delivered-artifact status.

Do not average Chaos costs across unrelated goals. Do not average only successful runs. Keep the worst family visible. Preserve each compatible known exact result and previously deliverable policy. A confirmed new crash, no-policy result on an old policy case, or loss of exact correctness blocks promotion.

Proposed materiality: for a major general-purpose change, require a new exact capability or meaningful same-budget gains on at least two non-Conquest base/pool families, without a confirmed material regression elsewhere. A 20% target-time/gap improvement is a reasonable predeclared engineering threshold; single-run near-threshold timing needs repetition rather than cherry-picking. This is a proposed gate, not an existing repository policy.

QVBS/QComp is relevant for separating model/property identity, correctness class and performance, and for exposing tradeoffs instead of forcing a single global score. The existing project measurement layer already implements much of that discipline. `[L01, L02, R22]`

## 9. Recommended next programme

### Chunk A — Cross-base baseline and high-water-mark recovery

Use the cohort and existing runner. Reconcile the partial 3→5 historical discrepancy first, then run current cross-base measurements and the same-source historical controls that are genuinely comparable. Independently re-evaluate retained old strategies as reference evidence, not search seeds. Record when a claimed regression is actually a changed target.

Exit with a capability matrix identifying the first limiting phase on each family. No new solver architecture is required for this chunk. Missing current outcomes stay missing, not inferred from Conquest.

### Chunk B — One shared frontier, memory or lifecycle repair

Select a failure reproduced in at least two different families, or a clearly universal correctness/delivery defect. Repair the earliest useful-work obstruction using current admission, continuation, portfolio, memory and cancellation owners. Keep ordinary discovery alive and the best verified artifact returnable. Validate full-row/properness/cap contracts with focused fixtures, then compare the relevant families and Conquest sentinels.

Keep the now-landed handoff, new-candidate row construction, deferred exact option kernels and compatible strict-cell lower consumer. Do not restore the older rejected patches unchanged, rebuild the same handoff, or relabel parent 4741 as the still-current obstruction. Trace a frontier through completed row/closure use; for the partition-memory branch, measure retained versus scratch overlap and repeated rebuilds. Closed original action-family coverage remains a separate requirement. `[R27–R33]`

### Chunk C — Run-local certified endgame reuse pilot

Proceed only when Chunk A/B identifies repeated compatible completion work after accounting for the new demand-driven and deferred-kernel mechanisms; kernel/row counts alone do not establish duplication. Use two families with different mechanic availability. Extract/build one small native-validated completion domain through the existing ladder; retain its entry scope and controller. Demonstrate reuse by at least two actual forward-reached entries or candidates, and measure saved assembly/evaluation/kernel work including certification overhead.

The pilot must yield an end-to-end policy/gap/proof benefit, not merely a cache hit. Keep the original final-goal predicate. Do not turn smaller junk-free goals into invalid relaxations or terminate on a partial item. No permanent manually authored fragment catalogue or second coarse SMDP planner.

### Chunk D — One limiting proof/query improvement, then broad qualification

Choose the next mechanism from the cohort evidence: a pool-aware relation, a complete descriptor bound, or a bounded projected expectation that avoids large support materialization. If numerical/transition compute genuinely dominates on multiple families and memory permits, a small native parallel prototype is an alternative, not an additional mandatory system.

Qualify on the entire small core and held-out families once the retained change is coherent. Update existing mathematical claims, research questions and report views. Do not build a new evidence framework. Keep failures, false leads and scope limits.

### Stop conditions

The useful-proof-time settings experiment is already deliberately stopped with its main goal unmet. Do not resume it merely because the old objective remains open. Stop or redirect a new branch when it only helps the familiar fixture; when no actual repeated compatible subproblem exists; when a cheap unchanged placeholder caps every possible gain; when it duplicates a full graph before producing an incumbent; when correctness requires suppressing a real action or renormalizing missing outcomes; when resource growth eliminates its end-to-end benefit; or when two discriminating variants reproduce the same failure with no new causal premise.

A sound local result can be retained as research evidence without being a promoted general-purpose feature. Increasing a test budget does not erase its old failure. Do not stop the programme merely after another helper passes; stop at the declared broad capability result or a concrete blocked diagnosis.

## 10. Complexity and removal decisions

The architecture is **overly complicated at some ownership and work-scheduling boundaries**, not because every proof distinction is unnecessary. Coarse selected values, executable entry certificates, exact alternatives and native lower models really do have different guarantees. Collapsing their types would make correctness worse.

Simplify responsibilities instead: one completion-request owner, one durable selected-controller owner, one lower contribution per declared scope, one candidate/evaluation/publication path, and one accounted place for each large payload. Require a consumer or independent-oracle purpose for every retained shadow lane.

Removal candidates require a complete local reference/build audit; this read-only search is not proof of dead code. Isolate old one-off probes and research-only verifier components from routine builds where possible, keeping minimal counterexamples and artifact reproduction. Do not delete existing shared checkers, the Lab runner, or the active retention producer just because they do not themselves close five goals.

The June ML document describes an older multiobjective/budgeted planning direction that differs from the current exact expected-cost target. It is explicitly marked research history; keep useful references but make obsolete objectives inaccessible as default implementation guidance. Do not create another large replacement ML architecture document before selecting a model experiment. `[R20]`

The mathematical backbone is worth keeping, but correctness documentation and millions of passing checks are not a proxy for cross-base capability. Current quantitative claims should link to cohort outcomes, including missing ones. The remedy is a small regression gate and reuse of existing question reporting, not more standalone plan/result ceremonies.

## 11. What to stop doing now

1. Approving a major solver change solely because Conquest/PDR improved or remained stable.
2. Comparing with the most recent catastrophic fallback while ignoring the best compatible historical policy.
3. Treating extra lower values, lookups, rows, services or exact synthetic checks as an end-to-end improvement.
4. Reading old non-Conquest caps as current facts—or assuming recent armour fixes resolved them without measurement.
5. Rebuilding a whole strict graph, all automatic kernels, or a full shared reforge DAG before identifying a useful consumer.
6. Reusing a selected-policy or option value as a lower, or erasing action/control state to manufacture transfer.
7. Rebuilding the now-landed handoff, treating source-floor lookups as action-specific proof, or repeatedly shifting its timer without a new premise.
8. Re-running identical large Simulator qualifications or broad suites after every iteration.
9. Replacing the working benchmark/Lab/backbone machinery instead of selecting and enforcing a diverse gate.
10. Training a neural model on a few familiar successful traces before establishing held-out base/pool evaluation.
11. Treating a longer watchdog, more workers, or more memory as a silent algorithmic improvement.

## 12. Strongest objection to this recommendation

A reusable endgame-basin project could become the next elegant local abstraction that costs more than it saves, repeating the failed carrier/fragment architecture. Cross-base qualification also spends effort on cases that may not match Oliver's most common usage.

That objection changes the sequencing: **measure breadth first, fix a shared concrete obstruction second, and prototype reuse only when repeated work and reachable entry domains are actually observed**. Conquest remains a core case, but no longer grants permission to generalize. A failed basin pilot should be removed with its counterexample; it is not a reason to keep adding state variables until it resembles the original full problem.

My confidence is high that the current progress narrative is too concentrated and that several historical capabilities need explicit recovery checks. Confidence is moderate that lifecycle-aware continuation reuse is the best next structural direction. Confidence is low in any claimed speedup from threading, learned guidance, or a new abstraction before the current diverse cohort is run.

**Bottom line:** change the unit of progress from “a stronger mechanism on Conquest” to “more different crafting problems return useful verified policies and close exactly under a declared budget.” Retain the hard-won proof infrastructure, but let a small diverse capability suite decide which part deserves the next month of work.

## Source register

- **R01** — [Latest handoff](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/HANDOFF.md)
- **R02** — [August 22 carrier-ladder result](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-08-22-exact-goal-carrier-ladder/result.md)
- **R03** — [August 26 cross-base quality matrix](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-08-26-solver-quality-debt-retirement/gate0-evidence.md)
- **R04** — [August 24 harmful ordering experiment](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-08-24-carrier-aware-control-authority/evidence/gate-b-result.md)
- **R05** — [August 30 candidate reclamation](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-08-30-carrier-ladder-released-candidate-reclamation-v1/README.md)
- **R06** — [September 9 continuation, preparation, and finish evidence](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/active/2026-09-09-empty-start-partial-continuation/README.md)
- **R07** — [Seeded corpus specification](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-natural-t1/v1/README.md)
- **R08** — [Seeded corpus generator configuration](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-natural-t1/v1/generator-config.json)
- **R09** — [Seeded corpus evaluation roles](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-natural-t1/v1/evaluation-roles.json)
- **R10** — [July complete-successor census](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-natural-t1/v1/evidence/true-successor-frontier-census-summary.json)
- **R11** — [Quality-ladder manifest](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-quality-ladder/v1/manifest.json)
- **R12** — [Solver Lab manifest](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-lab/v1/manifest.json)
- **R13** — [Phase lower context/reuse implementation](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/engine/src/solver_phase_lower.cpp)
- **R14** — [Phase probability frame and model implementation](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/engine/src/solver_phase_probability.cpp)
- **R15** — [Retained scheduling contract](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/solver/scheduling-bellman.md)
- **R16** — [Lower-bound retirement result](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-08-26-solver-quality-debt-retirement/gate5-evidence.md)
- **R17** — [Rejected full shared-reforge DAG](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-07-28-harvest-shared-reforge-frontier/report.md)
- **R18** — [Older five-base causal study](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-07-25-gap-directed-natural-t1-research/report.md)
- **R19** — [Historical option/fragment audit](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/archive/2026-08-27-solver-research-audits/executable-options-audit.md)
- **R20** — [Older ML planning proposal](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/future/ml.md)
- **R21** — [Native API lifetime/threading contract](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/engine/include/poecraft/api.h)
- **R22** — [Current benchmarking contract](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/solver/benchmarking.md)
- **R23** — [WASM execution model](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/engine/wasm.md)
- **R24** — [Shared working rules](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/AGENTS.md)
- **R25** — [Current strict closure contract](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/docs/solver/strict-closure.md)
- **L01** — [QVBS: models, properties, and tool results](https://qcomp.org/benchmarks/about.html)
- **L02** — [QComp 2020: correctness tracks and no global ranking](https://qcomp.org/competition/2020/)
- **L03** — [Schmalz and Trevizan, Efficient Constraint Generation for SSPs, Artificial Intelligence 354 (2026), 104505](https://arxiv.org/abs/2604.01855)
- **L04** — [Sutton, Precup and Singh, Between MDPs and semi-MDPs, Artificial Intelligence 112 (1999), 181–211](https://www.sciencedirect.com/science/article/pii/S0004370299000521)
- **L05** — [Han and Tschiatschek, Option Transfer and SMDP Abstraction with Successor Features, IJCAI 2022](https://arxiv.org/abs/2110.09196)
- **L06** — [Toyer et al., ASNets: Deep Learning for Generalised Planning, JAIR 68 (2020), 1–68](https://arxiv.org/abs/1908.01362)
- **L07** — [Kloessner, Seipp and Steinmetz, Cartesian Abstractions and Saturated Cost Partitioning in Probabilistic Planning, ECAI 2023](https://fai.cs.uni-saarland.de/kloessner/papers/kloessner-etal-ecai23.pdf)
- **R26** — [July constructive-policy acceptance and reuse](https://github.com/OliverOrton/poecraft2/blob/317438392c88c0151f41e48b4b0323b06b1d0037/fixtures/solver-natural-t1/v1/evidence/b6-acceptance-summary.json)

### Additional sources for the latest-main update

- **R27** — [Candidate handoff and demanded-continuation commit and diff](https://github.com/OliverOrton/poecraft2/commit/be553608ecabde28a3dec07856255911459ec97d)
- **R28** — [Useful-proof-time living evidence, F0–F14](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/README.md#useful-proof-time)
- **R29** — [Updated strict-closure contract](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/strict-closure.md)
- **R30** — [Updated benchmarking contract and native-only proof handoff](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/solver/benchmarking.md)
- **R31** — [Retained proof-time comparison data](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/proof-time-comparison.json)
- **R32** — [Strict whole-cell completion-floor implementation](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/engine/src/solver_policy_refinement.cpp)
- **R33** — [Current handoff and explicit stopped-experiment disposition](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/HANDOFF.md)
- **R34** — [Existing immutable 240/300-second derived profile](https://github.com/OliverOrton/poecraft2/blob/be553608ecabde28a3dec07856255911459ec97d/docs/active/2026-09-09-empty-start-partial-continuation/long-profile/manifest.json)
