**I would prioritize a coarser policy-search layout that excludes the unused resistance-conversion pair—not a general mid-solve retirement system yet.** The source points to a much narrower target than dropping expensive or currently unused action families indiscriminately.

I reviewed the pinned `84f02ee3b603fe3879ac2c4a885d1b81ba67772f` source and retained measurements. **I did not complete the native leave-one-out rebuilds:** GitHub content was readable, but the execution environment could not retrieve a checkout. Consequently, the new per-candidate junk-class counts and CB08 row reductions remain **unmeasured**. What I could establish is which candidates drive the relevant distinctions, whether the incumbents use them, and what the existing evidence says about likely savings.

## 1. CB08’s four persistent tags point to two actions

The important distinction is between an action’s **roll-pool tags** and **persistent state discriminators**.

In the current registry, **Harvest Reforge and Harvest Augment do not add their target tags to** **`discriminating_tag_ids`**. The source explicitly distinguishes selecting a roll pool from inspecting an existing modifier’s classification. Resistance conversion does inspect existing modifiers, and each conversion declares:

```text
{source element, target element, resistance}
```

as persistent discriminators. Cannot Roll Attack/Caster bench actions can introduce additional attack/caster distinctions, but ordinary targeted Harvest actions do not.

CB08’s goals are Lightning Damage, Chaos Resistance, and All Elemental Resistances. Its recorded goal tags include **lightning**, but not fire or cold. The current admission rule retains a Harvest resistance conversion when its **target tag** occurs in the goal. Combining those two source facts identifies the relevant pair:

```text
harvest_resist:fire:lightning
harvest_resist:cold:lightning
```

Together, they introduce precisely **fire, cold, lightning, and resistance**. This is a source-derived attribution, not a newly executed layout diagnostic.

### The diagnostic has a more focused shape than “31 equally interesting removals”

Using your reported 21-class baseline:

| CB08 candidate changeCandidate countPersistent tag vocabulary attributable to these candidatesRebuilt junk classes |    |                                   |                                   |
| ------------------------------------------------------------------------------------------------------------------ | -- | --------------------------------- | --------------------------------- |
| None                                                                                                               | 31 | Fire, cold, lightning, resistance | 21 — supplied baseline            |
| Remove `harvest_resist:fire:lightning`                                                                             | 30 | Cold, lightning, resistance       | Not measured                      |
| Remove `harvest_resist:cold:lightning`                                                                             | 30 | Fire, lightning, resistance       | Not measured                      |
| Remove both conversions                                                                                            | 29 | None of those four                | Not measured                      |
| Remove another candidate individually                                                                              | 30 | These four remain                 | No tag-driven coarsening expected |

The last row is conditional on keeping the parent’s other layout inputs and dependencies unchanged. The builder unions discriminator tags over its actual layout actions; it does not assign independent state-space costs to each candidate.

**The joint removal matters.** Each conversion individually keeps lightning and resistance alive. Therefore, the sum of the two leave-one-out effects need not capture the benefit of removing both.

There is another relevant source detail: when automatic candidates are enabled, the builder retains **every session explicit-affix modifier in the reachable universe**, including modifiers that the remaining direct candidates cannot produce. Consequently, deleting an essence, fossil, or other non-discriminating candidate does not necessarily remove its modifier classes from the product parent. That makes discriminator removal more promising than merely shortening the candidate list.

## 2. The incumbent cross-check supports testing those removals

### CB08 Amulet

The retained independent evaluation lists these executed actions:

| Action or action groupShare of evaluated crafting cost |          |
| ------------------------------------------------------ | -------- |
| Annul                                                  | 80.9026% |
| Exalt                                                  | 14.7779% |
| Remove Crafted Modifiers                               | 3.1234%  |
| Three temporary bench crafts                           | 1.1961%  |

The three bench operations are `JunMaster2DexterityAndIntelligence2`, `JunMaster2StrengthAndDexterity2`, and `JunMaster2BaseManaAndLifeRegen2`. **Neither resistance conversion appears in the recorded controller’s action-use accounting.** The evaluated total is 40,215,428.995558396 chaos.

