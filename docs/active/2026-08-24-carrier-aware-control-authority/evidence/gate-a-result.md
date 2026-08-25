# Gate A — Current-Semantics Control Baselines

**Status: passed (2026-08-24).**

The committed three-case corpus validates and every restored-Gate-1 native
case meets its declared bounded-policy contract. Each published graph completes
independent exact evaluation with complete pricing, success probability `1`,
and off-policy mass `0`.

| Control | Result | Proper upper | Graph | Wall |
| --- | --- | ---: | ---: | ---: |
| Warlord exact-terminal | `refused_resource_cap` with a proper compiled incumbent | `307.556312036793` | 21 / 45 | `29939.5031` ms |
| Non-armour partial-five, 10-second request | `requested_bounded_finish` | `6026985788.49406` | 154 / 387 | `58808.6924` ms |
| New tri-elemental Bow | `numerical_stability` bounded publication | `79273.3250308337` | 83 / 239 | `43599.1833` ms |

The Warlord lower remains `212.3`; the 1 GiB stop is expected and exact
closure is not claimed. Its compiled policy retains
`influence_exalt:warlord` and uses Alteration, Annul, Regal, Scour, and
Transmute.

The non-armour result is authority only for the committed 10-second requested
finish. It reproduces the same upper as the predecessor's clean Gate 1 run.

The tri-elemental case is new authority rather than a reconstructed historical
artifact. Its clean rare Spine Bow requests exactly
`LocalAddedFireDamageTwoHand10`, `LocalAddedColdDamageTwoHand10`, and
`LocalAddedLightningDamageTwoHand10`, with economic Restart explicitly in
scope. The solver reaches a stable bounded publication before the 60-second
host request fires. Its product graph uses `product_safe_restart` defaults and
retains the executable-policy recovery at the same 83-node / 239-edge census
documented for the lost historical request.

Evidence:

- `gate-a-warlord.json`
- `gate-a-non-armour.json`
- `gate-a-tri-elemental.json`

No simulator verification, release WASM, web tests, rendered review, or full
repository pipeline was run.
