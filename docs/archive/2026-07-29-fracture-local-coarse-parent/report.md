# Fracture-Local Coarse-Parent Prototype Report

**Status: qualified implementation; unrestricted exact optimality is not
claimed.**

Parent: [Fracture-Local Coarse-Parent Prototype](README.md)

Verified 2026-07-29 on `codex/fracture-local-coarse-parent`, starting from
clean `490b9f77d7f143d9f14bba888ea229f47bd6919b`. The qualification case is
`natural-t1-full-four-47d8b909aa88` with
`--goal-progress-gated-reforges`. No online mechanic research or new mechanic
ruling was used.

## Decision

The prototype qualifies. It removes Fracture's global exact-group identity
from the product parent, retains exact ordinary/reforge equivalence, and moves
physical Fracture observation into a solver-local operator. The frozen carrier
graph closes at 927 states, far below the former 200,000-state stop.

The returned result is `bounded_near_optimal` with an exact zero-width bracket
for the evaluated policy under the existing zero-progress reroll restriction.
The incremental action envelope is still open, so this is not an unrestricted
exact solve. The next owner-selectable boundary remains the deferred
three-/two-/one-goal executable anchor library.

## Frozen Gates

| Gate | Strict baseline | Prototype | Decision |
| --- | ---: | ---: | --- |
| Parent junk classes | `105` | `6` | pass |
| Online root Chaos support | `134,477` | `217` | pass |
| Discovered / expanded | `200,000 / 160` | `927 / 927` | pass |
| Frontier | `199,840` | `0` | pass |
| Fracture source rows | `158` | `215` | changed source set |
| Fracture raw physical outcomes | `706` entries | `1,029` | measured |
| Retained Fracture hit / miss entries | exact strict outcomes | `352 / 215` | pass |
| Parent Fracture miss states interned | strict miss identities | `0` | pass |
| Completed rows recomputed | `0` | `0` | pass |
| Returned policy | proper bounded upper | `29/29` selected Fracture rows proper | pass |

The plan's conditional `316`-entry target applied only if the same 158 source
rows were reached. The prototype instead closes a larger source set: 215
eligible rows produce 352 goal-hit entries plus exactly 215 aggregate misses.
No miss branch interns a fractured-junk parent state.

The baseline root bracket was
`432.40685295343258 <= V* <= 60341416.98784247`. The prototype returns
`L = U = 65715.029067523152` for its proper bounded policy and fires the
configured absolute and relative target gaps. This is a bounded certificate,
not a claim that the still-open action envelope has been exhausted.

## Representation And Exactness

Product construction now maintains two precision boundaries:

1. the ordinary/reforge parent excludes Fracture as a complete-group observer
   and uses the six-class layout;
2. an exact context remains available lazily for Calculator queries,
   feasibility, exact primitive evaluation, and local child programs.

The product-local Fracture kernel proves a physical hit identity before it
emits a row. It refuses overlapping cross-goal satisfying masks or
non-uniform metamod identity that the coarse carrier cannot distinguish. For
`n` live affixes and `k` acceptable physical goal affixes, every retained hit
has probability `1/n` and the single miss has probability `(n-k)/n`.
Below-tier goal-family affixes contribute only to `n` and miss mass. `k = 0`
does not admit a free-reset row.

Native controls cover all observed `n = 4, 5, 6` and `k = 1, 2, 3` shapes:

| Shape | Rows | Shape | Rows | Shape | Rows |
| --- | ---: | --- | ---: | --- | ---: |
| `n4 k1` | `47` | `n5 k1` | `38` | `n6 k1` | `20` |
| `n4 k2` | `34` | `n5 k2` | `31` | `n6 k2` | `18` |
| `n4 k3` | `10` | `n5 k3` | `10` | `n6 k3` | `7` |

Maximum probability-normalization error is zero in the qualification
telemetry. Strict and coarse Chaos projection controls agree to
`6.94e-17`; the strict reference has 105 classes / 134,477 outcomes and its
projection has the same 217 carriers as the six-class product row. Exact
primitive Fracture remains the Calculator and simulator authority.

The product reforge frontier also aggregates only externally group-disjoint
junk-only exclusion families whose side, blocking, tag, and weight behavior
is identical. Strict contexts keep the original individual buckets. The
frozen product root uses 518,056 reforge work versus 2,807,580 in its strict
reference while preserving the projected distribution.

## Compilation And Properness

Selected product-local Fracture rows compile through the existing strategy
vocabulary:

- 29 selected Fracture operations feed 29 local routers;
- every acceptable physical hit follows its corresponding success route;
- every default miss goes to one canonical Restart operation; and
- no synthetic Restart is classified as a gated reforge.

All 29 selected rows were checked and proved proper; none was improper or
unproved. The compiled strategy has 229 nodes, 571 edges, and 466,506 JSON
bytes. Both qualification runs produce strategy SHA-256
`e951df8287448fce5c6d6238622a8977fa547cb33202ffe00f9a460366d64f0e`.

## Deterministic Qualification

The two equivalent runs agree on:

| Field | Both runs |
| --- | ---: |
| States discovered / expanded / goal | `927 / 927 / 30` |
| Policy-reachable states | `319` |
| State-action rows | `8,015` |
| Retained transition entries | `16,899` |
| Raw outcome entries | `344,040` |
| Reforge work | `3,407,700` |
| Transition hash | `04a66ba6c6dfcabf` |
| Policy hash | `3e5d7530e7aed5fb` |
| Start lower / upper / evaluated cost | `65715.029067523152` |
| Compiled nodes / edges | `229 / 571` |

The second run requested the required 10,000 simulator executions with seed
`20260850`. The harness completed, reported `verification_passed = true`, and
recorded zero off-policy failures. Only 962 executions reached success before
the diagnostic action limit; 9,038 were limit failures. Consequently this
gate supports compiled routing and off-policy safety, but it is not presented
as a sampled mean-cost parity result.

## Acceptance

- focused solver tests: 1,148 checks, zero failures;
- solver API tests: 508 checks, zero failures;
- complete artifact-backed native suite: 470,205 checks, zero failures;
- standard and natural-T1 manifests: 12 and 146 cases validated;
- release WASM rebuilt and the complete non-visual web suite passed;
- `npx tsc --noEmit` passed; and
- no rendered or screenshot review was performed.

The final API count includes a stepped-solve regression: once a non-focused
solve has full non-goal closure and converged without a cap or target stop,
its last progress sample reports the same exact lower and upper bound as the
final summary.

The two qualification artifacts predate only that final progress-reporting
repair. The repair changes no transition, policy, price, hash, compilation,
or crafting behavior; complete native and rebuilt-WASM acceptance ran after
it.

## Scope Kept Unchanged

No public C ABI, binding protocol, strategy condition, product UI, primitive
Fracture mechanic, Calculator outcome, simulator behavior, authored strategy
semantics, canonical SQLite data, or compiled data artifact changed. Product
Fracture is an exact solver-local composition only.