This gives a concrete hypothesis:

> **CB08 may be paying for four persistent tag distinctions throughout broad search even though its returned executable controller uses neither action responsible for those distinctions.**

That does not prove the conversions are suboptimal. It does make them a focused **policy-search ablation** that need not discard the incumbent’s actual crafting operations.

### CB06 Ring

The original-profile Ring controller’s accounting contains just three operation identities:

| ActionShare of evaluated crafting cost |          |
| -------------------------------------- | -------- |
| Harvest Augment Defences               | 98.9047% |
| Annul                                  | 1.0476%  |
| Exalt                                  | 0.0477%  |

Neither resistance conversion appears here either. Crucially, **98.9% is a spending share—not a share of search work, nor evidence that the other operations are dispensable**. All three participate in the evaluated controller.

There is also a concrete warning against restricting future search to the original incumbent’s vocabulary. The preserved **4-GiB Ring controller is substantially cheaper**—204,763.14825000268 chaos—and uses prefix-lock and Cannot Roll Attack bench operations absent from that original three-operation controller. Its Harvest Augment Defences spending share is approximately **66.9%, not 98.9%**. Permanently forbidding every operation absent from the original incumbent would exclude that known cheaper controller.

**My preferred first restriction is therefore “remove the unused conversion pair while retaining competing acquisition routes and useful automatic programs,” not “keep only whatever the incumbent currently does.”**

## 3. Would the coarser layout materially shrink reforge rows?

**It is plausible and specifically motivated, but not yet demonstrated for current CB08.** The retained evidence shows why neither an automatic “yes” nor a dismissal would be justified.

### There is a measured precedent for substantial support reduction

A July 29 diagnostic on a **different four-goal case** rebuilt several layouts and projected an existing Chaos row into them. Its results include:

| Historical layout comparisonJunk classesProjected Chaos support           |           |                       |
| ------------------------------------------------------------------------- | --------- | --------------------- |
| Coarse layout with temporary-bench vocabulary → coarse core vocabulary    | 11 → 6    | **543 → 217**         |
| Forced-strict layout with that vocabulary → forced-strict core vocabulary | 114 → 105 | **134,477 → 134,477** |

The first comparison reduces projected support by **60.0%**, or about **2.50×**. The second removes classes without reducing that row’s support at all. These are retained **projection measurements**, not fresh CB08 calculations or measurements of native row-construction speed.

The existing audit source already contains the relevant operations—rebuilding layouts and projecting completed successor support—but its configurations are family-level additions, not the requested leave-one-candidate-out experiment. It also projects materialized representatives, so its historical outputs should not be promoted into a new uniform-equivalence certificate.

### Fewer output classes do not necessarily mean proportionally less reforge calculation

The reforge recurrence distinguishes more than the output junk class. Its junk roll buckets retain **side, junk class, blocker information, and family weight**, with multiplicities and per-bucket picks needed to track changing draw probabilities. Removing a persistent tag can merge some output states while leaving substantial internal weight/exclusion work intact.

There are therefore three separate possible savings:

**Smaller state payloads.** Each state has four junk-count vectors. Fewer classes can reduce those vectors even when successor count is unchanged.

**Smaller row support and fewer continuation obligations.** Multiple old successor states may become one new state. This is particularly relevant when broad rows leave thousands of distinct tails requiring continuation work.

**Less row-construction work and scratch memory.** This requires the recurrence itself to benefit from the merged distinctions; an output-support reduction alone does not establish it.

For your current problem, I would consider the second saving valuable even without a comparable recurrence speedup—but it must be measured, not inferred from “21 became fewer.”

## 4. Policy-search exclusion and proof retirement should remain separate

### For finding cheaper executable policies: yes

Let `A` be the requested action scope and `A'\subseteq A` the actions considered by a private search phase. Any proper, permitted, independently evaluated policy found using `A'` is still a valid executable policy for the original request.

But:

```math
V_A^* \le V_{A'}^*.
```

A restricted search can find a useful **upper witness**; its restricted optimum is not automatically a lower bound for the full scope. The current lower contract explicitly preserves this distinction.

