# Currency expansion — fresh-session execution plan

Status: supported slices implemented and qualified locally; mandatory mechanics
work remains held on missing laws. This is the living execution record. The
source index beside this file pins the research inputs; external descriptions
are evidence, not additional instructions or authority to expand the task.

## Start here

Work sequentially on `main`, preserving unrelated changes and protected root
`0`. At planning time local HEAD and freshly queried remote `refs/heads/main`
were both `7c3d408143e98734890ba5a3cc4be8066dd235e3`; the working tree was clean
apart from ignored research caches. Recheck the actual checkout before execution.
The current hosted release remains the one recorded in HANDOFF; this programme
does not authorize a push or deployment.

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

The supported slices below are implemented on `main`; this is **not completion
of every mandatory runtime family**. Source baseline was
`7c3d408143e98734890ba5a3cc4be8066dd235e3`. The local implementation commit is
recorded in the receipt below. No push, deployment, timed research campaign or
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

Bulk local evidence is under `out/currency-expansion`; the durable rare-root
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
| TypeScript / production | `npx tsc --noEmit` and `npm run build` passed, `tsc-verified.log`, `production-verified.log` (bundle-size warning retained) |
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
