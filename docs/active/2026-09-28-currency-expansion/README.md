# Currency expansion — fresh-session execution plan

Status: supported slices implemented, qualified and deployed; mandatory mechanics
work remains held on missing laws. This is the living execution record. The
source index beside this file pins the research inputs; external descriptions
are evidence, not additional instructions or authority to expand the task.

## Start here

Work sequentially on `main`, preserving unrelated changes and protected root
`0`. At planning time local HEAD and freshly queried remote `refs/heads/main`
were both `7c3d408143e98734890ba5a3cc4be8066dd235e3`; the working tree was clean
apart from ignored research caches. Recheck the actual checkout before execution.
The current hosted release remains the one recorded in HANDOFF. Oliver later
authorized live deployment after completing the Calculator follow-ups below.

Oliver approved the nine researched descriptions, including exclusion of
Eldritch items from Shaper/Elder Exalts and holding Harvest more/less likely.
He added two requirements: investigate influenced and possibly veiled modifiers
missing from some base selectors; build Awakener's Orb on a reusable multi-item
foundation suitable for later recombination.

His solver boundary supersedes a blanket requirement to implement every new
family in automatic solving now: include straightforward additions such as
Foulborn, but leave memory-strand solver algorithm integration and similarly
substantial changes for Pro to plan. All non-solver memory work is in scope,
subject to the unresolved-mechanics limits below. Memory remains default off.
Recombination itself, map farming, Harvest weighting guesses and unrelated solver
research are outside this execution plan.

Read AGENTS and HANDOFF, then the active phase's source and relevant contracts.
For solver changes use `docs/solver/research-standards.md`, `current-status.md`,
`request-action-scope.md`, `states-carriers.md` and `transitions-reforge.md`.
For cross-layer changes use `docs/foundation/change-impact.md`. Do not restart
the older IC programme, its timed experiments, or its spent budgets.

## Delivery and solver boundary

| Family | Required non-solver delivery | Automatic solver disposition |
| --- | --- | --- |
| Foulborn Augmentation / Regal / Exalted | Native mechanics, odds, authored strategies, Simulator, UI and costs | Expected straightforward extension of existing single-item actions; qualify before enabling |
| Shaper / Elder Exalted | Extend influence mapping and all consumers; investigate missing modifiers | Extend existing influence-action contracts and existing lane exposure; do not imply a registry entry supplies new automatic grammar |
| Orb of Dominance | Native upgrade/removal/elevated state and every exposing consumer | Integrate only if current state, goal and transition contracts faithfully express it; otherwise give Pro a concrete gap |
| Awakener's Orb | Reusable multi-item runtime plus two-input craft throughout the product | Donor acquisition, inventory search and multi-item optimization belong to Pro; do not disguise a free donor as a one-item action |
| Tempering / Tailoring | Current weighted enchantment pool, effects and socket consequences | Conditional on existing goal/state support; defer new enchantment/socket optimization design to Pro |
| Vaal / double corruption | Current outcome tables, corrupted/destroyed states, class-specific handling | Conditional on faithful terminal/failure/recovery and implicit-goal support; substantive new design belongs to Pro |
| Harvest more/less likely | Record held status; do not ship fabricated odds | Held |
| Memory strands / Remembrance / Unravelling | Concrete native state, supported mechanics, transport, authored workflows, history and UI | Reserved for Pro; unavailable/default off in Current and Finder until that integration is approved |
| Orb of Intention | Document as map preparation, not an equipment action | Outside equipment solver scope |

“Straightforward” means the existing representation already preserves all
facts the action, goal and continuation can observe; the existing kernel and
registry can express the complete probability law, legality and costs; and
existing compilation/evaluation and proof coverage can be extended without a
new abstraction, search grammar, lower-bound argument or closure mechanism.
Prove this with a focused witness and the actual consumers, not code size.

For every admitted action, preserve terminal truth, legal choices/information
timing, immediate cost and successor probability into every represented class.
These are the existing representation contract's conditions, not a new theorem
or permission to reuse a coarse projection unchecked. See
`docs/solver/mathematics/representations.md#equivalence`.

Keep four statuses distinct: sampled native execution, single-action
calculation, evaluation of a fixed authored strategy, and automatic search/proof.
Calculator math or authored execution can share files named `solver_*` without
being new automatic-search support. If their implementation needs the same new
abstraction as search, defer that exact capability too and retain an explicit
unsupported/estimated result rather than granting false exactness.

Use native family restrictions for direct actions and compound dependencies.
Missing prices must not become zero. A newly enabled cheaper action can
invalidate lower coverage; do not preserve an old exactness claim over an
expanded scope. Record Current and Finder admission separately. A deferred
family is visible as unsupported where useful, not silently excluded from a
request that purported to allow it.

## Approved mechanics and remaining evidence limits

The linked current wiki descriptions were read directly through its API on
2026-09-28; `sources.json` records their exact revisions. They inform the
approved implementation contracts below. Recheck changed revisions only for
the mechanic being implemented, and bring an actual contradiction to Oliver.