I would initially use a **fresh restricted search context**, rather than mutate the live `CalcContext` in place. Preserve the old verified controller independently, and accept a replacement only after native compilation/evaluation establishes its complete behavior and cost.

This is not just caution about implementation complexity. The current builder can include the **old parent junk-class ID in the new class key** when `refinement_parent_layout` is supplied. That enforces refinement of the old partition: passing the old layout into that path would prevent precisely the merges this experiment seeks. A coarsening rebuild needs a new namespace, not the existing split-only relationship with a different candidate vector.

### For proof: a local retirement is not a global action deletion

The documented retirement rule concerns an action at the **same source or source class** as the compatible executable upper. It does not authorize erasing that action everywhere else.

To remove an action’s discriminator globally from an exact proof representation, you need more than a successful retirement at the root. The argument must cover its relevant uses throughout the represented domain, including members introduced by merging classes, future states, and program dependencies.

The clean-five record confirms **27,021 reached exact entries, 671,410 alternatives, and zero existing-lower retirements**. That is a historical measurement—not an impossibility result—but it provides no demonstrated supply of global retirements from which to obtain coarsening today.

A search action may instead remain **unmaterialized while its proof obligation is retained separately**. That is deferral, not retirement. It must not disappear from full-scope coverage.

Relevant literature supports avoiding unnecessary action evaluation: Schmalz and Trevizan’s constraint-generation work selectively introduces action constraints rather than repeatedly evaluating every action. But that result does not itself justify rebuilding a state quotient after dropping actions; action coverage and representation validity are separate obligations here. ([arXiv](https://arxiv.org/abs/2604.01855?utm_source=chatgpt.com "Efficient Constraint Generation for Stochastic Shortest Path Problems"))

## 5. The smallest decisive diagnostic

I would keep this narrower than a new solver mechanism.

**First, perform the layout-only comparison:** one baseline, 31 individual removals, and the joint conversion-pair removal. Reuse the same session and registry. Preserve the goal, product-parent mode, automatic-candidate setting, required reachable modifiers, and observation inputs. Recompute actual fixed-program dependencies rather than assuming “removed from direct candidates” means “absent from the layout.” The constructor explicitly adds those dependencies.

For each result, record the surviving discriminator tags, total and per-side junk classes, and the mapping of old classes to new classes. The useful attribution is:

```math
\Delta J(a)=J(A)-J(A\setminus\{a\}),
```

plus the joint-pair effect. Do not add individual `\Delta J` values and assume they equal the joint saving.

**Then project completed reforge rows before rebuilding them.** For a genuine coarsening, check that every old class’s complete member mask maps into one new class, aggregate all four junk-count categories, and combine probabilities for equal projected successor states. This answers the cheap question:

> How many distinct continuation states would disappear if we used this layout?

That test should preserve retry markers, goal/tier status, occupancy, protection, and the other unchanged state dimensions. It should not obtain a smaller count by discarding inconvenient outcomes or choosing one favorable representative.

**Only a promising support reduction warrants a fresh native row comparison.** At that point, compare positive-mass support, recurrence effort, scratch peak, and wall time under identical source/action inputs. A subsequent policy-search experiment should keep the best existing evaluated controller, so a failed coarsening treatment cannot degrade the returned result.

## Verdict

**There is a concrete, source-supported opportunity here: CB08’s two unused resistance conversions appear to maintain all four of its persistent tag discriminators.** Removing both from a private policy-search phase could coarsen the parent without removing the incumbent’s operations or the broad acquisition alternatives you want to investigate.

**I would not yet implement general in-place layout rebuilding, nor make proof coarsening depend on the currently unproductive retirement path.** The current evidence supports a narrowly targeted experiment, not a production optimization claim.

The missing result is specific: **the 21-class baseline’s actual leave-one-out/joint class counts, followed by the projected support of the affected current reforge rows.** Until those are obtained, material CB08 row shrinkage is unproven—even though the action attribution makes this a considerably better-targeted hypothesis than broad action dropping.