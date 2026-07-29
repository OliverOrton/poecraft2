# High-Impact Partial-State Executable Upper Policies

**Status: archived final rejection boundary.**

Owner: Oliver

Branch: `codex/high-impact-executable-uppers`

Starting source: `3a6d191668f837090717295c8650ef7bbb5e0c0e`

## Objective

Improve the executable upper quality of the completed frozen Fossil and
Harvest root rows by discovering proper, progress-preserving continuations
from their highest-impact partial successors.

The implementation extends the existing focused-upper authority centered on
`begin_focused_upper_solve()`. It grows the existing exact parent graph,
retains state IDs and completed sparse distributions, schedules resumable
state-action work, evaluates one compatible proper policy, propagates its
state uppers backward, and reprices retained root rows without recomputing
their probability distributions.

This boundary targets executable upper quality and strategy discovery. It is
not a lower-bound project. Search estimates have scheduling authority only.
An incomplete restricted policy cannot prove global non-improvement,
rejection, exact closure, or a new global lower.

## Frozen Qualification Contract

The primary owner case is
`natural-t1-full-four-47d8b909aa88`. The secondary bounded confirmation is
`natural-t1-deep-four-low-probability-1a1102b0e06b`; it runs only after the
primary case qualifies or reaches a final measured rejection.

The milestone qualifies only if the primary case achieves at least one of:

1. strict upper-Q admission, using the existing numerical tolerance, of one
   completed qualifying Fossil or Harvest row; or
2. at least a 10% deterministic reduction in the upper Q of one completed
   qualifying Fossil or Harvest row, attributable to a verified
   progress-preserving proper multi-action executable continuation.

For action `a`, the frozen comparison is:

`reduction_percent(a) = 100 * (baseline_upper_q(a) - candidate_upper_q(a)) / baseline_upper_q(a)`

The qualifying action identities and `3a6d191` baselines are frozen:

| Action identity | Baseline lower Q | Baseline upper Q | 10% qualification ceiling |
| --- | ---: | ---: | ---: |
| `lucent_fossil` | 439.802286464883 | 60341420.46413261 | 54307278.41771935 |
| `harvest_reforge_attack` | 433.790212170739 | 60341418.63180959 | 54307276.76862863 |
| `harvest_reforge_cold` | 444.9999006934262 | 60341428.5593793 | 54307285.70344137 |
| `harvest_reforge_elemental` | 478.1739685343979 | 60341463.119121544 | 54307316.80720939 |
| `harvest_reforge_mana` | 519.8297640859563 | 60341500.471414946 | 54307350.42427345 |
| `harvest_reforge_physical` | 444.9999347168957 | 60341429.2781554 | 54307286.35033986 |

The frozen full-four root and deterministic comparison fields are:

| Field | Baseline |
| --- | ---: |
| Root lower / executable upper | 432.4068529534326 / 60341416.98784247 |
| Discovered / expanded states | 200000 / 8908 |
| Rows / transitions | 56413 / 1188077 |
| Reforge work | 14246493 |
| Q-refinement rounds / selected states | 9 / 9216 |
| Rows reconsidered | 75 |
| Completed rows recomputed | 0 |
| Unique kernels / carrier reuse | 8 / 8907 |
| Actions admitted / non-improving / unresolved / unevaluated | 0 / 0 / 7 / 195969 |
| Remaining action envelope | 195976 |
| Live / peak solver-owned bytes | 83599787 / 150805909 |
| Cap boundary | `max_discovered_states` |
| Transition hash | `d3c8789915cd57b4` |
| Policy hash | `8b2a568f3c9cfd35` |

Root bounds, every completed action interval, discovery and expansion counts,
row and transition counts, reforge work, action lifecycle counts,
completed-row recomputation count, transition hash, policy hash, and cap
boundary are the deterministic Gate 1 parity fields. Wall time is reported
but is not deterministic.

The baseline, action set, formula, 10% threshold, admission tolerance, and
deterministic comparison fields do not change after this plan commit.
Movement below 10% is diagnostic evidence, not milestone success.

## Authority And Shared-Graph Contract

