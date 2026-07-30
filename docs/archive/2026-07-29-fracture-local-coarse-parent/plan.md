# Fracture-Local Coarse-Parent Prototype

**Status: completed and archived; this plan no longer controls sequencing.**

Owner: Oliver

Branch: `codex/fracture-local-coarse-parent`

Starting source: `490b9f77d7f143d9f14bba888ea229f47bd6919b`

Governing specification:
[archived Fracture-local handoff](../2026-07-29-practical-four-goal-solving-research/handoff.md)

## Objective

Replace the product solver's globally strict Fracture-driven parent
representation with:

- a coarse ordinary/reforge parent layout;
- an exact solver-local Fracture operator;
- one explicit branch per acceptable physical goal-modifier hit; and
- one aggregate non-goal miss branch routed through priced Restart.

The exact Fracture primitive and Calculator, Emulator, simulator, authored
exact graph, C ABI, strategy vocabulary, and product web behavior remain
unchanged. This milestone does not implement the one-/two-/three-goal anchor
library.

## Frozen Evidence

The owner case is
`build/gate1-baseline-corpus/cases/natural-t1-full-four-47d8b909aa88.json`.
Before behavior changes, load or reproduce its committed 200,000-state
baseline without repeating the preceding multi-hour experiment:

| Field | Baseline |
| --- | ---: |
| Parent junk classes | `105` |
| Root Chaos support | `134,477` |
| Discovered / expanded states | `200,000 / 160` |
| Fracture rows / entries | `158 / 706` |
| Root lower / upper | `432.40685295343258 / 60341416.98784247` |
| Transition / policy hash | `d4346e90f923332c / 8b2a568f3c9cfd35` |
| Coarse projection | `6` classes / `217` root carriers / zero failures |

For the same 158 measured Fracture sources, the target is 158 acceptable
goal-hit branches plus 158 aggregate misses, 316 entries total, with no
parent junk-miss state IDs.

## Product Fracture Contract

Fracture remains uniform over every live explicit modifier. For `n` live
explicit modifiers and `k` acceptable physical goal modifiers:

- each acceptable physical goal hit has probability `1/n`;
- the aggregate miss probability is `(n-k)/n`;
- every non-goal hit is dead for product planning and uses priced
  Restart/base recovery;
- below-tier goal-family modifiers count toward `n` and miss mass only; and
- distinct successful hits remain distinct when their fractured-goal state or
  continuation differs.

`k` counts physical affixes. If one affix can satisfy multiple goal slots, the
operator must coalesce it correctly or refuse until physical hit identity is
proved. A live non-goal continuation also causes refusal. No free reset is
allowed when `k = 0`.

## Phase 1 - Parent And Exact-Child Precision

Trace every caller controlling complete group-exclusion identity. Then:

1. stop `pc_solver_create` from unconditionally forcing complete group
   identity for the product parent;
2. exclude product Fracture from parent-layout observer derivation;
3. keep exact primitive, mechanic, authored graph, and genuinely exact callers
   explicit and unchanged;
4. keep temporary blocker, follow-up, and cleanup evaluation inside the
   existing exact local child context;
5. keep standalone Remove Crafted Modifiers outside the product parent; and
6. construct the coarse live parent directly, never by materializing and
   renaming the strict frontier.

## Phase 2 - Solver-Local Fracture

Add a product-solver-local composition over the coarse state. It must derive
legality, `n`, acceptable physical goal identities, fractured-goal successor
masks, and crafted/fractured state exactly. It emits one `1/n` successor per
acceptable hit and one `(n-k)/n` branch directly to the existing priced
Restart witness without interning fractured-junk states.

The operator keeps action, Restart, base, and continuation costs exact and
refuses whenever the abstraction cannot prove hit probabilities or dead-miss
semantics. `CalcContext::outcomes(ActionType::Fracture)` remains unchanged.

Every retained row records the coarse source, legality, `n`, physical hits,
`k`, probabilities and successors, miss target/cost, normalized sum,
fallback provenance, graph/action identity, and properness result.

## Phase 3 - Executable Policy And Coarse Equivalence

Reuse the existing policy, provenance, reachability, closed-class, and
compilation machinery. Concrete compiled execution must choose the
corresponding continuation for each acceptable real Fracture hit and Restart
for every other result without parent junk-miss IDs. If this requires a new
strategy condition or public vocabulary, stop before changing it.

Collision-check coarse carriers against a strict reference for every admitted
non-Fracture parent family, covering legality, resource identity, goal
probabilities, below-tier and blocking behavior, projected successors,
self-loops, and total probability. A retained observer that invalidates the
coarse parent is a stop condition unless an existing exact child boundary
already handles it.

Completed reforge distributions and kernels must be reused. Raw evaluator work
is reported separately from retained support, transitions, and state IDs, and
completed-row recomputation must remain zero.

## Phase 4 - Focused Native Controls

Add controls for:

1. the frozen 6-class parent and online 217-state root Chaos support;
2. unchanged Fracture eligibility and exact primitive parity;
3. `n = 4, 5, 6` and `k = 0, 1, multiple`;
4. `1/n` hit mass, `(n-k)/n` miss mass, normalization, and below-tier misses;
5. distinct fractured-goal masks and physical-affix overlap without
   overcounting;
6. priced Restart/base recovery and zero parent junk-miss IDs;
7. action-local temporary cleanup and coarse/strict non-Fracture collision
   controls;
8. deterministic product runs, proper executable published policies, and zero
   completed-row recomputation.

## Phase 5 - Frozen Qualification

Run the primary 200,000-state request twice from clean equivalent inputs.
Required gates:

1. no more than 6 parent junk classes, or stop and justify any deliberately
   retained attack/caster ceiling no greater than 11;
2. exactly 217 root Chaos successors online;
3. exactly 316 local Fracture entries if the same 158 sources are reached;
4. zero aggregate-miss fractured-junk parent IDs;
5. unchanged primitive hashes/parity;
6. deterministic new transition and policy hashes;
7. zero completed-row recomputation;
8. a proper returned upper policy; and
9. closure below 200,000 states or at least 1,600 expanded states before the
   same cap, unless a stronger deterministic completion result is obtained.

Reaching 200,000 renamed states at approximately 160 expansions is a failed
prototype. The milestone does not need to solve the separate 60-million
upper-quality problem.

## Acceptance And Closure

After implementation and qualification:

1. run one appropriate complete native acceptance suite;
2. validate standard and natural-T1 manifests;
3. rebuild release WASM and run non-visual web plus TypeScript acceptance when
   shipped solver behavior changed;
4. run 10,000 simulator executions only if the compiled strategy materially
   changed;
5. record parent/support/state/row/work/kernel/memory/hash/action-family and
   Fracture hit/miss telemetry;
6. produce a dated archive report with before/after gates, exactness evidence,
   next boundary, and qualification decision;
7. update stable solver/evidence/archive documentation and leave HANDOFF at a
   clean no-active boundary; and
8. make one final local implementation/evidence/archive commit.

No visual review and no push are in scope. Both milestone commits end with
`Co-authored-by: Codex <codex@openai.com>`.

## Stop Conditions

Stop rather than broaden if the dead-miss rule becomes ambiguous, a non-goal
miss remains live, physical hit identity cannot be recovered, a non-Fracture
parent action breaks coarse equivalence, exact child exits cannot project,
strategy compilation needs an unapproved vocabulary change, completed rows
must be recomputed, online support is not 217, the strict wall remains, or
properness cannot be established.

If the prototype qualifies, the next selectable boundary is the deferred
three-/two-/one-goal executable anchor library. It is not part of this plan.
