# Shared Item Display Review

**Status: historical design evidence.** Parent: [Product design archive](../../README.md).
[Generation prompts](PROMPTS.md) are preserved with the review.

These are component hierarchy studies. Text and state come from the real
`pc-mod-list` model, but the implementation will use the engine-rendered stat
lines, actual family labels, influence mapping, and complete tag arrays.

## Variant F — Grouped affix ledger

`variant-f-grouped-ledger.png`

- Closest evolution of the live component: explicit Prefix and Suffix groups
  remain intact while tier/side leave the long helper sentence and become
  directly scannable.
- Tags and special states are clear without hiding the stat lines.
- The image is roomier than the intended implementation; real rows should use
  the existing 10–13 px scale and substantially less vertical padding.
- Best low-risk option if the current group structure should remain visually
  dominant.

## Variant G — Unified six-slot item

`variant-g-unified-six-slot.png`

- Strongest narrow-width hierarchy for both Variant E's Calculator rail and
  the Emulator item column.
- `P1–P3` / `S1–S3` and tier badges make each stable slot immediately
  identifiable without repeating large group headings.
- Stat text is clearly primary; family name, tags, and fractured/crafted state
  stay visible but subordinate.
- A subtle Prefix/Suffix divider should remain in implementation so the single
  list does not become visually flat.

## Variant H — Responsive split columns

`variant-h-responsive-split.png`

- Best direct comparison of prefixes against suffixes when the component has
  650 px or more.
- The live Emulator and Variant E Calculator are usually narrower, so both
  would see its stacked fallback most of the time.
- Adds the most responsive complexity and therefore gives less shared-layout
  value than the mock initially suggests.

## Recommendation

Use **Variant G** as the shared narrow/default component, borrowing Variant
F's explicit `PREFIXES` / `SUFFIXES` divider labels and Variant H's slim side
rails. At a genuinely wide breakpoint, the same rows can optionally enter the
two-column H layout later; that does not need to be part of this pass.
