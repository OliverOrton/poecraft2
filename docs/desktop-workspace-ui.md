# Desktop Workspace UI

## Purpose

The web app should feel like a focused desktop tool or IDE rather than a sequence of separate pages.

Users should be able to:

```text
open multiple items
open multiple strategies
open simulation results beside their strategy
rearrange tabs
split the workspace horizontally or vertically
resize panel groups
float selected panels
close and reopen documents
save items and strategies
restore their previous workspace
```

The target is an IDE-style workspace, not a fake operating system with many overlapping windows. Docked tabs and resizable splits should be the normal interaction. Floating panels are useful, but they should remain secondary.

## Technology Decision

The existing frontend stack supports this:

```text
Vite
TypeScript
native Web Components
plain CSS
IndexedDB
```

Add one focused dependency:

```text
dockview-core
```

Dockview is MIT-licensed and its core package has no runtime dependencies.

Use Dockview only as the workspace layout manager. It provides:

```text
tab groups
drag-and-drop docking
horizontal/vertical splits
resizable groups
floating panels
browser popout windows
layout serialization/restoration
edge groups for sidebars and bottom panels
vanilla TypeScript support
```

All panel contents remain custom Web Components. The application is still not React-based.

Do not adopt Lumino initially. It is capable of full desktop-style applications, but it would introduce a broader widget/application framework than this project needs.

Do not build docking, tab dragging, split layout, and layout restoration from scratch. Those interactions contain enough edge cases that a focused layout library is justified.

## Workspace Shape

Default workspace:

```text
+-------------------------------------------------------------+
| app menu / command bar / workspace controls                 |
+--------+------------------------------------------+---------+
|        |                                          |         |
| left   |          document workspace              | right   |
| tools  |      tabs, splits, floating groups       | tools   |
|        |                                          |         |
+--------+------------------------------------------+---------+
| bottom tools: history / trace / results / debug             |
+-------------------------------------------------------------+
| status bar: engine/data version, seed, run progress          |
+-------------------------------------------------------------+
```

Recommended structural areas:

```text
top:
  menu and command bar

left edge group:
  saved items
  saved strategies
  craft operation palette
  mod/base browser

center:
  item emulator documents
  strategy editor documents
  simulation result documents
  data/reference documents

right edge group:
  selected item/node/edge inspector
  properties
  condition editor

bottom edge group:
  crafting history
  strategy run trace
  aggregate simulation statistics
  debug candidate pool

status bar:
  data version
  engine status
  current seed when relevant
  background simulation progress
```

Edge groups may collapse to compact tabs when unused.

## Documents And Tools

The workspace should distinguish documents from tools.

### Documents

Documents represent user-created or inspectable resources. Multiple instances may be open.

```text
Item Emulator
Strategy Editor
Simulation Results
Mod Pool Inspector
Saved Run/Trace
Reference/Data Browser
```

Examples:

```text
Item: Spine Bow
Item: Hubris Circlet
Strategy: +2 Arrow Bow
Results: +2 Arrow Bow, 100k runs
```

Documents:

- appear in the central workspace
- may be tabbed or split
- may have a dirty/draft indicator
- have stable document IDs
- can be duplicated
- can be closed and reopened from recent documents

### Tools

Tools provide context or commands. Most should be singletons.

```text
Saved Items
Saved Strategies
Craft Palette
Inspector
History
Run Trace
Statistics
Debug Pool
Command Palette
```

Tools:

- normally live in left/right/bottom edge groups
- may be collapsed
- may follow the active document
- do not own the canonical item or strategy

## Document Model

Use a general document descriptor:

```ts
type DocumentKind =
  | "item"
  | "strategy"
  | "simulation-results"
  | "run-trace"
  | "mod-pool"
  | "data-browser";

interface WorkspaceDocument {
  documentId: string;
  kind: DocumentKind;
  resourceId?: string;
  title: string;
  icon?: string;
  dirty: boolean;
  transient: boolean;
  viewState: unknown;
}
```

`documentId` identifies one open view. `resourceId` identifies saved content.

This distinction allows:

```text
two views of one saved strategy
an unsaved item draft with no resource id
a saved item reopened in a new document
a transient simulation result that can later be saved
```

The workspace service should own:

```text
open document registry
active document
document titles and dirty state
Dockview panel IDs
layout serialization
recent documents
workspace profiles
```

Domain stores should own item, strategy, and result data. Dockview state should not become the canonical source for domain data.

## Item Documents

Each item emulator document owns:

