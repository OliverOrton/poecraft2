# Random recombination solver and blocking programme

**Research snapshot: UNBUILT / UNQUALIFIED.** Branch-only publication for Pro
research is authorized; the integrator owns the push. Native source remains
`0211d7a8604ac064c80d40ffd20beb915ef2297f`. Multiple-exclusive blocking and
advanced joint-law Apply remain held; the uncertainty contract is a proposal.

Selected by Oliver on October 3 2026 at 16:45 UTC. Sequential execution, no subagents.
Fresh worktree `poecraft2-recombs-solver`, branch `dot/recombs-solver-20261003`.
Available/local/cached/remote main were all
`72448deaacca2797058de0559378685ba71a983f`; normal checkout and prior recombination
worktree were clean (scoped checks exclude protected root `0`). Neither is edited.

## Question and bounded stages

Optimize declared acquisition/crafting versus random recombination while retaining
physical fillers, duplicate occurrences, complete group conflicts and recycled
outputs. Builder owns live item ports, feeder execution, inventory limits and
history. Native recombination owns pair laws and two-consumed/one-created receipts.

1. Verify current primary mechanics evidence and document unresolved categories.
2. Implement a native bounded exact-item/two-slot inventory policy search. Exact
   specifications include filler/blocker identities and represented carrier fields.
   Fully priced exact-spec acquisitions can represent purchases or completed
   crafting feeders, including multimod final products. All acquisition/retry/base/
   cleanup costs belong in those quotes. Recombination attempts require a separately
   complete declared cost. Recycled outputs remain real inventory, never salvage
   credits or free replacement inputs.
3. Focused changed-layer qualification after explicit parent LOCAL slot grant;
   local commits only, integrator publishes. No experiments or benchmarks allowed.

Current and Finder single-item solver admission is unchanged. This is a dedicated
pair/inventory model, with no general optimality or game-exact claims. Search and
numerical caps must report failure/capping instead of dropping positive outcomes.
Initial scope pins one selected base and item level. Gold/dust remain unknown
unless the caller explicitly provides a complete economic model.

## Mechanics evidence and disposition

