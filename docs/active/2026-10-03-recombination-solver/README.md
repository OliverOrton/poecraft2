# Random recombination solver and blocking programme

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