1. **Foulborn:** ordinary Augmentation/Regal/Exalted effects, with tier culling.
   For N eligible tiers retain the highest K, where K is `ceil(N/2)` for N <= 6
   and `floor(N/2)+1` otherwise. Each survivor receives weight `baseWeight*N/K`.
   Preserve unequal original weights; do not preserve a family's total weight
   by a different renormalization. The formula is community-tested, approved
   as the model here, not GGG-confirmed. Resolve modifier-type/tier identity
   from canonical data rather than UI display families. Audit the ordering
   relative to item-level eligibility, generation weights, locks and group
   filtering; an unresolved material ordering is a mechanic question.
   [Wiki](https://www.poewiki.net/wiki/Foulborn_currency_item)
2. **Shaper/Elder Exalt:** add influence plus one eligible influenced affix to
   a rare item with room, preserving other affixes. Exclude already influenced,
   fractured, synthesised, Eldritch, corrupted and mirrored inputs as applicable.
   Respect actual eligible modifier levels. Oliver approved the exclusion of
   Eldritch items despite contradictory prose on Shaper's wiki page. This
   supersedes the old four-currency scope ruling in the influence reference;
   historical implementation statements remain historical until code changes.
   [Shaper](https://www.poewiki.net/wiki/Shaper%27s_Exalted_Orb),
   [Elder](https://www.poewiki.net/wiki/Elder%27s_Exalted_Orb)
3. **Dominance:** magic/rare helmet, body armour, gloves or boots with at least
   two influenced explicit mods; remove one and upgrade another. T1 becomes
   elevated; an already elevated selection rerolls its values. Protected and
   legacy modifiers are not affected, and Eldritch implicits are outside this
   operation. Establish the selectable-pair distribution and insufficient
   eligible-pair behavior before claiming exact odds.
   [Wiki](https://www.poewiki.net/wiki/Orb_of_Dominance)
4. **Awakener:** same item class, distinct single influences, uncorrupted donor
   and receiver. Consume the donor; preserve receiver base, item level, sockets,
   links, quality and enchantments. Normally select one influenced mod from each,
   retain its tier, then reforge the other affixes ignoring metamods. Numerical
   values can reroll. All modifier-group exclusions matter; the wiki leaves
   possible additional exclusions unresolved. Recipient imprint restoration
   does not recover the consumed donor. Resolve unknown collision cases rather
   than silently rerolling until two compatible mods are selected.
   [Wiki](https://www.poewiki.net/wiki/Awakener%27s_Orb)
5. **Tempering/Tailoring:** replace the relevant weapon/body-armour enchantment
   using the eligible weighted pool and apply actual effect/socket consequences.
   Tailoring rejects fixed-socket items. The 3.29 pool excludes white-socket and
   all-red/green/blue enchants; stale datamined rows are not eligible merely
   because they remain in the source. Establish weights and socket behavior
   before exact probability claims.
   [Tempering](https://www.poewiki.net/wiki/Tempering_Orb),
   [Tailoring](https://www.poewiki.net/wiki/Tailoring_Orb)
6. **Vaal:** ordinary equipment has four 25% branches: corruption implicit;
   one random white socket becomes non-white; rare six-affix/socket/link
   reforge; otherwise unchanged. All survivors become corrupted. Reforge
   respects metamods. Use separate jewel outcome tables and special unique
   transformations, not six-affix equipment behavior for every base. The
   current model lacks unique rarity; do not convert such outcomes to an
   ordinary rare or no-op. If the exact special transformation law is unknown,
   flag that affected input rather than claiming complete support.
   [Wiki](https://www.poewiki.net/wiki/Vaal_Orb)
7. **Double corruption:** four 25% branches: two corruption implicits; all white
   sockets become non-white; influenced rare reforge; destruction. Reject
   already corrupted inputs. Preserve relevant metamod behavior and handle
   exceptional bases separately. Establish the brick branch's influence and
   affix/socket law; branch weights alone do not establish all internal odds.
   [Wiki](https://www.poewiki.net/wiki/Locus_of_Corruption)
8. **Harvest more/less likely:** held by Oliver. Recipe discovery and cost are
   known (200 Wild lifeforce), but current multipliers, matching identity and
   interactions are insufficiently established. Do not reuse the historical
   90x claim as a current exact rule.
   [Recipes](https://www.poewiki.net/wiki/List_of_harvest_crafting_options),
   [historical CoE explanation](https://www.craftofexile.com/faq)
9. **Memory:** retain strand count as real item state, including imprint
   restoration. Relevant crafts bias tiers and consume strands; published
   consumption intervals are not probability distributions. Harvest, resonators
   and beastcrafting do not consume strands, which alone does not establish
   every interaction with tier bias. Remembrance randomizes normal-equipment
   strands. Unravelling consumes all strands, may upgrade nothing, ignores
   locks and cannot turn crafted mods into natural mods or create elevated mods.
   Intention is map preparation and is not an equipment action.
   [Strands](https://www.poewiki.net/wiki/Memory_strand),
   [Remembrance](https://www.poewiki.net/wiki/Orb_of_Remembrance),
   [Unravelling](https://www.poewiki.net/wiki/Orb_of_Unravelling),
   [Intention](https://www.poewiki.net/wiki/Orb_of_Intention)

The memory research proposes, for S > 0, keeping
`floor(100*(N-1)/(2.5*S+147.5))+1` tiers; zero strands is the ordinary pool.
Foulborn adds 75 to the denominator when used with strands. It proposes
independent Unravelling upgrade chances
`min(1, S*sum(higher-tier weights)/(2*total-side weight))`, followed by destination
weights multiplied by tier rank from the top. The author explicitly leaves the
linear probability model uncertain; Remembrance has samples rather than an
established distribution. These are candidate empirical models, not certified
laws. Source: [Macookta's research](https://www.reddit.com/r/pathofexile/comments/1wr9445/329_everything_you_need_to_know_about_memory/).

Memory non-solver implementation is authorized, but do not invent a uniform
consumption/Remembrance law or quietly present estimates as exact. Implement the
known state, interface and behavior; retain explicit availability/evidence
metadata. A stochastic operation whose law remains unresolved stays unavailable
for exact calculation; an empirical simulation requires an explicit, versioned
model and clear disclosure, not an unapproved guess. Record the specific missing
law and continue independent families. Deferral to Pro for search is separate
from uncertainty about the underlying game mechanic.

The 3.29 socket changes are confirmed in
[GGG's patch notes](https://www.pathofexile.com/forum/view-thread/3985332#gemsocketchanges).
Do not implement the obsolete white-socket corruption outcomes.

## Source observations to act on

- `engine/src/data_loader.cpp` currently maps only four influence currencies.
  Shaper/Elder session pools already exist. This confirms the mapping omission,
  but does not explain every missing influenced modifier.
- `engine/src/session_builder.cpp` derives influence selector tags from
  normalized item class plus influence, then checks ordered spawn rules. Audit
  actual canonical class/base tags and all six influences, especially weapon
  classes and named armour variants. No base-name whitelist workaround.
- `apps/web/src/app/components/pc-mod-pool.tsx` currently drops reach kinds not
  classified as base, influence, crafted, essence or fossil. Veiled/unveiled
  reach kinds therefore have a concrete presentation gap. Native session
  admission separately checks unveiled spawn rules; both boundaries need review.
- `pc_item_state` already stores enchantments, implicits and sockets, but only
  normal/magic/rare rarity and no strand count or consumed/destroyed-item
  lifecycle. Existing fields do not prove the actions or goal semantics exist.
- Base, item level and dense mod IDs belong to a session, not the item struct.
  Two same-class bases can have different sessions and dense IDs. An Awakener
  implementation must never memcpy donor IDs into the receiver's namespace.

## Execution sequence

### 1. Fix modifier reachability and extend influence currency

Reproduce with representative existing bases across eligible classes and item
levels. Trace stable mod keys through raw ingest, canonical SQLite, compiled
data, native session catalog, action pool, worker transport and both modifier
pickers. Separate legitimate zero roll weight from absent catalog entries;
target selection must not hide a reachable mod just because the current item
has not acquired its influence. Preserve source labels and valid unveil offers.

Inspect `normalize_mods.py`, `compiled_data.py`, `session_builder.cpp`,
`data_loader.cpp`, `pc-mod-pool.tsx`, `pc-modifier-picker.tsx` and
`modifier-options.ts`. Use read-only SQLite queries and existing pool diagnostics
first. Correct the owning classification/tag/filter boundary and regenerate
derived data through ingest. Verify both positive and genuinely ineligible
cases; do not simply expose every mod for every base. Generic unveil currency
must not begin offering member-specific mods merely because they become visible.

Add stable public Shaper/Elder currency identities, aliases where appropriate,
prices, icons, registry entries and existing influence controls. Update the
old ruling/reference with the approved change and actual implementation status.

### 2. Add Foulborn through one native weight transform

Implement the three actions with a shared, explicitly parameterized tier/weight
transform used by sampling, diagnostics and exact transitions. Preserve ordinary
Augment/Regal/Exalt legality and side selection. Avoid integer truncation of
N/K; use the project's weight/numerical contracts consistently. Ordinary actions
must be unchanged at zero strands. Do not implement the modifier rule separately
in React, Simulator and solver. Add a distinct Foulborn family toggle.

Extend registry, action parser, strategy vocabulary/compiler, costs and existing
automatic admissions under the straightforward test above. Ensure native
disabled-family enforcement also covers generated dependencies and imported
strategies. Check original-request policy evaluation and proof coverage before
calling the family supported by either solver lane.

### 3. Establish reusable multi-item transactions, then Awakener

Use an explicit craft request with named input roles and an explicit result
describing retained, changed, created and consumed item identities. Bind each
input to its base/item-level/session and the same compatible data identity;
persist stable item/base/mod keys. Resolve transferred mods into the result's
namespace, including legal retained tiers outside its ordinary roll pool.

Make validation and mutation atomic: invalid inputs, missing mappings or rejected
operations consume neither item nor currency. Reject accidental self-donation
or aliasing. A successful Awakener operation consumes exactly its chosen donor
and updates exactly its receiver. Do not build separate donor mechanics in UI.

Reuse the workspace/stash rather than introduce a second item store. Keep
operation-role metadata general enough for future recombination to consume two
inputs and produce a result without a privileged receiver. Do not implement
recombination laws, inventory optimization or a speculative general workflow
engine in this phase.

Strategy nodes refer to explicit item roles/resources; repeated execution must
have an available or explicitly acquired donor, never regenerate it for free.
Track acquisition and currency costs separately to avoid double charging.
One undo/history transaction restores all affected workspace items and spend;
redo and branch navigation replay the same recorded result. Save/load, cloning,
restart and Calculator preview preserve role identity without consuming originals.
In-game imprint restoration remains different from application Undo and cannot
resurrect the donor.

Add the two-input selector using shared item cards and existing compact controls.
Compile authored multi-item operations into the same native runtime consumed by
Simulator. Exact fixed-strategy evaluation needs complete inventory/control
identity; if current evaluator abstractions cannot support it without research,
return a specific unsupported result and hand that part to Pro. Never model a
destroyed donor as an endlessly reusable parameter in a one-item probability row.

### 4. Dominance, Heist enchantments and corruption

Implement these as separate complete vertical slices after the relevant state
and data support exists. Ingest elevated relationships, enchantment eligibility
and weights, and current corruption pools through their existing owners.
Respect all exclusivity groups and actual base/level requirements. Keep ordinary
roll eligibility distinct from legal transfer/upgrade retention.

Where an action rerolls numerical values or changes enchantment effects/sockets,
represent and display that effect rather than replacing only a textual label.
Extend native item/goal/import contracts only for the supported effect. The
current structural engine does not gain stat-total correctness merely by storing
a numeric roll field. Identify unsupported effect/goal combinations explicitly.

Add consumed/destroyed outcome states and applicable unique outcomes where their
laws are established. A destroyed item is not a normal empty item or automatic
success. Authored failure/restart routes must charge actual replacement costs.
Use the distinct Vaal and Locus outcome tables, and proper jewel handling.
Evaluate each family's automatic solver eligibility independently; send any new
implicit/enchantment goal abstraction, destructive recovery or inventory search
design to Pro while finishing independent runtime and product work.

### 5. Memory non-solver integration and Pro handoff

Add an explicit bounded strand field with zero as absence, backward-compatible
import defaults and validation. Preserve it through all relevant native copies,
exports, workspace persistence, item display, history, imprints, strategy inputs
and Simulator traces. Unsupported old consumers must reject new semantics rather
than silently dropping the field. Partition caches by every new observed input.

Create a native per-action interaction contract: whether the action observes
strands, whether it consumes them, the evidence-backed probability law if known,
and when tier culling reads the count relative to consumption. Audit multi-roll
and Foulborn interactions, generation weights, special modifiers and tiers.
Record missing consumption distributions, Remembrance distribution, uncertain
Unravelling details and unknown interactions as separate unresolved entries.
Build only supported stochastic paths; complete state/UI plumbing independently.

Memory has its own family and defaults off. Until Pro's solver integration lands,
the native solve boundary for both Current and Finder must reject an affected
strand-bearing start or memory-dependent action/program request with a useful
reason. Turning off a family must not strip strands, treat them as zero, or
allow a dependency to bypass the restriction. Ordinary zero-strand solving
continues to work. Do not expose an enabled solver toggle that does nothing.

Append a compact Pro handoff here with actual implementation commit/data/ABI
identity, supported laws and source status, cache/state dimensions, changed
goals, concrete failing consumers and minimal witnesses. Pro owns how memory,
multi-item resources and other deferred state dimensions enter search,
abstraction/refinement, action coverage, lower bounds and exact closure. Do not
write that algorithm plan or activate it during this execution programme.

## Cross-layer completion checklist

For each implemented family, trace the changed contract through the applicable
parts of this existing ownership chain:

- Ingest/schema -> canonical SQLite -> compiled artifact -> native loader and
  sessions. Never hand-edit SQLite or the compiled projection.
- Native legality, sampling, complete probability calculation where supported,
  stable action IDs, item/goal state and diagnostics; strategy parser/compiler,
  Simulator and fixed-policy evaluator with honest capability distinctions.
- Exposing C/Python/WASM interfaces, lifetime/ownership, worker RPC and versioned
  persistence. Rebuild WASM for these semantic/ABI/vocabulary changes.
- Shared craft choices, modifier picker/list, item card, tooltips and data-driven
  art; Calculator, Emulator, Strategy Builder, Simulator and stash integration.
- Price/material identity and costs, donor consumption, failure recovery,
  current-history-path spend, Undo/Redo and save/load compatibility.
- Native family controls and lane-specific admission; solver-generated policies
  must compile and evaluate under the original goal, data, prices and scope.

Primary product owners include `pc-craft-controls.tsx`, `craft-choices.ts`,
`craft-costs.ts`, `game-assets.ts`, `edit-history.ts`, `strategy-model.ts`,
`workspace/persistence.ts`, `engine-protocol.ts`, `engine-worker.ts` and
`engine-client.ts` under `apps/web/src/app`; facade owner is
`bindings/wasm/wasm_api.cpp`. Native owners include `actions_basic.cpp`,
`session_builder.cpp`, `solver_registry.cpp`, `solver_calc.cpp`,
`solver_reforge.cpp`, `solver_compile.cpp` and `solver_eval.cpp`.

Preserve the current layout and shared components. Sharp corners, compact
controls and restrained existing colours remain the product direction. This is
not a redesign. New action art comes through `tools/ingest/poecraft_ingest/ui_assets.py`
and the asset catalog, not page-local currency URLs or hand-written rule tables.

## Validation and acceptance

Use focused tests that establish mechanics or cross-layer correctness, then
select final checks for the changed layers. Do not run the entire suite after
every phase or start a timed solver campaign. No experiment budget is assigned
here. If fresh Simulator qualification is necessary for changed strategies,
use the owner-approved 1,000 trials and keep it separate from exact assertions.

Required targeted witnesses include:

- Missing-mod fixtures at the actual broken boundary plus representative class,
  influence and item-level cases; valid generic unveils versus excluded sources.
- Foulborn N=1..13 boundaries, unequal tier weights, side/group/lock legality,
  normalized full probability and native sampled/exact agreement with the
  approved model. Independent small hand-derived distributions are preferable
  to a test that copies the implementation.
- Influence Exalt legal and refused inputs, atomic failure, correct currency
  pricing and no accidental change to the four existing mappings.
- Multi-item same-class/different-base and different-level transfers with
  different dense IDs; conflicts, self-alias rejection, donor consumption,
  per-attempt costs, persistence and atomic multi-item Undo/Redo. Use a generic
  transaction fixture to check the contract can support a future two-input result
  without implementing a recombinator.
- Dominance protected candidates/elevated retention; enchantment weighting and
  effects; separate Vaal/Locus branches, destruction, jewel/unique exceptions and
  the current socket outcome. Never validate only the 25% outer branch weights.
- Memory zero/default handling, nonzero preservation, imprint and history,
  unknown-law refusal and native solve rejection across direct/dependency paths.
- For admitted solver actions: enabled/disabled/missing-price scope, exact kernel
  mass and original-root compiled-policy evaluation, with Current/Finder and
  lower/upper/closure capabilities reported separately.

Build through `powershell -File scripts/build.ps1`; select relevant native and
Python checks through existing scripts. Regenerate affected data and run loader/
artifact checks. Rebuild with `scripts/build-wasm.ps1` (self-activates C:\emsdk).
For affected web work run `npm test`, `npx tsc --noEmit` and the appropriate
production build/parity checks in `apps/web`. Use the existing hosting workflow
only if deployment is later requested. Rendered design acceptance stays with
Oliver unless he explicitly asks for browser visual review.

Update mechanic owners, public data/ABI/persistence contracts and the source
index as implementation lands. Record source/build/data identity and passing,
failed, unavailable and unrun checks here. Update solver current-status only for
capabilities actually changed and qualified. Keep historical evidence intact.

Finish with a per-family capability table: complete, supported with a named
scope, mechanics evidence held, or solver integration reserved for Pro. Do not
call the entire programme complete while mandatory runtime work remains. A
specific uncertainty stops only dependent behavior; continue independent work.
Commits stay local unless Oliver asks to push, and use the agent co-author trailer.

## Planning receipt and next session

Completed now: read-only research, owner approval capture, source/contract review,
live remote/main identity comparison and this plan. No native/UI/data changes,
tests, simulations, timed experiments, commits, pushes or deployments were made
for this programme. The ignored research cache is
`out/research/crafting-currency-2026-09-28`; durable source identities are in
`sources.json`, so that cache is not required for execution.

Next concrete step: reproduce the modifier visibility defects, using the native
session versus shared picker comparison in phase 1, and extend Shaper/Elder
currency mappings. Then carry out the independent phases within the boundary
above. Keep this record as the implementation and Pro-handoff owner.

Suggested fresh-session prompt:

> Implement docs/active/2026-09-28-currency-expansion/README.md on main. Preserve
> the approved mechanics and existing UI flow. Include straightforward solver
> extensions, and complete non-solver work for the other approved families.
> Reserve memory and any substantive new solver modelling for Pro; keep memory
> solver support unavailable/default off. Hold Harvest more/less likely and do
> not invent missing probability laws. Investigate missing influence/veiled
> modifiers and make Awakener's multi-item foundation reusable by recombination.
> Follow AGENTS, preserve root 0, work without subagents, and do not push or deploy.


## Execution receipt and remaining holds

This section is the initial implementation snapshot at `52d5fc4`. Later dated
follow-ups below supersede its capability and deployment statements. The current
surface matrix is maintained in [Mechanics](../../mechanics/README.md).

The supported slices below are implemented on `main`; this is **not completion
of every mandatory runtime family**. Source baseline was
`7c3d408143e98734890ba5a3cc4be8066dd235e3`. Implementation commit: `52d5fc463c0e4d1b20db2b1d89330d02afc599bb`. No push, deployment, timed research campaign or
renewal of an earlier experiment allowance occurred.

### Per-family capability

| Family | Delivered runtime/product scope | Calculation / authored evaluation | Current / Finder / proof |
| --- | --- | --- | --- |
| Foulborn Augmentation, Regal, Exalted | All three structural native actions, C/Python/WASM, shared controls, costs and Simulator | Complete kernels in the existing structural carrier; fixed authored policies checked | Admitted through existing action machinery. Magic-item Augmentation witness produces original-root checked policies in both lanes. Rare witness below has no executable policy in either lane. Expanded scope has zero lower only, no inherited positive lower or closure. |
| Shaper/Elder Exalted | Six influence identities, canonical class selectors, existing exclusions, shared controls and costs | Existing Influence Exalt exact/action/strategy contracts extended | Registry exposure adds no standalone automatic Influence Exalt grammar. |
| Awakener | Scoped structural sampling; reusable named-role atomic transactions; stable-key cross-session transfer; donor acquisition/consumption; Emulator/Calculator preview, Stash/history and authored Simulator resources | Preview is sampled. Inventory-bearing fixed-policy exact evaluation explicitly refuses | Inventory/acquisition search and multi-item optimization reserved for Pro. |
| Dominance | Known action identity and explicit unavailable reason; elevated rows retained/displayed | Held: canonical ordinary-T1/elevated relationship and selectable-pair law absent | Reserved after mechanics evidence and representation qualification. |
| Tempering/Tailoring | Known identities and explicit unavailable reasons; retained enchantments transported/displayed | Held: current eligible weights and effect/socket law absent; zero-weight datamined rows are not probabilities | Enchantment/socket optimization reserved for Pro. |
| Vaal | Sampled structural item-level 86+ socketless amulets/belts only, shared Emulator action/costs and authored Simulator | Full four branches only within that narrow scope; no exact corruption strategy evaluator | Implicit goals and corruption recovery/failure modelling reserved for Pro. |
| Double corruption | Known identity, unavailable reason and explicit destroyed lifecycle | Held: inner influenced-reforge, sockets/links and exceptional unique laws | Reserved after mechanics evidence. |
| Memory / Remembrance / Unravelling | Bounded 0-100 state, native per-action evidence, transport/copy/Imprint/history/resource traces and manual input; unknown stochastic actions refuse | Held: consumption/bias timing and distributions, Remembrance count law and Unravelling transition law | Native Current/Finder reject nonzero strands; default off, integration reserved for Pro. |
| Harvest more/less likely / Intention | Harvest held as approved; Intention documented as map preparation | No guessed model / outside equipment scope | Held / out of scope. |

Awakener accepts same-class distinct single influences, different bases/levels,
and above-level/elevated retained modifiers. It refuses numerical rolled inputs,
protected selected slots and any input for which a possible selected pair has a
modifier-group conflict; it never silently resamples that pair. It retains receiver
base, level, quality, sockets/links, implicits and enchantments. Additional
non-group exclusions remain an evidence limit, so this is a structural model
with explicit limitations, not full game-mechanics certification. Its generic
two-input/new-output transaction fixture supplies a foundation, not recombination.

The missing-mod investigation fixed weapon selector normalization by projecting
canonical item-class influence tags from SQLite. Session retention rows now cover
above-level influenced/elevated modifiers and named veiled sources. Shared pickers
show veiled/unveiled entries; member-specific unveil rows remain outside generic
Veiled Orb offers. Retention data never enters natural-roll masks merely by being
visible. Canonical SQLite and historical `data/compiled/current` were not edited.

### Identity, prices and compatibility

- Runtime manifest SHA-256:
  `ef06ad1171b10975fd56a43fb596e94f789e8c5bae5092b0617f5331a285db99`.
  The compiler owns this snapshot; `apps/web/runtime.lock.json` selects it.
- Rebuilt WASM SHA-256:
  `b266f1b1ea2305751a65c76df1544c379b5b3fa21e1d3ef2a75c84d2a8c75db7`.
  Native, Python and WASM public ABI is 3. Old ABI consumers must rebuild.
- Price catalog is `2026-09-28.1`; existing published economy quotes remain
  unchanged. New catalog keys without quotes remain unknown, never zero.
  Resource acquisition has its own explicit price key and inventory entry.
- Stable item transport v3 records base/level and stable mod keys; checked import
  ignores conflicting dense IDs and rejects unknown keys, invalid lengths/counts
  and mismatched sessions. Legacy empty state defaults to zero strands/live.
  **Legacy nonempty dense-only saves have no trustworthy artifact identity and
  refuse migration into the new session.** Their stored records are preserved;
  recovery requires stable keys resolved against their original runtime. There
  is no automatic legacy migration in this delivery.
- Workspace transactions atomically update receiver, donor lifecycle, draft,
  history and current-path spend. Undo/Redo is a workspace edit; in-game receiver
  Imprint/restart does not recreate a consumed donor. Stale external edits abort.

### Validation receipt

Bulk local evidence is under `out/currency-expansion`; the durable normal-root
request/result projection is [pro-witness.json](pro-witness.json). Tests use the
existing owners; no new supervision or benchmark layer was introduced.

| Check | Result / evidence |
| --- | --- |
| Native build | Pass, `native-build-scope-final.log` |
| Native focused currency contracts | 6,246 checks, zero failures, `native-currency-contracts-final.log`; includes hand-derived Foulborn distribution, N=1..13/unequal weights, 1,000 sampled trials per action, atomic multi-item and solve rejection witnesses |
| Native family / compile / eval / API groups | Respectively 155,169 / 1,500 / 16,942 / 3,221 checks, zero failures, `native-{family,compile,eval,api}-final.log` |
| Python currency integration | 35 passed, `python-verified.log`; three Foulborn exact/authored cost witnesses, 1,000 trials per changed simulation witness, receiver-Imprint cannot restore donor, cross-session transfers, memory and scoped corruption |
| Python ABI subset | 6 passed, 10 deselected, `python-abi-final.log` |
| Ingest/artifact owners | 17 passed, `ingest-artifacts-final.log`; additional selected-runtime ingest witness passed in `ingest-delivery.log` |
| WASM rebuild | Pass, `wasm-final-verified.log` (existing switch warnings retained) |
| Web suite | `npm test` passed, `web-verified.log`; includes worker contracts, native missing-price/disabled-family checks and functional IndexedDB transaction tests |
| Final added Pro witness | Filtered worker smoke 4/4 passed, `pro-witness-verified.log`; normal-root rare goal failure is recorded as a capability limitation, not a successful solve |
| TypeScript / production | `npx tsc --noEmit` and `npm run build` passed, `tsc-verified.log`, `production-verified.log`; post-commit build `production-committed.log` binds source `52d5fc4` (bundle-size warning retained) |
| Static hosting parity | Chromium passed functional/runtime checks; Firefox could not launch (`spawn UNKNOWN`), `static-verified.log`. Firefox is unverified; no rendered-design approval is claimed. |

An earlier full native run (`native-final.log`) made 3,927,461 checks with 11
failures: stale price catalog expectations, four-versus-six influence set,
Foulborn operation-name helper, candidate counts and one registry-dependent
policy hash. Those expectations/helpers were corrected and each affected native
group rerun as listed above. The full suite was not repeated. An earlier core
run's catalog failures were the same obsolete all-keys-must-have-quotes
assumption. Existing broad native tests include inherited 10,000-trial cases;
new currency qualification uses the approved 1,000 trials. No statistical run is
used as an exactness certificate.

### Concrete Pro handoff and mechanics blockers

1. **Foulborn search/proof.** `pro-witness.json` fixes runtime/WASM identity, base,
   root, goal, action scope, prices, product profile and both lanes. On the rare
   coverage goal Current expands three states and returns no executable policy;
   Finder also returns none. The large finite Current workspace value is not a
   certified policy cost. The neighboring magic goal in the same worker test
   compiles and evaluates correctly in both lanes. Investigate grammar/continuation
   admission separately from any new positive lower argument. Direct and compound
   Foulborn dependencies both force zero-lower coverage; disabled families and
   missing prices cannot bypass native checks.
2. **Multi-item exact evaluation/search.** Native resource templates begin absent;
   `acquire_resource` charges and creates an inventory instance; Awakener consumes
   the donor. Exact evaluation currently refuses the inventory/control carrier.
   Pro must preserve resource identity, acquisition/reacquisition costs, absence,
   current-item checkpoint behavior and terminal truth before adding evaluation
   or optimization. Do not turn donors into free one-item action parameters.
3. **Memory and effects.** The carrier exists but transition laws do not. Pro's
   later model must retain strand count, consumption/read timing, enchantments,
   relevant rolls and socket/effect state before claiming equivalence. Do not
   fold these observations into the old ordinary proof abstraction.
4. **Mechanics decisions required from Oliver/source ingest:** Dominance's exact
   ordinary-to-elevated map and pair distribution; Awakener collision disposition,
   numerical rerolls and any non-group exclusions; weighted current Heist pool
   and socket/effect semantics; Vaal socket/link and jewel/unique behavior; Locus
   internal reforge/exception laws; memory, Remembrance and Unravelling laws.
   Questions already raised remain unanswered. These hold only dependent paths;
   the independent delivered slices above are retained.

Research disposition: pinned descriptions are incorporated into mechanic owners;
unreported probabilities remain explicit holds. There is no new claim/proof
packet, solver campaign, lower certificate or old-benchmark promotion. The next
execution must resolve a named hold or select a concrete Pro programme, not
silently continue the archived IC work.


## Currency follow-up 2026-09-29

Oliver asked to revisit Dominance and Vaal, check data freshness, put Vaal under
Basic currency and separate enchantments from corruption. He then explicitly held
Tempering/Tailoring because their random weights are not public, and approved
ignoring socket changes while retaining Vaal's 25% socket-only outcome. His later
mod-pool observation exposed a broad `generation_type == -1` filter that also
classified enchantments as implicits. That presentation error is corrected with
separate Implicits/Enchantments tabs and separate on-item selections.

The earlier receipt above describes its own delivery; this section supersedes
its Dominance/Vaal/data-freshness holds. Work remains sequential on main, local
only. No push, deployment, solver programme or new exactness claim is authorized.

### Implemented scope

- Dominance is a native sampled action in Emulator and authored Simulator,
  with currency accounting. It chooses a uniform ordered pair among eligible
  influenced modifiers, removes one and upgrades the other. Protected/legacy
  modifiers are excluded; insufficient candidates do not spend currency.
  Canonical mod-type/side/influence identity orders ordinary tiers; item level
  does not prevent an upgrade. T1 elevation uses 220 explicit stable-key links
  ingested into SQLite from the published CoE table, cross-checked with PoEDB.
  Elevated selections reroll their canonical value ranges. Untouched values,
  enchantments, quality and sockets are retained. An unmapped current modifier
  refuses atomically: Crusader gloves' energy-shield leech suffix has an old
  prefix-only elevated row and no compatible published link.
- Vaal now samples ordinary weapons, armour and jewellery without an arbitrary
  item-level floor. Four equal branches remain: corruption implicit, socket-only
  projected no-op, rare reforge targeting six affixes, and unchanged. A constrained
  pool stops at the existing reforge limit (the item-level 1 quiver witness reaches
  five). Numerical rerolls are populated; locked/fractured values and enchantments
  are retained. Replacing an Eldritch implicit clears its matching tier metadata.
  Survivors are corrupted. Socket fields are carried through this affix projection
  and are not a claim about the game's socket result. Jewel/unique transformations
  and unresolved strand interactions still refuse. Exact Vaal/Dominance evaluation
  and automatic search remain reserved for Pro.
- Vaal is under Basic currency; Dominance is under Influenced. Temple and
  Enchantments are distinct craft categories. Tempering/Tailoring remain held by
  Oliver's explicit decision. Double-corruption and memory-law holds remain.
- Reforge/scour preservation now carries the full modifier slot, including numeric
  values and unveil metadata, instead of reconstructing only ID/group/flags.

### Data identity and evidence

The previously selected raw export dated from June. Current upstream is RePoE
3.29.3.3, commit `a77305840b4cc8555eeeea144eac3eeddeff134b` (2026-09-15).
All twelve fetched files were matched byte-for-byte to immutable commit URLs.
The source lock now pins those archives, exact transforms and output hashes.
Current ingest has 40,355 modifiers and 5,383 bases, including 1,079 ordinary
session bases. Relative to the old export, 1,063 modifier keys were added and
2,154 modifier rows are new or changed. Heist enchantment weights remain absent.

Schema 3 adds the ingest-owned elevation relation. Canonical data hash:
`d511e23d1289674c4ac869d20ccd62a979af65f58d4aac83a01ac9bbb02c853e`.
Selected runtime manifest:
`82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d`.
The compiler regenerated the runtime and lean rule fixtures; the artwork owner
regenerated the matching catalogue. Immutable historical runtime/economy files
remain intact. The native cross-base test now accounts for newly catalogued bench
recipes as explicitly unquoted against its deliberately historical price snapshot;
no new zero price or fabricated market quote was introduced.

Final validation: all 52 Python currency tests pass, including 1,000-trial
Dominance/Vaal authored Simulator checks, low-level equipment, pair frequencies,
protected numeric values and enchantments. The native core passes 3,446,324 checks
with zero failures across all 1,079 ordinary sessions. The explicit mapping audit
exercised all 220 links across 222 class/modifier combinations with zero errors.
Ingest/compiled-data/locked-source checks pass all 19 tests. Full `npm test`,
TypeScript and the production build pass. The WASM smoke includes actual Dominance
and Vaal action transport, elevated results, corrupted-state refusal and cost keys.
Historical benchmark corpus unit tests now load their original immutable runtime
rather than the mutable `data/compiled/current` directory; their corpus pins and
research identities were not changed. No rendered UI review was requested.

Final WASM SHA-256:
`716c7473688e807adf7906cbb0d01fd9df995ec0fa8333a7de2348777c02f2f9`.
The artwork refresh contains 637 available images and 55 unavailable source images
using the existing fallback behavior. This is a local implementation receipt,
not a hosted-release or market-price-refresh receipt.

Detailed execution logs and compact audit: `out/currency-followup/`.

## Single-action Calculator follow-up (2026-09-29)

Oliver explicitly requested odds after one action for the newly added currencies,
including Awakener, and asked whether Vaal implicit weights exist. This supersedes
the earlier single-action Dominance/Vaal hold; it does not select a new strategy
research programme. Starting local revision was `3d65874`, with a clean relevant
working tree. Work remained sequential, local, and outside protected root `0`.

Calculator now calls a native terminal evaluator for Awakener, Dominance and
Vaal. Shared native preparation owns candidate eligibility, stable-key transfer,
upgrade destinations, collision refusal and Vaal implicit replacement. Awakener
uses the current receiver and a Stash donor without consuming either. Its donor
selection persists, its calculation reads the current saved donor, and diagnostics
include both inputs. Existing structural-input and unresolved-law refusals remain.
Foulborn already had exact single-action support; held currencies stay held.

The probability argument is a finite mixture over the existing execution law:

- Dominance enumerates all `n(n-1)` ordered upgrade/remove pairs at equal mass.
  Concrete resulting affixes are projected only after the selected operation.
- Awakener enumerates each donor/receiver pair at `1/(d*r)`, then evaluates the
  existing uniform 4/5/6 refill law from the concrete retained pair and combined
  influence signature. Metamod pool blocks remain ignored as in execution.
- Vaal retains all four 25% branches. Its rare branch uses the native preserved
  base, fixed-six refill target, side limits and empty-pool stopping. The implicit
  branch uses canonical positive weights and uniform replacement of one existing
  implicit; the other branches retain existing implicits. The socket branch is
  the owner-approved projection and is not given a fabricated socket model.

Refills reuse the native factored DP with complete physical modifier-group
conflicts through every draw. There is no continuation after this query, so final
junk observations can merge when goal status, counts and mechanic flags agree.
The clean/coverage predicate and slot marginals remain native-owned. This avoids
unneeded continuation distinctions on jewellery without approximating the draw
denominators. Concrete refill calls bypass the ordinary state/row cache, and
response state IDs are local to the query. No action is added to Current/Finder
or exact authored-policy evaluation. Explicit work/state/memory caps refuse
oversized rows rather than publishing partial probability.

Vaal's table is available without a goal and shows canonical weight, unconditional
roll chance and final-presence chance. Current SQLite has 522 corruption-implicit
records, 348 with positive spawn rules; each base/level uses its own eligible
subset. The inspection-only native handle refuses strategy solving. Ordinary
goal odds retain rarity, tier, minimum-slot and extra-modifier semantics. Currency
prices are shown through the existing economy; Awakener's estimate explicitly
excludes donor acquisition and replacement inputs.

Validation: the native Calculator suite passes 448,697 checks, including an
independent ordered-draw enumeration for retained-pair and fixed-six refills,
overlapping groups, side limits, pool exhaustion and ordinary-cache separation.
Eleven focused Python checks pass for pair/threshold/clean-goal probabilities,
above-level cross-session transfer, immutability, fracture protection, illegal
inputs and canonical Vaal weights across armour, weapons and jewellery. Existing
sampled Awakener/Dominance/Vaal preservation checks also pass in that selection.
WASM was rebuilt; full `npm test` passes, including all 35 engine smoke tests and
the new Calculator controller check. TypeScript and production build pass.
No new Simulator or rendered UI qualification was requested.

The general native build initially hit a locked `poecraft_solver_benchmark.exe`
owned by an already-running process. That process was left alone; the required
engine shared-library and native-test targets built successfully. No benchmark
result is attributed to this change. Logs: `out/currency-followup/calculator-*`.
WASM SHA-256: `c042a996eb6df24e800e9fbf8ab1c5b27847b913d39705947af2566a1aa2c392`.
This is a local delivery, with no push or hosted deployment.

## Double-corruption Calculator follow-up (2026-09-29)

Oliver approved the two outstanding choices: changed-mod bricks are failed
attempts, and the two implicits roll sequentially. The ordinary-equipment query
therefore uses four 25% branches, with brick and destruction represented as
distinct terminal failures. The socket branch carries the original affixes and
implicits under the existing owner-approved socket projection. The implicit
branch replaces every old implicit and clears Eldritch tiers.

For first modifier a with weight w(a), the first draw is w(a)/W. The second
denominator W(a) sums weights of modifiers sharing no group with a, also excluding
a itself. Each ordered draw contributes 0.25*w(a)/W*w(b)/W(a). The returned
unordered pair adds both orders; its marginals sum all pairs containing each
modifier. Pair mass sums to 0.25; added-implicit marginal mass sums to 0.5.
Existing implicit retention contributes only the socket branch's 0.25. A pool
with no compatible second draw refuses rather than dropping probability mass.
Full rows include both terminal failures, conserve total mass, and never award
goal success or slot satisfaction to either failure.

Calculator's Temple action is enabled, including goal-free inspection, native
pair selection, explicit-goal odds, terminal labels, costs and persistence.
Emulator's sampled influenced reforge remains held: no replacement influence or
affix distribution has been invented. Strategy search/evaluation scope is unchanged.
Validation passes: all 13 focused Python Calculator tests, including independent
canonical-group pair enumeration for armour, bows and jewellery; the complete
web suite including 35/35 release-WASM smoke tests; TypeScript; and the production
build. The controller checks native dispatch, terminal presentation, pair display
and read-only cleanup. The native shared library and release WASM were rebuilt.
No strategy changed and no new Simulator qualification was run. Logs:
`out/currency-followup/double-corruption-*`. WASM SHA-256:
`b87e423888d398d0cd29933b4d29f6b8997bfdb7b6666b9b4560e5cf9d74bb62`.

Oliver requested deployment after finishing all work. The hosted receipt will
record the selected source revision and verified archive separately below.

## Hosted currency release (2026-09-29)

Oliver authorized deployment after completing these follow-ups. Source
`332e8c1aa1fcf62010a4677f1cf871e8a551a884` is pushed and live at
https://oliverorton.github.io/poecraft2/ as Beta `96b8227a`. The manual
[hosting workflow](https://github.com/OliverOrton/poecraft2/actions/runs/36603885756)
passed Chromium and Firefox at both `/` and `/poecraft2/`, then published the
exact checked package. The deployed source includes the complete preceding
currency, source-grouping, corrupted-state and Unveil changes. Publication used
the existing `[skip ci]` convention; the manual hosting gates all ran. No unrelated
solver experiment or new Simulator qualification was run.

The [release receipt](hosted-release-2026-09-29.json) records source, build, bundle,
workflow and artifact identity. GitHub's archive digest matches the durable ZIP
under `C:/Users/Oliver/Documents/poecraft2-tester-archives/`; the full verifier
passes all 665 components. The live HTTPS deployment manifest matches the saved
manifest byte-for-byte. Previous known-good run `36481537616` and its durable
archive remain available for rollback; no live rollback rehearsal is claimed.

Fresh isolated Chromium checks passed the executing build, separate Unveil
reveal/confirmation, pending-choice edit/Undo locks across reload, Vaal in Basic,
the corrupted indicator and Undo, native double-corruption branch mass and pair
selection, persisted Temple selection, and Vaal weighted odds. No browser errors
were observed. Initial test-harness attempts targeted a hidden radio input and
case-sensitive text transformed by CSS; correcting the harness required no
product changes. Rendered design acceptance remains Oliver's. Detailed release
evidence is under `out/currency-followup/`; the tracked receipt is the durable
identity record. Only release-record prose follows the deployed source revision.


## Joint item-goal Calculator follow-up (2026-09-29)

Oliver requested normal Input → Goal probabilities for every Calculator action,
including Vaal, double corruption and Awakener, with implicit and influence editing.
The shared item editor now has implicit rows, rarity/corruption/ordinary-influence
controls, direct removal and Copy input to goal. Native atomic edits preserve valid
state; pending Unveil choices lock all new controls. Separate corruption odds tables
and pair picking are superseded by ordinary goal requirements and one result.

The native `pc_calc_create_goal` context accepts zero or more explicit slots plus
exact implicit keys and optional exact influence/corruption requirements. Combined
success checks all requirements on one successor. Aggregation keys carry both the
explicit state and implicit-satisfaction mask; thus incompatible Vaal branches
cannot combine their marginals into a spurious success. Double corruption's approved
brick/destruction failures and sequential weighted pair law are unchanged.

Ordinary actions reuse their existing kernels; native Eldritch draws and fossil
implicit side effects preserve the same observation through the terminal predicate.
Bestiary checks its deterministic successor against the goal, and Awakener retains
the receiver's implicit state and union of donor/receiver influences without
consuming either item. Oversized queries refuse rather than approximate.

Concrete refills without explicit requirements and with extras allowed may omit
affix details: after admission/forced-mod checks, every refill has the same observed
rarity/influence/corruption result. These rows explicitly mark affixes unobserved;
no continuation, affix counts or strategy authority is asserted. Constrained refills
retain complete physical group exclusions and weighted draw denominators. Default
ConcreteRefill behavior and ordinary strategy kernels are unchanged. Native and UI
guards keep these extended goals out of strategy solving and Solver Lab export.

Validation passes: 20 focused Python checks and 448,697 native Calculator checks;
full `npm test` including 36 release-WASM smoke tests; TypeScript and production
build. Independent canonical Vaal/double/Eldritch weights, joint branch
impossibility, Awakener goals, fossil side effects, editor atomicity and native
solver refusal are covered. WASM parity agrees with the prior endpoint for ordinary
reforges, add/remove actions, Essence, Fossil, Harvest and Veiled Chaos. Bestiary's
successful imprint is correctly zero probability for a mismatched rare goal.

Fresh Chromium functional checks pass native implicit/rarity/influence/corruption
editing, Copy input to goal, Vaal preservation plus corruption requirements, two
ordinary implicit rows matching double-corruption pair probability, reload
persistence and Strategy finder refusal. No browser errors occurred. Playwright's
instant checkbox assertion initially ran before the asynchronous native edit had
returned; clicking then awaiting the existing controller completion passed without
a product change. Rendered design acceptance remains Oliver's. No strategy changed
and no new Simulator qualification was run. Logs: `out/currency-followup/item-goal-*`.
Release WASM SHA-256:
`c67d99dbfc93cbfe0db0eaf7bc2c13436cf28612fadb05d9711e5ce53bc97977`.

## Hosted joint item-goal release (2026-09-29)

The authorized follow-up is committed, pushed and live from source
`a468feb4a457dc7033489cf63d6531c46753beb4` as Beta `1d1fe26c` at
https://oliverorton.github.io/poecraft2/. The manual
[hosting workflow](https://github.com/OliverOrton/poecraft2/actions/runs/36631500946)
passed Chromium/Firefox at `/` and `/poecraft2/`. Fresh live Chromium checks
passed editing, Copy input to goal, combined Vaal success, double-corruption
pair goals, reload persistence and the extended Strategy finder guard, with
no browser errors. The executing build matches the live deployment manifest.

The [release receipt](hosted-item-goals-2026-09-29.json) records all identities
and validation. Its durable archive matches GitHub's digest and verifies all
665 components; the live manifest equals the archived manifest byte-for-byte.
Previous known-good run `36603885756` and its archive remain available for rollback.
No rollback rehearsal or rendered design acceptance is claimed. Only release-record
prose follows the deployed source revision.

## Modifier editing and layout follow-up (2026-09-29)

Oliver requested direct implicit selection from the modifier pool, automatic item
state, editable corrupted Calculator inputs, larger currency controls and an
Emulator layout with crafting/history in the middle and modifiers on the right.
The shared item card no longer has an Add implicit button. Pool rows remain the
selection path, with existing source groups and separate enchantments retained.

The native atomic editor now authors explicit modifiers by stable key and sets
their canonical crafted/veiled flags and ordinary influence. It validates session
affix capacities, all exclusion groups and incompatible influence/fracture/Eldritch
state. Adding a Vaal implicit sets corruption; Eldritch additions/removal maintain
their side's tier. Calculator uses this editor even for crafted modifiers on a
corrupted fixture. Actual currency odds and Emulator bench actions keep their
crafting legality. Pending Unveil continues to block manual edits.

Goal-row selection adds the corresponding Vaal corruption or ordinary influence
requirement, and selected Eldritch sources appear in the goal header. Removing a
modifier leaves separately authored corruption/ordinary-influence properties in
place. Extended requirements remain single-action only. No transition law, strategy
vocabulary, solver programme or previously held mechanic was expanded.

The documentation audit updates Calculator, workspace and native item contracts,
the full 36-action surface matrix, Bestiary clone/goal behavior, separate Unveil
controls and Awakener/Restart support. Initial execution snapshots are explicitly
historical; dated receipts are preserved. All changed documentation's local link
targets resolve.

Validation passes: 21 focused Python tests; rebuilt release WASM; full `npm test`
including 36/36 WASM smoke tests; TypeScript and production build. New checks cover
native automatic state, atomic conflict refusal, corrupted fixture authoring,
goal-row property updates and native editor dispatch. Fresh Chromium functional
checks pass column order, enlarged currency targets, absence of Add implicit,
Eldritch tier/header updates and removal, Vaal corruption, ordinary/influenced/
crafted editing while corrupted, actual Chaos refusal, automatic goal properties
and reload persistence. No browser errors occurred. Harness corrections used
stable selectors, explicit search focus and CSS-case-insensitive labels; no
product change was needed for those harness failures. Rendered review remains
Oliver's. No new Simulator qualification was required because strategies are unchanged.
Logs and the functional harness are under `out/currency-followup/editor-*`.
Release WASM SHA-256:
`c62ceb9ea0c4a3da74feeb201e49e74e80ede1e2799a30a86d2668eeacbbc413`.

The first hosting attempt, run `36636768896` from `0fdb91d`, stopped before
deployment on the material-tooltip check. Larger controls exposed a reproducible
focus-scroll dismissal: the selected Essence tier remained visible but its tooltip
closed after pointer entry. Visible anchors now reposition their tooltips on scroll;
clipped anchors still close. The original failing browser check passes without
weakening it, and the complete local Chromium static smoke passes after rebuilding.
Local Firefox could not launch (`spawn UNKNOWN`); the required Linux hosting matrix
must still pass both browsers and paths before publication. No native change or
second WASM rebuild was needed for this presentation fix.

Retry `36637352130` passed Chromium and the Firefox craft/tooltip interactions,
then stopped on the existing all-artwork wait. Larger Basic rows can leave lazy
icons outside Firefox's loading region. The smoke test now brings every icon into
view before requiring a complete decoded image, retaining the full artwork check.
Neither failed run published; the prior live release remained unchanged.

## Hosted editor follow-up release (2026-09-29)

The complete authorized update is pushed and live from source
`f43d6473b20adc36e6464f74d5c3fc10c4e721b5` as Beta `18dbec78` at
https://oliverorton.github.io/poecraft2/. The successful
[hosting workflow](https://github.com/OliverOrton/poecraft2/actions/runs/36637818241)
passed Chromium and Firefox at both `/` and `/poecraft2/`, including the unchanged
tooltip assertions and the complete artwork check. The two earlier failed
attempts above were resolved before publication.

Fresh live Chromium checks pass the executing build identity, direct modifier
selection, automatic Eldritch/corruption state, corrupted Calculator editing,
ordinary influence and crafted flags, actual Chaos refusal, automatic goal
properties, persistence, larger controls and Emulator column order. No browser
errors occurred. The [release receipt](hosted-editor-followup-2026-09-29.json)
records the exact identities and validation. GitHub's archive digest matches the
saved ZIP; all 665 components verify, and the live deployment manifest matches the
archived bytes. Previous run `36631500946` remains available for rollback. No
rollback rehearsal or rendered design acceptance is claimed. Only release-record
documentation follows the deployed source revision.


## Overnight integration repair (2026-10-02)

The native action-family tuple includes `foulborn` and `memory`; the Solver Lab
Python mirror omitted both. Windows CI run `36943669926` exposed the exact-tuple
failure. The mirror now follows the native owner without relaxing tuple equality,
unknown-name validation or disabled-family request identity. Ten focused contract
and patch checks and the service identity check pass. This does not admit Memory
search. Local repair commit: `67d8475d5488d7dc8b4d9af7de2fcc7b5eb66b8c`.

The frozen [witness](pro-witness.json) starts from a **normal** empty item, not a
rare root. Its historical result stays frozen. Under its original runtime,
root, goal, explicit actions and prices, a complete authored Alchemy/Scour retry
controller has Alchemy success probability `0.4414928760559298`, expected paid
cost `4.65658864251745`, and expected actions `3.5300844214451907`. This baseline
exact evaluation is a correctness oracle, not an automatic-search qualification.
Bulk baseline evidence is in `out/mechanics-overnight/foulborn-oracle-*`.

The Current recovery proposal is limited to an exact normal root with no explicit
affixes. It requires a complete ungated native renewal row, and every positive
non-goal exit must admit a deterministic, paid, caller-permitted Scour returning
to that same root and exact renewal signature. The gated zero-progress carrier
is virtual and may be empty; it cannot establish Scour legality or reset identity.
Missing prices, disabled families, protected survivors, changed root flags,
zero success or incomplete reset laws refuse the candidate.
The existing first-candidate owner queues one attempt between complete parent
rows. A private calculator closes the two-operation controller's observations,
advances the full roll and miss checks cooperatively, and debits reforge work to
the parent's allowance. Its miss states never enter the parent namespace.
Generation scratch is released before original-root graph certification.

For success mass `p > 0`, renewal cost `c`, and miss probabilities `q_i` with
recovery costs `r_i`, the complete cycle satisfies
`U = c + sum_i q_i (r_i + U)`, hence
`U = (c + sum_i q_i r_i) / p`. Its success and reset laws reproduce the same
original-root experiment on every miss, so repeated cycles terminate almost
surely. This constructs an upper proposal only. The native compiler emits the
paid operations and exact original goal; the existing graph checker must verify
original-root execution, properness, scope and complete accounting before
retention. A supplied root-only controller keeps its checked graph unchanged;
ordinary statewise policies still use the existing paired product-graph compiler.
The checker requires an explicit internal original-root mode, a supplied graph
and root-check request, with no parent decisions, nonroot values or compiler
bindings. Empty statewise policy arrays cannot select that mode. Mode and entry
requests bind evaluation reuse identity. Private generation releases its copied
goal before suspension and borrows the stable parent-priced operator; each
checkpoint charges the private calculator, nested frame and signature capacity.
The checker reserves its own retained object and both graph copies within the
remaining parent memory. Zero candidate limits inherit; public zero resource
options select ordinary defaults. Exhausted internal capacities refuse work.
The retained root-only certificate supplies no parent statewise continuation
values or new lower/exactness authority. Socket/link, strand, absent-resource and
retained-enchantment carriers remain outside this proposal's representation.

The isolated native test target, benchmark and DLL build successfully. The
focused recovery case passes 1,082 checks, including full-context differential
probabilities on a complete pool, partial protected miss refusal, explicit scope
exclusion, disabled families, missing prices, zero success, changed root flags,
quality/split preservation, costly cleanup, exhausted state/transition/work
capacities, cancellation and stale authority. The follow-up controls require
explicit root provenance, reject parent bindings/nonroot values, charge retained
signature capacity, refuse a 64-byte private allowance, cancel live private
generation without work leakage, and exercise unlimited-width caps. The existing
assertion-owner suite passes 384 checks. Bulk logs and actual executable hashes are under
`out/mechanics-overnight`. No timed solve or worker invocation has run; the frozen
original-runtime Current request passes the native validate-only check and is
frozen for the parent's serial slot.
These fixtures qualify the native slice, not general Foulborn search or an
original-request performance result. No Simulator or WASM qualification is
claimed in this worktree.

Subsequent parent-selected N4 qualification reports a checked original-request
Alchemy/paid-Scour controller at 4.656588642517449c, success effectively one and
zero off-policy mass. Lower remains zero `TargetNeutralZero`; Foulborn has zero
rows/no selected use. Native exit 2 from a stale expected-status field remains
recorded alongside runner exit 0, one complete case and zero survivors. The
[combined overnight record](../2026-10-02-current-overnight/README.md#isolated-current--mechanics-integration)
retains this parent-reported disposition while full report/lifecycle paths are
pending. This updates the earlier preflight-only state without promoting full
Breach/Foulborn, Simulator or final combined-byte worker qualification.

The compact [Dominance assessment](dominance-assessment.json) preserves the
verified baseline identities and fixtures. Dominance remains held. Its existing single-action kernel retains ordered
upgrade/removal pair probabilities and explicit elevation mappings, but the
strategy evaluator refuses it. Current refinement features cannot distinguish
all upgrade destinations. A concrete pinned-runtime counterexample replaces
`ElementalDamageCannotBeReflectedPercentUber1` with
`PhysicalDamageCannotBeReflectedPercentUber1`, holding the helper
`AdditionalCriticalStrikeChanceWithSpellsUber2_`, prefix side, level 68, full
`ReflectedDamage` group, `influence_mod` classification, natural flags and total
item influence bits 40 fixed. On BodyInt17 at level 86, the clean goal
`ElementalDamageCannotBeReflectedPercentUberMaven` has terminal probabilities
`0.5` and `0`. Equality of these existing observations does not imply an equal
Dominance law. No claim of tested strict-key equality is made.

The positive fixture with `LocalIncreaseSocketedActiveGemLevelUber1` and
`AdditionalCriticalStrikeChanceWithSpellsUber2_` has probability 1 of either
requested elevation at threshold one, and probability 0.5 of a specified one,
at both item levels 1 and 86. Bulk read-only baseline evidence and library identity
are in `out/mechanics-overnight/dominance-preflight.json`. Any future authored
Dominance continuation must add a gated full affix-identity representation,
propagate replacement/survivor identity through refinement and compilation,
bind the upgrade-map representation in caches, and retain existing unsupported
carrier and lower-proof guards. This overnight slice adds no Dominance support.
