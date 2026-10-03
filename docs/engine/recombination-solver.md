# Bounded native random recombination inventory solver

The versioned [C API](../../engine/include/poecraft/recombination_solver.h) and
[native implementation](../../engine/src/recombination_solver.cpp) propose and
evaluate policies under the existing estimated random pair model. This is
dedicated recombination search, not admission to Current/Finder or a global
crafting-optimality certificate. [Programme receipt](../active/2026-10-03-recombination-solver/README.md)
owns qualification status and mechanics gaps.

## Exact specifications and acquisition boundary

One request pins a native session, selected base/level, shared structural goal set,
data artifact, price identity and declared acquisition/attempt costs. Items retain
every represented field, canonical explicit identity, flags, recorded rolls,
implicit/enchantment payload, sockets, quality and memory. State interning compares
these meaningful fields; it ignores padding and inactive arrays. It never projects
items to desired-mod masks. Dense output IDs are mapped by canonical identity back
into the pinned session; output base and level must still equal the selected scope.

The inventory has at most two physical items. Duplicate spec IDs represent two
distinct occurrences, not one aliased resource. Acquiring adds an item, recombining
consumes both and creates one complete native outcome, and discarding removes one.
Native rarity capacity and full-group structural validation also apply to acquisitions, including terminal finished items. Native pair enumeration supplies every positive outcome. A retained failure is
available for the next attempt; no market salvage credit or free fresh input is
invented. Discovery caps reject the request rather than discard branches.

A catalogue entry quotes the expected complete cost of obtaining one exact item:
purchase, or a completed crafting feeder. Base, setup, retries, failures and cleanup
are included. A finished multimod item can therefore compete with recombination,
even when its uncertain recombination classification prevents using it as a pair
input. These are **caller declarations**, not native feeder certificates. A quote
identity does not verify a feeder law. Builder owns qualified feeder invocation,
actual produced item/session, typed A/B ports, paid attempts and inventory budgets.

The attempt quote must explicitly include gold/dust. Native game quantities remain
unknown; this API refuses incomplete economic inputs. A complete declared cost
model is sufficient to evaluate that model, not to claim verified game prices.
Initial item costs are counted once as entry costs. Subsequent acquisition counts,
recombination attempts and discards are separately returned for an audit.

## Terminal observation and finite policy argument

The existing bounded shared-goal parser and terminal observer decide success in
the pinned session. Structural goals do not observe recorded totals, defence
percentiles, memory, sockets or enchantments. Goal observation clears only memory
and enchantments on a copy; the physical state retains them. Unsupported goal fields
refuse through the same projection validator as the pair Calculator.

Let W initially contain all discovered inventory states. Within W keep only actions
whose every positive successor stays in W. Remove states that cannot reach a goal
through those actions; repeat to stability. This greatest reachability construction
retains the finite states from which an almost-sure goal-reaching policy can be
constructed. A branch into a losing state is never erased from a probability row.

Assign terminal rank zero and successive ranks to states having an admissible
action with a positive edge to an already ranked state. Choose such an action.
Every closed recurrent nonterminal class would have a minimum-ranked state and an
edge to a strictly lower rank outside that class, a contradiction. The constructed
finite policy is therefore proper. The implementation additionally checks that
every winning policy state can reach a terminal state under the selected rows.

For that proper policy the nonterminal matrix P is transient, so costs and resource
counts satisfy X = R + PX. One pivoted solve of (I-P) evaluates all counters, with
finite/nonnegative/pivot and residual checks. Strict policy improvements are
proposals; they are checked for properness and evaluated again. A policy-iteration
cap can return a proper evaluated proposal with search_converged false. Discovery,
numerical-work and unresolved-pivot failures return no spurious finite result.

No claim transfers from this finite, two-slot, catalogue-limited estimated model to
the full game or general crafting SSP. The native source has bounded work and a
cancellation callback; its C API uses the bounded synchronous work envelope.

## Held semantics and activation

The v1 ordinary count/weight/tier/roll approximation remains pinned. Magic and
normal ordinary inputs are admitted; output remains rare. A separate model ID,
`poe1-random-spawn-proxy-native-constraints-preserve-tier-roll-no-upgrade-v2`,
admits a bounded extension: at most one known exclusive physical occurrence,
with a positive proxy on both possible carriers, and ordinary natural tiers
available through a native guaranteed Essence source even when non-native to
their input base. The adopted physical-count-before-filter coefficients remain
unchanged. With no second exclusive and no cross-side group overlap, neither
case needs an exclusive-dependent side-order assumption. Their game probabilities
remain estimated. Solver callers must select v2 explicitly; v1 excludes v2 pairs.

The [constraint inspector](../../engine/src/recombination_constraints.cpp) reports
metadata-backed origins, tri-state exclusivity, physical counts, full-group/output
conflicts and carrier-specific native spawn proxies. It does not supply a probability
law. Essence-only, metamod, Delve, unveiled/veil template, canonical elevated relations
and the four beast Aspect mod types identify known exclusive origins. Generic crafted
rows are unresolved, not automatically exclusive. Incursion/Breach have no authoritative
origin tag in the frozen runtime metadata and remain unresolved. A zero special proxy
means missing selection weight authority, not certain ineligibility. A positive proxy
is separate from ordinary natural eligibility.

Multiple exclusive occurrences, uncertain crafted classes, fractures, generic influence
and the exceptional 1p0s + 0p1s joint law remain explicit pair exclusions. The known
at-most-one-exclusive output constraint cannot determine current padding counts or
which side selects first. Do not apply the old 3.25 multimod padding law to these pools.

The source is unqualified until the programme receipt says otherwise. No Builder
adapter, WASM export or public product activation is implied by this native API.