```text
item document id
saved item resource id, if saved
session configuration
current ItemState
craft history
undo/redo position
selected craft
optional debug state
```

Multiple item documents may use different:

```text
base types
item levels
influences
mechanic sets
```

The engine client should maintain session handles by session key:

```text
base id
item level
mechanic/data version
other session-build options
```

Documents with the same session key may share immutable session data. Each document still owns its own mutable `ItemState` and history.

Item document commands:

```text
New Item
Open Saved Item
Save
Save As
Duplicate
Reset
Undo
Redo
Open Debug Pool
Use As Strategy Start State
```

## Strategy Documents

Each strategy document owns:

```text
strategy graph
board pan/zoom
selected nodes/edges
unsaved changes
last run configuration
references to open result/trace documents
```

A strategy start state may be:

```text
embedded item snapshot
reference to a saved item
base/item-level initialization settings
```

For reproducible saved strategies, prefer embedding or versioning the start-state snapshot. A loose reference to a saved item may change later.

Strategy document commands:

```text
New Strategy
Open Strategy
Save
Save As
Duplicate
Run Once
Run Batch
Open Results
Open Trace
Open Start Item In Emulator
```

## Result And Trace Documents

Simulation results should be first-class documents rather than temporary modals.

Result document:

```text
strategy/resource version
run count
seed/run configuration
success rate
cost distribution
step/node statistics
final-item summaries
created timestamp
```

Trace document:

```text
one simulation path
ordered node transitions
operation and attempt counts
matched edge
item snapshots
currency used
failure/success reason
```

This allows useful layouts:

```text
strategy editor on the left
results on the right
trace on the bottom
```

## Tabs, Splits, And Floating Panels

Normal behavior:

```text
click resource -> open or focus its document
drag tab onto center -> join tab group
drag tab onto edge of group -> create split
drag divider -> resize groups
drag group out -> float group
double-click tab/group -> maximize/restore
```

Initial scope:

- docked tab groups
- horizontal/vertical splits
- resizable dividers
- reorderable tabs
- maximized group
- collapsible edge groups
- in-app floating groups

Later:

- browser popout windows
- multiple named workspace profiles
- nested specialist layouts

Browser popouts should not be required for the first version. Popup blocking, lifecycle, and cross-window synchronization make them more complex than in-app floating groups.

## Saving And Persistence

Use IndexedDB for structured user data:

```text
items
strategies
simulationResults
runTraces
workspaceLayouts
recentDocuments
settings
drafts
```

IndexedDB is appropriate because this data is structured, potentially large, asynchronous, and local to the browser origin.

Suggested records:

```ts
interface SavedResource<T> {
  id: string;
  name: string;
  createdAt: string;
  updatedAt: string;
  schemaVersion: number;
  dataVersion: string;
  payload: T;
}
```

Workspace layout record:

```ts
interface SavedWorkspace {
  id: string;
  name: string;
  layoutVersion: number;
  dockviewLayout: unknown;
  openDocuments: WorkspaceDocument[];
  activeDocumentId?: string;
  updatedAt: string;
}
```

Keep Dockview's serialized layout separate from resource payloads. The layout references document IDs; the document registry resolves those IDs to saved resources or drafts during restoration.

## Autosave And Explicit Save

Use both:

```text
autosave:
  protects drafts and workspace state

explicit save:
  creates/names a durable item, strategy, result, or trace
```

Recommended behavior:

- autosave open document drafts after a short debounce
- autosave workspace layout after panel changes
- restore the last workspace on startup
- explicit Save promotes a draft into the saved library
- Save As creates a new resource ID
- closing a saved clean document is immediate
- closing an unsaved draft offers Keep Draft or Discard

Because drafts are autosaved, the app should not constantly interrupt the user with browser-style unsaved-change dialogs.

## Export, Import, And Portability

IndexedDB is origin-local browser storage. Users should be able to export their work without an account or backend.

Support:

```text
export one item as JSON
export one strategy as JSON
export selected results/traces
export a workspace bundle
import individual resources
import a workspace bundle
```

A workspace bundle should contain:

```text
bundle schema version
saved items/strategies/results selected by the user
document descriptors
optional workspace layout
data/engine version metadata
```

Import should create new resource IDs when collisions occur unless the user explicitly chooses Replace.

Do not require the File System Access API. Normal browser download/upload flows are sufficient and broadly compatible.

## Item History

Each open item document should keep an undo/redo history.

History entry:

```text
action request
seed/RNG state
before ItemState
after ItemState
currency/cost
timestamp
```

