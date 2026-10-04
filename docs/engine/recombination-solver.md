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
## Pending sensitivity contract: no default first-side probability

This is an unimplemented, unapproved analysis contract. Neither v1/v2 nor the C ABI
accepts an ambiguity set. The earlier suggested 10% first-side value is withdrawn as
a default: observations do not establish it. Count normalization, count/selection
stage, taxonomy and missing-weight policies remain separately identified assumptions.
The current source/qualification receipt owns their pending approval.

Freeze the candidate count-first kernel, approved positive selection proxies, full
canonical groups, at-most-one-exclusive output rule, carrier/session mappings and
complete economic inputs. For each physical pair and carrier c, enumerate two full
output kernels K(c,P) and K(c,S), filling prefixes or suffixes first. Both keep every
positive output and already drawn requested count. No desired-only state quotient.
Use the existing shared observer to evaluate each goal and their union separately.

For an unknown prefix-first chance alpha(c) in [0,1], the pair transition row is

    P(alpha) = .5 * [alpha(A) K(A,P) + (1-alpha(A)) K(A,S)]
             + .5 * [alpha(B) K(B,P) + (1-alpha(B)) K(B,S)].

Thus one-attempt probability of an event is affine in both parameters; its extrema
are attained at the four carrier/order vertices. This gives an interval **within
this declared kernel family**, not a bound on hidden game mechanics. It covers
carrier-dependent first-side chances, not unknown alternative count laws, weights,
count-dependent side priority or unresolved origins. Do not infer a narrower interval
from the sparse survival observations. A narrower band requires explicit support or
owner-selected assumptions. Keep structural goal union/cooccurrence obligations.

For example, the existing equal-weight triple-prefix toy has success
`.015 + .16815*q`, where q is the exclusive-only suffix-side-first chance. The full
candidate family gives 1.5% to 18.315%; .1 supplies just one interior scenario. The
symmetric equal-weight P+exclusive / S+exclusive example gives 55.527775% under both
orders, so its one-attempt probability is stable over this particular order family.
Weights/categories/count model are still conditions of both statements.

### Repeated policies: samples are not certified cost bounds

For one fixed proper policy, expected cost solves (I-P(alpha)) J = c. J is generally
rational, not affine, in alpha. Evaluating endpoints, or endpoints plus .5, does not
bound all interior values. A two-state counterexample with cost one per step is:
state 0 goes to state 1 with probability q and to goal otherwise; state 1 returns to
state 0 with probability 1-q and goes to goal otherwise. Every constant-q policy is
proper; J(0) = (1+q)/(1-q+q*q). Values at q=0,.5,1 are 1,2,2, yet q=3/4 gives 28/13.
Report a finite scenario table as sampled sensitivity, with named configurations and
the **same policy/economic inputs**, not as a confidence interval or full-family bound.
If separately optimized policies differ, report the reversal instead of averaging
policies or calling their separate minima one executable plan.

A possible robust extension allows the row uncertainty independently at every
state/action/carrier (rectangular ambiguity). This is conservative relative to one
fixed global q and also covers an unknown state-dependent order rule within the
candidate family. For the fixed policy, finite nonnegative h and U, both zero on
goals, must satisfy for every allowed vertex row e at every nonterminal state s:

    h(s) >= 1 + sum_t P_e(s,t) h(t)
    U(s) >= c(s,policy(s)) + sum_t P_e(s,t) U(t).

The h inequalities telescope up to the stopped goal time, giving expected steps
at most h(s); finite h establishes almost-sure termination even with changing
allowed row choices. The U inequalities likewise bound accumulated nonnegative
cost by U(s). Since the one-step expressions are affine in the row, inequalities
for all vertices cover their convex hull. This is a sufficient robust properness
and cost certificate for the declared family; neither existing floating residual
checks nor a scenario table automatically supplies it. A numerical implementation
needs a distinct verified-inequality check with conservative rounding/error bounds
before publishing U as an upper bound. Otherwise publish a numerical estimate only.

A future search can reuse the bounded acquisition/recombine/discard item graph and
propose policies minimizing verified U among the retained candidates. This is upper
proposal ordering, not a global robust-optimality certificate. Native caps, complete
branch discovery and cancellation still apply; a missing h/U certificate reports
unresolved robust evaluation instead of a finite guarantee.

In the same counterexample, allowing q to vary by state lets nature pick q=1 in
state 0 and q=0 in state 1, forming a non-goal cycle. A global-q scenario table misses
this; the h test cannot pass. A robust policy must have a suitable fallback, or return
no finite robust recommendation. A completely declared goal-reaching feeder acquisition
can be such a fallback, within its caller-declared cost model, not a native feeder
certificate. For example, U(blocking policy) below that constant feeder cost proves
conditional dominance. Lower worst-case cost than another policy is not itself proof
of dominance in every model; do not merge these result authorities.

### Output and activation boundary

Future result fields must distinguish named scenario values, one-attempt interval
scope, robust properness/cost evidence, and inconclusive/model-sensitive comparisons.
Pin data/projection/count/conflict/taxonomy/weight/ambiguity identities, costs, caps and
all positive branches. Unknown prices, weights or classes remain explicit exclusions.
Unknown zero weights get no silent 1,000 default; approved finite weight scenarios
can be compared separately without claiming their endpoints bound all weights.

There is no probability distribution to sample from an ambiguity set. Advanced Apply
would require an explicitly selected, versioned point law; robust reports must not
silently select 10%, 50%, an adversarial endpoint or an average. Existing v1/v2 Apply
and recorded receipt replay remain the declared authorized model. No new selected-mod
mode, broader solver admission, frontend mechanics or experiment supervisor follows.
This mathematical contract is source-only and does not grant heavy work or activation.
