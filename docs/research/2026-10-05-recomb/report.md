# Random recombination: blocking, paid feeders, and bounded inventory planning

Research lane `recomb` · 2026-10-05 · documentation proposal only

## Decisions first

1. **Keep the current native joint blocking kernel and its explicit scenario boundary.** Main already implements pooled exclusive count collapse, physical occurrence selection, counts before blocking, full canonical exclusions, and a global maximum of one selected exclusive. There is no justification for rebuilding that kernel from the older Pro plan or for choosing a hidden default prefix/suffix order.
2. **Extend feeder preparation in the native owner, one full-output action family at a time.** The released checked-feeder checker supports Annul, Scour, and crafted removal; it is not a general paid acquisition-crafting certificate. Purchases and declared completed-feeder quotations already compete with recombination, but a quotation is not evidence that its child actually produces the item.
3. **Use complete expected continuation cost to choose feeders, fillers, duplicates, and recycling.** A filler can increase requested counts and still reduce clean-target success. A multi-mod feeder can improve structural retention enough to justify its price, but one-attempt success alone cannot establish the economic ranking.
4. **Preserve the two-input/one-output Builder and restricted native export/check bridge.** It is already released. Expand that bridge only after the native full-item laws and paid resource identities are checked. Advanced v3 export/Apply, general authored inventory evaluation, and automatic Current/Finder admission remain held.
5. **No new execution is authorized or needed by this report.** The first falsification work is a finite native oracle extension after a separately selected implementation programme. No new timed runs, Solver, Simulator, build, test, dependency installation, price/data refresh, deployment, or main merge occurred here.
6. **Treat 3.29 coverage as an explicit evidence and admission gap.** Official notes introduce Ducats; the newer nontransferable-filler discussion is an unqualified lead. Neither establishes a replacement count law or proves the existing bounded kernel wrong. New canonical families require the dispositions and future evidence gates in §3.2–3.3; a desired output base never chooses the physical carrier.

The strongest new planning contribution is the finite oracle set in §5 and the economic comparisons in §6. They distinguish non-transitive group conflicts, sequential weighted selection, clean-target penalties, overlapping multi-mod feeders, and asymmetric paid recycling. All numerical examples there are derived mathematics of the adopted model, not new native measurements or game odds.

## 1. Source, authority, and research disposition

### Snapshot and work boundary

Remote `main` was independently read through the connected GitHub branch endpoint at the start and rechecked before publication with the same full SHA:

`7252027c80856628ed16734583bfc9d6e166458b`.

The ordinary local checkout was `7eb16ac3d63834fd5d3256ab42483f47d264b764`. Current-main objects were available locally, so explicitly selected documents and source files were read at the full remote SHA into this task's own source snapshot. No checkout reset or shared source/document edit was used. Applicable root `AGENTS.md` was read; the available `.agents` directory contained no applicable skill entries. No independently readable KIDS attachment identifier was supplied in this task; the explicit delegated authorization and restrictions were applied, and no extra authority was inferred from unavailable attachments.

The causal solver worktree's observed local HEAD was `fb59476f54601f81dc18a8bf2457d8eb7d3eb36b`. It was read only to identify the supplied research-input directory and newer source state. No recombination qualification is transferred from that separate solver continuation. Its matched Conquest negative, root-only/statewise distinction, and budgets are independent of this lane.

Startup reading covered AGENTS, research standards, current status, HANDOFF, research sections 4/5, and the existing handoff template. The user-authorized documentation-only lane overrides the general canonical-integration workflow for this delivery: §10 supplies exact doc-ready proposals, while canonical documents stay untouched.

