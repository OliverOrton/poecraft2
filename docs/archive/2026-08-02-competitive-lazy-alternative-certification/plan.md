# Competitive Lazy Alternative Certification

**Status: Gates 0 through 4 complete; Gate 5 reached a binding selected-closure
structural stop. Gates 6 and 7 were not run.**

Owner: Oliver

Parent: [Documentation archive](../README.md)

Starting commit: `bd288c9041a5b54fa4ec134c7e1dec90486ac385`.

Source evidence:
[Exact Reforge-Work Growth Diagnostic](../2026-08-02-reforge-work-growth-diagnostic/README.md).

## Objective

Replace eager pre-partition certification of every admitted alternative with
selected-policy-first exact certification. Every filtered, admitted, priced
action remains represented. An action avoids exact expansion only while a
sound carrier-wide lower bound proves it cannot improve the current certified
upper. Exactness requires every potentially cheaper action to be certified or
proved noncompetitive.

The primary product success is a current, proper, compilable bounded policy
inside existing limits. Exact optimality is desirable but is not required for
this milestone.

## Frozen boundaries

Do not change crafting mechanics, prices, objective, filtering, admitted
vocabulary, the frozen 20M product-case work cap, the 1 GiB memory cap,
watchdogs, public C ABI, strategy JSON, WASM contract, or frontend authority.
Do not special-case named actions or fixtures. Deterministic checkpoint/replay
remains deferred.

Reuse the existing quotient, proof, generation invalidation, Bellman,
properness, compiler-routing, focused-upper, and exact-reconciliation
authorities. Do not create a parallel policy subsystem.

## Gate 0 — frozen evidence and selected-closure risk

**Status: complete.** Tracked evidence is
[`competitive-lazy-alternative-certification-gate0.json`](../../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate0.json).

The new bounded aggregate telemetry distinguishes inherited/current selected
rows from alternatives, including rows begun and completed, reforge work,
transitions, work to first partition, work to first executable upper, and
exact alternatives materialized before that upper. It is diagnostic only and
does not participate in selection, proof, or memory caps.

A fresh `reliability-class-belt` run before and after telemetry has identical
inputs, product action order, bounds, work counters, transition and policy
hashes, refinement results, compiled strategy SHA-256, exact evaluation, and
10,000-run simulation. The telemetry-only result is:

| Field | Selected | Alternatives |
| --- | ---: | ---: |
| Rows begun / completed | `115 / 115` | `88 / 88` |
| Exact reforge work | `39,690` | `4,180,979` |
| Exact transitions | `1,476` | `275` |

First partition and first executable upper both occur at `4,220,669` exact
work, after all 88 alternatives have already been materialized. This confirms
the ordering problem on the medium case without changing its behavior.

The hard two-goal case retains exactly `14,077,632` coarse work and only
`5,922,368` exact work at the unchanged 20M cap. Broad rows have measured
about `2,111,010` marginal work and `172,596` transitions each. The archived
10,466 selected-policy kernels are a serious risk indicator, not a proved
current selected-closure requirement.

No defensible numerical hard-case selected-closure range exists before the
new scheduler measures selected rows: the archived selected kernels average
about 40 transitions each, while the newly measured broad rows emit 172,596.
The frozen prediction is therefore **not presently estimable**. The binding
known lower bound is that the root selected row and all recursively selected
successors must fit in 5,922,368 exact work. A conditional 10,466-row model at
the broad-row slope is about 22.1B work, but it is explicitly a risk scenario,
not a forecast or stop authority.

## Gate 1 — explicit unresolved alternative obligations

**Status: complete.**

Introduce a first-class `UnresolvedAlternativeObligation`; never manufacture a
fake stochastic row for an action whose outcomes have not been calculated.
Each obligation retains source cell, admitted action and choice identity,
price and vocabulary identity, requirement/source/target/partition
generations, sound optimistic lower-Q provenance, scheduling priority,
resumable-work identity, and lifecycle status.

Statuses must distinguish at least unscheduled, lower-only, scheduled,
partially evaluated, certified, conditionally noncompetitive, stale, and
resource-interrupted. A lower is valid only when sound for every exact carrier
covered by the source cell; incompatible carrier bounds are never averaged.
Use a trivial safe lower when no tighter authority exists and report its
weakness.

Every admitted action must be the current certified selected row, another
certified row, or an explicit unresolved obligation. Measure noncompetitive
and unresolved fractions, exact rows avoided, selected/alternative work, and
bound tightness without imposing an arbitrary pruning threshold.

Focused acceptance covers complete action accounting, action-identity
collisions, carrier-wide lower soundness, lower-only upper exclusion,
competitive exactness blocking, deferred noncompetitive actions, every
identity/generation invalidation, deterministic order, and memory accounting.
Stop only if even a trivial sound lower cannot be represented.

