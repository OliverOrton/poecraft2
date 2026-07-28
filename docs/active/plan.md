# Pre-Expansion Probability-Lower Audit

**Status: active, falsification-first solver research.**

Owner: Oliver

Branch: `codex/pre-expansion-probability-lower-audit`

Starting source: `8976750` (`main`)

## Decision To Make

Determine whether a strictly graph-free probability relaxation can produce
action-class lower bounds strong enough to separate one known exact
pre-expansion renewal upper before a hard natural-T1 solve attempts its first
broad row.

This is not permission to wire the current clean-goal MDP into root pruning.
`prepare_goal_cover_cost` calls `calc.outcomes` for destructive actions while
building `exact_destructive_envelopes`; at a hard root that can materialize the
same distributions the audit must avoid. The `relax_cover` helper also has no
current `probability_aware=true` call. Existing probability primitives are
inputs to a new isolated measurement, not an already completed graph-free
bound.

## Frozen Evidence

Use the same product-cap portfolio and identity as the completed
[root-action feasibility pass](../archive/2026-07-27-certified-root-action-feasibility/README.md):

- `natural-t1-smoke-dire-pelt-three`;
- `natural-t1-full-three-24920b3b28de`;
- `natural-t1-deep-three-low-probability-af4719c816f3`;
- `natural-t1-full-four-47d8b909aa88`; and
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

The comparison targets are the exact fixed-renewal candidates already pinned
by the
[broad-action report](../archive/2026-07-25-broad-action-separation-research/report.md):

| Hard case | Selected renewal action | Exact upper |
| --- | --- | ---: |
| full-three | `harvest_reforge:lightning` | 1,918,267.5087923806 |
| deep-three | `harvest_reforge:fire` | 575,497.52262412792 |
| full-four | `harvest_reforge:attack` | 193,266,777.27582425 |
| deep-four | `harvest_reforge:fire` | 175,126,199.48640418 |

Those uppers were research measurements under a 100M diagnostic allowance,
not current product-cap incumbents. The compact evaluator fit 11M only on the
two four-mod cases. They may be used as fixed numerical targets during this
audit, but not published as current solve results.

## Gate 0 - Proof And Non-Interference Contract

### Graph-free means graph-free

The candidate may read immutable session data, goal masks, action descriptors,
prices, spawn weights, exclusion groups, and the unchanged concrete start
item. It may not:

- call `CalcContext::outcomes` or any transition evaluator;
- intern or materialize an abstract successor;
- build or retain a sparse row;
- consume reforge-frontier work;
- change operator admission, ordering, Bellman values, focus, pruning,
  termination, policy, or caps; or
- trust a sampled probability or learned estimate.

At every observation, state count, retained rows/transitions, and reforge work
must match the no-probe control at the pre-row boundary.

### Lower-bound objects

Record two distinct controls:

1. **Per-slot acquisition floor.** For every required goal slot, minimize the
   cost of a guaranteed legal placement or an optimistic weighted placement
   over every admitted placer, then take the maximum across slots. It is a
   coarse global lower and must not add a first-action price unless a separate
   conditioned proof prevents double counting.
2. **Action-conditioned relaxation.** Force one exact first executable action
   class, then solve a finite optimistic goal-progress model whose stochastic
   event probabilities are upper bounds and whose choices, preservation, and
   successor abstraction can only help the relaxed controller. Class lower is
   the minimum over every admitted planner operator with that first action.

Weighted Fossil additions, forced Fossil modifiers, Essences, Harvest tag
pools, ordinary reforges, deterministic bench placement, and every other
admitted placer must be classified explicitly. Unknown placement semantics or
an unavailable bound make the affected class unavailable; they are not
treated as impossible.

The measurement is limited to the current clean empty-rare frozen roots.
General fractured, influenced, protected, or Eldritch carriers are outside its
validity domain.

### Certification comparison

