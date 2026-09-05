# Native Joint-Goal Lower Refinement v1

**Status: completed; private complete-envelope gain accepted.** Reviewed main:
`65f3d5dbb23e3f1cb01fbb2124d7414cd7e6ce8b`; the initial checkout matched and
was clean. Parent: [archive](../README.md). Reuses the
[probabilistic lower repair](../2026-09-05-native-probabilistic-lower-repair-v1/README.md).

The existing native probability model now uses conditional-history joint caps,
reopens limiting computational price floors and consumes its full-scope source
lower across the complete action/family envelope. Both saved sources improve
from **36.42861718910441 to 39.209999996079** in the private complete model and
independent portfolio. Ordinary solving and public bounds remain unchanged;
no end-to-end solver quality or speedup is claimed.

## Retained implementation

- `solver_phase_probability.cpp::prepare_probabilistic` adds same-side,
  per-exact-mask joint bounds to the existing finite projection and quotient.
  `phase_joint_assignment_upper` reuses the older permutation/placement idea
  with the current native conditional integer witnesses.
- `phase_minimum_event_allocation` factors the existing independent-cap greedy
  minimizer for direct testing. Final records retain every event capacity and
  its frozen minimum value, allowing independent verification of minimization.
- `PhaseRelationReason` separates candidate price shortcuts, unsupported
  effects, native domain exits, fixed boundaries, native effects and probability
  envelopes. Minimum-owning shortcuts reactivate, including equality after the
  quotient's conservative numerical repair.
- `PreparedPhasePotential::whole_scope_source_lower` issues an exact-source,
  identity-bound lower through the existing quotient evidence type. Its private
  constructor covers the whole declared caller scope; a restricted numerical
  certificate or program-only result cannot use this issuer.
- `compose_impl` evaluates both old and new potentials over the same native
  integer exit stream. Shared immutable draw evidence avoids rebuilding the
  78 pool witnesses. No new graph, solver, cache service or public option exists.

## Native history proof and probability result

The frame is unchanged: the same immutable session/base/item level/goals,
prices and caller scope; exact naturally fractured suppression modifier 28
(goal mask 16); rarity/current exact goal mask/prefix occupancy/suffix occupancy.
All Eldritch phases and ordinary hidden modifier exclusions are covered.
Generic influence, metamods, extra fractures, veiled modifiers and item flags
remain outside. The existing caller excludes Imprint; authored or enabled
restore/checkpoint scope refuses this view. The old strict Eldritch guard stays.

For each exact-mask group, remove retained and potentially forced goals.
A joint shortcut requires same-side goal events that no native modifier can
satisfy together. The producer checks native hit masks; current goal-layout
construction independently refuses overlapping members. Unknown overlap uses
the old conservative relation, never an unjustified distinct-draw zero.

Native `preserved_reforge_base` clears unprotected, non-fractured affixes.
Direct Essence/Fossil modifiers precede random draws. Ordinary redraws therefore
permit depth bounds using the initial fractured occupancy plus the number of
earlier same-side draws. Forced cases instead use the worst possible same-side
occupancy at every position and omit potentially forced goals from the joint
event. Other-side blockers are bounded by three throughout, covering arbitrary
interleaving. Natural and guaranteed Harvest pool bounds are maximized at
each position; no cross-side product or independence assumption is used.

The existing native integer witness keeps full target weight N and lower-bounds
other surviving weight by max(0, B-D). Its ratio N/(N+max(0,B-D)) holds over
every allowed preceding exclusion history, including target-weight deletion
and overlapping blocker effects. Item tag signatures depend on immutable base
tags and generic influence; covered ordinary draws cannot change those inputs.
Metamod-producing pools retain their domain escape.

For the measured three-prefix event, each target has N=1,000, B=41,500,
maximum earlier prefix-blocker removal 13,000 and zero suffix-blocker removal.
The successive conditional bounds are 2/85, 2/59 and 2/33. Chain rule followed
by union over six distinct-position assignments yields:

```text
P(all three new prefixes) <= 6 * (2/85) * (2/59) * (2/33)
                         = 16/55165
                         ~= 0.0002900389739871295
```

| Capacity | Previous | Joint treatment |
| --- | ---: | ---: |
| Exact source-derived upper | 2/11 | 16/55165 |
| Upward 2^-24 capacity | 3,050,403 / 2^24 | 4,867 / 2^24 |
| Applied probability upper | 0.1818181872367859 | 0.00029009580612182617 |

The dyadic allowance is about 626.752 times smaller. This is tightening of a
proved upper bound, not a measured native event probability or a donor forecast.

Caps describe applied redraws. Failed/no-op behavior may preserve all current
goals, so every redraw outcome also offers the current source as an optimistic
observed choice. It does not count a retained source goal as a fresh draw.
Pool exhaustion and early stopping remain covered. Tiny positive products
retain positive upward mass even in generic underflow fixtures.

Only independent exact-mask capacities are tightened. Multiple masks may still
spend the same subset-event allowance independently; no aggregate cut or new
optimizer was added. The greedy minimum follows the exchange argument: mass
can move from a dearer event into a cheaper unsaturated event without increasing
expectation. A merely feasible distribution is insufficient. Every affected
row is reoptimized and checked for the final frozen vector before authority.

## Computational floors and final obstruction

The initial treatment exposed two discovery plateaus, both repaired:

1. A feasible smaller candidate could be accepted immediately after reopening
   its price cap. The builder now solves the changed model and continues if
   that model can raise a represented value.
2. Comparing a price only with the quotient's globally shrunken candidate could
   hide a tied floor. Reactivation now compares against the actual minimum
   action RHS. Scheduling tolerance grants no acceptance authority.

The final construction uses 15 bounded rounds and reopens 496 source/action
shortcuts. It does not eagerly evaluate every expensive action. The formerly
recorded 19.7247 Fossil floor is not treated as a permanent ceiling. Final
acceptance verifies that no minimum-owning candidate-price shortcut remains.

Both source values are now limited by `eldritch_chaos`, cost 39.21, with an
**unsupported-effect** continuation lower of zero. Its computational shortcut
was reopened and the detailed branch still lacks the side-retaining relation.
This is a genuine retained model boundary, not evidence that native continuation
cost is zero or that 39.21 is the native optimal action cost.

The exact finite-model ceiling is the stored price
`5518316918412411 / 140737488355328`. The accepted 39.209999996079 is slightly
below it because the unchanged quotient repairs proposals conservatively.
Native `do_eldritch_chaos` uses an ordinary reforge without dominance and
otherwise rerolls the dominant side while retaining the opposite side and
fracture. The minimum next information is a certified continuation relation
covering these phase branches. That is the one evidence-selected next action;
no further implementation is active. No aggregate event cut was needed.

## Complete-envelope and program comparison

The original source has three satisfied prefixes and fractured suppression.
The second is the saved distinct source with the first goal prefix removed.
Both use the same level-86 Conquest Lamellar session and economy.

| Quantity | Original source | Prefix-removed source |
| --- | ---: | ---: |
| Previous probability donor | 13.716999591201551 | 13.716999591201551 |
| Joint donor | 39.209999996079 | 39.209999996079 |
| Donor gain | 25.49300040487745 | 25.49300040487745 |
| Previous mandatory program | 17.26620674029301 | 17.389339591201463 |
| New mandatory program | 42.53036513076388 | 42.792528498149 |
| Raw program gain | 25.26415839047087 | 25.403188906947534 |
| Compatible program-action gain | 6.101747941659475 | 6.363911309044589 |
| Complete model before | 36.42861718910441 | 36.42861718910441 |
| Complete model after | 39.209999996079 | 39.209999996079 |
| Complete-model gain | 2.781382806974591 | 2.781382806974591 |
| Independent portfolio gain | 2.781382806974591 | 2.781382806974591 |

The mandatory Ichor/Exalt program still charges setup once. Each source visits
72 native modifier exits with total integer weight 55,700; true-goal weights
remain 500 and zero. Original failure values equal the source donor. Second
failure values range from 29.2049991267027 to 39.209999996079. The full weighted
sum is essential: replacing them with their minimum would yield only about
32.87734 for that program. Every exit and its certified cell value is retained.

