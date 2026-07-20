# Shared Item Display Brief

## Purpose

Refine the existing `pc-mod-list` into one reusable, compact item display for
the Emulator and Calculator. The component should make a modifier's side,
tier, family, rolled stat text, classification tags, and special state easy to
scan without imitating the oversized in-game tooltip.

## Current-state diagnosis

The current component has the correct stable-slot behavior, but every filled
slot reserves 124 px and the important hierarchy is weak: the long helper line
contains side, family, and tier in one sentence; stat text sits below it; tags
occupy a mostly empty footer. Empty slots inherit much of the same vertical
footprint. A six-affix item therefore becomes much taller than the workbench,
while tiers and family names are harder to compare than they should be.

## Grounded component data

`pc-mod-list` already receives:

- rarity and influence labels;
- implicit, prefix, and suffix arrays in stable engine item order;
- fixed maximum prefix and suffix counts;
- per modifier: session id, stable key, family/display name, family tier
  index, rendered stat lines, classification tags, fractured state, and
  crafted state.

The shared component may gain optional base name and item level presentation,
but it must not derive mechanics or modifier meaning in TypeScript.

## Required hierarchy

1. Header: base name / item level when supplied, rarity, influence badges, and
   compact explicit count.
2. Implicits, when present, above explicits and visually quieter.
3. Stable explicit slots. Prefixes and suffixes remain distinguishable even
   when the layout becomes narrow.
4. Each filled slot exposes at a glance:
   - side (`P` or `S`) and stable position;
   - `T1`, `T2`, etc.;
   - readable family name;
   - exact engine-rendered stat lines as the primary content;
   - useful classification tags;
   - Crafted and Fractured state without relying only on color.
5. Empty slots are clearly retained but compact and quiet.

## Representative populated item

- `Vaal Regalia · iLvl 86`
- `Rare · Shaper`
- Prefixes:
  - `T1 · +(91-100) to maximum Energy Shield`
    - family: `Maximum Energy Shield`
    - tags: `DEFENCES`, `ENERGY SHIELD`
  - `T2 · (101-110)% increased Energy Shield`
    - family: `Increased Energy Shield`
    - tags: `DEFENCES`, `ENERGY SHIELD`
  - `T3 · (39-42)% increased Energy Shield / (16-17)% increased Stun and Block Recovery`
    - family: `Energy Shield and Stun Recovery`
    - tags: `DEFENCES`, `ENERGY SHIELD`
- Suffixes:
  - `T1 · +(46-48)% to Cold Resistance`
    - tags: `COLD`, `RESISTANCE`, `ELEMENTAL`
  - `T2 · +55 to Intelligence` with `FRACTURED`
    - tags: `ATTRIBUTE`
  - `Crafted · +35% to Fire Resistance` with `CRAFTED`
    - tags: `FIRE`, `RESISTANCE`, `ELEMENTAL`

## Required states and interactions

- Normal, magic, rare, influenced, crafted, fractured, implicit, empty, and
  fully occupied items.
- Keep item-slot order stable. Pool highlighting changes; item rows do not
  reorder.
- Preserve the existing right-click-to-mark-fractured interaction and its
  discoverability. Do not invent a new crafting path in the display.
- At 280–420 px container widths, rows wrap cleanly and the component remains
  useful in Variant E's context rail. At wider widths, a two-column treatment
  may be used only if it has a clear stacked fallback.
- Tags should be readable without dominating the stat text. Showing two or
  three primary chips plus `+N` is acceptable if the full list remains
  discoverable.

## Visual variants

### Variant F — Grouped affix ledger

Keep explicit `PREFIXES` and `SUFFIXES` section headings. Each modifier is a
compact horizontal ledger row: side/tier badge at left, family and stat text in
the center, tags beneath or trailing. Empty slots are one-line dashed rows.

### Variant G — Unified six-slot item

Remove large prefix/suffix blocks. Use one stable six-row list labelled
`P1–P3` and `S1–S3`; side color and the slot code preserve structure. This is
the most compact option and works identically in narrow Calculator and
Emulator rails.

### Variant H — Responsive split columns

At wider Emulator widths, show Prefixes and Suffixes as two balanced columns
with three stable rows each; at narrow Calculator widths, stack the columns.
Use strong tier badges and restrained tag lines. This prioritizes direct
prefix/suffix comparison but needs the most responsive CSS.

## Style constraints

- Use the existing poecraft palette and token system.
- Compact expert desktop UI: 1 px borders, 3 px radii, 10–13 px type.
- Stat text is stronger than metadata; tier and side are faster to scan than
  in the current helper sentence.
- No glow, ornate item frame, gradients, inventory art, mod icons, generic
  SaaS cards, or huge empty padding.
- Mockups are component studies, not permission to invent controls, values, or
  engine behavior.

