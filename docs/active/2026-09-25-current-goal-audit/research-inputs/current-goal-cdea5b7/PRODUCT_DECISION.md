# Product semantics: recommendation, not an authorised default migration

## Recommendation

Expose what the user is asking for rather than making “exact calculation” imply “no extra explicit affixes.” A future goal interface should distinguish requested modifiers/tiers/rarity/threshold from extra-affix policy and optional prefix/suffix occupancy/open-slot constraints.

For a newly versioned goal/form, requested coverage with extras allowed is a reasonable proposed default for ordinary “obtain these mods” intent. A clear Clean preset can request no unmatched explicit affixes, and explicit counts/open slots can express intermediate crafting objectives. This is a product-design recommendation, not a measured statement about every user's preference.

The user's request selects an investigation, not automatic reinterpretation of existing saved requests. Keep existing v1 goals, persisted workspaces, frozen runs, tests and old graph acceptance on the current clean meaning until a deliberate migration/capability decision is approved.

## Vocabulary

Suggested UI distinctions:

- “Requested modifiers achieved” for the coverage event;
- “Complete target achieved” for the full declared constraints;
- “Extra explicit modifiers: Allowed / Not allowed”;
- explicit prefix/suffix count ranges or minimum open slots when specified.

“Exact probability” describes the calculation's model/accuracy, not the item shape. “Exact/optimal solution” describes proof status within a scope, not the extras policy. Do not label Current as always exact: it can produce a bounded result.

## Example

A4 current clean intent can be displayed as:

    all three requested prefix families at required tiers
    + the requested suffix at its tier
    + Rare
    + exactly 3 prefixes and 1 suffix

On the validated ordinary cap domain that means two open suffix slots. Those constraints are equivalent for that all-required goal; a generic “one open slot” toggle or fixed total count does not express all any-k cases.

## Migration requirements for a later selected product task

The August semantic switch occurred within v1, so old raw v1 requests are ambiguous without provenance. Do not automatically reinterpret a pre-boundary historical request as today's clean target during an audit.

Version the goal contract, advertise build support, round-trip it across native/WASM/worker/persistence/export, and reject unsupported fields/version rather than ignoring them. Present a saved legacy goal's meaning explicitly before conversion. Preserve old graph comparison evidence and do not rewrite its hash/history.

Build the normal form natively; the frontend edits native-declared constraints and displays results. Do not reconstruct target evaluation or PoE mechanics in JavaScript. Include the resolved terminal identity in strategy exports and request-bound checks.

## What this programme may change now

Correct the existing product reference's incomplete definition. A narrow resolved-label clarification can be qualified if its owner is already touched. The experimental L/E/R selector remains native-private. No default extras change, new general-purpose goal language, or public alpha feature is required to obtain the controlled Current results.