There is one upper-policy authority. New scheduling, occupancy, provenance,
and witness retention feed the existing focused upper solve, Howard policy
improvement, SCC properness checks, and exact fixed-policy evaluation. No
independent state-local solver or parallel upper-policy subsystem is allowed.

The solve reuses the exact parent graph:

- preserve strict state IDs, stable late-row ownership, retained delayed rows,
  and completed probability distributions;
- reuse exact kernels across compatible carriers;
- retain Chaos-support membership, exceptional Fossil/Essence support, and
  Eldritch delta-state handling;
- append completed rows to the shared graph and run Bellman/policy
  improvement over that graph;
- never introduce the rejected dense structural DAG, merge by satisfied-goal
  count, or copy scalar uppers between isolated solver contexts; and
- continue to report `completed_rows_recomputed: 0`.

Every unfinished nonterminal state keeps an explicit executable fallback:
local terminal continuation when certified, otherwise Restart followed by the
existing proper Chaos/constructive fallback. This coverage includes unmatched
compiled states, new or exceptional support, Eldritch delta states, choice
successors, unevaluated preferred actions, and actions invalidated later.

## Gate 1 - Observational Provenance Baseline

Before changing upper-policy behavior, add bounded deterministic telemetry
that samples the high-impact states explaining the current upper. Each sample
records:

- state ID and satisfied-goal subset;
- rarity and prefix/suffix occupancy;
- relevant preserved, fractured, crafted, protection, influence, and
  Eldritch state;
- promising root action and direct or policy-occupancy influence;
- current executable upper, selected upper-policy row, fallback source, and
  whether the continuation is local or Restart/Chaos;
- already-materialized candidate state-action pairs and the reason no cheaper
  executable continuation was installed;
- contribution to the promising parent row's upper Q; and
- policy-witness identity and properness status.

Samples use ascending deterministic tie-breaks after impact ordering, the
existing `max_diagnostic_samples` cap, and the existing telemetry JSON byte
limit. Telemetry reports explicit retained and omitted counts. Collection
must not influence scheduling, discovery, action generation, Bellman order,
policy selection, cap accounting, or resource accounting.

Run the primary frozen case with high-impact behavior disabled and require
exact parity for every frozen deterministic field above. A mismatch is a hard
stop until telemetry is observational.

## Gate 2 - Upper-Reduction Influence And Occupancy

Retain `P(s') * (U(s') - L(s'))` for proof-directed refinement. Add a distinct
upper-improvement score for state-action scheduling.

Immediate root successors use their exact root-row transition probability.
Deeper influence combines the probability of entering the candidate policy
from the promising root row with the expected visit count under the current
proper selected-row policy. Expected visits come from, or are validated
against, the engine policy graph and exact cyclic policy-evaluation machinery.
They may exceed one and are never clamped to a probability.

The deterministic scheduling estimate may combine influence, the current
state upper, the cheapest executable upper Q from materialized exact rows, a
non-certifying estimate for an unevaluated filtered action, and the reduction
required for strict root admission or the frozen 10% threshold.

Optimistic estimates guide work only. They cannot raise a lower, reject an
action, close an envelope, establish exactness, or replace an executable
witness.

## Gate 3 - Resumable State-Action Scheduling

Replace sequential carrier-envelope draining with deterministic resumable
state-action scheduling by expected upper reduction:

- preserve the existing goal-relevant filter and family interleaving;
- retain exact per-pair lifecycle state, cooperative stepping, caps, stable
  completed rows, and exact kernel reuse;
- advance one bounded state-action unit at a time, or another documented
  resumable unit already supported by the row evaluator;
- do not complete every Fossil, Harvest, Essence, Eldritch, bench, cleanup,
  protection, fracture, and finish for one carrier before another carrier can
  receive work; and
- leave skipped pairs explicitly unevaluated and unresolved.

Work order may change an incomplete bounded result but cannot change eventual
exact closure.

## Gate 4 - Shared Executable-Upper Loop

Within the gated incremental lifecycle:

