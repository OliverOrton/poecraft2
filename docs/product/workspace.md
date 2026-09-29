# Workspace And Persistence

**Status: stable implemented product reference.**

Parent: [Product](README.md)

Verified against code: 2026-07-19 @ d5e38e3. Scope: workspace shell,
document registry, IndexedDB/local-storage persistence, Stash, dirty-close
flow, and economy selector. No rendered or visual review was performed.

## Workspace Shell

The 2026-09-27 continuity migration retains Vite, Dockview, saved document
formats and the custom palette. React owns the application/document shells,
shared item card, modifier pool/picker, searchable combobox, base picker,
craft controls, condition composer, node cards, Stash, economy selector and
Simulator controls. Small custom-element adapters retain existing controller
APIs. The Emulator and Calculator now reuse one `pc-craft-controls` view.

Dockview initialization waits until React's enclosing commit finishes.
Document disposal waits for native work before unmounting its React shell;
tab detachment does not close document handles. Leaf views retain controller
models and stable modifier slots across detach/reconnect. Native work makes
item/goal/craft editing temporarily inert so edits cannot race a pending action.

The SVG board/edge routing, detailed Calculator/strategy reports, inspector
controllers, dirty-close modal and persistence remain their existing owners.
This is an incremental presentation migration, not a second crafting engine.
Oliver's visual acceptance remains pending; implementation and functional
checks are recorded in the [continuity record](../active/2026-09-27-ui-continuity/README.md).

`pc-workspace` wraps `dockview-core` and creates four content types:

```text
emulator
calculator
strategy
stash
```

Emulator, Calculator, and Strategy documents receive a generated `docId` and
may be opened multiple times in tabs or splits. Stash uses one fixed panel id
and focuses the existing panel when reopened. Simulator and exact
whole-strategy Calculator are runner modes inside Strategy Builder, not
workspace document kinds.

The workspace owns panel creation, layout serialization, titles, dirty dots,
document registration, and close mediation. Domain components own their item,
goal, graph, engine handles, and work results.

Code authority:
`apps/web/src/app/components/pc-workspace.ts` and
`apps/web/src/app/workspace/registry.ts`.

## Layout, Drafts, And Saved Resources

Three storage concerns are separate:

| Concern | Store | Meaning |
| --- | --- | --- |
| Dockview layout | local storage | panel arrangement and document ids |
| Emulator/Calculator/Strategy drafts | IndexedDB | reload/crash recovery for open work |
| Stash | IndexedDB | manually saved items and strategies |

Layout changes save automatically. Domain components also update their draft
records as content changes. A draft is not a Stash resource and draft recovery
does not count as a manual Save.

Emulator and Strategy Builder expose Save and Save As. Save updates the bound
Stash record; Save As creates a new record and binds the document to it.
Import/copy creates an independent unsaved document. Dirty close presents
Save, Discard, or Cancel through the shared modal. Confirmed close disposes
document-owned work/handles and removes the document drafts.

The current Stash stores item and strategy records and offers All, Items, and
Strategies filters. Its item cards can Edit, Import, or open Odds; strategy
cards can Edit or Import. Search, folders, tags, richer sorting, and account
storage are not implemented contracts; they are listed in
[Product Notes](NOTES.md).

Code authority:
`apps/web/src/app/workspace/persistence.ts`,
`apps/web/src/app/workspace/dirty-modal.ts`, and
`apps/web/src/app/components/pc-stash.tsx`.

## Implemented Handoffs

- Emulator `Use in Strategy` creates an unsaved Strategy Builder document
  whose start state is the complete current item snapshot.
- Emulator `Odds` creates a Calculator document seeded from that item.
- Stash item Edit preserves its saved identity; Import creates an unsaved
  Emulator copy; Odds seeds Calculator.
- Stash strategy Edit preserves its saved identity; Import creates an unsaved
  Strategy Builder copy.
- Any Calculator result with `policy_available` can open its exact or bounded
  compiled policy as an unsaved Strategy Builder copy.
- Duplicate commands create independent unsaved documents.

The full strategy base-state transfer includes rarity, quality, flags,
influences, Eldritch tiers, and stable modifier keys with crafted/fractured
flags. No handoff fabricates a goal item from a complex success route.

Code authority:
`pc-emulator.tsx`, `pc-stash.tsx`, `pc-calculator.tsx`, and
`strategy-model.ts::createStrategyFromItemSnapshot`.

## Emulator State

Each Emulator document owns one native item, an action context, selected craft
controls, a draft, and a displayed craft history. The action list comes from
the engine's Emulator-available catalog; applying an action mutates only that
document's native item.

The Emulator gives the item and modifier pool full-height columns. Crafting
and cost/history share a third column with separate scrolling regions, so
long Essence or Harvest panels do not push the item and history below the
window. Item rows retain their normal single-column layout.

The shared item card reserves 72-pixel modifier rows (96 pixels for goal rows
with tier controls), so empty, single-line and multiline mods do not resize the
ledger. Exceptionally long content scrolls within its row; concrete modifiers
also expose their full text on hover.

