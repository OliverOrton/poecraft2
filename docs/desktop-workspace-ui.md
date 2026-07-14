# Desktop Workspace UI

## Product Shape

The app has four core areas:

```text
Emulator
Simulator
Strategy Builder
Stash
```

The desktop-like behavior means these areas can be opened in tabs, placed beside each other in splits, and resized. It does not mean creating a general-purpose windowing environment with many unrelated panel types.

The initial product targets desktop and laptop browsers. Mobile and touch-first graph editing are not initial requirements.

Examples:

```text
Emulator item beside Strategy Builder
Strategy Builder beside Simulator
two Emulator items in separate tabs
Stash beside an open item
```

## Technology

Keep:

```text
Vite
TypeScript
native Web Components
plain CSS
```

Use `dockview-core` for:

```text
tabs
horizontal/vertical splits
resizable groups
tab dragging/reordering
layout serialization/restoration
```

Dockview supports vanilla TypeScript and does not require React. Domain UI remains custom Web Components.

Floating panels and browser popouts are not initial requirements. Tabs and splits cover the intended desktop feel.

## Workspace Tabs

Each open tab has a stable `documentId` and one of four kinds:

```ts
type DocumentKind =
  | "emulator"
  | "simulator"
  | "strategy"
  | "stash";
```

Multiple Emulator, Simulator, and Strategy Builder documents may be open. Stash is normally a singleton tab.

```ts
interface WorkspaceDocument {
  documentId: string;
  kind: DocumentKind;
  resourceId?: string;
  title: string;
  dirty: boolean;
  viewState: unknown;
}
```

`documentId` identifies an open tab. `resourceId` identifies a manually saved item or strategy.

## Emulator

An Emulator tab owns:

```text
one mutable item
engine session configuration
craft history tree
position within the history tree
watched modifiers
selected crafting operation
unsaved-change state
```

Several Emulator tabs can be open at once. Items with the same base/session configuration may share immutable engine session data, but never mutable `ItemState`.

Commands:

```text
New Item
Open From Stash
Save
Save As
Import Copy
Undo
Redo
Reset
Send To Strategy Builder
```

Sending an Emulator item to Strategy Builder creates a new unsaved strategy whose start state is the current item snapshot.

### Watched Modifiers And Action Odds

An Emulator tab carries a watched-modifier tray: the user pins the modifier
families (with tier thresholds) they are crafting toward, using the same
picker as the modifier pool. Once watched mods exist, every craft control
shows the chance that this action on this item hits watched mods, computed
by the calculation engine's outcome call
(see [crafting-solver-plan.md](crafting-solver-plan.md)). Hovering a craft
control expands to the full outcome distribution — the Calculator view
rendered in place. Odds are ambient in the Emulator; the Calculator tab is
for deeper two-item and comparison work, not the only place odds appear.

### Craft History Tree

Craft history is a tree, not a line. Undoing and then crafting again
creates a branch instead of discarding the abandoned future. Each branch
tracks its accumulated currency spend from the root item.

```text
Undo/Redo    walk the current branch
history panel  shows the tree; clicking any node jumps the live item
               to that state and makes its branch current
```

History is per-tab session state. Saved items store the item, not the
tree; layout/persistence rules for content are unchanged.

## Strategy Builder

A Strategy Builder tab owns:

```text
strategy graph
start item state
success/failure terminal routes
board pan/zoom
selection
unsaved-change state
```

Reaching a success terminal defines strategy success. There is no separate global end-condition object. Conditions on edges determine whether execution reaches success, failure, another operation, or a restart path.

Commands:

```text
New Strategy
Open From Stash
Save
Save As
Import Copy
Run In Simulator
Open Start Item In Emulator
Open Goal Example In Emulator
```

For strategies with multiple success routes, the app must not fabricate a single exact goal item. `Open Goal Example In Emulator` uses a concrete successful result selected from a simulator run. If no successful result is available, the user must run the strategy or choose a saved example first.

### Simulator And Calculator Modes

The Strategy Builder keeps one authored graph and switches its bottom work
surface between two evaluation modes:

```text
Simulator
  Monte Carlo run controls, progress, aggregate results, retained traces

Calculator
  exact whole-graph terminal odds, expected work and consumption,
  board flow annotations, selected-node incoming-state classes
```

Simulator remains available for every engine-supported strategy. Calculator
asks the native strategy evaluator to propagate exact probability mass through
the complete graph, including restart loops, and refuses graphs whose actions
or conditions do not yet have an exact evaluator. The UI displays the native
refusal verbatim; it does not keep a frontend support list or approximate the
missing mechanic.

Calculator mode annotates operation nodes with expected visits and terminal
nodes with absorb probability. Edge labels show each edge's conditional share
of its source node's evaluated outflow; hover shows absolute expected
traversals. Selecting an operation still opens its graph-editing controls in
the right inspector and additionally shows the evaluator's top incoming
abstract-state classes in the bottom inspector. Structural graph changes
trigger a debounced re-evaluation, while node movement, viewport changes, and
display labels do not. Previous results stay visible but stale during that
transition.

Expected consumption quantities come from the engine and remain
price-independent. The bottom cost rows dot those quantities with the shared
workspace price overrides, so price edits update subtotals immediately without
changing routing or re-running evaluation; missing prices stay explicit. The
selected mode is crash-recovery state on the Strategy Builder draft and legacy
drafts open in Simulator mode.

## Simulator

A Simulator tab runs a strategy and displays:

```text
run configuration
progress
success rate
average/median/percentile cost when complete
cost status and missing price keys
average craft actions per run
node/step statistics
successful result examples
failure summaries
```

Simulator tabs may be opened from Strategy Builder or from a saved/published strategy in Stash.

The standardized publication run is:

```text
100,000 simulations
```

Simulator results remain associated with the exact strategy version, engine/data version, and economy snapshot used for the run.

### Cost Distribution

Completed runs show the full cost distribution, not only summary numbers:
a histogram with P10/P50/P90 markers and a budget query — enter a budget X
and see the chance of finishing at or under X. Expected cost alone hides
the variance that gamble-heavy and recombinator strategies are made of.

### Materials Summary

Completed runs also aggregate expected consumption per input across all
runs: currencies, essences, fossils and resonators, bases, and bought
items (trade-leaf feeders when present). Quantities come from run
averages; prices come from the active economy snapshot, with missing keys
explicit as elsewhere. The summary exports as a shopping list. This is a
general Simulator feature for every strategy, not a recombinator feature.

## Stash

Stash is the user's resource library. It contains manually saved:

```text
items
strategies
```

The Stash should use searchable cards, folders, tags, sorting, and filters. It should not imitate the Path of Exile stash grid.

Suggested Stash sections:

```text
Items
Strategies
Published
Favorites
```

Item card:

```text
item name/base
compact item preview
key mods
updated time
tags/folder
```

Strategy card:

```text
title
description
compact success-route summary
publication state
success/cost summary when available
author/fork attribution when applicable
```

Complex strategy goals should be represented by title, description, and a success-route summary rather than synthesizing a fake goal item.

## Economy

Prices are a workspace-level service, not a per-tab setting or a buried
options page.

```text
league selector          choose the active league (or a custom profile)
snapshot fetch           client fetches published league price snapshots
                         (poe.ninja-style source), cached in IndexedDB;
                         offline falls back to the cached snapshot
manual overrides         per-key price edits layered over the fetched
                         snapshot; overrides persist locally
```

The active economy is the fetched snapshot plus the override layer,
compiled into the immutable Economy Snapshot JSON defined in
[strategy-editor-ui.md](strategy-editor-ui.md) whenever a run or solve
starts, so results keep pinning an exact snapshot as they do today.

Every cost surface reads the active economy and reacts live: Emulator
spend counters, Simulator summaries and materials lists, solver costs and
strategy-board annotations. Solver re-costing after a price change is
cheap by design — transition caches are price-independent
(see [crafting-solver-plan.md](crafting-solver-plan.md)) — so a price edit
updates cost displays without recomputing distributions.

## Background Work

Long-running work — Run N simulations, solver runs, feeder summary
recomputes — is owned by the tab that started it. Progress and cancel
live in that tab; there is no global task manager. A tab with active
background work shows a busy indicator on its tab title so buried work
stays discoverable. Jobs continue while the tab is unfocused and survive
tab switches; closing the owning tab cancels its jobs after confirmation.

