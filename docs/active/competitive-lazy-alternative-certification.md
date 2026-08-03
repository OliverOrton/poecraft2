# Competitive Lazy Alternative Certification

**Status: Gate 0 complete; Gate 1 is the active implementation boundary.**

Owner: Oliver

Parent: [Active work](README.md)

Starting commit: `bd288c9041a5b54fa4ec134c7e1dec90486ac385`.

Source evidence:
[Exact Reforge-Work Growth Diagnostic](../archive/2026-08-02-reforge-work-growth-diagnostic/README.md).

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
[`competitive-lazy-alternative-certification-gate0.json`](../../fixtures/solver-reliability/v1/evidence/competitive-lazy-alternative-certification-gate0.json).

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

**Status: active.**

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

## Gate 2 — selected-policy-first quotient construction

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

## Gate 3 — competitive alternative scheduler

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

## Gate 4 — medium integration qualification

Run `reliability-class-belt` through the full native workflow. Require complete
action accounting, selected closure before broad alternatives, exercised
unresolved and competitively scheduled obligations, a proper lumpable policy,
zero reference calls, compilation, exact reconciliation, deterministic repeat,
and 10,000 successful simulations with zero off-policy failures.

Also run a bounded scaled-breadth witness proving first partition and first
upper can precede complete alternative certification. Record separate work,
milestones, expansions before/after upper, pruning, replacements, quotient
sizes, memory, time, and Gate 0 prediction scoring.

## Gate 5 — frozen two-goal product case

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

Only if Gate 5 produces a valid upper, run the qualified Fracture full-four,
natural representative four-goal, five-goal scale, 27-case smoke, 49-case
portfolio, and relevant ring/armour representatives. Use 10,000 simulations
where compiled verification is required. Five-goal failure limits only the
five-goal scale claim.

## Gate 7 — conditional release and final acceptance

Only after native qualification, rebuild release WASM, verify all 61 exports
and ABI 2, run required WASM reliability, `npm test`, `npx tsc --noEmit`, and
then `scripts/test.ps1` once as final repository acceptance. Visual browser
review remains Oliver-owned and is not run unless requested.

Archive the final report, extract stable solver/evidence facts, update
`HANDOFF.md`, commit each completed gate locally with the co-author trailer,
and do not push or merge.