Emulator and Calculator share material-choice rows for Essences, Fossils,
Influence Exalts and Harvest. Essences select a type, then a tier from that
type; Fossils toggle up to four materials. Harvest resistance conversion remains
in the Harvest panel with Reforge/Augment. Selecting a material stages the choice; the separate
text-only craft/Calculate button applies it. Calculator keeps staged Fossils
separate from its currently evaluated loadout.

Material artwork and hover/focus descriptions come from the Python-generated
asset catalogue joined to the selected canonical data. Essence tooltips show
the listed modifier for the current item class; Fossils show canonical game
descriptions. These are presentation data, not a new mechanics authority.

Undo/Redo and clickable history rows restore native exported item snapshots,
including Imprint checkpoints and pending unveil choices. Item creation and
base changes are reversible too. Crafting after rewinding replaces the redo
branch; this is a linear timeline, not a retained tree of alternate crafts.
Restoring an item does not rewind the action context's random stream.

Craft spend follows the current history path: Undo, Redo and selecting an
earlier history entry restore its cumulative material counts. New crafts after
Undo discard the abandoned branch's spend. Totals use current shared economy
prices and per-material overrides, so price changes revalue existing counts.
Missing prices and older steps without recorded consumption remain explicit.
Native action descriptors supply consumption vectors; Bestiary results supply
their actual consumed keys. Refused actions, item creation and manual item
edits add no spend. Applied bench crafts do count. Base acquisition is excluded.

Cumulative counts are stored in every history frame, so trimming old Undo
steps retains their spend. Existing drafts retain their item timeline and mark
unrecorded earlier spend; they do not invent historical material counts.
Duplicate and Stash imports start a new tracker because Stash saves item state,
not the crafting timeline. Harvest material quantities and artwork use the
same recipe manifest and asset catalogue as the shared craft controls.

Emulator and Strategy Builder keep up to 100 snapshots per document, bounded
to 8 MiB of serialized UTF-16 history (a single larger current snapshot is
still retained). The timeline and current position persist with the draft.
Older Emulator logs remain readable, but steps without saved states cannot
be restored. Undo never changes a saved Stash resource; Save does that.
Ctrl/Command+Z undoes, Ctrl/Command+Shift+Z or Ctrl+Y redoes. Focused text
controls retain their native text undo.

Mechanic tabs and item-state behavior are documented in the
[mechanics library](../mechanics/README.md) and [Engine](../engine/README.md).

## Shared Economy Selector

`pc-app` mounts one title-bar `pc-economy-selector` outside the document tabs.
The popover groups current temporary, permanent, archived, and manual profiles;
shows fresh/stale/offline/manual status and source age; and switches only after
the target snapshot has downloaded and verified. Failed switches preserve the
previous profile and show the error.

All product price consumers use the shared compatibility facade over
`EconomyService`. See [Economy](../economy/README.md) for cache, override,
fallback, and pinning semantics.

## Current Boundaries

- Tabs show dirty state, but no implemented job-busy indicator. Closing a
  document disposes its owned work after the dirty-close flow.
- Draft recovery is automatic on layout reload; there is no separate
  user-facing Recover/Discard journal browser.
- Stash is local-only. Accounts, publishing, fork attribution, and guest merge
  policy are deferred designs.
- Retained Emulator history trees, richer Stash organization, and broader
  workspace fluency remain non-authoritative notes.

See [Product Notes](NOTES.md).

## Multi-item resource history

Awakener selects an existing Stash donor using the shared item card. Native
mutation runs on prepared copies; one IndexedDB transaction compares all saved
before-values and writes donor consumption, receiver state and the document's
history/spend snapshot. Undo restores all affected resources and spend; Redo
replays the recorded result without sampling or charging again. Branch history
carries the resource snapshots. Conflicting later edits refuse the entire
transaction. A stale editor cannot save over a consumed/destroyed record.

Calculator preview uses copies and opens a new Emulator result without consuming
Stash originals. Saved identity excludes accidental self-donation. Memory state
edits record history without currency cost. Consumed/destroyed Stash cards cannot
be opened as live inputs. Stable-key import boundaries are documented in
[Item state](../engine/items.md#export-import-and-cloning).


The modifier pool has separate Prefixes, Suffixes, Implicits and Enchantments tabs.
Enchantment membership comes from the native reach kind, never the absence of a
prefix/suffix side. Their on-item selections use separate item arrays. In crafting
controls, Vaal Orb belongs to Basic currency, Dominance to Influenced, and Temple
and Enchantments are separate categories.

Implicits have collapsible Base, Vaal, Searing Exarch and Eater of Worlds groups.
Enchantments group the native catalogue by Heist, Labyrinth, Harvest and Blight
source, showing sources represented on the selected base. Families from different
sources remain separate even when they share an exclusion group. Concrete item
cards and Stash rows show a red border and Corrupted label from the native item
flag; refreshing or undoing the state updates both.