- [GGG 3.26 patch notes](https://www.pathofexile.com/forum/view-thread/3787013),
  June 12 update: non-normally-rollable input modifiers reduce average retained
  counts. Incorporate as a guard against importing old exclusive padding laws.
  This official statement does not specify a quantitative probability kernel.
- [Butsicles original 3.26 tests](https://www.reddit.com/r/pathofexile/comments/1ldc3lz/326_recomb_psas_a_few_useful_tipsconfirmations/):
  exclusives still mutually exclude; preliminary tests suggest multiple exclusive
  occurrences count as one on a side. Side order is not verified 50/50 for unequal
  exclusive/crafted pools. Open, not a complete quantitative law.
- [Original 3.26 analysis](https://www.reddit.com/r/pathofexile/comments/1lfyxxd/326_recombinators_analysisguide/)
  and [author's guide](https://codeberg.org/poe_notes/poe_notes/src/branch/main/Recombinators.md):
  assumes old count coefficients; explicitly excludes gold/dust economics and NNN
  transfer. Incorporate the importance of recycling all intermediate configurations;
  do not import its unweighted charts as probabilities of our weighted model.
- [Butsicles follow-up](https://old.reddit.com/r/pathofexile/comments/1lfyxxd/326_recombinators_analysisguide/mytikd8/):
  opposite-side exclusive padding can help the exceptional first 1p/1s construction,
  but weights and unequal side ordering remain caveats. Open model decision.
- [3.25 original guide](https://www.reddit.com/r/pathofexile/comments/1exyavx/325_updated_guide_to_recombinators/):
  historical duplicate/group/NNN foundation and non-exhaustive exclusive taxonomy.
  Its general multimod padding strategy is superseded by the above current change.
- [Recent 3.29 first-hand report](https://www.reddit.com/r/pathofexile/comments/1vilzr8/poe_329_recombinator_rules_changed/):
  conflicting explanations and observed crafted/NNN behavior; anecdotal, not a
  replacement probability law.

Oliver permits roll/spawn weights as a selection approximation, not invented
special weights or a claim that all selection rules are perfect. Preserve the v1
count coefficients and tier/roll/no-upgrade approximation until another explicit
model decision. Multiple-exclusive/fracture/exceptional joint laws stay gated; bounded v2 is described below.
Ordinary filler and full group-blocking probabilities are computed from complete
native outcomes, not a desired-only projection.

## Acceptance and gates

Finite native witnesses: physical duplicate mass and second/full-group conflicts;
fillers can improve counts or hurt weighted selection; complete output mass;
input carrier mappings; non-goal failure recycling versus buying fresh;
complete acquisition/attempt costs; wrong base/session/model/prices and caps refuse;
proper policy evaluation and no zero-cost nonterminating policy acceptance.

LOCAL is requested, not granted. No build, test, WASM or benchmark has run in this
new programme. Shared native API contract is documented here before touching any
Builder-owned adapter. Final receipt must retain exact source/artifact identities,
completed/held capabilities, unrun/failed checks and next gate.

## Source checkpoint (unqualified)

Native inventory search and versioned C API are implemented as source, with no
new build or test evidence. Full represented item keys retain fillers and groups;
two-slot graph discovery includes acquisitions, recombination and discards.
Proper-policy construction/checking precedes pivoted fixed-policy evaluation.
Costs and expected acquisition/attempt/discard counters share the same matrix.
Native item/state/work caps and cancellation return explicit failures.

Ordinary normal/magic source admission is broadened for the documented alt-built
feeder pathway; output remains rare and input rarity capacity is checked through
the existing native item helper. The v1 estimated law is unchanged for previously
supported rare pairs. No inherited rare-only qualification is attributed to this
new source capability.

Focused test sources cover physical duplicate count advantage (.6 versus .333),
irrelevant filler dilution, secondary-group exclusion, recycled failures (analytic
cost 1+2/.333 versus fresh-input cost 3/.333), completed-feeder price comparison,
unknown cost refusal, capacity/work/cancellation, numerical zero-cost properness,
C API model identity/lifetime/buffer and input preservation. THEY HAVE NOT RUN.

Builder boundary: result inventory/spec IDs are semantic identifiers. Two equal
spec IDs remain two distinct physical items at execution. Reached acquisition
rows invoke the designated paid fresh feeder/purchase. Recombine rows require
actual A/B resources and use the existing atomic pair Apply/receipt with its own
output session; Undo/Redo replays that stored receipt. Solver does not invoke
feeders or mutate Builder flow. Quotes include all acquisition/crafting retry/
cleanup costs and complete declared gold/dust/attempt cost. Quote provenance is
reported as a caller declaration, never promoted to native feeder certification.

No native/DLL/header/Python/WASM/web/TypeScript checks or experiments have run.
The edited-source whitespace check is the only intended source-only check.
Heavy LOCAL grant and the multiple-exclusive law disposition remain pending; no parent slot
has been used, no compiled artifact replaced, no main merge/push/deployment.

## Category and selection boundary for Builder

| Native input or selection case | Current authority and disposition |
| --- | --- |
| Ordinary explicit, canonical flags zero, special/metamod absent, influence absent, positive source spawn weight | Supported by declared v1 proxy kernel, with complete canonical groups. |
| Normal/magic ordinary item | Source now admits native rarity capacity; output remains rare. New qualification pending. |
| Same canonical mod twice, or shared full group on one side | Physical occurrences enter count/weight pool; selecting either removes all conflicting candidates. Adopted model, not newly verified hidden game law. |
| NNN natural tier | Positive-source transfer remains v1; native guaranteed Essence non-native sources enter bounded v2. Carrier filtering follows physical counting. Solver pins one base/level. |
| Prefix/suffix canonical group overlap | Refused: current quantitative side-order law has not been established. |
| Known exclusive origin | Native classifier identifies supported metadata origins. One physical Essence-only/unveiled/Delve/beast Aspect occurrence with positive proxies on both carriers enters v2. Multiple occurrences, zero special proxies and other output categories stay held. |
| Incursion/Breach | Frozen compiled metadata lacks an authoritative origin registry. Unresolved; no name/zero-weight guess. |
| Bench crafted ordinary versus exclusive | Do not classify every crafted flag as exclusive. Current generic crafted names cannot replace the historical named/unnamed distinction. No verified canonical exclusivity registry exists here. Native v1 refuses these inputs. |
| Metamods/elevated/veil templates | Classified exclusive from canonical metadata/relations. Pair output treatment stays held; finished-feeder acquisitions may still be compared. |
| Fracture | Historical guide calls it non-exclusive, but retained fracture/output flag mechanics are outside this qualified native model. Refused. |
| Bare 1p0s + 0p1s | Original research identifies an exceptional joint law; new weighted/current full coefficients are not established. Refused, not silently treated as independent .59 draws. |

Frozen manifest special-kind codes are base_implicit, corrupted_implicit, delve,
eldritch_implicit, unveiled and veiled_template. These origin fields support the bounded
native classifier, not a full current probability law. Incursion/Breach/crafted
ambiguities need an authoritative source mapping. Selection classification must
not be inferred from display names or from a zero spawn weight.

The native tri-state classifier and bounded one-exclusive/natural-Essence extension
are now source-complete below. The remaining blocking-optimization gap is the current
multiple-exclusive joint count/side-selection law and zero-proxy special weights.
Ordinary desired-independent fillers/full groups and the explicitly selected bounded
v2 subset are represented by the solver. Multiple-exclusive optimization is still held.

## Current evidence and classifier checkpoint (18:30 UTC)

[The recent calculator creator](https://www.reddit.com/r/pathofexile/comments/1vwuddy/nnn_recombinator_calculator/)
and their [published calculator](https://recomb.rngissue.org/) are original current
implementation evidence, not game authority. Read-only copies of the published HTML
and `/assets/index-BhWogcci.js` are preserved in the execution Temp directory as
`recomb-rngissue.html`/`recomb-rngissue.js`; the 3.26 author's original guide is
`recomb-current-primary.md`. The current calculator:

- Handles non-native natural modifiers relative to either carrier, including both.
- Explicitly refuses most pools with more than one exclusive physical occurrence;
  its symmetric opposite-side special case supplies a weight-dependent bound, not
  a complete joint numerical law. Therefore it does not resolve general blocking.
- Uses count rows different from Oliver's adopted 3/4/6-occurrence rows. No silent
  replacement has been made. The approved thousandths remain the model authority.
- Excludes cross-side groups; its author specifically cites a Delve/Bone Ring minion
  group counterexample. Our full canonical group guard preserves that exclusion.

These findings agree with the official 3.26 reduction note without deriving a full
current kernel from it. The provisional multiple-exclusive-collapse observation
and a 50/50 side-order assumption are separate decisions: roll-weight approval
alone authorizes neither. No new game experiment has been run or proposed.

Native metadata constraints are now implemented independently of those probabilities.
Known exclusive origins: essence_only, metamod, delve, unveiled, veiled_template,
canonical influence-elevation output relationships, and beast Aspect semantic types
`GrantsBirdAspect`, `GrantsCatAspect`, `GrantsCrabAspect`, `GrantsSpiderAspect`.
Non-elevated influenced modifiers are classified non-exclusive but their pair/output
semantics remain held. Native ordinary positive base eligibility, or a positive ordered
spawn-row witness on an actual compatible ordinary base, establishes the natural class.
Names, arbitrary positive selector rows and zero spawn weights are not category guesses.
Generic crafted rows and missing Incursion/Breach origins remain unresolved.

`pc_recombination_constraints_json` version 1 accepts actual resource snapshots and
reports full groups, canonical origin/exclusivity, physical counts, known output
conflicts, each carrier base/level and native spawn proxies. A positive special proxy
is explicitly separate from ordinary natural eligibility. Effective counts, count
model and side order are null; probability_law_complete is false. The inspector can
explain an unsupported pair without preparing a probabilistic pair or drawing RNG.

Bounded source extension uses a separate model ID:
`poe1-random-spawn-proxy-native-constraints-preserve-tier-roll-no-upgrade-v2`.
It admits one known exclusive occurrence only when both carrier selection proxies
are positive, and normal non-exclusive modifiers sourced through native guaranteed
Essence links even if non-native to their input base. These preserve the already
adopted physical-count-before-filter law. There is no opposite exclusive to block,
and cross-side full-group overlap still refuses, so no new side-order assumption is
introduced. Known exclusive zero proxies remain missing weights, not filtered as
impossible. Multiple exclusive physical occurrences refuse. Metamod, elevated,
veil template, generic crafted, fracture and generic-influence output semantics stay
held despite classifier facts. Recorded rolls and canonical tiers are preserved;
unknown upgrades remain omitted under the approved approximation.

Calculator/Apply report the actual v1/v2 model. Solver requests pin the model and
exclude v2 pairs from v1 requests; no silent scope promotion. Bounded v2 outcomes
feed the same full-item inventory graph, acquisition comparisons and recycling.
Builder receives native facts/contracts only; no Builder, worker, Python or UI file
has been edited in this checkpoint. Its owner supplies fresh paid feeder outputs
and physical inventory execution, using the atomic pair receipt/output session.

Focused test sources add fixed canonical taxonomy witnesses, positive single-exclusive
weighted selection and recorded-roll preservation, zero-special-weight refusal,
multiple-exclusive inspection/refusal, two-carrier NNN natural Essence exhaustion,
constraints version/buffer/input preservation, analytic v1 exclusion/v2 recycling, illegal acquisition refusal, and dynamic v2 Apply/static model
identity after pair destruction. The inventory suite is registered in native CTest.
ALL ARE UNRUN. Only source whitespace checking has passed.

## Pending advanced-blocking proposal (not activated)

This is a concrete approval proposal, not a claim of verified current game rules.
Implementation source remains `0211d7a8604ac064c80d40ffd20beb915ef2297f`; no native,
API, model activation or heavy qualification changed during this research continuation.

**Pending candidates**, with a new model/configuration identity. The point-side-order default is withdrawn; the sensitivity-first contract below supersedes that recommendation:

| Decision | Proposed value | Evidence and alternative |
| --- | --- | --- |
| Exclusive padding count | On each pooled side, effective count = all non-exclusive physical occurrences + one if any exclusives occur. Duplicates and NNN natural occurrences still count physically. | Best-supported current candidate from the original 3.26 tests and guide. Alternative per-input collapse or old full physical padding gives substantially higher counts; do not silently restore it. |
| Count and removal order | Draw both requested counts from Oliver's unchanged adopted table before selection. Select one side, removing canonical duplicates, full groups and all remaining exclusives after the first exclusive; then fill the other side using its already drawn count. Pools can exhaust. | Current calculator author describes counts preceding cross-exclusive removal. Recounting the second side after removal is a materially different candidate. Counts are independent under this proposed estimated law except the named bare split exception. |
| First side | **No default point probability.** Analyze both first-side kernels, allowing unknown chances in [0,1] per state/action/carrier under the separately declared candidate count model. | Evidence supports investigating bias, but provides no measured numeric bound. Earlier q=.10 is retained below only as a toy scenario, not a recommendation; q=.50 is historical sensitivity, not verified current law. |
| Zero proxy | Keep approved positive carrier roll/spawn proxies. Give classified transferable exclusive or allow-listed bench mods lacking such a proxy weight 1,000. Ordinary NNN zero weights remain ineligible. | Original tester tentatively suggested uniform crafted weight around 1,000; no current universal special-weight measurement exists. Alternatives: explicit per-key weights, or retain unsupported-zero exclusions. This fallback needs approval beyond existing positive roll-weight approval. |
| Bare split 1p0s + 0p1s | Adopt probabilities 1/3 for both, 1/3 prefix only, 1/3 suffix only; no empty branch. Scope only this explicit input shape. | Current primary calculator implements those three rows. Existing v1/v2 refuse this shape. Independent .59 draws instead give 34.81% both and an empty branch. This is another explicit estimated-law choice, not an existing activated rule. |

Only identified categories enter the optimizer. Native origin facts (essence_only,
metamod, Delve/unveiled/veil special kinds, elevation relations, beast Aspect types)
identify canonical sources; **the recombination-exclusive mapping comes from primary
mechanics evidence, not a game-data exclusive bit**. Native groups and origin identity
are separate from count/order/weight probabilities. Non-elevated influence and fracture
are non-exclusive in that evidence but their output/eligibility semantics remain held.
Incursion/Breach and general bench exclusivity still need closed canonical registries;
zero weight and a natural stat counterpart are not proofs of exclusivity or NNN status.

A newly resolved primary source is the original tester's
[non-exclusive bench diary tab](https://docs.google.com/spreadsheets/d/10slavP-n-rCjnu5sVFi9DQTUybI2ZDoBbZsZ2UxM7Xg/edit#gid=1986319032).
It lists simple life on helmet/body/gloves/boots/ring/amulet, shield spell block,
and belt resistance/Dex/Int. Combined with the current 3.26 confirmation of regular
life/resistance crafts, this supports a **closed candidate allow-list**, not a rule
that every crafted modifier or every natural-stat counterpart is non-exclusive:

- Life keys: `HelenaMasterIncreasedLife1`, `EinharMasterIncreasedLife2`,
  `EinharMasterIncreasedLife3`, `EinharMasterIncreasedLife4`,
  `EinharMasterIncreasedLife5_`; restrict to diary-covered classes and actual
  frozen native bench class links, with sole stat `base_maximum_life`.
- Shield spell block: active keys `EinharMasterSpellBlockPercentage1` and
  `EinharMasterSpellBlockPercentage2`, sole stat `base_spell_block_%`.
- Belt elemental resists: `HelenaMasterFireResist1`, `HelenaMasterColdResist1_`,
  `HelenaMasterLightningResist1`, `EinharMasterFireResist2`,
  `EinharMasterColdResist2`, `EinharMasterLightningResist2`,
  `EinharMasterFireResist3_`, `EinharMasterColdResist3__`,
  `EinharMasterLightningResist3_`. Native bench class links include Belt.
- Diary belt Dex/Int do not have active Belt bench links in this frozen dataset;
  do not fabricate their feeder availability. Hybrid and other Jun recipes stay
  unresolved. The same spell-block stat also occurs in a distinct Jun Body Armour
  recipe, so matching stats alone cannot classify every recipe.

These mappings are reviewable provisional registry entries, not activated classifier
changes. Registered bench eligibility should use the existing native bench class
relationships, rather than interpreting their ordinary zero spawn weight as NNN.
The bench `master` field is not reliable origin authority here: 747 active options,
including ordinary life and Jun-key recipes, say Niko. Do not classify them all as
Delve or derive Betrayal exclusivity from that vendor field.

### Concrete per-attempt differences under candidate laws

All following values are exact rational arithmetic of toy hypotheses, **not observed
game odds, engine tests or experiments**. All modifiers have distinct groups and equal
positive weights unless the last row specifies otherwise. Both carriers are compatible.

| Inputs / goal | Candidate A | Candidate B |
| --- | --- | --- |
| Four exclusive prefixes only, two on each input; no suffixes | Pooled collapse: 41% empty, 59% one exclusive | Per-input collapse or full physical count: 0% empty when first/only side selects |
| Prefix pool P+exclusive, suffix pool S+exclusive; keep P and S | Counts before blocker removal: 55.527775% | Recount second side after removal: 41.874775% |
| Prefix pool P+exclusive, suffix pool one exclusive; keep P | Pure-exclusive-side first q=.10: 68.61765% | q=.50: 76.48825% (q=0: 66.65%) |
| Prefix pool three desired + three exclusives; suffix pool two exclusives only; keep all desired prefixes | Collapsed counts, q=.10: 3.1815% | Same counts, q=.50: 9.9075% (q=0: 1.5%) |
| Desired weight 100 + exclusive with no proxy, one side; keep desired | Exclusive fallback 100: 66.65% | Fallback 1,000: 39.363636%; 10,000: 33.960396% |

Derivations: write r=.667, t=.333. Symmetric both-desired odds with counts first are
`t + (r/2)*(t+r/2)`; recounting changes the first term to `t*.59`. The asymmetric
one-desired odds are `(1-q)*(t+r/2) + q*(.59 + .41*(t+r/2))`. For triple prefixes,
first filling prefixes succeeds with `.30*(3/6)*(2/5)*(1/4)=.015`; first filling
suffixes succeeds with `.59*.30+.41*.015=.18315`, giving `.015+.16815*q`.
The fallback-weight example is `.333+.667*100/(100+w)`. These formulas show why
side priority and weights can alter whether paid blockers are worth considering;
actual costs still require complete acquisition/attempt quotes and all failure
recycling. The triple-prefix example is deliberately an abstract feasible affix
layout (A: two desired+one exclusive / one exclusive; B: one desired+two exclusive /
one exclusive), not a claim that every required craft is presently available.

Compare named sensitivity scenarios using the same policy and economic inputs; no
point law is the default. Separately optimized costs are not one-policy bounds, and
endpoint costs need not bound interior repeated-policy costs. Robust recommendations
need explicit family-wide termination/cost evidence, as specified in the canonical
contract below. Preserve full output specs, losing mass, prices, inventory and proper
policy obligations. No new supervisor or game experiments are required.
Keep the approved carrier/rarity/level/tier/roll approximation unchanged. Known bench
flags need explicit represented output treatment; the original guide describes the
selected crafted blocker as still crafted/removable. Fracture, generic influence and
other uncertain output categories can remain outside this first advanced subset.

Source receipts: `Temp/recomb-primary-nonexclusive-bench.html` SHA256
`4af3550198a89278aab8ede78ecbb6bd899f1db28bc825ac36f5dfabce656260`;
current calculator JS SHA256
`4e236651fbd3a8d910a1bbb62b96a84876722c1e8595dbec8e57b24d14fa6890`.
Public Google HTML transport worked after web extraction failed. Sparse primary
observations do not determine q; no MLE was claimed. Read-only SQL matched active
canonical bench keys/classes; lightweight Fraction arithmetic evaluated the listed
formulas only. No native build/test, WASM, benchmark or model activation occurred.

## Sensitivity-first disposition after repair priority

Parent withdrew the unsupported 10% default and selected no new heavy work while the
urgent solver-repair release is active. No user outreach was made. The active joint
law remains unchanged; implementation checkpoint is still `0211d7a8`.

The [canonical uncertainty contract](../../engine/recombination-solver.md#pending-sensitivity-contract-no-default-first-side-probability)
replaces a guessed order frequency with both full first-side kernels per carrier.
One-attempt goal intervals cover their declared convex hull. Named policy scenario
costs are sensitivity evidence only; a separately defined robust extension needs
termination and cost inequalities over every allowed row. The contract preserves the
argument once, including an interior-cost counterexample and the distinction between
global-parameter scenarios and state-dependent uncertainty. No implementation or
qualification is claimed. There is no probability distribution to sample for Apply;
advanced simulation needs a separately selected point law.

Three genuine owner decisions for **later**, not questions sent during repair:

| Decision | Choice to prepare | Recommended first step |
| --- | --- | --- |
| Advanced model scope | Approve provisional pooled-exclusive count collapse/count-first blocking for classified inputs, or retain the current bounded v2 scope. | If expanded, label it estimated and use order uncertainty; leave the bare split exception and other unapproved output categories held. |
| Missing selection weights | Keep zero-proxy specials/bench fillers excluded, or approve explicit per-key/finite-scenario weights (1,000 is a candidate, not a default). | Preserve positive approved proxies now; additions require concrete registry/weight approval. |
| Recommendation and Apply | Sensitivity/model-stability reports with advanced Apply held, or choose one explicit point law for optimization and simulation. | Sensitivity first; no automatic winner when model choices reverse ranking. Robust conditional recommendations only after their native evidence passes. |

No builds, native tests, WASM, benchmark, data refresh, new LOCAL lease or native API
changes were made. Lightweight rational arithmetic verified the illustrative formulas
only. Native qualification remains queued after the repair/Builder/integration owners.

## Qualification request and remaining model gate

Request parent LOCAL after Builder/law qualification. Use existing owners serially:
`scripts/dev-engine.ps1 -Task Engine -Jobs 2`, then `-Task Tests -Jobs 2`;
header smoke, `--random-recombination-only` and `--recombination-solver-only` against
frozen `C:/Users/Oliver/Documents/poecraft2/data/compiled/current`. No solver timed
experiments, Simulator or benchmark expansion. After native qualification, rebuild
matching WASM and check existing exposed pair bindings/worker tests and TypeScript;
new inspector/solver adapter exports require agreement with Builder owner first.
Release LOCAL immediately when heavy checks end, before documentation/commits.

Irreducible probability parameters for future multiple-exclusive optimization are:
current effective-count adjustment for several exclusives on a side; the joint
count/first-side rule, including unequal pools and exceptional first construction;
and positive selection weights for special origins whose ordinary proxy is zero.
A future explicit provisional model could choose these with a new version and
estimated odds, but this branch has not activated those choices. This is a mechanics
research disposition for parent, not a blanket refusal of ordinary Builder work.

## Baseline, frozen identities and delivery state

At parent's request the isolated source commits were rebased onto qualified main
`d29d0682648ecc24cf83aa4284f4b3791fb05793`. The earlier source `3e0c6d71` is now
`77669e76c89503f5affb28febf2ba1bc97409b17`; preceding receipt commit is
`99cae74475ed18a30ab5cee2509b0c99354330c9`. No unqualified solver-repair branch was
imported. A read-only normal-checkout recheck at 18:30 UTC found HEAD
`4ad4058086cbaa766a7178aad7ac5625eb8772a8`, scoped tracked paths clean; that advance
was external to this work. The normal checkout/dev server and protected `0` are
untouched. Prior qualified random branch remains `e830a3a54ad3fd2c71660d113c5e5f3f6f72d043`.

Frozen native manifest source/data identity:
`d511e23d1289674c4ac869d20ccd62a979af65f58d4aac83a01ac9bbb02c853e`;
source `b458174874efbe64cf830d603f9e3effddd2042d0f3faa548b40c0e69a68ca7a`;
game-data JSON `604e36ae98351b1dd12de65f2c8e91d60bbb1e7951ff27d5212be574104b727e`;
strings JSON `463affab617c1f1622b2f97ea4f9497d8626e8a3f833f40e5b40c45a83e784f6`.
No data or price refresh. Read-only SQLite queries inspected taxonomy/source links.
A few exploratory query column names were corrected against the actual schema;
these were source inspection errors, not test receipts. Source-only whitespace check
passes. Every new DLL/header/Python/WASM/web/TypeScript qualification remains unrun.
No LOCAL lease has been used, no compiled artifact replaced, no merge/push/deployment.

## Combined Sol 6.1 source checkpoint (2026-10-04)

Oliver approved sensitivity-first estimated v3 and successful checked single-item
feeder/export work. The new isolated `dot/sol61-recomb-builder-20261004` starts
from main `29d9e666`, merges recombination `1aac44c1` and Builder `7d60c969`, and
retains main's solver repairs and WASM pending a matching rebuild. Only HANDOFF
and the old WASM conflicted. Protected `0`, the normal checkout and other
worktrees were not inspected or changed. No subagents, deploy or data refresh.

Source `9736e766` is **UNBUILT / UNQUALIFIED**. The [source checkpoint](sol61-combined-source-checkpoint.json)
records exact source trees, implemented source, prepared witnesses and held work.
Its native v3 count-first joint law retains physical candidates, canonical/global
blocking and explicit order scenarios. No default order chance or zero-proxy
weight is introduced; advanced Apply and bare split stay held.

The first checked child language uses full native Annul, Scour and crafted-removal
outputs, native routers and successful single-current-item termination. It binds
exact embedded document/revision, complete output law, paid start and action costs.
No initial random unveil offers, nested inventory or failed-child recovery is
claimed. General authored exact evaluation still refuses inventory/full-item
routing. Unchecked feeder quotes remain hypothetical.

Restricted export reconstructs the compiled graph independently, preserving live
slot multiplicity and original owned-input costs. Explicit declared slot use
avoids A's incoming-control override; continuation acquires only the missing item.
The original Ring1/80 `1+2/.333` witness and 1,000 trials at seed62667494 are
**prepared, not run**. Explicit all-in scenario attempt prices are separate from
unknown physical gold/dust quantities. An opt-in incomplete-cost report returns a
proper feasible resource policy without priced improvement/ranking or export.

Native cancellation now has callbacks at source/child/commit boundaries. Browser
exposure remains held: no merely cosmetic Cancel route has been shipped. Matching
WASM, actual-worker tests and UI adapter delivery follow the finite native gate.
The new UI owner retains visual tokens, typography and shared styling; this task
changes only behavior/opaque model contracts.

LOCAL is not granted. No heavy command or owned compiler/solver process has started.
The next batch is the existing two-job native Tests/Engine build, header/DLL smoke,
four focused selectors and affected Python tests on frozen runtime82fb60a2. Keep
all internal caps/tolerances and deterministic cleanup; no timed research run is
needed. Release the parent slot immediately after the batch. Do not publish this
draft as tested or transfer either component's historical receipts to it.

Presentation migration can proceed against the [stable source interface handoff](ui-interface-handoff.md).
It records connector geometry, authored-template wording and native trace provenance;
execution adapters and the queued qualification gate remain with this owner.

## Combined finite gate completed (2026-10-04)

Native source `823657c0` passes header/DLL smoke and four focused selectors
(237/358/105/300 checks), Python 12 tests plus 7 subtests, the independent Ring
recycling export, and 1,000 native Builder trials at seed62667494 (1,000 successes;
cost6826/actions11739). Cache repair `63ff9c12` is separate for UI cherry-pick; its
async regression, pre-fix negative control and TypeScript pass. The source checkpoint
links the compact receipt under `out/strategy-feeder/sol61-finite-20261004`.

Retained failures led to standard-header/API-smoke fixes, admitted export IDs,
frozen non-explicit fixture correction and explicit canonical A/B bindings. No
represented modifier order was merged away. LOCAL is released with no owned
survivors. Matching WASM, real planner worker/cancellation transport, full web tests,
rendered combined UI and hosted combined qualification remain unrun/held. The
unchanged metadata file used by the mocked cache test is not runtime qualification.

## Planner transport source checkpoint (2026-10-04)

The next slice is source-only and unbuilt. The typed service creates a fresh,
read-only request worker; it captures the full authored request and verifies
native ABI/data/model/price/goal/scenario/export identities. Abort and wall timeout
terminate that worker, and completion terminates it before result delivery. Main
live crafting handles never enter this lifetime. The synchronous native facade
uses the existing full-item importer and bounded C solver/export owners.

Prepared lifetime tests include a real blocked-worker termination witness;
the matching-WASM Ring test checks paid complete children, the independent
recycling export, explicit scenarios and 1,000 actual compiled-worker trials.
These tests are unrun. The earlier native gate at `823657c0` remains separately
qualified; its receipt does not qualify this facade/transport. No compiled file
or frozen data was changed. The source checkpoint records the next LOCAL request,
serial stage limits, compiler jobs=2, cleanup and remaining product gates.

The [bundle approval receipt](bundle-review-rejection.json) retains the exact
rejected command and full reviewer reason. Nothing ran from that command and
no retry occurred. The script would deterministically bundle existing frozen
bytes but mutate derived public bundles and metadata. Parent subsequently clarified the boundary: packaging exactly the same verified
bytes is approved; upstream refresh, runtime selection changes and frozen-input
changes remain excluded. The local bundler is identical to verified main
`29d9e666` (Git blob `0a0a534e5c1996024c5549877857925cd15ab4d7`). Read-only
preflight recorded 16 unchanged input hashes, including all referenced economy
snapshots. Upon a new LOCAL grant, one retry of the exact rejected command is
allowed; stop on renewed denial. No retry ran during this source-only turn.
Before/after product-gate hashes must be compared; after hashes remain pending.
Test fixtures reading frozen bytes in memory do not qualify product bundle/source
metadata.

## Matching WASM checkpoint stopped on fixture identity (2026-10-04)

Source `43ef96cf` built matching WASM with two compiler jobs in 392.3 seconds,
peak job memory 2.94GB under 8GiB, with no surviving process. The one authorized
exact bundle retry passed, including its cache regression; all16 frozen-input
hashes match before/after. Real termination/lifetime and stale-identity tests pass.

The matching-WASM planner fixture then failed: `goal slots 0 and 1 have overlapping
members`. Its candidate loop incorrectly assumed legacy `pcw_item_add_mod` checks
canonical groups; that function calls the raw item slot helper. The native solver
refused the overlapping goal before policy output or trials. Repair the fixture
selection to establish the original native Ring witness's full group disjointness.
Do not weaken the goal or planner guard. No subsequent qualification command ran.

LOCAL is released, all exec sessions ended and escalated process inspection found
zero owned survivors. WASM/MJS outputs are preserved uncommitted and unqualified;
passing native `823657c0` remains separate. The transport receipt under
`out/strategy-feeder/sol61-transport-20261004/qualification-receipt.json` retains
source/artifact/build identities, successful stages, failure and unrun stages.

## Reconnected source-only fixture repair (2026-10-04)

After KIDS reconnected, checkpoint `5c288abe`, all16 frozen inputs and retained
MJS `8ec20cf7` / WASM `c8b4991a` hashes matched. Engine, facade and production
adapter source scopes are unchanged from compiled `43ef96cf`; no owned survivor
was found. CI owns LOCAL; no new build, test or packaging command ran.

The web Ring fixture now adds each candidate through native `pc_item_edit_json`,
which checks all canonical groups and updates the item only on success. It
verifies the full accepted prefix list/flags and full-item preservation on known
group refusal; unexpected failures stop. A deliberate overlapping-goal case
keeps the planner's refusal guard. The same-revision negative case changes only
child document content, preserving its cost/output law to isolate identity.

The test-only repair is unrun. The next requested LOCAL batch begins with the
repaired planner/export/Ring1000 fixture, followed by remaining pair/feeder/client
and TypeScript checks. Reuse the matching build and successful lifetime/cache
stages; preserve the one successful exact bundle retry without repeating it.

## Repaired fixture exposed adapter constructor violation (2026-10-04)

At source `84d7757c`, native full-group candidate admission passed; the first
planner call then refused `Recombination item contains conflicting physical
modifiers`. The batch stopped before policy output/trials. All later checks are
unrun. Peak job memory1.44GB, wall1.36s, no timeout or survivor. LOCAL is released
and escalated process inspection found zero owned survivors.

The facade violated the retained `build_session` overload's explicit fresh-object
precondition: it built again into the already-completed ordinary session. Its
source now constructs a fresh SessionImpl with the same immutable data/base/level,
then imports all complete items through that universe. No canonical group, goal,
resource, pricing or probability guard is changed. This fix is unbuilt; the old
WASM is retained as failure evidence and cannot qualify the changed facade.

The next requested LOCAL batch needs one causal two-job matching WASM rebuild,
then repaired planner/export/Ring1000 and remaining pair/feeder/client/TypeScript
checks under the existing limits. No repeated bundle command is requested. Keep
prior build/lifetime/bundle successes, both failed gates and native823657c0
qualification distinct. The second compact receipt is under
`out/strategy-feeder/sol61-transport-repaired-20261004/qualification-receipt.json`.

## Fresh-session call audit and direct regression prepared (2026-10-04)

All seven production `build_session` call sites were inspected at source
`9b312ef7`: ordinary/configured API, pair output, goal observation, constraints
carriers, donor resources and the corrected planner facade allocate fresh objects.
All15 test call sites do too. The ordinary overload forwards once to the retained
constructor; no other initialized-session reuse was found. No common engine
helper, mechanics or other programme was changed.

A new direct ownership regression shares the already checked native Ring input
owner with the full fixture. It uses an already-completed owned item, with no
feeder/retry/sampling/export: repeated planner calls must have identical receipts,
changing only paid entry cost3->11 must change that cost without new acquisitions,
and dataset, request and live item ownership must remain unchanged. It also
checks live-worker/planner capability refusals and closes both workers.

Its optional negative-control mode pins the old failing artifacts and accepts
only the exact constructor-conflict failure, after native input admission. This
is prepared to run before the next full build to establish fixture preconditions;
then build once and run the positive direct check before the larger planner gate.
Both modes are unrun while UI/core own LOCAL. The living checkpoint records the
bounded causal order, unchanged main-envelope limits and smaller ownership case.


## Fresh-session matching gate and guard fixture correction (2026-10-04)

At source75bea4df, the pinned old-artifact negative control passed, then the
two-job matching build passed in388.75s with2.94GB peak job memory. Its WASM is
50c98f555d4ef9f6786c9a5fcfd138442a5d01ae1b3759b09712dbae059cdd9c;
MJS remains8ec20cf7c0f86cbad4c5d0638e0733da3110610cf02091c0807d4b87186d2b97.
Direct repeated-call/paid ownership now passes. A reported executor disconnect
was reconciled against the existing session/process identities; no build relaunch.

The larger planner reached the general evaluator refusal, then its old error-text
assertion failed before sampled trials. Native returned the correct restricted
full-item routing guard rather than the legacy generic inventory guard. The
fixture now checks EngineError/code1 and the exact current refusal, retaining
physical input preservation. No production code/artifact or guard changed.
This failure remains in
out/strategy-feeder/sol61-transport-fresh-session-20261004/planner-ring-worker.json.
The causal test-only retry and remaining targeted checks are pending while LOCAL
is still held; no rebuilt artifact is qualified for integration yet.


## Focused native and matching-WASM qualification passed (2026-10-04)

The8f1ac866 test-only correction passed without another build. Matching WASM
50c98f55/mjs8ec20cf7 passed direct repeated-call ownership, complete checked child
identity/costs, physical recycling/export, explicit v3 analysis-only scenarios
and refusals. The unchanged Ring1/80 witness retained expected cost1+2/.333,
attempts1/.333 and acquisitions1+1/.333, below fresh-pair3/.333. All1000 actual
compiled-worker trials at seed62667494 succeeded with cost6826/actions11739,
matching native823657c0. Pair, feeder, transfer, strategy and TypeScript checks
also passed. The feeder station-cost-incomplete component remains a separate
regression identity.

All16 frozen inputs are unchanged. Existing deterministic supervision enforced
the declared build1800s/8GiB and test180s/4GiB bounds, at most2 compiler jobs.
The build took388.75s/2.94GB; planner109.91s/1.47GB; feeder111.21s/1.02GB.
Every process session completed and escalated cleanup found zero owned survivors.
LOCAL is released. Retain the old constructor negative and stale guard-assertion
failure with earlier failures; no production guard was weakened.

Compact source/data/model/checker/artifact pins, stage logs and cleanup:
out/strategy-feeder/sol61-transport-guard-fixture-20261004/qualification-receipt.json.
Exact request construction and complete input admission are hash-pinned fixture
sources; raw request JSON was not separately saved. The artifacts now qualify
this focused native/worker bridge, not full product delivery. Full npm, integrated
rendered UI and hosted Build/Test remain unrun; the integration owner must align
generated product metadata with matching source/artifacts. No bundle packaging,
refresh, main merge/push, deployment or dev-server action occurred in this batch.

## October 6 retained-item Annul increment

Oliver resumed recombination alongside Current work at 03:28 UTC. This bounded
implementation continues the reviewed [October 5 report](https://github.com/OliverOrton/poecraft2/blob/ea5b77beb3e97ed1692ef3bc48c4aef08c1bcad5/docs/research/2026-10-05-recomb/report.md),
not its entire proposed programme. Remote main was independently verified as
`7252027c80856628ed16734583bfc9d6e166458b`. Fresh branch
`dot/sol61-recomb-optimizer-20261006` starts there; the normal checkout remains
`7eb16ac3` and other worktrees/research branches are untouched. An initial
checkout rolled back on Windows archive path lengths; the short task-local
worktree completed with per-command long-path support. No archive was researched.

**Baseline and selected slice.** The existing native planner already searches
acquire/recombine/discard, complete admitted outputs, two-slot recycling, quoted
finished feeders and checked Annul/Scour/crafted-removal children. Calculator
observes one pair; worker/planner transport exists, while the inspected Calculator
component does not author an acquisition catalogue. Builder already supplies
paid typed physical inputs and checked exports. The new C++-only opt-in is paid
Annul of an already held item, including a retained recombination output. Scour,
crafted-removal inventory actions and growing-feeder generation are later slices.

The existing removal kernel provides full payloads and every positive failure.
The other held item stays physical and unchanged; initial paid costs and child
costs are settled once. The same proper-policy factorization also evaluates new
preparation counters. Export/check uses existing move/Annul/router vocabulary,
with an initially absent `current` slot. No pair model, selector weights,
exclusive/count/order/side guards, frozen data/prices, base choice or goal changes.
Roll/spawn-weight proxies remain provisional adopted assumptions. Ducat/Pantheon
origins and missing special weights remain unadmitted as in the reviewed report.
The checker also requires an actual satisfied declared output contract at success;
a goal held elsewhere cannot excuse an absent designated output. A tampered-slot
negative is included for every new comparison fixture.
Prepared export requires ordinary-root canonical mapping to preserve the runtime
slot-to-current guard; additional retained-session mappings remain held.

**Focused native qualification (October 6, 05:01 UTC).** The CI owner built the
frozen source, passed header smoke and the existing `--recombination-solver-only`
(1,533 checks) and `--random-recombination-only` (237 checks) selectors, with zero
failures and exit code zero. This replaces the earlier unbuilt/unrun gate for this
increment only. Both comparison arms use the same binary, with preparation disabled
versus enabled; they are not separate old/new-source performance runs.

The CI owner's external retained receipt is
`ci-evidence/recomb-optimizer-batch-handoff-20261006.json`, SHA-256
`3c98a8cb09e1ac1597c09fdf5aaaafe2be49047f73dbb45434900666977d212a`.
Its sibling `recomb-optimizer-qualified-batch-20261006` directory owns preflight,
build provenance, qualification, native records, selector logs and cleanup evidence.
Verified qualification identities are:

- Source `a2c18955800ac87c9692ac416ad7d3978f8ff310`, tree
  `134277568beee0dbadf6cc19bbb5c475513cb038`, engine tree
  `15a980bcb66dce564380029033771a5a32be3591`, based on main `7252027c`.
- Test executable SHA-256
  `8e82ef2f36ee9fe32b3fbb87488b8ecbe3836a1d758000037d47535d41d05856`;
  frozen runtime manifest SHA-256
  `82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d`.
- Solver log SHA-256
  `45908daef8e2b042ded3fe0b6f308842246b6d572e12268f731a002ab04d3181`;
  random-pair log SHA-256
  `2b7d795b1018710527df75c885587a1d6fb2a046d7ab9f41c81247ab7c5931c2`.

**Six measured, independently checked paid-policy comparisons.** The printed
native expected costs below preserve log precision; floating-point evaluations
are subject to the unchanged numerical gates. Prices are declared fixture inputs:
clean AB costs 20, supplied dirty/filler/clean donor starts cost 1, Annul costs .1
(200 in the expensive control), and an all-in recombination costs 1 (1,000 in the
two-held and paid-multimod cases). Incoming entry cost is 4, or 5 with the second
owned item. These prices are not refreshed market data.

| Case | Control expected cost | Annul-enabled expected cost |
| --- | ---: | ---: |
| Incoming dirty ABC, clean AB goal | 24 | 17.43333333333333 |
| Two held physical items | 25 | 18.43333333333333 |
| Paid dirty multi-mod feeder | 20 | 3.2999999999999989 |
| Ordinary filler A+X plus B | 12.082073497804984 | 9.40780780780781 |
| Expensive cleanup | 24 | 24 |
| Clean donors | 7.0060060060060056 | 7.0060060060060056 |

All six control/treatment policies passed the independent restricted Builder
export/check, complete-cost/convergence checks, positive-row mass checks and
counter reconciliation. The second physical item remains untouched on every
preparation outcome; a goal in the wrong declared output slot refuses. Missing or
mismatched prices, unsupported/duplicate actions and closure-cap refusals passed,
as did the repricing cost/count checks. `checked:true` alone is not the acceptance
verdict; the zero-failure summary establishes that the other assertions passed too.

The incoming and two-held treatments each use one expected Annul and no
recombination. Paid-multimod uses approximately three paid starts/Annuls and two
failure discards, also without recombination. Ordinary-filler uses
4.0030030030030046 expected Annuls and 3.0030030030030033 recombinations; the
predeclared gain assertion passed. Both economic controls choose zero Annuls.
Thus the first three establish conditional cleanup/acquisition value; the fourth
establishes incremental cleanup value within its supplied filler catalogue. It
does not prove fillers beat clean donors, alter exclusive-blocking mechanics, or
certify a crafting route to the paid multi-mod start. No global optimum is claimed.

The fixed Ring1/level-80 identity uses the adopted
`poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1` model and native
AddedColdDamage1/AddedFireDamage1/AddedLightningDamage1 modifiers with proxies
500/500/500, full-group disjointness and a clean shared AB goal. All represented
payloads and every positive failure remain accounted for. No selected-mod mode,
live-game weight verification or mixed-base optimization follows. The existing
first-selector Ring Builder control completed its 1,000 runs successfully; no
additional real-quality or Simulator invocation was made.

**Resources, cleanup and integration gate.** Two compiler jobs used 407.2641703 s
combined build time inside the CI owner's 600 s ceiling; configuration had 120 s
and each selector 180 s. Solver/random selectors took 126.1391036/0.3684600 s.
Existing native item/state/iteration/work limits remain 64/256/32/20,000,000.
These are this completed batch's receipts, not a renewed execution allowance or
a performance claim. LOCAL was released at 05:00:47.577655 UTC: all 377 original
tracked identities were absent and the final live-process count was zero.

Documentation finalization at `4a0342f0` changes no engine source or tested binary.
The parent-authorized `dot/recomb-optimizer-integration-20261006` branch starts
from independently verified main `1b038ef8bec8b11092e9fef1f51f726ab8e93bc8`,
preserves all four tested code blobs, and reconciles these four documents while
retaining the newer qualified Current comparison record. Exact-head hosted
Windows and Solver knowledge success gate the guarded non-force main
fast-forward. That hosted integration gate is separate from the native fixture
qualification above. The visually rejected UI and unfinished lower/causal work
are outside this narrow change. Public ABI/worker/UI/WASM,
Current/Finder activation, general feeder crafting, mixed-base optimization and
unresolved blocking laws remain held. No further implementation or benchmark is
selected. This native increment does not depend on the separate UI layout repair.
