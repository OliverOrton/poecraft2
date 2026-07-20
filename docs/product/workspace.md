# Workspace And Persistence

**Status: stable implemented product reference.**

Parent: [Product](README.md)

Verified against code: 2026-07-19 @ d5e38e3. Scope: workspace shell,
document registry, IndexedDB/local-storage persistence, Stash, dirty-close
flow, and economy selector. No rendered or visual review was performed.

## Workspace Shell

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
`apps/web/src/app/components/pc-stash.ts`.

## Implemented Handoffs

- Emulator `Use in Strategy` creates an unsaved Strategy Builder document
  whose start state is the complete current item snapshot.
- Emulator `Odds` creates a Calculator document seeded from that item.
- Stash item Edit preserves its saved identity; Import creates an unsaved
  Emulator copy; Odds seeds Calculator.
- Stash strategy Edit preserves its saved identity; Import creates an unsaved
  Strategy Builder copy.
- A converged Calculator solve can open its compiled policy as an unsaved
  Strategy Builder copy.
- Duplicate commands create independent unsaved documents.

The full strategy base-state transfer includes rarity, quality, flags,
influences, Eldritch tiers, and stable modifier keys with crafted/fractured
flags. No handoff fabricates a goal item from a complex success route.

Code authority:
`pc-emulator.ts`, `pc-stash.ts`, `pc-calculator.ts`, and
`strategy-model.ts::createStrategyFromItemSnapshot`.

## Emulator State

Each Emulator document owns one native item, an action context, selected craft
controls, a draft, and a displayed craft history. The action list comes from
the engine's Emulator-available catalog; applying an action mutates only that
document's native item.

At d5e38e3 craft history is a linear append-only list of action summaries. It
is not a branchable state tree, and the current Emulator does not expose the
previously specified Undo/Redo history model. That proposal is retained only
as an open product note.

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
- Emulator history branching/undo, richer Stash organization, and broader
  workspace fluency remain non-authoritative notes.

See [Product Notes](NOTES.md).