1. select promising retained Fossil/Harvest root rows;
2. identify partial states and state-action pairs capable of reducing them;
3. schedule the highest-impact resumable units;
4. append completed exact rows to the shared graph;
5. install the existing executable fringe fallback;
6. run focused upper Bellman/policy improvement over the affected graph;
7. assemble one compatible selected-row candidate policy;
8. verify it as a proper absorbing Markov policy;
9. install only strictly cheaper executable continuations;
10. propagate improved state uppers through predecessor rows;
11. reprice every retained completed alternative without rebuilding its
    probability distribution;
12. admit only a complete upper Q strictly below the current executable
    incumbent by the existing tolerance; and
13. repeat while primary qualification remains reachable within the frozen
    budget.

Bellman search discovers multi-action continuations; no crafting sequence is
prescribed.

## Gate 5 - Compatible Proper Witnesses

Every installed or published upper identifies one compatible executable
witness containing:

- the selected row for every represented nonterminal state;
- explicit fringe fallback and reachable policy graph;
- state-graph, goal, start-carrier, economy/price, action-vocabulary, and
  transition-dependency identities;
- policy hash, properness result, exact evaluated state values, and
  invalidation version.

The candidate selected-action graph retains exact Restart, destructive
reforge, retry/self-loop, choice-group, Eldritch, and delta-state behavior.
SCC validation refuses a non-goal closed recurrent class. Proper cyclic
components are evaluated exactly. Properness must hold from every state whose
upper is published, not only the root.

State-local scalars from different witnesses cannot be combined. Individually
useful rows may be combined only after constructing and verifying their one
assembled selected-row policy. Any dependency change affecting legality,
cost, transitions, compilation, or fallback invalidates or recomputes the
witness.

## Gate 6 - Filtered Scope And Status

The action scope is the existing goal-relevant set:

- ordinary currency;
- Fossil;
- corrected Harvest;
- goal-relevant Essence;
- automatic Eldritch Annul/Chaos Prefix/Suffix when eligible;
- existing bench, cleanup, protection, fracture, and finishing actions
  retained by the product filter; and
- Restart fallback.

This boundary does not redesign the Fossil/Harvest/Essence filter and does not
add Veiled automation, Influence Exalt automation, new mechanics, or frontend
crafting authority.

A verified cheaper restricted policy is a valid upper improvement only.
Skipped pairs stay unevaluated, completed overlapping actions stay unresolved,
the action envelope stays open, and exactness stays blocked. The global lower
changes only through already-valid admissible proof machinery. Failure to find
a cheaper policy proves no global non-improvement.

## Gate 7 - Native Exact Controls

Native toy cases must prove:

1. a high-impact partial state initially uses Restart/Chaos fallback;
2. provenance telemetry is observational and deterministically capped;
3. shared-graph rollout discovers a cheaper multi-action continuation;
4. the continuation preserves existing goal progress;
5. its upper propagates backward and lowers a parent row's upper Q;
6. the retained parent distribution is not recomputed;
7. two individually materialized actions can form a useful combined policy;
8. immediate-root influence uses exact transition probability;
9. deeper influence includes expected policy occupancy;
10. a proper cycle can produce expected occupancy greater than one;
11. a proper retry cycle receives its exact expected value;
12. an improper nonabsorbing cycle is refused;
13. every published state upper retains a compatible policy witness;
14. incompatible scalar uppers are not combined without assembled-policy
    verification;
15. properness is valid from every published state;
16. unfinished fringe states retain executable fallback coverage;
17. a restricted upper improvement does not alter the global lower;
18. skipped state-action pairs remain unevaluated and unresolved;
19. strict upper-Q separation admits a row and triggers another Bellman cycle;
20. Q overlap remains honestly unresolved; and
21. repeated runs retain deterministic transition and policy hashes.

At least one properness case includes exceptional support and an automatic
Eldritch action so delta-state requirements cannot be bypassed.

## Gate 8 - Frozen Qualification And Evidence

Use the same artifact, economy, goals, and run-local limits as the completed
milestone:

- `max_reforge_work = 100000000`;
- `max_transitions = 10000000`;
- `max_solver_owned_bytes = 536870912`;
- `max_discovered_states = 200000`;
- `max_expanded_states = 25000`;
- `max_state_action_rows = 300000`;
- one cooperative work item per step;
- bounded 600-second primary watchdog; and
- partial-output snapshots.

