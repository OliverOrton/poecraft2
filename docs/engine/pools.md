# Modifier pools

**Status: implemented technical contract.** This page defines how compiled
modifier data becomes a weighted candidate pool. It does not decide the
gameplay behavior of a craft.

Parent: [Engine](README.md)

Verified against code: 2026-07-19 @ d5e38e3

For action sequencing and mechanic exceptions, see
[Mechanics](../mechanics/README.md). For the exact arithmetic applied after
filtering, see [Weights](weights.md).

## Vocabulary

The engine keeps these concepts distinct:

| Concept | Meaning |
| --- | --- |
| Global mod row | One compiled modifier tier with a stable key |
| Session mod ID | Dense runtime ID valid only within one `pc_session` |
| Spawn/generation selector tags | Ordered tags used to choose weight rows |
| Classification tags | The mod's `implicit_tags`, used for Harvest, fossils, and attack/caster classification |
| Exclusivity group | All-group legality: one occupied group blocks every candidate sharing any group |
| Display family | Primary group + stat signature + side + acquisition source, used for tier display rather than legality |
| Candidate mask | Mods that survive the current rule filters |
| Weighted pool | Candidate rows plus cumulative weights used for sampling |

Catalog or mask membership never means that a modifier is globally
craftable. It means only that the modifier satisfies that particular data or
filter dimension.

## Global data and session compilation

The compiled artifact preserves ordered spawn weights, ordered generation
weights, classification tags, every exclusivity group, generation type,
required level, influence reachability, special-kind metadata, and stable
keys. It also contains direct mechanic tables for essences, fossils, bench
mods, veiled templates, and implicits.

Opening a session for a base and item level builds one dense mod universe. A
row can enter through ordinary base reachability, supported influence
reachability, or a supported special mechanic. Item-level and stable
base/class restrictions are applied during this build. Current item influence,
occupied groups, metamods, and action-specific targeting remain dynamic.

The session eagerly computes the base tag signature's weights and masks. An
action context interns uncommon signatures created by the item's generic
influences and computes their weight tables lazily.

## Implemented session masks

All masks use the same session-mod-ID universe:

- universe; prefix; suffix; base-explicit universe; ordinary random-roll
  candidates;
- crafted, essence-only, delve, implicit, veiled-template, unveiled, and
  generic-unveiled membership;
- corrupted, eldritch, Searing Exarch, and Eater implicit membership;
- positive spawn weight and positive normal base weight;
- masks for each exclusivity group, classification tag, and influence code.

Direct tables identify base implicits, bench mods, veiled prefix/suffix
templates, eldritch tier mods, fossil sell-price mods, guaranteed essence
mods, and fossil-added/forced mods. Not every useful relationship is a bitset.

## Candidate construction

Ordinary explicit selection starts with `normal_random_roll_mask`. Fossil
selection starts there and adds the selected fossils' added modifier IDs. The
builder then applies, in order:

1. An optional prefix or suffix filter.
2. Positive-weight membership for the effective tag signature. Normal and
   fossil pools require a positive spawn-and-generation result; Harvest
   targeting requires positive spawn weight only.
3. For a Harvest pool, the requested classification-tag mask.
4. Either one requested influence mask or the union of uninfluenced rows and
   the item's active generic influences.
5. Cannot Roll Attack/Caster classification blocks when the action respects
   them.
6. The union of every exclusivity-group mask occupied by the item or by mods
   already selected during the action.
7. Final padding-bit cleanup before iteration.

The generic shape is therefore:

```text
candidate = source mask
          & allowed side
          & positive weight
          & mechanic target, when any
          & allowed influence
          & ~applicable metamod blocks
          & ~occupied groups
```

The mechanic layer chooses the source, side, target, influence mode, and
whether metamod pool blocks apply. In particular, the implemented Essence and
Fossil outcome generation ignores all active metamods; that exception is an
action rule, not a different definition of tags or groups.

## Pool kinds

`PoolWeightKind` has three implemented values:

- `Normal`: ordered spawn weight multiplied by ordered generation percent.
- `HarvestSpawnOnly`: the requested classification tag is required and only
  spawn weight participates.
- `Fossil`: normal base weight, then the selected fossil multipliers and any
  `rolls_lucky` level adjustment.

Forced and guaranteed modifiers do not have to be sampled from these pools.
Mechanics can resolve them through direct session tables, add them, and then
include all their groups in the block mask before subsequent selections.

## Groups, tags, and family identity

A modifier may have multiple exclusivity groups. Pool legality uses the full
membership list and blocks a candidate if any of its groups is occupied. The
slot's cached primary group and the session's display family are diagnostic/UI
identities only.

Spawn/generation selectors and classification tags are separate data
channels. A classification such as attack, caster, life, or fire does not by
itself supply a spawn weight. Conversely, a selector row does not make the mod
part of a Harvest or fossil classification.

The canonical artifact contains `adds_tags`, but the current native
`DataImpl` does not load or apply runtime tag mutation from that field. Pools
use the session's effective base/influence tag signature.

## Caching and debugging

The action context owns reusable candidate, block, influence, and occupied
group scratch. Its main cache keys a completed pool by candidate mask, tag
signature, pool kind, target tag, and selected fossils. A refill cache also
keys the dynamic inputs used to derive that mask: occupied-group mask,
influence state, side, explicit influence target, attack/caster blocks, and
metamod-respect flag.

Each cache is bounded at 4,096 entries. Reaching the bound clears the pool and
refill caches; cached values never cross a context or session.

Native debug APIs can report accepted and rejected rows, active ordered weight
rows, the first failed filter, blocking group, special multiplier, totals,
and cache behavior. The WASM facade exposes the pool-debug result as JSON.

## Current boundaries

- The supported universe is for ordinary one-item crafting. Cluster-specific
  eligibility and two-item/recombinator transfer pools are not implemented.
- The engine has no separate runtime domain-mask inventory; the session
  compiler produces the concrete masks listed above.
- Dynamic item facts are scanned into context scratch rather than stored as
  permanent catalog-sized masks on every item.
- Pool construction answers eligibility and relative selection weight. It
  does not own mechanic sequencing, rarity changes, cost, or product policy.

## Invariants

- Every mask in a session has the same word length and zero padding.
- Every yielded ID is below `session.mod_count`.
- Normal random-roll membership excludes special-only and implicit outcomes.
- All exclusivity groups participate in blocking; display family never
  substitutes for group legality.
- Harvest-targeted pools use spawn-only weights.
- A cached pool is reused only when every input that can affect its candidates
  and weights matches.

Implementation entry points: `engine/src/session_builder.cpp`,
`engine/src/engine_internal.hpp`, `engine/src/actions_basic.cpp`, and the pool
debug functions in the public C ABI.
