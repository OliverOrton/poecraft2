# Weight calculation

**Status: implemented arithmetic reference.** This page defines how an
already-eligible modifier receives a selection weight.

Parent: [Engine](README.md)

Verified against code: 2026-07-19 @ d5e38e3

[Pools](pools.md) describes eligibility. Mechanic-specific choice and
sequencing belong in [Mechanics](../mechanics/README.md).

## Ordered selector rows

Each compiled mod row has ordered spawn-weight rows and ordered
generation-weight rows. For an effective tag signature, the engine scans each
list in artifact order and uses the first row whose selector tag is present.
There is no most-specific or largest-weight tie-break.

If no row matches:

- spawn weight falls back to `0`;
- generation percent falls back to `100`.

A zero or negative matched value is non-positive. A non-positive spawn value
excludes the row from every current weighted pool. A non-positive generation
value excludes it from normal and fossil pools but does not affect a Harvest
spawn-only pool.

## Effective tag signatures

The common signature is the selected base's compiled effective tag set. The
item's active generic influences add their selector tags to form uncommon
signatures. The session stores the common weight arrays eagerly; the action
context interns and caches uncommon tables lazily.

For every session mod and signature, a table records:

```text
spawn_weight
generation_pct
base_roll_weight
matched spawn/generation tag and row ordinal (uncommon tables)
positive_spawn_mask
positive_base_mask
```

The artifact's ordered rows remain authoritative; these tables are runtime
memoization, not rewritten data.

## Normal weight

For ordinary selection:

```text
base_roll_weight = floor(spawn_weight * generation_pct / 100)
```

The calculation is integer arithmetic. Only positive results enter the normal
positive-weight mask.

When prefix and suffix candidates are both allowed they share one weighted
pool. The chance of selecting either side is therefore the sum of that side's
candidate weights divided by the combined total; there is no separate 50/50
side roll in the generic pool sampler.

## Harvest spawn-only weight

A Harvest-targeted pool first requires the requested classification tag, then
uses:

```text
final_weight = spawn_weight
```

Generation percent is reported as `100` for the pool row and does not affect
selection. Classification tags determine targeting; ordered selector tags
still determine the active spawn weight.

## Fossil weight

Fossil selection begins with normal `base_roll_weight`. Each matching positive
or negative fossil classification rule is then applied in selected-fossil and
source-row order.

The multiplier uses a fixed scale of `1,000,000`:

```text
multiplier = 1,000,000
for each matching fossil rule:
    if rule value <= 0: multiplier = 0
    else: multiplier = floor(multiplier * rule_value / 100)

final_weight = floor(base_roll_weight * multiplier / 1,000,000)
```

Thus truncation happens after every matched percentage and once more when the
combined multiplier is applied. Overflow paths saturate the intermediate
value rather than wrapping.

For a selected fossil whose compiled `rolls_lucky` flag is set, the current
engine then applies:

```text
level_pct = 100 + max(required_level - 40, 0)
final_weight = floor(final_weight * level_pct / 100)
```

This implements the current Sanctified-fossil level weighting. Multiple
selected `rolls_lucky` entries apply the adjustment repeatedly.

The stored `PoolEntry.final_weight` is clamped to `uint32_t`. Pool totals and
cumulative prefix sums are `uint64_t`.

## Sampling

Accepted candidates are iterated in ascending dense session-mod-ID order.
The engine appends each positive final weight to a cumulative `uint64_t`
prefix-sum array, draws `roll = rng.next_below(total_weight)`, and chooses the
first cumulative value greater than `roll` with `lower_bound(roll + 1)`.

This gives each candidate probability `final_weight / total_weight`. Empty or
zero-total pools fail without sampling. There are no alias tables in the
current engine.

## Direct weighted choices

Some mechanics select from a direct lookup set rather than the generic pool.
Those paths still use the same ordered spawn-row interpretation and cumulative
integer sampling unless their mechanic specifies a uniform or deterministic
choice. The mechanic reference, not this arithmetic page, owns which path an
action uses.

## Debugging

Pool-debug rows expose the active spawn and generation values, matched row
identity, whether generation was applied, the fossil special multiplier,
final weight, side totals, and combined total. This is the supported way to
explain a concrete result; UI display-family tier grouping has no effect on
the calculation.

## Current boundaries

- The engine computes structural modifier odds, not numeric stat-roll
  distributions.
- Weight tables are cached per action context and effective tag signature.
- No prewarmed signature inventory or alias sampler is implemented.
- Economy prices, strategy value, and craft costs do not alter modifier
  selection weights.

## Invariants

- Ordered weight rows are never sorted or treated as additive.
- Normal and fossil pools require a positive integer
  `floor(spawn * generation / 100)` result.
- Harvest targeting never applies generation multipliers.
- Fossil multiplier truncation order is deterministic.
- Sampling uses 64-bit totals and never selects a zero-weight row.

Implementation entry points: `engine/src/session_builder.cpp`,
`engine/src/actions_basic.cpp`, and `engine/src/rng.cpp`.