The new full-scope value is evaluated at each exact source, then maximized with
existing compatible per-action and whole-family floors. It raises all remaining
common floors without requiring a separate family kernel. Source keys, caller
scope, prices and the native certificate remain bound; restricted evidence
cannot make this inference.

Both models keep 28 admitted descriptions, six native inapplicabilities and
all ten family identities. Imprint remains excluded by the unchanged caller;
Eldritch side intent has a complete finite set of three/six descriptions; the
eight other residual families remain open (IDs 1,2,3,4,5,7,9,10).

Original minimum ties: Annul, Scour, Chaos, Harvest Physical, suffix Eldritch
Annul/Chaos and all eight residuals. The second additionally ties Harvest
Defences and all three prefix Eldritch descriptions. The next higher complete
constraint is Harvest Defences at 39.23694999999999 for the original, and
Restart at 41.11443172876641 for the second. The complete tie lists are in
[reference.json](reference.json). None of these floors is labelled an actual
optimal action cost.

The predeclared local positive-gain and resource gates pass on both semantic
sources. Consumption remains the opt-in complete-envelope proof query.
Ordinary scheduler, action admission, incumbent, public lower and exactness
rules are unchanged; no whole solve was needed to establish this local gain.

## Resources and acceptance

| Final matched measurement | Result |
| --- | ---: |
| Existing original preparation | 4.2271676 s |
| Adapter/support preparation | 1.1470 ms |
| Matched marginal-control preparation/check | 113.4393 ms |
| Joint construction, refinement and checking | 1.3137967 s |
| Program query, original / second | 0.7858 / 0.7868 ms |
| Existing second-source preparation | 4.2662466 s |
| Entire matched process | 10.203004 s |
| Shared native pool witnesses / newly built | 78 / 0 |
| Joint event witnesses | 52 |
| Final relations / probability relations | 1,381 / 662 |
| Projected priced-action checks | 274,560 |
| Retained joint view / shared control / support | 764,912 / 688,864 / 613,732 bytes |
| Combined additional proof/scratch peak | 14,481,908 bytes (13.811 MiB) |
| Shared calculator, counted once | 852,941 bytes |
| Original owner including shared calculator | 1,249,769 bytes |
| Process peak working set | 219,901,952 bytes (209.715 MiB) |

The 1 GiB total / 16 MiB additional limits hold, including concurrent native
scratch, quotient work and live shared evidence. The isolated shared-ledger
peak is not substituted for the combined total. Construction uses prepared
pools; it is not a standalone cold timing or a production speedup. Shared
evidence is retained under existing ProofStore ownership; rejection/cancellation
grants no partial authority.

Final native build and `--solver-phase-lower-only` pass **110 checks, zero
failures**. Fixtures cover conditional draw enumeration, correlation, overlap
refusal, forced and no-op goals, hidden blockers, tiny upward mass, independent
versus aggregate caps, nonminimal/stale allocations, equality reactivation,
scope refusal and existing cancellation/accounting. The exact Python audit
checks 52 integer-derived joint bounds, all 1,381 event minima and simultaneous
inequalities, both weighted program sums and the complete-envelope comparison.

Two test-fixture assumptions were corrected during development: the existing
integer stream requires Eldritch Exalt rather than ordinary Exalt, and the
goal-layout owner rejects overlap before probability construction. Final
acceptance is after those corrections. The earlier discovery plateaus are
retained as bounded findings, not superseding final evidence.

Evidence: [native](native.json), [exact audit](reference.json),
[proof tests](phase-tests.txt), [build](acceptance-build.txt),
[development findings](development-findings.json),
[provenance](provenance.json). Source/object/binary/input hashes and exact
commands are recorded. Prior archives remain immutable and hash-verified.
No agents, Simulator, broad suite, full census, micro replay, UI, binding or
WASM test was run. Protected `0` was not accessed. No push.