The retained proof-store implementation satisfies this boundary without
changing production scheduling. `CarrierWideOptimisticLowerQ` can only be
constructed through a zero lower justified by the nonnegative-cost contract
or through complete per-carrier witnesses; the latter stores their minimum,
never their average. Its interned obligation identity is collision checked
over source, requirement, action/program/choice, price, vocabulary, all
generations, lower provenance, priority, and resumable-work identity.

All eight lifecycle states are explicit. A certified row id is required before
an obligation may support an executable upper; every other state remains
rowless. A current conditional noncompetition proof can close exactness only
while both its source upper and Q generation still agree. The accounting audit
accepts each admitted action exactly once as selected-certified,
other-certified, or an explicit obligation. The dedicated memory-ledger
category is independently recomputed from retained capacities. Focused native
acceptance passed 316 checks with zero failures; tracked evidence is
[`competitive-lazy-alternative-certification-gate1.json`](../../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate1.json).

## Gate 2 — selected-policy-first quotient construction

**Status: complete.**

Split the eager row API into operations that obtain the current selection,
enumerate cheap alternative descriptors, create obligations, and certify one
requested action transactionally.

For inherited coarse selections, certify the selected row first, discover its
exact successor closure, and create obligations for all other admitted
actions without constructing their kernels. Where no inherited choice is
usable, use existing focused upper/local optimization and sound lower
information to certify candidates one at a time until a proper executable
continuation exists; never name a preferred action in code.

Refine incrementally around selected closure. Preserve open frontier identity,
resolve it incrementally, and invalidate or reproject dependent proofs on
closure. Unknown successors never merge merely because their current
observations match. Every installed upper retains its complete selected-row,
fallback, graph/vocabulary, generation, and properness witness.

Focused acceptance covers selected-first ordering, partition before
alternative exhaustion, frontier separation, split invalidation, cycles,
improper-policy repair, multiple entries, replay identity, and proof-memory
accounting. Stop if first partition or a selected executable policy still
inherently requires every admitted alternative.

Production now exposes separate operations for cheap deterministic alternative
descriptors, the inherited/current selected row, and transactional exact
certification of one requested descriptor. The selected row alone grows the
strict successor closure. Descriptor identities participate in cell
partitioning, but their unknown outcomes do not become arcs or mergeable
successors. Each final cell then interns one rowless lower-only obligation per
admitted descriptor and audits its selected-certified plus unresolved action
accounting before Bellman publication.

The focused production integration reaches its first partition after selected
work only, builds zero alternative kernels, retains nonzero obligations and
avoided-row counts, proves a proper upper, compiles it, and exact-reconciles the
artifact. The solve suite passes 98,140 checks and the quotient proof,
partition, and Bellman suite passes 316 checks, all with zero failures. Tracked
evidence is
[`competitive-lazy-alternative-certification-gate2.json`](../../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate2.json).

## Gate 3 — competitive alternative scheduler

**Status: complete.** Tracked evidence is
[`competitive-lazy-alternative-certification-gate3.json`](../../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate3.json).

After a proper upper exists, schedule an unresolved alternative only when it
cannot currently be proved noncompetitive or a refinement counterexample
invalidates the policy. Priority may use optimistic Q versus the certified
upper, expected policy occupancy/visits, counterexample relevance, stale
dependencies, expected upper reduction, and deterministic tie-breaking.
Scores guide work and carry no proof authority.

Each scheduled alternative is resumed or built transactionally, installed
only when complete, followed by required cell refinement, dependent proof and
witness invalidation, Bellman improvement, and exact properness evaluation.
Root repricing reuses unchanged completed probability rows. Partial work is
deterministic, accounted, resumable where supported, and never certified.

Explicit revocation tests cover source/target split, requirement growth,
price change, and vocabulary change. A revoked potentially competitive
obligation blocks exactness. Resource outcomes remain honest: no upper keeps
the existing refusal; an upper plus unresolved competitive alternatives is a
bounded policy; resolving all potentially improving actions may allow exact
publication.

Production now compiles the selected-only proper upper before optional
alternative work. The deterministic scheduler compares each carrier-wide
optimistic lower with the current certified source upper, then certifies one
requested descriptor across every carrier of that quotient cell. A row is
installed only when all carriers agree on its action, exact cost, closed
successor projection, and already-satisfied routing requirement. Newly exposed
frontiers or split requirements remain `partially_evaluated`; they never become
fabricated arcs or certified rows.

Installed complete rows enter the existing proof store and Bellman graph.
Potential improvements rerun Bellman and properness, and a replacement artifact
becomes current only after compilation and exact reconciliation. If exact row
construction or replacement compilation reaches a resource cap, the last
compiled selected upper remains publishable and the envelope stays explicitly
bounded. Rowless obligation lowers now participate in the optimistic lower
relaxation but remain excluded from executable-upper selection.