Product defaults do not change.

Every measured repetition reports root bounds, all qualifying row Q values
and absolute/percentage changes, installed continuation states/actions,
preserved progress, expected occupancies, witness identity/properness,
distribution recomputation count, action lifecycle counts, graph/work/kernel
counts, memory, wall, cap hits, and transition/policy hashes.

Repeat the primary result to establish deterministic fields. Only after
qualification or final measured rejection, run one bounded deep-four
confirmation. If an artifact-backed or frozen policy materially changes,
compile the real ordinary strategy, preserve setup/currency/fallback
operations, run exact compiled-policy evaluation where supported, and run
10,000 simulator executions with success, illegal-action, unmatched-edge,
fallback-use, failure, exact-cost, and empirical-cost reporting.

## Gate 9 - Integration And Closure

Preserve unrestricted behavior, the goal-relevant filter, Fossil/Harvest/
Essence/Eldritch mechanics, automatic Eldritch side actions, economy
accounting, compiled strategy semantics, product defaults, manual Calculator
and Emulator behavior, bounded/open-envelope statuses, and deterministic
tie-breaking.

Run the appropriate complete native acceptance once after implementation.
If retained engine behavior changes, rebuild release WASM before running the
downstream non-visual web suite and TypeScript no-emit. Validate the standard
and natural-T1 benchmark manifests. Update the stable solver reference,
tracked evidence, evidence indexes, final report, archive indexes, and
HANDOFF. Validate JSON, Markdown links, `git diff --check`, commit scope, and a
clean final worktree.

## Commands

Gate 1 and qualification use a run-local corpus/case copy with only the frozen
diagnostic caps and the new opt-in behavior switch changed. The native runner
is invoked with:

```powershell
$native = Get-ChildItem build/engine -Recurse -Filter poecraft_solver_benchmark.exe |
    Select-Object -First 1
& $native.FullName `
    --artifact data/compiled/current `
    --corpus <run-local-manifest> `
    --case natural-t1-full-four-47d8b909aa88 `
    --output <full-output> `
    --partial-output <full-partial> `
    --goal-progress-gated-reforges `
    --progress `
    --skip-verification
```

The secondary command substitutes
`natural-t1-deep-four-low-probability-1a1102b0e06b`.

Final acceptance commands are:

```powershell
powershell -File scripts/build.ps1
ctest --test-dir build/engine --output-on-failure
& $native.FullName --artifact data/compiled/current `
    --corpus fixtures/solver-benchmarks/v1/manifest.json --validate-only
& $native.FullName --artifact data/compiled/current `
    --corpus fixtures/solver-natural-t1/v1/manifest.json --validate-only
powershell -File scripts/build-wasm.ps1
Push-Location apps/web
npm test
npx tsc --noEmit
Pop-Location
git diff --check
```

The exact native executable/configuration may add the new opt-in flag, exact
evaluation, strategy output, and 10,000-run verification arguments required
by a qualifying changed policy. No rendered or screenshot review is run;
Oliver owns visual review.

## Rollback And Stop Rules

If the implementation does not satisfy the frozen qualification:

- restore all experimental production scheduling and upper-policy behavior;
- retain bounded observational telemetry;
- retain a test only when it exposes an independently justified pre-existing
  bug;
- retain documentation and measured evidence describing the exact failure;
  and
- report the milestone as rejected, never successful.

Do not substitute larger caps for implementation. Continue within this
boundary through evidence-backed fixes to scheduling, witness compatibility,
properness, fallback propagation, and focused-upper integration after an
initial miss, small movement, cap, or failed combined policy.

Stop only for a genuine mechanic ambiguity requiring Oliver, an access
blocker, a consequential external action, a proven architectural impasse, or
completed final measured rejection under the frozen rule.

The milestone produces exactly two local commits:

1. this active plan/HANDOFF boundary and its indexes; and
2. the final retained implementation or restored rejection, tests, generated
   artifacts, evidence, archive, report, indexes, and no-active-boundary
   HANDOFF.

Both commits end with `Co-authored-by: Codex <codex@openai.com>`. Nothing is
pushed.
