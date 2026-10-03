# Current Random recombination: native estimated model

Model `poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1`, projection
`structural-output-preserve-tier-roll-v1`, pair API version 1. Oliver approved
spawn-weight-proportional provisional selection and, on October 3 2026, preserving
selected tiers/recorded rolls without unverified upgrade bonuses. These are
explicit approximations. Enumeration computes this model; it does not certify
hidden game odds or global crafting optimality.

Native source is [recombination.hpp](../../engine/src/recombination.hpp),
[recombination.cpp](../../engine/src/recombination.cpp) and the versioned
[C API](../../engine/include/poecraft/recombination.h). The
[living receipt](../active/2026-10-03-random-recombination/README.md) separates native
qualification from held joint Calculator, WASM and browser work.

## Structural law and selection

Inputs are two distinct, live rare ordinary equipment resources, with compatible
immutable data identity and the same item class. Different compatible bases and
levels are accepted. This first scope excludes jewel/cluster capacities, special
or fractured explicit modifiers, generic influence, corruption, mirroring and
foresight. Ordinary explicit identity is established from canonical native flags,
special/domain/influence metadata and positive source base spawn weight. No
special-weight or exceptional joint-law rule is inferred.

Choose carrier A or B with probability 1/2. Its represented quality, memory,
item flags, sockets/links, implicit/enchantment slots and Eldritch properties stay
with it. Output is rare and has item level
`min(floor((levelA + levelB)/2) + 2, max(levelA, levelB))`. Represented levels are
integers from 1 to 100. A carrier-specific fresh session represents the output
base and level. Its retained union includes incoming canonical explicit IDs and
carrier-owned slot IDs; normal required-level roll filtering never discards a
selected transferred modifier. Retained-only ordinary tiers have transfer reach
and share the ordinary display family signature, while remaining outside normal
roll masks. Existing guaranteed Essence catalogue entries above the output level
receive that same transfer identity.

Each side counts physical input occurrences before applying target-carrier
eligibility. Duplicate occurrences count separately. Adopted coefficients are
integer thousandths, without normalization or silently changed precision:

| Pooled side | Keep 0 | Keep 1 | Keep 2 | Keep 3 |
| ---: | ---: | ---: | ---: | ---: |
| 0 | 1000 | 0 | 0 | 0 |
| 1 | 410 | 590 | 0 | 0 |
| 2 | 0 | 667 | 333 | 0 |
| 3 | 0 | 400 | 500 | 100 |
| 4 | 0 | 100 | 600 | 300 |
| 5 | 0 | 0 | 430 | 570 |
| 6 | 0 | 0 | 300 | 700 |

After the count draw, select sequentially from positive output-carrier base
spawn-weight proxies. These are spawn weights, not ordinary generation-percent,
fossil, price or solver ordering weights. Ignore normal required level for retained
selection. After each selection remove the selected canonical identity and every
candidate sharing any full exclusion group. If the pool exhausts, retain fewer
modifiers than the requested count. Distinct input occurrences of the same
canonical modifier remain distinct until that conflict removal, including their
recorded numerical payloads.

For each requested count, sequential conditional weights sum to one until the
requested number is reached or the pool is empty. Collecting all draw orders for
each unordered occurrence set therefore preserves that count row's mass. Count
rows each sum to 1000; a side distribution sums to one. On supported ordinary
pairs, prefix/suffix selection streams factor conditionally on the carrier;
carrier-conditioned products multiplied by 1/2 preserve total mass one. Full
cross-side group overlaps refuse because their order law is unresolved. The
exceptional `1p0s + 0p1s` input pair refuses in both orientations.

Selected slots preserve canonical tier, flags and recorded rolls. No numerical
reroll or tier-upgrade bonus is invented. Rolled-stat-total and defence-percentile
goals are excluded from this model's declared Calculator projection scope.

## Transactions and consumer authority

The pair handle snapshots both input states, identities and sessions without
mutating resources or RNG. Calculation returns carrier identity, canonical
selected occurrences, requested counts and full native structural output items.
Every outcome must be observed in that carrier's output session; receiver-only
dense IDs are not a valid mapping for a mixed-base pair.

Apply atomically consumes both pinned inputs and returns one new output identity,
its own independently owned session handle, and three before/after resource
effects. Distinct input addresses/identities, stale snapshots, replay and output
identity collisions are checked against the supplied inventory. Failure preserves
resources, RNG and the result buffer. Stored receipts retain complete output and
session identity; Undo restores stored inputs and Redo commits the stored output
without resampling. The caller owns history, acquisition and inventory budgets.

Gold and dust costs are unknown. JSON returns `null` and `cost_complete:false`;
typed Apply returns both completeness flags false. An empty consumed currency-key
list carries no zero-cost or pricing-completeness authority. Builder owns feeder
execution, subgoals/ports, priced acquisition, retries/recycling and budgets.
No selected-mod mode, dedicated recombination solver, strategy registry entry,
Current/Finder action scope or exact closure is introduced.

## Qualification boundary

Native public pair/API/transaction checks pass with the pinned frozen artifact;
the Python adapter delegates all mechanics to the C API. Shared goal schema and
Calculator UI belong to the multi-goal owner. Their mixed-carrier dispatcher and
native finalizer must bind each conditional stream to its actual output session,
combine goal/union membership using carrier mass rather than independent events,
and retain carrier identity in sparse rows. Matching WASM, web transport and
TypeScript qualification remain required before browser activation.