For renewal action class `A` with archived exact upper `U_A`, separation
requires:

```text
U_A + tolerance < min L_B for every complete-scope class B != A
```

Unpriced, unsupported, unprojectable, resource-deferred, or otherwise missing
members inside the disclosed product envelope block the result. Same-first-
action planner operators are grouped before the minimum.

A finite bracket, a larger global lower, a stable action ranking, or a lower
that remains below the archived upper is not a pass.

## Gate 1 - Code Audit And Small Oracles

1. Trace every current probability helper and mark calls that can mutate
   `CalcContext`, generate exact outcomes, or depend on a restricted carrier.
2. Specify the smallest isolated probability-only data flow. Do not reuse
   `prepare_goal_cover_cost` as a black box.
3. Prove the draw-count/placement envelope against current engine capacity and
   exact small kernels. If proof depends on an unresolved PoE mechanic rather
   than implemented capacity, stop and ask Oliver before giving the candidate
   bound authority.
4. Add small deterministic/weighted/forced-placement controls and adversarial
   cases for overlapping goal masks, same-action classes, zero probability,
   and multi-slot actions.
5. Assert the relaxed result never exceeds exact values on completed small
   oracles and that evaluating it changes no state/work counter.

Exit: a written admissibility argument and a positive non-interference oracle,
or an early rejection with no hard-case run.

## Gate 2 - Measurement-Only Shadow

Add only the minimal full-evidence diagnostic needed to emit:

- root operator and first-action-class counts;
- complete/incomplete scope and projection status;
- per-slot floor, binding slot, cheapest placer, and probability envelope;
- per-operator and per-class conditioned lower;
- archived selected renewal action/upper, lowest blocking class, and margin;
- evaluator operations and wall time;
- state, row, transition, reforge-work, and owned-byte counts before/after; and
- an explicit reason when no separation exists.

The probe must be capped, typed JSON telemetry and must not change solve
behavior. Run a small exact oracle before any hard case.

## Gate 3 - Frozen Portfolio Decision

Run the five fixed cases once under unchanged production caps, one worker, and
the existing watchdog policy. Compare against the completed root-action
baseline and require identical deterministic ordinary-solver behavior.

Exit paths:

- **qualified:** at least one previously refused hard case has complete-scope
  strict class separation against its archived exact renewal upper before
  broad-row work, with zero probe-induced state/work growth;
- **rejected:** no hard case separates, the selected lower depends on exact
  outcomes/state growth, scope is incomplete, or the proof fails an oracle.

Do not weaken the gate because the lower is numerically much larger than the
current immediate-price cover.

## Gate 4 - Retention Or Restoration

If rejected:

- restore every measurement-only engine/test edit;
- retain only the proof audit, raw-hash summary, and decision documentation;
- do not rebuild WASM or run web tests.

If qualified, stop and write a new implementation plan before changing solver
behavior. That later plan must recreate the exact renewal upper under current
identity/caps, define the internal certificate, and separately decide whether
bounded-policy or verified-next-action product work is justified.

For either exit:

- run only the focused native tests needed by retained source;
- report deterministic work as portable and wall time as machine/compiler
  bound;
- archive this plan, clear `HANDOFF.md`, and commit locally with the required
  co-author line.

## Stop Rules

Stop without production integration if:

- any candidate calls exact outcome generation;
- an upper probability is not proved conservative for the frozen scope;
- the class minimum omits an admitted competitor;
- the only sound combination is the weak per-slot control;
- no hard case strictly separates;
- separation needs raised caps or a restricted action envelope; or
- the candidate helps only after the cap-failing row.

## Out Of Scope

- Public root-certificate result, ABI, WASM, worker, or product work.
- Restoring the fixed-renewal upper in production.
- General all-state probability bounds.
- Fractured/influenced cleanup or state equivalence.
- Raising caps, GPU work, ML guidance, or approximate pruning.
- The 30M Dire Pelt selected-memory investigation.
