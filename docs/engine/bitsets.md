# Engine bitsets

**Status: implemented storage reference.** Bitsets are an internal
representation for sets of dense session modifier IDs.

Parent: [Engine](README.md)

Verified against code: 2026-07-19 @ d5e38e3

The meanings of the concrete pool masks are documented in
[Pools](pools.md).

## Representation

Bit `k` represents session mod ID `k`. Word `i` is a `uint64_t` containing IDs
`64*i` through `64*i+63`. A mask for `N` mods occupies:

```text
ceil(N / 64) 64-bit words
```

The helpers in `engine/include/poecraft/bitset.h` operate on caller-owned word
spans. They do not allocate, retain, or know the session. This lets the same
operations work on vectors owned by immutable sessions and on reusable action
scratch.

Implemented operations are set, clear, test, zero, zero final-word padding,
population count, AND, OR, AND-NOT, and ascending set-bit iteration. Output
may alias an input for the word-wise operations.

`pc_bitset_for_each` is bounded by `word_count * 64`, not by the logical mod
count. Session builders zero padding, and callers that expose IDs also guard
`id < session.mod_count`.

## Physical mask inventory

`SessionImpl` currently stores:

- universe, prefix, suffix, base-explicit universe, and normal-random-roll;
- crafted, essence-only, implicit, delve, veiled-template, unveiled, and
  generic-unveiled;
- corrupted-implicit, eldritch-implicit, Searing Exarch, and Eater;
- positive spawn weight and positive normal base weight;
- one mask per exclusivity group, classification tag, and influence code.

These masks all have the session's word length. Empty sparse entries, such as
an absent tag or group ID, may be represented by an empty inner vector rather
than an allocated all-zero mask.

The base signature's positive-weight masks are stored in the session. Each
uncommon effective tag signature cached by an action context owns another
positive-spawn and positive-base pair.

## Dynamic scratch

`ActionContextImpl` owns reusable candidate, occupied-group block, and
influence masks. Item state is small enough that the context scans its live
explicit slots, collects all group IDs, and ORs the corresponding session
group masks. It then combines session masks with AND/OR/AND-NOT to build the
action candidate set.

There are no permanent catalog-sized `current_item`, removable, preserve, or
reroll masks inside `pc_item_state`. Those are either small slot lists or
ephemeral action facts.

## Tables beside masks

Bitsets answer membership. Dense and sparse tables carry values or direct
identities that cannot be represented by one bit:

- ordered spawn/generation results and base roll weights;
- full per-mod exclusivity-group and classification-tag lists;
- stable/global ID mappings, generation side, required level, influence, and
  family/tier identity;
- direct essence, fossil, bench, veiled, and implicit modifier IDs;
- weighted-pool entries and cumulative sums.

A set bit does not contain a weight and does not mean that the mod is legal
for every action.

## Public inspection

Native debug APIs can dump selected masks and describe the filtering of
individual pool rows. The WASM facade returns higher-level JSON summaries and
pool-debug rows rather than exposing raw bitset memory to TypeScript.

## Current boundaries

- No recombinator, two-item transfer, cluster-specific, or general domain
  masks exist in the current runtime inventory.
- Mask memory is ordinary C++ allocation inside session/context objects; it is
  not a stable serialized artifact format or a public ABI layout.
- The engine uses direct cumulative weighted pools, not bitset-rank or alias
  sampling.

## Invariants

- Every allocated session mask uses `session.words` words.
- Padding bits above `session.mod_count` remain zero or are explicitly ignored
  before an ID escapes.
- Group legality uses the union of every occupied exclusivity group.
- Action scratch can be discarded and rebuilt without changing item truth.
- Dense IDs and their bits are valid only for the owning session.

Implementation entry points: `engine/include/poecraft/bitset.h`,
`engine/src/bitset.cpp`, `engine/src/engine_internal.hpp`, and
`engine/src/session_builder.cpp`.