The focused production witness creates and schedules 12 obligations, certifies
6, leaves 6 explicit partial blockers, performs zero alternative work before
the first upper, and returns a proper compiled reconciled bounded policy. A
separate exact-kernel-cap witness interrupts the first alternative after the
selected publication and retains that executable artifact. Explicit tests
revoke conditional verdicts and recreate schedulable blockers after source and
target splits, requirement growth, price change, and vocabulary change.
Focused solve acceptance passes 98,156 checks and quotient
proof/partition/Bellman acceptance passes 353 checks, both with zero failures.

## Gate 4 — medium integration qualification

**Status: complete.** Tracked evidence is
[`competitive-lazy-alternative-certification-gate4.json`](../../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate4.json).

Run `reliability-class-belt` through the full native workflow. Require complete
action accounting, selected closure before broad alternatives, exercised
unresolved and competitively scheduled obligations, a proper lumpable policy,
zero reference calls, compilation, exact reconciliation, deterministic repeat,
and 10,000 successful simulations with zero off-policy failures.

Also run a bounded scaled-breadth witness proving first partition and first
upper can precede complete alternative certification. Record separate work,
milestones, expansions before/after upper, pruning, replacements, quotient
sizes, memory, time, and Gate 0 prediction scoring.

Two native runs are semantically identical. Both return `bounded_feasible` at
cost `9.143792577895411`, retain the frozen coarse hashes, compile to strategy
SHA-256 `87a5c6a5...855bb`, exact-reconcile, and complete 10,000/10,000
simulations with zero off-policy failures and zero production reference calls.
The 19-cell quotient is proper and lumpable.

Selected closure is 15 rows, 882 exact work, and 232 transitions. Partition
and the first executable upper both occur at work 882, after 21.16-27.24 ms and
65.46-71.80 ms respectively, with zero alternatives materialized. The
post-upper scheduler attempts 180 alternative rows using 6,355,232 work,
certifies 31 obligations, and retains 149 partial competitive blockers. The
trivial carrier-wide lower prunes zero obligations, reported as a real 0%
pruning result; the policy is therefore bounded rather than exact.

The full 17-action / 180-obligation run plus the focused selected-row
exact-kernel-cap control is the bounded scaled-breadth witness. The first upper
uses only 0.0139% of eventual exact work and exists before every alternative.
Peak solver-owned memory is 126,975,163 bytes under the unchanged 1 GiB cap.
Relative to Gate 0, first-upper work falls 99.979%; total work rises 50.595%
because carrier-wide post-upper attempts cover more exact rows. Gate 0's hard
selected-closure prediction was deliberately “not presently estimable,” so no
numeric hard-case prediction is scored from this medium measurement.

## Gate 5 — frozen two-goal product case

**Status: structural stop.** The single frozen invocation exhausted the full
5,922,368-work post-coarse allowance on two selected rows before the first
partition or executable upper. No alternative row was attempted and no
obligation was created. The result is `refused_resource_cap`, with compilation,
exact evaluation, and simulation correctly not applicable.

Run `natural-t1-breadth-two-4e65dda9c53b` once under the unchanged 20M/1 GiB/
900-second limits and complete admitted vocabulary. Success requires a
current, proper, compilable executable upper, complete action representation,
lower participation by unresolved alternatives, closed selected rows,
compiler routing, exact reconciliation, and 10,000 simulations when a policy
is produced.

Report bounded versus exact honestly. A cap after an upper retains the bounded
policy. A cap before an upper because selected closure itself consumes the
budget is a selected-closure structural failure. A cap with competitive
alternatives remaining records the measured pruning/scheduling boundary
without weakening exactness.

## Gate 6 — conditional broader native qualification

**Status: not run because Gate 5 produced no upper.**

Only if Gate 5 produces a valid upper, run the qualified Fracture full-four,
natural representative four-goal, five-goal scale, 27-case smoke, 49-case
portfolio, and relevant ring/armour representatives. Use 10,000 simulations
where compiled verification is required. Five-goal failure limits only the
five-goal scale claim.

## Gate 7 — conditional release and final acceptance

**Status: not run because native qualification did not proceed.**

Only after native qualification, rebuild release WASM, verify all 61 exports
and ABI 2, run required WASM reliability, `npm test`, `npx tsc --noEmit`, and
then `scripts/test.ps1` once as final repository acceptance. Visual browser
review remains Oliver-owned and is not run unless requested.

Archive the final report, extract stable solver/evidence facts, update
`HANDOFF.md`, commit each completed gate locally with the co-author trailer,
and do not push or merge.