## Edit And Import

`Edit` and `Import` have distinct behavior.

```text
Edit:
  open the saved resource
  manual Save writes back to that resource

Import:
  create a new unsaved copy
  original resource remains unchanged
```

Cross-area imports:

```text
Emulator -> Strategy Builder:
  current item becomes the strategy start state

Strategy Builder -> Emulator:
  import a selected concrete successful goal example

Stash item -> Emulator:
  Edit opens the saved item
  Import opens an unsaved copy

Stash strategy -> Strategy Builder:
  Edit opens an owned saved strategy
  Import/fork opens an independent copy
```

For another user's published strategy, Edit is unavailable. Fork creates a new owned strategy with attribution to the source version.

## Manual Save Policy

Items and strategies do not autosave.

```text
Save:
  write changes to the current saved resource

Save As:
  create a new saved resource

unsaved new document:
  has no resourceId until saved
```

Closing a dirty tab prompts:

```text
Save
Discard
Cancel
```

Workspace layout may be restored automatically because it is a UI preference. Layout persistence must not save item or strategy content.

The app should maintain a separate local crash-recovery journal for dirty or unsaved documents:

```text
recovery snapshot:
  local browser storage only
  not visible in Stash
  not uploaded to an account
  replaced as the document changes

on clean Save, Discard, or confirmed close:
  delete the recovery snapshot

after crash/reload:
  offer Recover or Discard
```

Recovery is protection against accidental loss, not a saved resource or a version history.

## Guest And Account Storage

Guests can use the full local application and manually save resources to IndexedDB.

Signed-in users save to their account backend, with a local cache for responsiveness and offline reading. Manual account Save requires a network connection in the initial implementation. Do not queue a hidden upload for later; offer Save Local Copy or JSON export instead.

On first sign-in with existing guest data, ask:

```text
Merge local items and strategies into this account
Keep local data separate
Discard local data
```

Never merge guest data silently.

## Workspace Persistence

Persist workspace/UI state automatically:

```text
open tab descriptors
tab groups and splits
active tab
split sizes
view-specific pan/zoom/scroll state
```

Persist dirty item or strategy content only in the separate crash-recovery journal described above. Restoring a recovery snapshot must clearly label the document as recovered and unsaved.

## Initial Layout

Default:

```text
top-level app tabs/commands
central docked workspace
Stash available as a primary tab
```

Each core area manages its own internal controls:

```text
Emulator:
  item, crafting controls, history

Strategy Builder:
  graph, inspector, condition editor

Simulator:
  run controls, progress, statistics

Stash:
  search, filters, folders/tags, resource cards
```

Avoid turning every inspector or result into another top-level workspace application.

## First Workspace Slice

1. Add `dockview-core`.
2. Open real Emulator and Stash tabs; do not create disposable Simulator/Strategy Builder implementations.
3. Support multiple Emulator tabs.
4. Support tab reorder, split, resize, close, and focus.
5. Restore layout without saving resource content.
6. Implement manual Save, Save As, and dirty-close prompts.
7. Implement the local crash-recovery journal.
8. Implement local Stash storage in IndexedDB.
9. Implement Edit and Import Copy.
10. Add Simulator and Strategy Builder tabs when their native simulator/editor slices are ready.
11. Connect Emulator -> Strategy Builder start-state import.

## Invariants

- The app has four core areas, not an open-ended collection of document types.
- Tabs/splits provide the desktop feel.
- Item and strategy content is saved manually only.
- Layout persistence never counts as saving resource content.
- Crash recovery is separate from Stash and never counts as a manual save.
- Stash contains saved resources, not unsaved drafts.
- Edit preserves resource identity; Import creates a new unsaved copy.
- Reaching a success terminal defines strategy success; restart transitions do not.
- Complex success routes are summarized, not converted into invented goal items.
- Guests can remain local; account merge always requires confirmation.

## References

- [Dockview introduction](https://dockview.dev/docs/overview/introduction/)
- [Dockview GitHub repository](https://github.com/mathuo/dockview)
- [IndexedDB API](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API)