[Pinned main](https://github.com/OliverOrton/poecraft2/tree/7252027c80856628ed16734583bfc9d6e166458b) · [Research standards](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/research-standards.md) · [HANDOFF](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/HANDOFF.md) · [Research intake/knowledge ownership](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/research.md#4-research-outside-the-checkout-integration-inside-it).

### Evidence labels used throughout

| Label | Meaning |
| --- | --- |
| **Observed source** | Directly read behavior at the pinned main SHA; not a new execution receipt. |
| **Retained receipt** | Previously recorded checks with their own source/artifact/input scope; not rerun here. |
| **Derived** | A proof or exact rational calculation under explicitly stated premises. |
| **Hypothesis** | A candidate interpretation of hidden game mechanics that the observations do not identify. |
| **Proposal** | Future implementation or documentation work, unimplemented and unqualified here. |

The prior recombination model and feeder/Builder reports in Oliver's Library item `libfile_c7a9f2eb2d9c81919d8c3f61454d6a63` were read in relevant line windows, rather than reconstructed from search snippets. Their useful original reasoning is already represented in the native recombination contract and living programme record. The four later causal/economic Pro reports are present in the supplied solver programme's research-input directory; this lane did not reread unrelated Conquest arguments or duplicate them.

| Prior material | Disposition at current main |
| --- | --- |
| Pooled-side exclusive collapse, count-first blocking, physical duplicates, full groups | **Incorporated** in v3 native analysis; prior “implement the joint kernel” milestone is complete. |
| Explicit carrier-specific first-side scenarios; no default q=.10 | **Incorporated**; still estimated, with advanced Apply/export held. |
| One-attempt affine interval versus repeated-policy rational cost | **Incorporated** in the canonical conditional argument; robust certificate unimplemented. |
| Checked paid single-item feeders and restricted Builder export/check | **Incorporated within narrow scope**; removal laws only, no nested inventory or failed-child recovery. |
| Prior “source unbuilt” and component testing-failed statuses | **Historical**; current released integration has its separately named acceptance. Preserve old negatives. |
| General feeder generation, ordinary/exclusive bench registry, missing special weights | **Open**; no blanket classification or selected-mod mode follows. |
| General exact inventory evaluator and Current/Finder recombination action scope | **Held/out of this programme**. |

## 2. What current native code actually does

### Three model identities

`recombination.hpp` declares:

- v1: `poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1`.
- v2: `poe1-random-spawn-proxy-native-constraints-preserve-tier-roll-no-upgrade-v2`.
- v3: `poe1-random-blocking-sensitivity-preserve-tier-roll-no-upgrade-v3-draft1`.
- v3 configuration: `pooled-exclusive-counts-first-positive-proxy-order-scenario-v1`.

v1 ordinary/v2 bounded extended pairs can Apply under their adopted estimated point model. v3 requires a nonempty scenario ID and explicit finite carrier-specific prefix-first probabilities in [0,1]. It sets `full_item_apply_supported=false`. The sample path refuses before random selection, and the restricted Builder exporter rejects scenario/v3 requests. This is an intentional distinction between an analysis distribution and an approved executable random law.

[Model declarations](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.hpp#L9-L19) · [Pair validation/preparation](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L97-L117) · [Apply refusal](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L358-L368) · [Export boundary](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L515-L522).

### Inputs, carrier, and output state

Two distinct live resources and distinct roles are required. Same ordinary equipment class and compatible frozen data identity are required. Normal/Magic/Rare capacity validation is native; jewels/clusters, unsupported item flags, generic influence, fractured/crafted/veiled slot treatment, unknown classifications, and unsupported special categories refuse. The inspector can recognize a category that the pair provider still refuses; recognition is not admission.

The random pair path supports different compatible input bases and levels. Each actual input is a carrier at adopted probability 1/2, with its represented quality, memory, sockets, implicits, enchantments, flags, and Eldritch state. Output is Rare at

`min(floor((levelA+levelB)/2)+2, max(levelA,levelB))`.

Canonical IDs are retained and remapped into each new carrier-specific output session. Selected tiers/recorded rolls are preserved; normal output-level roll masks do not silently remove transferred occurrences. No upgrade or numeric reroll is invented. Structural goals exclude rolled totals and defence percentiles even though full Apply state preserves payloads.

The inventory planner is narrower: `root_item` requires the output to share the selected data object, base, and level. Mixed-base Builder execution is not evidence that the optimizer can price mixed-base inventory continuations. It must currently refuse outcomes outside that pinned scope.

[Native input structure/class contract](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_constraints.cpp#L121-L171) · [Carrier preparation and canonical remapping](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L235-L287) · [Planner base/level boundary](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L50-L74).

### Exact blocking within the estimated kernel

For physical occurrences x,y define the native conflict relation

`C(x,y) = same canonical mod ID OR shared full canonical group OR both known exclusive`.

That relation is applied against selected occurrences, not its transitive closure. Primary-group labels alone are insufficient. Duplicate occurrences retain input/slot identity and their individual rolls until selection excludes the remaining canonical duplicates. Dense session IDs are not cross-session canonical identity.

v1/v2 count physical occurrences before carrier eligibility filtering and enumerate the sides independently, within their at-most-one-exclusive/no-cross-side-group scope. v3 uses, for side d,

`n_d = ordinary physical occurrence count + indicator(any known exclusive occurrence)`.

Only exclusive **count contribution** collapses. Every exclusive occurrence remains a distinct weighted candidate. Both requested counts are drawn first. The chosen side fills sequentially; selected exclusives block all remaining exclusives across both sides; the second side fills to its already drawn count. If the pool exhausts it returns fewer occurrences, including zero. There is no redraw, replacement, or renormalization over successful/desired outcomes.

Cross-side canonical group overlap remains explicitly held by pair validation, even though the abstract joint helper checks conflicts across both sides. Removing that guard would expand native admission and is not justified by the helper alone. The exceptional bare `1p0s + 0p1s` pair remains refused in either orientation for all three models.

[Full conflict predicate](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_constraints.cpp#L112-L119) · [Count-first joint implementation](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L290-L356) · [Bare-split refusal](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L212-L216).

### Taxonomy and positive weight authority

Observed classification marks metamods, Essence-only, Delve, unveiled/veil templates, canonical elevated relations, and four beast Aspect types exclusive. Generic crafted rows remain unresolved. Some natural non-native tiers have native guaranteed Essence-source admission. Influence classification in the inspector does not bypass pair influence refusal.

An ordinary occurrence with carrier proxy zero contributes to the count but cannot be selected on that carrier. A known exclusive with zero carrier proxy refuses preparation: the code does not convert absent special-weight authority into either weight 1000 or certain impossibility. Supported specials still need positive proxies on both carrier branches. The selected weight is output-carrier **base spawn weight**, not generation percent, a price, a fossil adjustment, or a ranker value.

Bench availability, exclusivity, selection weight, and output crafted-flag/removal behavior are four separate contracts. A recipe's vendor field, name suffix, stat resemblance, or zero ordinary proxy cannot settle them collectively. The prior diary-derived closed candidate registry remains a proposal; current classifier code has no blanket crafted allow-list.

[Classifier](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_constraints.cpp#L74-L110) · [Supported pair categories](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L68-L95) · [Missing-weight refusal](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L273-L283).

## 3. Public mechanics evidence and uncertainty

### 3.1 Historical evidence retained within its scope

The official 3.26 addition states qualitatively that unpredictable recombination retains fewer modifiers on average when many input modifiers cannot normally roll, including veiled/Essence examples. It does not publish an exclusive-collapse formula, revised count coefficients, side-order probability, or special-weight map. The same official notes distinguish unpredictable random recombination from selectable recombination; only the random mode is relevant here. [GGG 3.26 notes, Recombinator Changes](https://www.pathofexile.com/forum/view-thread/3787013).

Butsicles' first-hand June 2025 report describes roughly 70 tests, including four exclusive occurrences becoming zero, as evidence for one count contribution per side. Its unequal-pool account says 17 tests but narrates one suffix-craft result and 15 prefix-craft results. Retained modifier outcomes combine count, order, selection weights, and conflicts; they do not directly reveal which side selected first. This supports retaining a leading hypothesis and uncertainty, not estimating q from the reported survival ratio. [Original tester report](https://www.reddit.com/r/pathofexile/comments/1ldc3lz/326_recomb_psas_a_few_useful_tipsconfirmations/).

The original 3.26 discussion includes 8 successes in 23 attempts and another 7 in 21 with different modifiers. The tester's explanation explicitly conditions its approximate calculation on weights, including a assumed crafted weight. Those observations do not identify the joint order/weight law. A detailed staged explanation in the same discussion retains count-two outcomes on the second side after exclusives disappear; it is useful correspondence evidence for the count-first convention, not a configuration-complete experiment proving it. [Original analysis and tester follow-up](https://www.reddit.com/r/pathofexile/comments/1lfyxxd/326_recombinators_analysisguide/).

This session re-opened those primary reports and the official notes. The Codeberg manuscript could not be read through web extraction. The previously inspected bench diary/calculator scripts were not independently downloaded again; their exact extraction/script hashes and provisional classification limitations remain attributed to the repository packet. No current calculator is treated as an independent measurement of the game. No price refresh or private trade source was used.

| Question | Current working contract | Evidential limit |
| --- | --- | --- |
| Physical duplicates and canonical/full-group removal | Exact implementation of the declared proxy kernel | Game selection weights still estimated. |
| At-most-one-exclusive output | Metadata-backed known constraint plus first-hand historical/current observations | Unresolved origins still refuse. |
| Multiple exclusives' count contribution | Pooled-side collapse in explicit v3 analysis | Leading hypothesis, not official quantitative law. |
| Count draw versus blocking stage | Both counts fixed before selection in v3 | Explicit candidate stage law; recounting is a different model. |
| Unequal-pool side order | Explicit alpha(A), alpha(B), no default | Sparse survival observations do not identify parameters or count-dependent ordering. |
| Ordinary selection | Approved positive carrier spawn proxy | Not a claim that hidden game weights equal these proxies. |
| Special/bench zero proxy | Unsupported unless explicit new authority is approved | Neither zero probability nor universal weight 1000 is established. |
| Bare split `1p0s+0p1s` | Refuse | Independent .59 draws and a thirds law cannot be substituted silently. |
| Dust/gold | Native quantities unknown; explicit all-in scenario quote where admitted | No inferred game price or preparation discount. |

For provenance of the earlier diary and model discussions, see [Mechanics evidence and pending-blocking proposal](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-03-recombination-solver/README.md#mechanics-evidence-and-disposition). Do not reopen broad historical research without a new premise.

### 3.2 Dated current-version delta and modifier-family disposition

**Primary evidence, accessed 2026-10-05.** GGG's 3.29.0 notes have a July 17, 2026 footer. They introduce Ducats without specifying their recombination taxonomy or selection law. The following concise official changes are provenance, not permission to refresh frozen data. [GGG 3.29.0 notes](https://www.pathofexile.com/forum/view-thread/3985332).

| Evidence date/version | Official change | Disposition under the pinned native contract; derived from source |
| --- | --- | --- |
| 2026-07-17 / 3.29.0 | Ducats alter items. | No broad Ducat origin/admission exists. Inspect actual canonical generation/domain/flags/type/groups/reach metadata. An unknown ID, unresolved origin, unrepresented output treatment, or absent weight authority refuses. The patch text does not prove any Ducat modifier is exclusive or nontransferable. |
| Same | New rare-item bench options reroll one or three modifiers. | Not an Annul/Scour/crafted-removal child. General preparation remains held pending a complete native full-output law, paid resource identity and child checker correspondence. It is not a finished-feeder quotation shortcut. |
| Same | Caster/staff affix tiers, values, weights and availability change, including special-source counterparts. | Ordinary `Natural` rows can retain existing admission only when the frozen canonical descriptor satisfies all current guards. Essence-only/unveiled/Delve/beast Aspect rows retain their specific positive-proxy boundaries. Crafted rows remain unresolved; influence rows remain refused by the pair despite inspector recognition. No new tier, recipe or weight is silently imported. |
| Same | Talismans gain special enchantments and lose default corruption. | An enchantment is not a pooled explicit filler. Existing represented carrier enchantments are preserved, but new canonical/base support and acquisition law need separate evidence. Structural solver goals do not certify an enchantment target. Corrupted/Unique or unavailable-base variants still refuse. |
| Same | Enshrouded Uniques transform into Uniques with Vestigial implicits. | Unique input/output semantics remain outside the ordinary random-pair contract. Implicits do not contribute prefix/suffix survivor counts. No ordinary-feeder classification follows from a similar stat. |
| Same | New corruption implicits appear. | The input category guard continues to refuse corrupted items; implicit metadata does not create explicit-filler admission. |
| Same | Pearlescent Amulet adds an elemental-resistance implicit. | A new base requires canonical frozen-data identity/support. If later admitted, it is an actual possible input carrier with its own properties; the solver's chosen goal base cannot substitute for it. Mixed-base optimization remains held. |
| Same | Socket defaults and colour mechanics change. | Represented sockets remain carrier properties. This does not alter explicit selection counts or establish new socket-preparation laws or hard-budget costs. |

The table's native decisions follow the [classifier and natural witness](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_constraints.cpp#L12-L31), [origin guards](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_constraints.cpp#L74-L110), [pair admission](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L68-L95), and [carrier preparation](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L235-L283). They are source-derived scope statements, not game observations or a refreshed catalogue.

**Community lead, not a measured receipt.** The review supplied an indexed date of 2026-08-12 for *New Belt Recomb Tech Is Wild*. The directly read page says “1mo ago”; its exact publication date is unresolved. Comments claim new Pantheon and belt-augment Ducat modifiers are non-native natural fillers, mention attribute conversion, and assert count contribution without transfer. They also speculate about older crafted/Aspect treatment. The linked clip was not inspected. The discussion supplies neither a configuration-complete trial ledger nor canonical IDs/weights; its simplified odds include a disputed pool count. No quoted percentage, current price or universal nontransferability is admitted here. [Community lead and comments](https://www.reddit.com/r/pathofexile/comments/1vm0rxj/new_belt_recomb_tech_is_wild/).

The community vocabulary must not overwrite native categories:

| Candidate family from the lead | Native correspondence or refusal now | Unresolved obligation |
| --- | --- | --- |
| Pantheon/Ducat “aspect” | `BeastAspect` names exactly four canonical type keys: Bird, Cat, Crab and Spider. A display word “aspect” does not match those keys. Otherwise classify by actual metadata; unresolved rows refuse. | Establish canonical type, explicit side, origin, complete groups and output treatment. Prove whether it is ordinary, known exclusive, or another category before assigning count contribution. |
| Belt-augment Ducat filler | No belt-name exception or family-wide registry. If actually `Natural`, it must satisfy `natural_on_source || guaranteed_natural_essence_source`; non-native source without that Essence witness refuses. | Separate source admission, count contribution, carrier selectability and selection weight. “Cannot transfer” is not evidence that a missing special proxy equals zero. |
| Attribute-converted Ducat modifier | A same-class natural witness on another base can make the inspector classify `Natural`, while a zero source proxy and no guaranteed Essence origin still fail pair admission. | Record both actual input bases, first-match tag weights, canonical converted ID and paid acquisition provenance. Do not forge an Essence witness or replace the source base with the goal base. |
| Claimed change to older crafted/Aspect modifiers | Current generic crafted origin remains unresolved; known beast Aspect retains exclusive classification and positive carrier-proxy requirement. | Contemporary matched evidence is needed to change these declared model assumptions. Community assertions alone establish neither a source defect nor broader supported scope. |

There are four independent predicates: contributes to requested count, eligible for selection on carrier c, conflicts with other occurrences, and has an approved selection weight. Existing ordinary source-admitted occurrences may count before becoming ineligible on another actual carrier. Existing known-exclusive zero-proxy occurrences refuse, because missing weight authority is unresolved. A new purported universally nontransferable filler does not automatically inherit either case. Its counting law is itself an unresolved premise. A category may be visible in the inspector yet refused by pair validation or later output preparation.

This closes the report's version-coverage omission without claiming a current-game-exact kernel. The official source confirms new crafting surface; it does not identify count coefficients, first-side ordering, exclusive collapse, or the lead's purported zero-selection law. O1–O6 remain conditional proofs of the adopted model. Their arithmetic is not invalidated by an unadmitted modifier family, and no implementation conclusion follows before native and empirical correspondence is established.

### 3.3 Minimal current-version evidence gate; no execution here

The first step is a small canonical evidence ledger, not a broad data refresh or a new run allowance. For each proposed family, obtain the exact versioned modifier ID and full descriptor, input/base/tag identities, obtainable origin, all groups, explicit side, slot/item flags, and represented output payload. Bind any game observation to a dated build, complete physical inputs before/after, both carrier outcomes, and a complete attempt denominator. A successful selected clip cannot estimate failure probabilities. Collecting such receipts or running fixtures belongs to a separately selected owner/window; none occurred here.

| Proposed bounded case | Control/premise | Falsification or expected current refusal |
| --- | --- | --- |
| D0 — Two physical carriers | Two approved compatible ordinary inputs A/B, one carrying a base property the goal prefers | The adopted law keeps both carrier branches at 1/2. Success on one carrier only has mass `u/2`, not `u`. Swap A/B with carrier-specific order parameters and costs. Goal-base forcing, dropping failed-base outputs or cloning a preferred carrier falsifies correspondence. Planner mixed-base continuation still refuses. |
| D1 — Pantheon versus known beast Aspect | Compare exact canonical descriptors; no display-name dispatch | Known type-key Aspect has its existing classification. A new unresolved Pantheon descriptor must refuse before sampling; an absent weight must not be replaced with 1000 or treated as proved zero. |
| D2 — Alleged count-only filler | Source-admitted ordinary control with carrier-specific zero versus the new candidate; distinct physical slots and complete groups | First establish the candidate's counting category independently. Under the ordinary control law count precedes filtering. Under a known-exclusive zero proxy the result is unavailable. Competing physical-count and exclusive-collapse hypotheses stay separate named laws; no hidden recounting. |
| D3 — Non-native attribute conversion | Natural witness on another compatible base, zero own-source proxy, no guaranteed Essence reach | Inspector recognition can succeed; current pair admission must refuse. Broadening origin proof is an implementation proposal requiring a paid obtainable source law and output authority. |
| D4 — Duplication/conflicts/order | At most two candidate occurrences added to an approved tiny pair, with complete canonical groups and recorded rolls | Compare full support, actual survival, both named order scenarios and carrier outcomes. Canonical duplicates remain physical until selection; no group-component collapse, renormalization to desirable results, or omission of positive failures. Cross-side groups and bare-split scope remain held. |
| D5 — Paid feeder preparation | One predeclared Ducat or new-bench action with full output/resource law, otherwise a labelled finished-feeder price scenario | Refuse executable checked-child export without that law. Include start item, action/station payments, every positive failure, cleanup, retries, disposal and retained inventory; all paid quantities are settled once. A scenario quote cannot certify the child or unknown costs. |

No proposed case authorizes selected-mod recombination, base-specific recipes, weakened numerical gates, renewed resource experiments, or Current/Finder admission. Current/Finder, authored Builder and WASM applicability remains exactly as in §8: a new native family/law needs separate adapter and artifact qualification. The model configuration and frozen-data hash must remain explicit even if later public data proves different weights; user authorization of roll-weight proxies does not authorize misclassifying blockers.

## 4. Mathematical kernel specification and correspondence

### Adopted survivor requests

These integer thousandths are unchanged coefficients, not exactly 1/3, 2/3, or newly measured current probabilities:

| Effective/physical n | k=0 | k=1 | k=2 | k=3 |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 1000 | 0 | 0 | 0 |
| 1 | 410 | 590 | 0 | 0 |
| 2 | 0 | 667 | 333 | 0 |
| 3 | 0 | 400 | 500 | 100 |
| 4 | 0 | 100 | 600 | 300 |
| 5 | 0 | 0 | 430 | 570 |
| 6 | 0 | 0 | 300 | 700 |

[Native count owner](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination.cpp#L143-L150).

For a selected set S, eligible candidates are unselected positive-weight occurrences conflicting with none of S. A draw of occurrence x has conditional mass `w_x / sum(w_y)` over that current pool. The probability of an ordered leaf is the count-row mass times the product of its conditional draws. An exhausted pool ends the branch with all remaining mass. Summing leaves with the same requested counts and selected physical occurrence sets produces the structural row. Materialization then preserves the chosen physical payload.

**Mass proof.** Each interior node distributes its incoming mass into children whose conditional weights sum to one. Every path selects at most three occurrences per side, so the tree is finite. Exhaustion is a leaf, not lost mass. Each requested-count row sums to one, and carrier/order mixtures are convex combinations. Thus the full declared row sums to one. This proof assumes valid input occurrence identity, complete conflict checks, positive finite integer proxies, and no omitted leaves. Floating accumulation still needs native numerical acceptance; this is not a machine-level exact rational certificate.

**Compatibility proof.** Every new selection is checked against every previously selected occurrence. Induction gives pairwise canonical/group compatibility and at most one exclusive in the final union. It does not prove a hidden category's legality; unresolved classification is rejected before enumeration. It also does not imply conflicts are an equivalence relation.

**Actual number survival.** Requested count k and actual count m are different variables. `m<=k`; exhaustion can make several k values produce the same item. Retain k for model/audit purposes and the actual full item for continuation. Reporting the count table as the final number-survival distribution would be wrong whenever duplicates, exclusions, or carrier eligibility exhaust the pool.

**Order family.** Let K(c,P), K(c,S) be the two full kernels on carrier c. For the current explicitly count-independent order scenario,

`P(alpha)=.5*[alpha(A)K(A,P)+(1-alpha(A))K(A,S)] + .5*[alpha(B)K(B,P)+(1-alpha(B))K(B,S)]`.

A one-attempt event is affine in the two alphas, so its extrema over [0,1]^2 occur at four vertices. That interval covers only this family: not alternative count tables, missing special weights, classification changes, or count-dependent side order. A count-dependent side-order hypothesis would require a different configuration and count-conditioned mixing; it cannot be absorbed into the present scalar without proof.

Fixed repeated-policy costs solve `(I-P(alpha))J=c` and are generally rational in alpha. The canonical counterexample `J0(q)=(1+q)/(1-q+q*q)` has sampled values 1,2,2 at q=0,.5,1, but `28/13>2` at q=.75. Allowing q to vary by state can create a non-goal cycle. Endpoint scenarios do not certify robust expected cost.

Preserve the existing sufficient certificate: finite nonnegative h,U, zero on goals, satisfying `h>=1+P_e h` and `U>=c+P_e U` for every allowed vertex row. Stopped-time telescoping bounds expected steps and accumulated nonnegative cost. Convexity covers the row hull. This requires conservative verified inequalities beyond current floating residual checks; it is a separate future capability, not a prerequisite for named sensitivity reports. [Canonical uncertainty/properness argument](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/engine/recombination-solver.md#pending-sensitivity-contract-no-default-first-side-probability).

The distinction between proper finite-expected-termination policies and unrestricted accumulated-cost solutions is supported by Bertsekas' primary SSP treatment. The finite equations and arguments here are derived for the bounded native graph; no infinite-state theorem is used to bypass native correspondence. [Bertsekas, Proper Policies in Infinite-State Stochastic Shortest Path Problems](https://arxiv.org/abs/1711.10129).

## 5. Small exhaustive oracles

All new witnesses below are analytical specifications. None was executed here. They are mechanics-level oracle inputs, not base-specific production recipes. Real fixture materialization must prove each input's native legality and physical acquisition separately.

### O1 — Existing count-first global blocker witness

Equal weights; distinct groups; prefix pool P+Ep and suffix pool S+Es. Let r=.667,t=.333. Both desired modifiers survive under either first-side order with

`t+(r/2)*(t+r/2) = .55527775`.

Reason: first-side count two retains its desired plus exclusive, leaving the other desired selectable under its already requested positive count; first-side count one retains desired with probability r/2, after which second-side desired retention is t+r/2. Recounting after removal changes the first term to `t*.59`, yielding `.41874775`. This difference falsifies the common recount-after-blocking shortcut.

Current main already contains this witness and the triple-prefix order witness (.015 versus .18315). Reuse them; do not claim them as a new implementation or test run. [Existing native blocking witnesses](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/tests/test_recombination_solver.cpp#L12-L51).

### O2 — Selection is sequential, not normalized products

Three compatible ordinary candidates with weights 1,2,3, conditional on requested count two. For pair {i,j}, sum both orders:

`Pr({i,j}) = w_i/W * w_j/(W-w_i) + w_j/W * w_i/(W-w_j)`.

The unordered distribution is `{1,2}:3/20`, `{1,3}:4/15`, `{2,3}:7/12`; it sums to one. Normalizing pair products instead gives `{1,2}:2/11`, which differs. This tiny oracle falsifies replacing the recursive native law with weighted subset products. With conflicts, the denominator must also remove candidates blocked by the selected occurrence.

### O3 — Full groups are not a transitive partition

Let X have groups {g1}, Y have {g1,g2}, Z have {g2}, all equal ordinary weights. X and Z can coexist, while Y conflicts with both. A legal structural arrangement is X+Z on one input and Y on the other.

For n=3 the complete unordered final distribution is:

| Final set | Mass |
| --- | ---: |
| X alone | 2/15 |
| Z alone | 2/15 |
| Y alone | 1/3 |
| X+Z | 2/5 |

Requested count one contributes 2/15 to each singleton. At requested count two or three, choosing Y first exhausts the pool; choosing X or Z first yields X+Z. In particular, requested count three can finish with one or two. A union-find/connected-component conflict compression would incorrectly make X and Z incompatible and destroy the 2/5 branch. The native predicate is pairwise against selected occurrences and is consistent with this oracle.

### O4 — Ordinary filler: coverage and clean success differ

Equal-weight desired A,B, with optional unrelated ordinary filler X, all compatible on one side. Without X, both survive with `.333`. With X, n=3:

- Coverage of A and B: `.100 + .500/3 = 4/15`.
- Clean exactly A+B: `.500/3 = 1/6`.
- A+B+X branch: `.100`, acceptable only if the terminal permits X or pays for a legal cleanup continuation.

The existing filler fixture tests coverage, not a claim that 4/15 is clean success. If a clean retry policy accepts ABX for one ordinary Annul attempt, only its 1/3 X-removal branch reaches clean AB, assuming those are the three removable affixes and no side is locked. The other 2/3 branches are paid failures with retained output. A fresh-discard policy then has success `1/6+.1/3=1/5` and unconditional extra Annul cost `.1*d`. Its cost is `(a+b+f+t+.1*d)/(.2)`, where f includes filler preparation and all base obligations. This is a derived restricted policy, not optimal cleanup or native qualification. Crafted removal cannot be substituted for ordinary X. The native full-item removal kernel assigns uniform mass over actual unlocked, non-fractured removable occurrences. [Removal owner](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/actions_basic.cpp#L1804-L1838).

### O5 — Overlapping multi-mod feeders

Equal-weight ordinary A,B,C with distinct groups, three-prefix clean target ABC, compatible fixed base/level, and no suffix complication. Feeder AB plus singleton C gives physical n=3; ABC survives only at requested three, probability .1.

Feeder AB plus feeder AC gives physical pool A1,B,A2,C, n=4. Duplicate A occurrences count twice but cannot both be selected. The complete canonical output distribution is:

| Output | Mass |
| --- | ---: |
| ABC | 3/10 |
| AB | 1/4 |
| AC | 1/4 |
| BC | 1/10 |
| A | 1/20 |
| B | 1/40 |
| C | 1/40 |

At requested two, weighted canonical first-selection probabilities are A:1/2,B:1/4,C:1/4, giving conditional pair probabilities AB:5/12,AC:5/12,BC:1/6. Multiply by .6. Requested three always selects all three canonical families; requested one contributes the singleton row. Sum is one. If the two A occurrences have different rolls, the canonical table must be refined into physical payload outcomes for the native state graph; it is not permission to merge them.

Unlike a two-mod toy where AB is already terminal on one feeder, neither AB nor AC is terminal for ABC. This is a genuine nonterminal feeder-composition witness. It establishes increased conditional structural retention under these premises, not a universal multi-mod economic advantage.

### O6 — Count-before-carrier-filter and real base requirements

Physical ordinary P,X with P selectable and X carrier-ineligible yields P with probability one: n=2 requests one/two; both exhaust to P. Filtering X before count would incorrectly yield .59. Conversely two physical copies of one canonical modifier produce actual one with probability one despite requested two mass .333; their weights choose which recorded occurrence survives.

For two different input bases, a target-base requirement applies to the full output event. If success is possible only on carrier A with conditional probability u, one-attempt target success is `.5*u`, not u. The wrong-base half remains a failure/continuation. A same-base substitute may cost more but avoids this factor, changes weights, and changes future acquisition legality. Do not “fix the base” by selecting it after the random result.

### Independent reference algorithm for later finite qualification

Use a small rational reference that enumerates ordered physical selections without importing the production helper:

1. Validate input occurrence/slot IDs and native structural legality separately.
2. Construct each candidate carrier's full canonical groups and explicit weights.
3. Freeze both requested-count rows, then recurse in each first-side order.
4. At each step enumerate all still-positive, pairwise-compatible physical candidates, multiply exact rational conditional mass, and stop on count or exhaustion.
5. Aggregate only at leaves, preserving requested counts and physical payload selections.
6. Materialize through native sessions and compare full probability support, items, rolls, flags, counts, and goal union—not just a success scalar.

Enumerate small pools of 0–3 candidates per side with weights {1,2,3}, duplicates, known exclusives, and the non-transitive group pattern. Keep pair-provider refusals as separate acceptance cases. O2/O3/O5 are the smallest new discriminating witnesses; existing O1/O6 checks are reused. An oracle requiring a production call to compute its expected probabilities would not be independent.

## 6. Paid economics and inventory decisions

### One-step Bellman comparison

For complete full-item inventory state I and action a:

`Q(I,a)=c(I,a)+sum_o P(o|I,a)*V(I_after(o))`.

Every output—including unwanted groups, exclusives, wrong base, low rolls, or singleton failure—has a continuation or a losing classification. Purchase, finish by crafting, feeder preparation, recombine, discard, and eventual recovery belong in the same declared catalogue/controller comparison. A quote for the successful feeder alone is insufficient if its acquisition misses/retries/cleanup are omitted.

A filler is economically useful only when the full Q difference, including preparation, station-cost change, cleanup and failure inventory, is favorable. The official/tester discussion about cheap opposite-side mods reducing dust is qualitative evidence to investigate; no state-dependent dust formula or discount is present in the current native quantity model. The planner currently takes one all-in attempt scenario price. It cannot certify a feeder-dependent dust saving merely because a user assigns one constant t.

### Exact asymmetric two-mod recycling formula

Consider the existing two ordinary disjoint single-mod feeder witness, desired clean AB, equal carrier properties, adopted p=.333. Let acquisition prices be a,b and attempt cost t. Failure masses are

`u=.667*wA/(wA+wB)` for A-only, `v=.667*wB/(wA+wB)` for B-only; u+v=1-p.

Retain the output and buy its missing complement. With H the continuation from immediately before an attempt's paid operation,

`VA=b+H`, `VB=a+H`, `H=t+u*VA+v*VB`.

Therefore

`J_empty=a+b+(t+u*b+v*a)/p`.

Counters are attempts `1/p`, A acquisitions `1+v/p`, B acquisitions `1+u/p`, and zero discarded failures. For a=b=1,t=1 this becomes `1+2/.333 = 7.006006...`, versus fresh-pair restart `3/.333 = 9.009009...`. These equations explain the retained Ring fixture, without rerunning it.

The new asymmetric formula shows why weighted survival matters economically even when both-mod probability p is unchanged: retaining expensive A saves the A reacquisition price, whereas retaining cheap B obliges another expensive A. If acquiring a finished AB has complete price F, compare F with J_empty. If initial A is already owned, distinguish sunk incremental cost `VA` from full original-entry cost `a+VA`; do not reprice owned inventory as free in the same-root comparison.

[Existing Ring/recycling/filler assertions](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/tests/test_recombination_solver.cpp#L330-L412).

### Multi-mod feeder price threshold under a deliberately restricted policy

Use O5. Let complete prices x=AB,y=AC,z=C and constant attempt price t. A fresh-discard/restart policy costs

`J_single=(x+z+t)/.1`; `J_overlap=(x+y+t)/.3`.

The overlapping-feeder policy is cheaper exactly when

`y < 2*x+3*z+2*t`.

This conditional threshold is useful for catalogue ordering and a finite acceptance witness. It is not the threshold of the optimal recycling policy. O5's AB/AC/BC failures carry real value: their continuation may buy an appropriate complementary feeder instead of discarding. The singleton outputs require their own paid acquisition/composition plan. Compare the full two-slot policy, plus direct finished ABC acquisition, before recommending a method.

### Why a cheapest finished-feeder quote is not enough

For a checked feeder f returning full item X with expected complete reward c_f, expected-risk-neutral composition can use

`Q_f(I)=c_f+E[V(I union {X})]`

when the child has finite expected termination, all positive outputs are accepted, the parent retains at most one item, and all child's costs are additive and paid. Cost/output correlation need not be retained for that unrestricted expectation: linearity suffices. It *is* needed for hard remaining-cost/action caps, nonlinear utility, conditional affordability, or output-dependent continuation permissions. A mean quote is not a joint law of cost, actions, and output. No hard-budget optimization claim follows from the existing macro.

The current checker fixes child document, revision, output contract, paid start, frozen economy and native session. Its graph includes full-item removal outcomes, route defaults, all positive terminal outputs, properness, and absorption. It refuses nested supplies, random initial unveil offers, failed/no-match terminals, and operation families beyond Annul/Scour/crafted removal. Child execution pays actual start/actions; expected planner quote must not be charged again.

Growing feeders with Alteration, Essence, Exalt, bench setup or metamods therefore require additional full-item action-law authority in this owner. A Calculator goal probability alone does not supply the required output distribution. Start with one existing native law that can materialize all represented outcomes within the unchanged caps; refuse if such a law is unavailable. Do not duplicate mechanics in TypeScript or introduce a broad new crafting solver.

[Checked feeder implementation and allowed operations](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L192-L298) · [Acquisition provenance](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L351-L373).

### Base choice and inventory extension

The immediately executable optimization remains one fixed base/level. Compare several individually pinned candidate base/level requests if needed, with their own complete feeder prices and the same terminal definition. Those are alternative procurement plans, not one optimizer silently switching base mid-policy.

A future mixed-base planner must intern `(data/session identity, base, level, full item)` per specification; every output retains its own observation session. Goals must explicitly say whether base matters. Inventory sorting is safe only if swapping A/B preserves costs, feeder/port semantics and the carrier-specific scenario under the swap. Swapping inputs without swapping alpha(A)/alpha(B) can change the kernel. Do not reuse the current symmetric multiset representation for an asymmetric provider without a correspondence proof.

Recycling is a controller over inventory and observed full output. It must not cache a consumed saved-feeder result as a reusable resource. Equal specs may share immutable representation but must still be two distinct live physical instances. No free replacement, implicit third slot, invented market salvage, or conditional branch deletion is allowed.

## 7. Native policy authority and resource costs

The finite inventory graph contains acquire, discard and recombine actions, with complete positive output discovery. Terminal success uses the shared structural observer. The greatest almost-sure reachability set retains only actions whose every positive successor stays in the candidate winning set. A rank-based seed chooses an action with a positive edge to a lower rank. Any nonterminal recurrent class would have a minimum-rank member whose selected positive edge leaves the class, a contradiction. The finite policy is proper.

For its transient nonterminal P, `(I-P)X=R` evaluates costs and all resource counters through the same factorization. Complete graph, original entry, terminal scope, nonnegative finite prices, and numerical acceptance remain premises. A zero-cost discard/acquire loop or an unrelated terminal root is not a reason to skip properness. A catalogue-restricted evaluated policy is a feasible conditional upper; it is not a lower or global crafting optimum. [Native winning/seed/properness](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L76-L140) · [CLM-0001/0002 and premises](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/claims.md#clm-0002--a-proper-evaluated-fixed-policy-supplies-an-entry-scoped-upper) · [Canonical fixed-policy argument](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/mathematics/policies.md#1-fixed-policy-evaluation).

Keep the existing numerical gates: pair/child/bridge row mass tolerance 1e-10; pivot magnitude greater than 1e-13; expectation at least -1e-8 before nonnegative clamp; residual at most `1e-8*max(1,value)`; strict improvement exceeding `1e-10*max(1,best)`. Policy-iteration limit does not imply convergence. These are current floating estimated-policy acceptance rules, not rigorous robust certificates.

### Costs of the proposed work

| Owner | Current bound or derived complexity | Consequence |
| --- | --- | --- |
| Pair enumeration | At most six physical occurrences/side, at most three selections/side; sum of ordered lengths 0–3 is 157 for a six-candidate side | Conservative maximum 157² ordered leaves per carrier/order, or 98,596 across two carriers/two orders, before conflict pruning. This is an analytical bound, not measured runtime. |
| Occurrence subsets/full outputs | Up to 42 occurrence subsets per side for sizes 0–3 before conflicts/count support | Full identity and rolls can create many outcomes; a desired-mask quotient would falsely hide them. |
| Inventory graph | Catalogue <=32; default item cap 64, absolute item cap128; states<=256; two physical slots | At 64 discovered specs, all unordered inventory sizes 0–2 would number 2145; the state cap can refuse a complete closure. Do not promise arbitrary ladders fit. |
| Dense policy evaluation | N<=256, R=acquisitions+4; O(N³+N²R) work and O(N(N+R)) doubles | Raw augmented matrix at N=256,R=36 is about 598,016 bytes, excluding vectors, full items, graphs, sessions, bridge copies and allocator overhead. Not a whole-process peak. |
| Checked child | document<=256 KiB; nodes/states<=256; current removal laws | Acquisition generation can expand the full-item graph; same work/discovery caps still apply. |
| Work and iteration | default20,000,000 work; maximum500,000,000; default32 iterations, maximum64 | No cap increase proposed. Refusal is retained evidence, not an invitation to truncate outputs. |
| Browser planner | fresh owned worker, frozen request/bundle copy; wall limit<=180,000ms | Existing real cancellation/termination and stale-identity checks should be reused; worker copies contribute to resource accounting. |

The leaf bound counts ordered histories, not unique items or observed work. Native pair enumeration has bounded internal recursion; parent work ticks/cancellation are not a claim that each recursive arithmetic step is independently budgeted. If a new law makes that local recursion expensive, instrument the existing owner, not an external polling service. Mixed-base sessions or hard-budget child expansion can greatly increase state identity and should not enter the first milestone.

[Caps/evaluation owner](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L143-L189) · [Request caps](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L300-L320) · [Request-owned worker](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/apps/web/src/app/recombination-planner.ts#L29-L104).

## 8. Builder integration and per-consumer applicability

Current released Builder already has typed A/B item supplies, one physical output, paid acquisitions/checked-feeder invocation, actual input/output identity, inventory traces, and an independent restricted export checker. The exporter names physical slots, sets `start_item_present=false`, records initial paid/sunk input treatment, and uses `use_declared_inputs=true` so a control-carried output cannot override the retained slot binding. The checker reconstructs compiled native transitions and compares cost, attempts, acquisitions, discards and child actions.

General authored exact evaluation still rejects inventory/recombine/full-item routing because its single-item domain cannot certify inventory/control identity. Keep that guard. Expanded feeder contracts should be embedded into the existing restricted language and validated there. No concurrent dependency scheduler or frontend mechanics interpreter is needed.

| Consumer | Current authority | Proposed scope and necessary evidence |
| --- | --- | --- |
| Current | No random recombination action/proof activation | Remains unchanged. Any later admission needs original-root inventory semantics, complete action scope and independent policy checking; no lower/closure transfer. |
| Finder | No automatic recombination producer activation | Future proposal only after compatible checked resource controller handoff. Feeder heuristics may rank candidates, not merge states or certify omitted actions. |
| Authored Builder | v1/v2 pair execution and restricted checked export; general exact resource evaluation refused | New single-item full-output feeder law plus complete paid runtime contract. Failed-child recovery/nested inventory separate. |
| Calculator | Native read-only full carrier structural law and goal union | Extend display metadata only; no approximate frontend law, rolled-total claim, or advanced Apply. |
| Native bounded planner | Fixed base/level; purchases, declared quotes, narrow checked children; v3 scenario analysis | Add finite preparation catalogue within caps; retain incomplete-cost/nonconvergence statuses and unsupported categories. |
| WASM/worker | Released matching artifacts qualified in prior scope | Any new source/action ABI needs fresh matching build and focused adapters after approval; desktop source reads do not qualify browser changes. |

[Export/check implementation](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/recombination_solver.cpp#L524-L743) · [General authored guard](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/solver_eval_helpers.hpp#L1143-L1152) · [Native A/B and attempt execution](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/engine/src/simulator.cpp#L2549-L2715).

Acceptance cases must cover fresh pair versus recycled output, A/B swap, equal specs/distinct identities, wrong-base output, no-match positive branch, consumed-result revisit, revision/content change, cancellation before A/between A+B/during child/before atomic pair commit, Redo replay, and actual costs paid once. Cancellation preserves settled costs and acquired resources; it cannot convert a partially paid operation into a free attempt. One typed input edge cannot fan out into cloned physical items.

### Retained qualification and negatives

Main's current status records the released recombination/Builder integration at `81d4f47106993d7c36af502595fcba24fa0b7e51`, with exact-source Windows run37206763122, solver-knowledge run37206763077, matching WASM50c98f55/MJS8ec20cf7, all frozen inputs unchanged, and the b04 packaged/rendered A/B flow. These retained receipts establish their named functional scope, not new feeder economics or full robust inventory evaluation. [Released status](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/solver/current-status.md#combined-recombinationbuilder-product-qualification-2026-10-04) · [Product composition receipt](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-04-sol61-product-integration/qualification-composition.json).

The recombination programme's focused matching-worker record retains the original Ring expectations and 1000 successful compiled-worker trials. It also retains earlier constructor ownership, stale fixture, Python refusal-expectation, and lifecycle failures. This report reuses those records without touching their raw traces or claiming a new run. [Focused receipt summary](https://github.com/OliverOrton/poecraft2/blob/7252027c80856628ed16734583bfc9d6e166458b/docs/active/2026-10-03-recombination-solver/README.md#focused-native-and-matching-wasm-qualification-passed-2026-10-04).

Preserve all mechanic negatives: withdrawn q=.10; no universal1000 special weight; ordinary filler counterexample; number-request versus actual survival; unresolved bench classes/output flags; inaccessible manuscript assets; calculator coefficient disagreement; bare-split ambiguity; endpoint-cost counterexample; missing gold/dust; fixed-base optimizer limitation; and discovery-cap refusals. Tests passing the declared proxy model cannot resolve hidden game uncertainty.

## 9. Bounded implementation milestones and falsification

This is a proposed next programme, not implementation authorization. Use the existing native owners and living recombination record. **New timed solver/Simulator/game experiments proposed: zero. Local builds/tests performed by this research task: zero.** A later implementation owner must obtain its normal execution window and preserve existing budgets; this lane grants no replacement allowance.

| Milestone | Concrete bounded deliverable | Gate and stop rule |
| --- | --- | --- |
| R0 — Reconcile the current contract | Correct stale present-tense unbuilt/ABI comments and integrate dated §3.2–3.3 version evidence through coordinated canonical docs; retain historical sections | No model/source/data change; distinguish released bridge, unverified new families, held general evaluator and advanced Apply. |
| R1 — Independent finite oracle | Add O2/O3/O5 expected rows to existing native test owners, plus independent small rational reference | Complete support/full payload comparison, unchanged numerical gates. Stop on any positive branch lost or invalid native fixture; no timed run substitute. |
| R2 — Bounded preparation catalogue | Represent no filler, admitted ordinary filler, duplicate-overlap feeder, and finished-item alternative with complete quote/certificate provenance | Same goal/root/prices/model; no name/base dispatch; unknown weights/classes refuse. Compare full Q, not only success chance. |
| R3 — One growing feeder law | Extend `check_recomb_feeder` with one existing complete native full-item law selected after source audit | All positive outputs, prices, paid start/setup/cleanup/retries, properness and immutable child identity; preserve 256-state/20m-default work bounds. If only a terminal projection exists, return unavailable. |
| R4 — Restricted export/runtime | Extend the existing compiled language/checker only for the new R3 law | Fixed-policy expected cost and all counters agree; actual execution pays once; child failure and nested supplies still refuse. v3 stays analysis-only. |
| R5 — Consumer qualification | Matching native/Python/WASM/worker adapters and focused Builder cases after a selected execution programme | Exact source/artifact/ABI identity, cancellation/stale result/physical multiplicity checks. Current/Finder and robust authority stay unchanged. |

**Minimal falsification experiment to select implementation:** O5's AB+C versus AB+AC full rows on a tiny native-valid, nonterminal three-prefix target, with caller-declared complete prices x,y,z,t and a finished ABC competitor. First establish both exact full pair laws and all seven overlapping outputs. Then evaluate predeclared fresh-discard policies and the restricted adaptive recycling policy through the existing fixed-policy/check-export owners. This is finite qualification, not a benchmark or a Simulator request. A single row mismatch, missing canonical roll branch, invalid acquired item, incomplete cost, or unavailable full-law child stops the dependent milestone. Existing Ring/O1 receipts supply controls; do not rerun them just for continuity.

R2 can proceed with declared finished-feeder price scenarios while R3 waits for a law. It must label those results as caller-declared economics and refuse an executable saved-feeder export. A recommendation reversing across side-order/weight scenarios is “model-sensitive,” with the same fixed policy evaluated in each scenario. Separately optimized policies must retain separate identities; their minima are not one robust executable controller.

Each future receipt binds full source/executable/WASM hashes, frozen runtime/string/economy identity, actual full inputs and ownership, goal/base/level/tier predicates, scope/model/configuration, taxonomy and weights, explicit order scenario, complete station/acquisition/child prices, child bytes+revision, all positive laws, policy/compiled export/checker identity, caps/work/numerical rules and failure disposition. A similarly named branch or successful old build is not a qualification source.

## 10. Exact canonical integration map

No canonical edits were made. Integrate the following through the parent coordinator after reconciling other lanes; retain this full report once in research-inputs or a durable research path and link it.

| Canonical owner and existing heading | Proposed insertion/correction | Supporting report section |
| --- | --- | --- |
| `docs/engine/recombination.md` — “Structural law and selection” | Explicitly distinguish requested and actual surviving counts; reference pairwise non-transitive full groups and O2/O3. Retain v1's declared scope and link v3 separately. | §§2,4,5 |
| `docs/engine/recombination-solver.md` — “Pending sensitivity contract: no default first-side probability” | Update stale present-tense “unbuilt/unqualified” language to observed v3 analysis implementation/retained qualification; retain historical claim limits, no default, no robust certificate/advanced Apply. | §§1,2,4,8 |
| Same file — “Checked single-item feeder and Builder bridge” | State exact allowed child operation families (Annul/Scour/crafted removal); add expectation-versus-hard-budget premise and one-law extension gate. | §§6,9 |
| Same file — “Exact specifications and acquisition boundary” | Add asymmetric recycling equation, multi-mod conditional price threshold, and fixed-base/mixed-base scope distinction. | §§5,6 |
| `docs/solver/mathematics/policies.md` — “Fixed-policy evaluation” | Add a brief resource-macro composition corollary: full output law plus mean additive reward suffices only for proper unrestricted risk-neutral expectation; hard caps need extra state/joint law. | §6 |
| `docs/solver/claims.md` — CLM-0002 history/correspondence | Link finite inventory application and premises without changing general accepted theorem or claiming new native qualification; add O3/O4/O5 as bounded counterexample references if useful. | §§5,7 |
| `docs/product/strategies.md` — existing resource/feeder/recombination sections | Clarify quoted versus checked feeder provenance, cost settled once, actual output retention, typed A/B ownership and held failed-child/nested inventory semantics. | §§6,8 |
| `docs/solver/current-status.md` — “Combined recombination/Builder product qualification” | Compact gap update only after accepted work: general growing-feeder laws, state-dependent dust, mixed-base optimization, robust cost and advanced Apply remain distinct held capabilities. | §§6,8,9 |
| `docs/active/2026-10-03-recombination-solver/README.md` — living current disposition | Import this original once; mark earlier Pro joint-kernel/bridge milestones incorporated, identify new finite-oracle/feeder-law proposals and zero new runs. Preserve failures and historical source labels. | §§1,5,9 |
| Same file — “Mechanics evidence and disposition”; `docs/engine/recombination-solver.md` — existing constraints/uncertainty sections | Add dated official 3.29 evidence and unqualified community lead; record family-by-family admission/refusal and D0–D5 evidence gates. Preserve physical carrier law, frozen data, and absent special-weight authority. No hidden assumption upgrade. | §§3.2–3.3 |
| `docs/solver/research.md` — section4 intake disposition | Short disposition pointer; no duplicate theorem, programme database or evidence ledger. | §§1,10 |
| `HANDOFF.md` — qualification gaps | One pointer and selected-next-step gate if Oliver later selects this programme; do not silently replace the causal solver work or grant LOCAL. | §9 |

Doc-ready native-contract paragraph:

> The survivor table specifies requested selections, not the final affix count. Duplicate canonical occurrences, full-group conflicts, global exclusive blocking, and carrier eligibility may exhaust the pool earlier. Physical occurrences remain separate until selection, including their recorded rolls. The conflict relation is pairwise and need not be transitive; collapsing its connected components or normalizing products of candidate weights changes the declared sequential kernel.

Doc-ready feeder-contract paragraph:

> Checked single-item feeder acquisition currently admits full-item Annul, Scour, and crafted removal only. A general crafting probability or a completed-feeder quote is not a checked output law. For a proper child under additive risk-neutral expectation, complete full outputs and expected paid rewards compose with the parent continuation. Hard cost/action limits require the relevant joint reward/output law or expanded child states. Execution charges actual child costs once; the expected acquisition quote is not an additional fee.

Doc-ready scope paragraph:

> Builder pair execution can retain mixed-base carrier sessions, while the bounded inventory planner pins one base and level and refuses continuations outside that scope. A future mixed-base planner needs session/base/level-bearing specifications, explicit base goals, and A/B-swap correspondence for scenario probabilities and resource costs. Random carrier selection is not replaced by a chosen target base.

Doc-ready current-version evidence paragraph:

> The dated 3.29 evidence does not qualify new Ducat/Pantheon modifier families for random recombination. Count contribution, carrier eligibility, conflicts and approved weights require separate canonical evidence. Generic crafted and unresolved origins remain refused; same-class natural recognition alone does not bypass the non-native source guard. A community filler lead is a hypothesis, not measured odds or proof of a kernel defect. Preserve both actual carrier branches and every positive failure. Any new preparation action needs its complete paid full-output law before checked feeder export or consumer qualification.

## 11. Delivery and remaining decisions

The task-local full report is the sole proposed branch artifact. It contains repository/public-source links, math and planning; no private local paths, secrets, unrelated personal data, bulk traces, or archived contents belong in the publication. Main is read and verified, never written. The branch and Library identity are returned separately after confirmed publication and saving so successful writes are not duplicated.

The required substantive owner decisions are conditional, not a request to repeat prior approvals:

- If zero-proxy special/bench modifiers are needed, select a reviewed canonical registry and explicit weight scenarios/output-flag treatment; approval of positive spawn proxies does not settle them.
- If current-version Ducat/Pantheon support is selected, resolve the canonical evidence ledger and D0–D5 gates first; the community lead does not establish an executable family or current-game odds.
- If advanced v3 random Apply/export is desired, select an explicit versioned point order law; sensitivity analysis is not a sampling distribution.
- If broad feeder generation or mixed-base optimization is selected, authorize its bounded native correspondence programme and execution window separately.

The immediate useful next step is R1 plus a declared-price O5 catalogue proposal, while preserving the existing ordinary v1/v2 execution path. New game-exact mechanics, robust optimization, general exact inventory evaluation, automatic Current/Finder integration, and broad growing-feeder certification remain unestablished.

This session performed targeted source/document/primary-web/Library reads, analytical derivation, and report generation only. No tests or numerical solver/oracle scripts were run. All retained measurements retain their original source/input/cost identities. The restricted gaming windows remain protected; this task grants no future scheduled work.
