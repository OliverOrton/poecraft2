# Existing-owner design: strict preparation

## 1. Concrete path being measured

`PolicyExactLiftWork` / `PersistentQuotientSession`
→ `lift_policy_quotient_pass_task`
→ `ProductionPolicyOracle::initialize_cooperatively`
→ coarse discovery / operator import / observation fixed point
→ strict selected-locator closure
→ cached observation/exact partition-node construction
→ unchanged partition and Bellman/evaluation/publication owners.

Do not use the final graph's 811 nodes as the size of every intermediate input.
D1 has 8,453 serviced locators, 16,863 strict exact states and 893 coarse IDs in
one measured population. Those counts name different scopes. [R3–R5]

## 2. Instrumentation contract

Instrument synchronous calls at the caller and accumulate time around actual child
`resume()` calls. A coroutine scope spanning a suspension is elapsed task lifetime,
not active work. Keep inclusive parent counters and exclusive child counters named;
retain an explicit remainder. The legacy discovery timer remains historical and
backward-compatible unless an additive field explains it. A later persistent pass
cannot be counted by summing cumulative elapsed snapshots.

For observation preparation count input nodes/edges, distinct full selected
payloads, distinct full successor vectors, propagation groups, rounds, transfer
calls, unique transfer inputs, requirement union/canonicalization calls, canonical
requirement cardinality and retained bytes. For partition preparation count complete
(selected-row payload, alternative-descriptor payload, observation mode, generation)
combinations and state-specific materialization/key emission separately.

Track full-inventory helper calls and elements visited only when needed; keep
ordinary execution free of per-element timers. No JSON while the timed computation
runs. Aggregates and a few deterministic collision-checked witnesses suffice.
Check the profiler's overhead against the same fixed logical input. Do not raise
caps to accommodate it or treat a partial diagnostic as a population census.

## 3. Branch A: consume existing observation sharing

The propagation API already permits a node to name a one-level source for its
selected action and successor list. Its native validator rejects absent or chained
sources. Another refinement graph producer uses this interface. The oracle's
coarse-policy producer currently copies the payloads and leaves the authority
fields unset. [R7–R10]

A first implementation can construct deterministic source IDs only after full
payload correspondence. Preserve the original node/state IDs in every assignment.
A source must be an owning node in the same frozen input, not an ID from another
calculator or a pointer to a moved object. Reuse existing interned selected-action
identities and checked runtime/observation descriptors; do not recreate an incomplete
key from just action names, costs or goal masks.

Group computation only if the selected action semantics used by the transfer,
initial/direct requirements, routing observations and successor dependencies agree.
Sharing just action storage does not establish shared propagation. Equal successor
requirements in one iteration are not equality for all future iterations without
a separate invariant. Each instance's output mapping remains intact.

A safer narrower option is memoizing the pure transfer
`selected_preserved_requirement(requirement, selected)` within a fixed epoch or
invocation. Key it by its **complete actual inputs**, including runtime path/control
contracts and canonical downstream requirement. This does not need equal successor
lists, state values or probabilities. Preserve exception/refusal semantics and
retain output storage in the correct owner. Measure key construction, lookup and
copy costs—memoizing a cheaper operation with a more expensive key is not a win.

Do not change selected operators, probabilities, policy values or allowed actions.
A new worklist/SCC scheduler is not necessary for this branch. Prefer grouped or
memoized synchronous rounds with the original round/cancellation semantics.

## 4. Branch B: prepare one requirement; observe many distinct states

The partition builder already interns raw-row payloads and alternative descriptor
vectors. Its `replay_node` unions the routing requirements for these dependencies
per locator, then calls `canonical_observation_identity`, which canonicalizes the
requirement and observes/canonicalizes the actual feature signature. [R5, R8]

A prepared requirement may be keyed by the **content-valid identities** of the
selected-row payload vector, alternative descriptor vector, include-observations
mode, runtime contract generation and relevant native observation semantics.
The disabled-observation and enabled-observation variants are different keys.
The value of this key is the canonical requirement (and, if proven useful, a small
prepared selector plan), not a complete state-specific observation identity.

Apply it to the actual state features and coarse parent separately each time.
Differences in tags, required level, literal members, crafted/fractured state,
counts, checkpoints or offers still produce different observations and partition
keys. A below-tier member cannot borrow a satisfied member's observation. Unknown
members retain the existing refusal/refinement behavior.

Do not cache a partition node wholesale using a requirement ID: selected semantic
keys, costs, actual transitions and observation features have separate dependencies.
Do not weaken all-action partition observations to the selected-only subset to
make a smaller key. The existing final-policy compilation deliberately chooses
selected observations later; that is a different, already-owned boundary.

## 5. Branch C: remove a measured quadratic inventory walk

Two current helpers walk growing or shrinking vectors. An exact owner-local
inventory can retain:

    bytes = container_capacity * sizeof(element) + sum(live_nested_charge_i).

On append, charge actual capacity growth plus the appended nested storage. On
move/replace, remove the old charge and add the new charge at the actual ownership
transfer. Parent vectors and moved-to destinations retain their own charges.
When a vector can reallocate, preserve the existing transient projection; cached
net live size is not the allocation peak. Saturation cannot be “undone” by blindly
subtracting from a saturated sentinel—reconcile or use a checked representation.

Full scans remain available at stable debug/test boundaries and before accepting
ambiguous cap decisions. The inventory must include the same fields the authority
intends to own, not merely match a known undercount. In the inspected oracle input,
direct_observes is empty; generalizing that helper to nonempty direct data requires
including its ownership too, not assuming the current special case universally.

The oracle already audits on a 512-check cadence and caches its non-child estimate.
Do not replace that owner with another global byte ledger. This branch targets the
identified repeated explicit vector scans, not every `estimated_owned_bytes` call.

## 6. Memory, resumption and interface limits

A4's recorded selected peak is approximately 925 MB against 1 GiB. New keys and
shared structures must fit concurrently with the parent, raw rows, oracle,
partition states/nodes and checker. Count owners, not pointer references; preserve
in-flight memory until the last real owner releases it.

Prefer invocation-scoped immutable objects, no cross-run cache. An existing
certificate for an unchanged whole controller remains valid only under its full
original context; sharing observation computation is not certificate transfer.
No public `pc_solve_progress` layout, grammar, global state interning or original
resource-price change is selected. Keep oracle implementation in its established
translation unit. Do not move hot functions or restore stronger LTO as part of
this treatment.
