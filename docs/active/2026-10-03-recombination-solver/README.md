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
model decision. Special/exclusive/fracture/exceptional joint laws stay gated.
Ordinary filler and full group-blocking probabilities are computed from complete
native outcomes, not a desired-only projection.

## Acceptance and gates

Finite native witnesses: physical duplicate mass and second/full-group conflicts;
fillers can improve counts or hurt weighted selection; complete output mass;
input carrier mappings; non-goal failure recycling versus buying fresh;
complete acquisition/attempt costs; wrong base/session/model/prices and caps refuse;
proper policy evaluation and no zero-cost nonterminating policy acceptance.

LOCAL is requested, not granted. No build, test, WASM or benchmark has run in this
new programme. Shared native API contract is being prepared before touching any
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
Heavy LOCAL grant and exclusive model decision remain pending; no parent slot
has been used, no compiled artifact replaced, no main merge/push/deployment.

## Category and selection boundary for Builder

| Native input or selection case | Current authority and disposition |
| --- | --- |
| Ordinary explicit, canonical flags zero, special/metamod absent, influence absent, positive source spawn weight | Supported by declared v1 proxy kernel, with complete canonical groups. |
| Normal/magic ordinary item | Source now admits native rarity capacity; output remains rare. New qualification pending. |
| Same canonical mod twice, or shared full group on one side | Physical occurrences enter count/weight pool; selecting either removes all conflicting candidates. Adopted model, not newly verified hidden game law. |
| NNN ordinary transfer to different compatible carrier | Native pair uses carrier-specific positive spawn eligibility after physical counting; exhausted pools can retain fewer. Dedicated solver initially pins one base/level. |
| Prefix/suffix canonical group overlap | Refused: current quantitative side-order law has not been established. |
| Exclusive special Essence, Incursion, Breach, Aspect, elevated, Delve, veil categories | Historical research identifies categories; official current change and original tests do not provide a complete joint probability/weight law. Not admitted. |
| Bench crafted ordinary versus exclusive | Do not classify every crafted flag as exclusive. Current generic crafted names cannot replace the historical named/unnamed distinction. No verified canonical exclusivity registry exists here. Native v1 refuses these inputs. |
| Metamods | Canonical type identifies them, but current exclusive side/count/selection law is unresolved. Refused as pair inputs; fully specified finished-feeder acquisitions may still be compared. |
| Fracture | Historical guide calls it non-exclusive, but retained fracture/output flag mechanics are outside this qualified native model. Refused. |
| Bare 1p0s + 0p1s | Original research identifies an exceptional joint law; new weighted/current full coefficients are not established. Refused, not silently treated as independent .59 draws. |

Frozen manifest special-kind codes are base_implicit, corrupted_implicit, delve,
eldritch_implicit, unveiled and veiled_template. They are NOT a current exclusive
classifier; Incursion/Breach/crafted distinctions require another authoritative
source mapping. Canonical metamod types are available separately. Selection-law
classification must not be inferred from display names or from a zero spawn weight.

The important held implementation gap is the explicit exclusive classifier plus
current joint count/side-selection law and non-naturally-rollable selection weights.
Ordinary desired-independent fillers/full groups are represented by the new solver;
exclusive-blocker optimization has not been implemented or claimed correct.