History can initially remain document-local and be persisted with the draft. Saved items do not need to retain unlimited history by default.

Recommended limits:

```text
keep recent 100-500 actions per open draft
allow clearing history
optionally save named checkpoints
```

## Workspace State And Domain State

Keep these separate:

```text
WorkspaceState:
  panels, tabs, splits, active document, tool visibility

DocumentState:
  selection, scroll, pan/zoom, local history

DomainState:
  saved items, strategies, results, traces

EngineState:
  session handles, ItemState values, running jobs
```

Do not put everything in one global `AppState` object.

Recommended services:

```text
WorkspaceService
DocumentService
ItemRepository
StrategyRepository
ResultsRepository
SettingsRepository
EngineClient
SimulationJobService
CommandService
```

Services can remain small TypeScript classes with explicit events. A frontend state framework is not required.

## Commands And Menus

Desktop-style behavior benefits from a command system.

Command examples:

```text
workspace.newItem
workspace.openItem
workspace.newStrategy
document.save
document.saveAs
document.close
document.duplicate
item.undo
item.redo
strategy.runOnce
strategy.runBatch
view.toggleInspector
view.toggleHistory
view.resetLayout
```

Each command defines:

```text
id
label
icon
keyboard shortcut
enabled predicate
execute function
```

Menus, toolbar buttons, context menus, and the command palette should call the same commands.

Initial keyboard shortcuts:

```text
Ctrl+N            new item
Ctrl+Shift+N      new strategy
Ctrl+O            open
Ctrl+S            save
Ctrl+Shift+S      save as
Ctrl+W            close document
Ctrl+Z            undo
Ctrl+Y            redo
Ctrl+P            command palette
```

Do not show shortcut instructions as permanent body text. Use menus, tooltips, and command-palette metadata.

## Startup And Restore Flow

On startup:

```text
load settings
open IndexedDB repositories
load last workspace layout
resolve open document descriptors
restore saved resources/drafts
create Dockview panels
restore active document
initialize engine sessions lazily
```

If restoration fails because a resource or panel type no longer exists:

```text
skip the invalid panel
preserve remaining documents
show one concise recovery notification
save a repaired layout
```

The app should never fail to start because one saved panel is stale.

Saved resource migrations should be versioned and narrow. If a resource cannot be migrated, preserve its raw payload for export and show it as unavailable rather than deleting it.

## Default Workspace

First launch:

```text
left:
  Saved Items
  Saved Strategies
  Craft Palette

center:
  Welcome/New Item document or first item emulator

right:
  Inspector

bottom:
  History
```

When a strategy opens:

```text
center:
  Strategy Editor

right:
  Node/Edge Inspector

bottom:
  Run Trace / Statistics
```

Users may rearrange this freely, and the app restores their layout.

## Responsive Behavior

Desktop workspace behavior targets laptop/desktop screens first.

For narrow/mobile screens:

- disable complex drag docking
- present one active document at a time
- move tools into drawers or full-screen panels
- keep saved items/strategies and basic emulator usable
- do not promise the full strategy graph editing experience on phones

The stored document/resource model remains the same across layouts.

## First Workspace Slice

Implement the workspace shell before building every tool panel:

1. Add `dockview-core`.
2. Create the app shell and central document workspace.
3. Add left/right/bottom edge groups.
4. Open multiple placeholder item/strategy documents.
5. Support tab reorder, split, resize, close, and focus.
6. Serialize/restore layout.
7. Add IndexedDB repositories for drafts, saved items, and workspace state.
8. Connect the first real item emulator document.
9. Add Save, Save As, Duplicate, and recent documents.

This slice proves the desktop model without requiring the strategy editor or every engine action.

## Invariants

- Dockview owns layout, not domain data.
- Each open panel has a stable document ID.
- Saved resources have stable resource IDs independent of panel IDs.
- Closing a panel does not automatically delete the saved resource.
- Multiple documents can share immutable engine session data but never mutable `ItemState`.
- Workspace restoration tolerates missing or outdated panels.
- Draft autosave protects work; explicit Save controls the saved library.
- The app remains usable without browser popout windows.

## References

- [Dockview introduction](https://dockview.dev/docs/overview/introduction/)
- [Dockview GitHub repository and license](https://github.com/mathuo/dockview)
- [Dockview edge groups](https://dockview.dev/docs/core/groups/edgeGroups/)
- [IndexedDB API](https://developer.mozilla.org/en-US/docs/Web/API/IndexedDB_API)
