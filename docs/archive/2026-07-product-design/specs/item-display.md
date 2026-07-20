# Shared Item Display Implementation Spec

Approved direction: `design/mockups/item-display/variant-g-unified-six-slot.png`
with the compact separate Implicits block from
`variant-f-grouped-ledger.png`, approved by Oliver on 2026-07-13.

The mock controls hierarchy only. Actual modifier order, tiers, family names,
stat lines, classification tags, influences, rarity, crafted/fractured state,
slot capacity, and interactions remain grounded in the engine-fed
`ModListModel`.

## Shared component contract

`pc-mod-list` remains the single reusable concrete-item display. Emulator and
Calculator both provide the same model. Add optional `baseName` and
`itemLevel` fields so the component can own its grounded item header when a
host supplies them.

No modifier classification or crafting behavior is derived in the component.
It renders fields already returned by the engine/session catalog.

## Header

- Primary line: resolved base name and `iLvl N` when supplied.
- Secondary line: rarity, engine-derived influence badges, and
  `N explicit · NP / NS`.
- Keep the existing rarity and influence token colors. Remove the current
  rarity-colored glow and thick ornamental border.

## Implicits

- Render a separate `IMPLICITS` section above explicits only when present.
- Each implicit row uses the engine-rendered stat lines and visible tags.
- Implicits stay visually quieter than explicit affixes and do not consume an
  explicit P/S slot code.

## Explicit ledger

- One stable list in engine item order: prefix slots first, suffix slots
  second. Do not reorder occupied mods.
- Retain subtle `PREFIXES N/M` and `SUFFIXES N/M` dividers for scanning.
- Every slot has a fixed code (`P1`–`P3`, `S1`–`S3`, or the actual capacity),
  tier badge (`Tn`; `C` only when a crafted mod has no tier), family name,
  exact stat lines, visible classification tags, and explicit
  `FRACTURED`/`CRAFTED` markers.
- Prefix/suffix side color is a slim rail and slot-code accent, not a fully
  tinted card.
- Empty slots remain in place as compact one-line dashed rows.

## Interaction preservation

- Filled explicit rows keep the existing context-menu event contract.
- Fractured rows cannot dispatch another fracture request.
- Component rerenders preserve host-owned engine state and stable slot order.
- No new click action is added to `pc-mod-list` in this pass.

## Sizing

- Default/narrow layout works at 280–420 px with wrapped stat text and tags.
- Filled rows use content-driven height with a compact minimum, not the current
  fixed 124 px.
- No wide two-column breakpoint is required in this milestone; Emulator and
  Calculator can reuse the same narrow/default hierarchy.

## Token mapping

- container and rows: `--pc-bg`, `--pc-bg-raised`, `--pc-bg-panel`
- dividers: `--pc-border`
- primary stat text: `--pc-text-strong`
- family/metadata: `--pc-text`, `--pc-text-dim`
- side rails/codes: `--pc-prefix`, `--pc-suffix`, `--pc-implicit`
- rarity/influence/crafted/fractured: existing tokens and influence badge
  colors; no new palette tokens.

## Verification

- TypeScript and web tests.
- Browser: empty normal item, populated rare item, crafted mod, fractured mod,
  influenced item, implicit item, and long two-line stat text.
- Confirm right-click fracture still works and that the item and pool retain
  stable/highlighted ordering.
- Confirm the same component renders correctly in Emulator and Calculator.

