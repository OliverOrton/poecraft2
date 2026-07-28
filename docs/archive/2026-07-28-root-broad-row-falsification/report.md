# Root Broad-Row Falsification — Final Report

Date: 2026-07-28

Plan: [Root Broad-Row Falsification](plan.md)

Evidence:
[tracked summary](../../../fixtures/solver-natural-t1/v1/evidence/root-broad-row-falsification-summary.json)

## Decision

Retain exception-safe interrupted-row ownership telemetry. Reject and restore
the streaming success-only evaluator.

The candidate produced no proved terminal mass inside the actual 192,420
work units left by the completed gated Chaos row. It therefore produced no
finite `cost / p_lower` policy upper and could not improve either retained
Chaos incumbent. No production evaluator, scheduling rule, action deferral,
cap change, or solver-objective change survives this milestone.

## Direct Cap Ownership

The earlier reports could attribute only rows that returned. The retained
telemetry now records the state, root ownership, complete planner action,
operator index, stable cursor, cap, work, cache traffic, and wall time when a
row throws `SolverResourceLimit`.

| Case | Interrupted action | Operator | Cursor | State | Work | Cap |
| --- | --- | ---: | ---: | ---: | ---: | --- |
| full-four | one-socket Lucent Fossil (`...CurrencyDelveCraftingMana`) | 25 | 11 | root `0` | 192,420 | `max_reforge_work` |
| deep-four | one-socket Jagged Fossil (`...CurrencyDelveCraftingPhysical`) | 27 | 11 | root `0` | 192,420 | `max_reforge_work` |

Both completed gated Chaos rows still consume 2,807,580 work. Both final
solves still consume exactly 3,000,000 work and expand only the root.

Unrestricted controls identify Chaos itself as the interrupted root owner at
the same 3,000,000 cap and preserve the prior no-policy result.

## Bounded Streaming Result

The temporary oracle reused the exact reforge frontier, interned no outcomes,
accumulated only proved goal mass, and left every unprocessed branch
unclassified. It was run twice per case at the actual remaining-work boundary.

| Case | Processed work | Exactly resolved mass | Proved goal mass | Hash |
| --- | ---: | ---: | ---: | --- |
| full-four / Lucent | 192,416 | `0.044288507981371794` | `0` | `d0112f4aeef462c4` |
| deep-four / Jagged | 192,415 | `0.018771718433355226` | `0` | `2bcc212379dd59a2` |

Work, probability bits, and hashes repeated exactly. Because `p_lower = 0`,
the conservative fixed-policy upper is infinite. This triggers the plan's
stop rule. The larger diagnostic ceiling below is assessment evidence, not a
qualification under the product budget.

## Complete Fossil Census

The restored candidate was also measured to completion before its binary was
replaced, solely to assess structural-frontier reuse:

| Case | Exact work | Exact goal probability | Fossil repeat value | Chaos incumbent |
| --- | ---: | ---: | ---: | ---: |
| full-four / Lucent | 1,883,672 | `8.16389693564203e-08` | `102,922,661.3985813` | `60,341,416.98784247` |
| deep-four / Jagged | 2,194,169 | `3.7206071645216781e-08` | `292,223,809.69633406` | `185,688,651.38279814` |

Each complete work count, probability bit pattern, and hash repeated exactly.
Both Fossil repeat policies are worse than the existing Chaos policy. Their
possible value is therefore to complete competing Bellman rows, not to replace
the incumbent.

Sequential Chaos plus first-Fossil work is 4,691,252 and 5,001,749,
respectively. Merely caching a completed row or avoiding successor interning
cannot fit either sum under the unchanged 3,000,000 cap.

## Canonical Chaos Structural-Frontier Assessment

The proposal is technically credible for these two root rows:

- full-four's Lucent pool contains 131 mods, all drawn from the 142-mod Chaos
  pool; there are no Fossil-only mods;
- deep-four's Jagged pool contains 152 mods, all drawn from the 153-mod Chaos
  pool;
- both Fossils have zero compiled added/forced-mod rows, no lucky-roll flag,
  and no mirroring flag;
- every required goal modifier remains supported; Lucent changes one goal
  weight by 10× and Jagged changes two goal weights by 10×; and
- at the empty rare roots, preserved boundary, rare affix cap, and the
  four-to-six target-count schedule match Chaos.

The reusable authority would not be Chaos probabilities. It would be a
canonical structural DAG whose bucket identity contains side, goal/junk
classification, blocker mask, complete exclusion-group family, and
multiplicity. Each action supplies its own bucket-weight vector. At every DAG
node the action still computes its own eligible total and normalized outgoing
probabilities.

Fossil added mods are topology deltas because they can introduce new bucket
families and cross-conflicts. Forced mods are deterministic seed deltas:
they consume side capacity, occupy groups, and can satisfy or block goal slots
before random filling. Treating either as a post-hoc probability adjustment
would be unsound.

The current exact reforge memo is insufficient: it is keyed by action plus
preserved base and retains a completed distribution. A future prototype would
need a separate action-independent structural cache, stable canonical bucket
keys, per-action weight vectors, and deterministic accounting that splits:

1. structural node/eligible-edge construction;
2. action-lane probability propagation; and
3. successor projection/interning.

The main risk is that probability propagation, not topology construction,
owns most of the current `1 + bucket_count` work per frontier state. If so,
reusing topology improves wall time and memory but does not make the complete
root envelope affordable. A useful prototype must test lockstep multi-weight
propagation, not only cache one DAG and replay every action sequentially.

Harvest reforge is excluded pending an Oliver mechanic ruling on its
spawn-only guaranteed first-pick pool. This assessment does not treat Harvest
as an implementation exception or alter the current mechanic.

## Verification Boundary

The retained change is telemetry-only and does not cross the C ABI, WASM
exports, mechanics, SQLite, compiled artifact, bindings, or web. Native solver
acceptance and deterministic frozen controls are the appropriate closure.

The retained native build passed `166,384` checks with zero failures. It
reproduced the pre-existing GCC optimizer warning in
`prepare_goal_cover_cost`; no new warning or error was introduced. The
retained benchmark executable hash is
`29adc4209150d84966238865fc0a80063536e9c08cc9e78612ab4ecd40f83643`.

Final frozen gated controls reproduced both incumbent values, discovered and
expanded state counts, work caps, transition/policy hashes, and interrupted
Fossil owners. Final unrestricted controls reproduced `96,025/1` states,
3,000,000 Chaos work, no policy, transition hash `566809fa56c7e2a4`, and
policy hash `0cfb7e9c1000a4da` on both cases. Their benchmark process exit `2`
records a gated-corpus expectation mismatch; the native measurements
completed normally and matched the intended unrestricted controls.

Release WASM and the full repository pipeline were not run because the
retained source changes neither the engine C ABI nor strategy vocabulary and
has no downstream mechanics, artifact, binding, or web surface.

Wall times are machine/compiler-bound and do not survive a hardware or
compiler change. Work, probability bits, pool support, action identity, and
hashes are the portable evidence.
