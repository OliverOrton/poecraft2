# Deep Slate UI cleanup

Source baseline: `29d9e666b9d11dbe9ba58959f2598df9495dc49d`.
Branch: `dot/sol61-deep-slate-20261004`; isolated sibling worktree.
Selected direction: C2 Deep Slate, Noto Sans, stronger ember execution buttons.
The complete delivered plan was read through Library as text; the reference
image was not inspected. No native, goal, price, action or persistence contract
changes are intended. The task remains source-only until the parent grants LOCAL.

## Owners and migrated consumers

| Owner | Consumers / authoritative input | Retained state owner / validation |
| --- | --- | --- |
| `app.css` | Shell, buttons/fields, shared cards and workbench metadata | Existing selectors/hooks; late `continuity.css` merged and removed |
| `pc-item-badges.tsx` | Concrete/target ItemCard, property choices, Stash | Explicit actual/required/exact/choice contexts; no generated constraints |
| `item-card-model.ts` | Emulator, Calculator input, `readItemCard` | Native `itemInfo`; pure metadata projection, no session ownership |
| `item-display.ts` | Live, imported and compact influences | Catalog codes and native Eldritch tiers; missing labels stay unknown |
| `pc-mod-list.tsx` | Actual/goal ledgers, donor/Lock previews | Native IDs vs family keys, stable slots, existing CustomEvents; keyboard Fracture emits existing action |
| `pc-stash.tsx` | Compact saved native facts | Existing save/edit/import/resource identities and lifecycle controls; guarded async catalog/read state |
| `ui-presentation-checks.mjs` | Existing static-host/UI fixture runner | Computed contrast/focus, fixed square slots, optional screenshot checkpoints |

Noto Sans Latin normal variable WOFF2 is 35,820 bytes, obtained from the official
Google Fonts v42 stylesheet. The same upstream asset is assigned to 400/700;
CSS exposes 400-700, including 600 headings. Its OFL notice and source/hash record
are bundled. No OS font installation or runtime third-party font request.

## Source measurements and current evidence

- Implemented primary/dark text: 9.01:1; hover/dark text: 10.45:1.
- Body/card: 10.16:1; muted/card: 6.15:1; focus/card: 7.57:1.
- Field boundary/field: 4.99:1; boundary/card: 3.47:1.
- Magic text was lifted within its blue semantic role to `#9a9aff` (5.34:1 on card).
- `git diff --check` passed. These are source/arithmetic observations.
- Tests, TypeScript, packaging and browser QA have not run; screenshots do not yet exist.

Focused item tests now cover actual/required/any/exact-none/exact influence,
unknown labels, shared live/imported metadata, recorded rolls and crafted/fractured
facts, cleanup on success/failure, and read-only keyboard fracture behavior.
Existing goal, currency, Lock, Unveil, history/resource and layout checks retain
their semantic/numerical authorities. No new supervisor or native workload.

## LOCAL continuation

Request the serialized LOCAL window before starting checks. The isolated checkout
currently lacks `node_modules`, generated web metadata and ignored compiled data;
the normal checkout has them. Materialize/reuse dependencies and frozen inputs
without changing the normal checkout, refreshing data or rebuilding native/WASM.
Use the existing web data/build hooks and retain the frozen runtime identities.
Run `npx tsc --noEmit`, the affected item/goal/currency/Lock/Unveil/resource checks,
then the broader web suite when justified by the shared presentation surface.
Use the existing static-host smoke runner with `POECRAFT_UI_CAPTURE_DIR` pointing
to `out/deep-slate-ui/after`. It captures Emulator, Calculator input/goal, Stash
and font fallback in a fresh browser profile. Capture the corresponding original
revision views separately for actual before/after evidence. Retain watchdog/cleanup
and serialize browser/build/test work with the parent. Do not launch solve batches.

## Retained exceptions and integration

The Builder's feature/protocol/graph work belongs to its separate owner. Its
geometry is retained, including 14px circular connection ports despite the new
28px default button minimum. Builder-specific warm theme literals, graph rank/
active/taken semantics, and Unveil's deliberate green illustration remain for
bounded migration/review; this is not a claim that every app surface is cleaned.
Dense modifier footprints remain 72px (targets 96px), with keyboard-accessible
inner scrolling for long content. Rendered density/zoom/keyboard/preview review
is still required before qualification; the source changes alone do not prove it.

The recombination branch currently overlaps only by appending
`.pc-edge-path.is-item-supply { stroke-dasharray: 5 4; }` to `app.css`.
Preserve that feature rule when integrating; UI work does not edit its controllers
or protocol. The CI branch has no overlapping web diff in the inspected revision.
All commits stay local; CI/integrator owns merging and pushing qualified changes.
