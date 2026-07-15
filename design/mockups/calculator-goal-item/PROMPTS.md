# Calculator Goal Item - Image Model Prompts

## Shared references

- Image 1: `design/refs/calculator-current-phase-c.png` - current Calculator
  layout reference.
- Image 2: `design/mockups/item-display/variant-g-unified-six-slot.png` -
  approved shared item-frame hierarchy and style reference.
- Image 3: `design/mockups/calculator/variant-e-stacked-context-rail.png` -
  approved populated Calculator hierarchy reference.

These are generation references, not literal edit targets. Preserve the current
three-pane Calculator, mechanic/action bands, central Modifier Pool, and right
Odds inspector. Change only the left Goal requirements card.

## Variant A - Literal twin

```text
Use case: ui-mockup
Asset type: desktop web-app implementation reference
Primary request: show the existing poecraft Calculator with Input item and Goal item as literal twin item frames; change only the Goal requirements card
Input images: Image 1 current Calculator layout reference; Image 2 approved item-frame style reference; Image 3 populated Calculator hierarchy reference
Style/medium: realistic shippable product UI, dense Path of Exile crafting tool, not concept art
Composition/framing: flat 16:9 desktop Calculator tab; top mechanic/action bands unchanged; left stacked Input item and Goal item; wide Modifier Pool center; Odds right
Goal item: Vaal Regalia, iLvl 86, Rare, small TARGET badge, fixed P1-P3 and S1-S3 rows matching Input item exactly; P1 and P2 and S1 contain realistic target modifier text with T1 (best) selectors and small remove controls; other rows say No prefix requirement or No suffix requirement
Controls: compact Finished rarity Rare and Success means All 3 band directly above the Goal item frame
Text (verbatim where legible): "INPUT ITEM", "GOAL ITEM", "TARGET", "Vaal Regalia", "iLvl 86", "Rare", "Finished rarity", "Success means", "All 3", "P1", "P2", "P3", "S1", "S2", "S3", "T1 (best)", "No prefix requirement", "No suffix requirement", "MODIFIER POOL - EDITING GOAL", "ODDS", "1.64%", "Chaos"
Color palette: #14110d #1d1a14 #211d16 #3a3327 #c8b88f #8a7d5e #e8dcae #af6025; prefix #78d6c4; suffix #d8a7f2; rare #ffff77
Constraints: preserve existing Calculator information hierarchy; goal frame must visibly reuse the input item component; restrained 1px borders and 3px radii; dense readable typography; no gradients, glow, glass, fantasy ornament, currency art, oversized KPI cards, browser frame, logos beyond existing poecraft wordmark, or watermark
```

## Variant B - Quiet target frame

```text
Use case: ui-mockup
Asset type: desktop web-app implementation reference
Primary request: show the existing poecraft Calculator with Goal item using the same item-frame UI as Input item, but with a quieter target treatment and controls in a compact footer; change only the Goal requirements card
Input images: Image 1 current Calculator layout reference; Image 2 approved item-frame style reference; Image 3 populated Calculator hierarchy reference
Style/medium: realistic shippable product UI, dense Path of Exile crafting tool, not concept art
Composition/framing: flat 16:9 desktop Calculator tab; top mechanic/action bands unchanged; left stacked Input item and Goal item; wide Modifier Pool center; Odds right
Goal item: Vaal Regalia, iLvl 86, Rare, tiny GOAL TARGET cue in the item header; the same fixed P1-P3 and S1-S3 ledger, rails, dividers, and typography as Input item; occupied target rows use a subtle bullseye marker, realistic modifier text, T1 or better badge, and a small remove icon; unrequired rows use the same empty-row rhythm and say No requirement
Controls: compact footer inside the Goal card with Rare and All 3 selectors; no separate form card above the item
Text (verbatim where legible): "INPUT ITEM", "GOAL ITEM", "GOAL TARGET", "Vaal Regalia", "iLvl 86", "Rare", "All 3", "P1", "P2", "P3", "S1", "S2", "S3", "T1 or better", "No requirement", "MODIFIER POOL - EDITING GOAL", "ODDS", "1.64%", "after Chaos"
Color palette: #14110d #1d1a14 #211d16 #3a3327 #c8b88f #8a7d5e #e8dcae #af6025; prefix #78d6c4; suffix #d8a7f2; rare #ffff77
Constraints: preserve existing Calculator information hierarchy; goal frame must visibly reuse the input item component; target cue is restrained and cannot tint the whole card; 1px borders and 3px radii; dense readable typography; no gradients, glow, glass, fantasy ornament, currency art, oversized KPI cards, browser frame, logos beyond existing poecraft wordmark, or watermark
```
