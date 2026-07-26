# Gap-Directed Natural-T1 Solver Research

**Status: archived after Oliver accepted the Gates 0–5 conclusion on
2026-07-25. This historical research plan does not authorize further work.**

Parent: [Archive record](README.md)

Report: [Gap-directed natural-T1 research report](report.md)

## Objective

Determine the smallest exactness-preserving architecture change most likely to
improve certified bounded and near-optimal policies for items with three or
four natural T1 goals under browser-sized time and memory budgets.

The product objective is useful anytime behavior, not fastest exhaustive
closure:

- find a proper executable policy and finite upper bound `U` early;
- raise the certified optimal-cost lower bound `L`;
- spend later work where it can materially lower `U` or raise `L`; and
- reach requested gap targets sooner without weakening `L <= J_pi <= U` or
  changing the eventual exact answer.

Exact small cases remain correctness controls. Exact closure on the selected
three-/four-T1 cases is not an acceptance requirement.

## Questions

1. What determines time to the first finite `U`, and why does the incumbent
   remain expensive?
2. Which current relaxation owns `L`, where does it lose information, and
   which admissible abstraction gives the best lift per unit of work?
3. Which states, actions, and outcomes create the discovered/expanded gap and
   consume rows, transitions, reforge work, and memory?
4. Can unseen actions retain a safe lower envelope and be materialized by an
   exact separation/violation test only when they could improve the solution?
5. What allocation of upper-policy, lower-proof, and exploration work closes
   the start-state gap fastest?
6. What one production milestone has the best evidence-backed chance of
   improving time-to-gap?

## Starting Evidence

Historical evidence is input, not a conclusion:

- one three-goal case hit 25,000 discovered states after 2,122 expansions at
  approximately `L=140`, `U=58,065`;
- native arithmetic did not reproduce the browser wall or quadratic scaling;
- exact occupancy-gap scheduling reached fracture gateways but not their
  locked continuations; and
- forcing one post-lock state exhausted reforge work, so a state quota alone
  was rejected.

See the archived
[exact guided-search review](../2026-07-22-exact-guided-search-design/README.md).
The earlier raw scaling report remains under
`build/solver-scaling-goal-slots/report.md`. Capture fresh baselines before
accepting any historical cause.

## Scope And Hard Stops

In scope:

- existing T1-only natural-goal fixtures;
- bounded native baselines and finalist headless-WASM confirmation;
- bound, policy, graph, action/outcome, memory, cache, and step telemetry;
- diagnostic-only instrumentation when Gate 1 identifies an otherwise
  unanswerable causal question;
- offline or isolated prototypes for better constructive uppers, admissible
  stochastic abstractions, and exact partial-action generation; and
- primary-source review of relevant SSP/MDP algorithms.

Out of scope:

- production behavior, defaults, caps, ABI, mechanics, action scope, compiler,
  UI, tracked fixtures, or corpus changes;
- approximate values presented as certified bounds;
- pruning inferred only from observed action non-use;
- tier-range generation; the natural-goal generator is intentionally T1-only;
- the exact natural two-T1 oracle;
- economy ingest, repair, refresh, publishing, fixtures, snapshots, or prices;
- browser visual review; and
- unrelated cleanup or optimization.

If a Path of Exile mechanic is ambiguous, stop for Oliver. Do not research or
infer the ruling.

## Process And Evidence Contract

The selected source baseline is clean `main` commit `28324d7`. Execute on
`codex/gap-directed-natural-t1-research`. At Gate 0 verify the branch, clean
tree, HEAD, source-baseline ancestry, and every changed file since `28324d7`.
Only this plan's documentation changes are expected. Any unexpected source,
fixture, compiled-data, WASM, or economy delta is a hard stop.

Every build, benchmark, evaluation, simulation, and test process uses a
detached 900-second watchdog with process-tree termination and a survivor
check. Record command, environment, start/end time, exit code, timeout,
survivors, wall time, log hash, and output hash. A timeout is data; a surviving
process is a failure.

Put raw tools, manifests, logs, and reports under
`build/gap-directed-natural-t1-research/`. Run-local inputs may change only
case selection and declared caps/gap targets. Do not edit tracked fixtures.
Capture fresh uninstrumented baselines before instrumentation or prototypes.

Diagnostic source must be behavior-identical, separately committed, and
verified by stable bounds, statuses, counts, and hashes. Research prototypes
do not become production source in this task. Keep `HANDOFF.md` current after
each gate and preserve negative results.

## Portfolio And Primary Metrics

Before reading fresh solve results, select cases using committed metadata and
pin IDs, hashes, goals/sides, base/class, probability stratum, caps, artifact,
and economy identity. Use at minimum:

- the existing three-goal smoke case;
- two distinct three-goal full/deep cases; and
- two distinct four-goal full/deep cases.

Use small exact synthetic/regression cases for proof parity. Never substitute
the prohibited natural two-T1 oracle. A loose bounded two-goal case is allowed
only for a named regression question and must never be changed into that
oracle.

Exploratory single runs may choose finalists. Final comparisons require one
warmup and three measured native repetitions under identical inputs and
budgets. Use one hard-case worker unless owned-memory evidence proves safe
concurrency.

The primary result is the complete `L(t), U(t)` curve, not final wall time.
Record:

- time to first finite `U` and every reached standard gap threshold;
- `L`, `U`, `J_pi`, gaps, status, termination, and caps;
- expanded/discovered/frontier states, focused rounds, and policy evaluations;
- rows, outcomes, transitions, reforge work, caches, and discoveries by action
  or outcome family;
- incumbent kind, action counts, policy reachability, and compilation size;
- live/peak owned bytes and allocation category;
- solve-step count/median/p95/max, timer leaves, and fallback components; and
- transition/policy hashes where comparable.

Report bound improvement per wall time, owned byte, expanded/discovered state,
action row, and reforge work.

## Gates

### Gate 0 — Boundary

Verify provenance and tools, inspect current solver/benchmark/WASM ownership,
create and smoke the watchdog, and hash every executable, artifact, manifest,
and script used later. Stop on any mismatch; do not repair or regenerate
inputs inside this plan.

### Gate 1 — Fresh Baseline

Build native through the watchdog:

```powershell
powershell -File scripts/build.ps1
```

Pin and run the portfolio with the tracked corpus runner. Produce bound curves,
policy/action summaries, graph/work/memory attribution, and a per-case
assessment of whether `U`, `L`, or graph generation is the largest observed
obstacle. Change no source. Name any missing diagnostic before Gate 2.

### Gate 2 — Causal Attribution

If needed, add only the missing counters identified in Gate 1. Likely useful
measurements are new-state contribution by action/outcome family, materialized
rows never selected by either bound policy, incumbent candidate provenance,
lower-bound components by state stratum, or eventual action rank under
available optimistic scores.

Replay affected baselines and prove instrumentation parity. End with a ranked
causal diagnosis; do not select an algorithm merely because it appeared in the
starting evidence.

### Gate 3 — Adaptive Candidate Studies

Study only the two leading causal paths:

- **Upper:** audit existing renewal/progressive-fracture candidate generation,
  validation, refresh, and exact cost. Test only compositions of implemented
  operators; do not invent mechanic behavior.
- **Lower:** prototype exact abstractions over goal subset, side/capacity,
  rarity, blockers, crafted state, and fractured progress. Incomplete action
  sets are ordering evidence only unless a complete admissibility proof exists.
- **Partial actions:** replay or isolate expansion with initially promising
  actions, specifying an admissible unseen-action envelope and exact
  separation rule. Measure avoided work, delayed useful actions, separation
  scans, repricing/vocabulary invalidation, and small-case counterexamples.

Reject partial-action generation if its envelope or separation rule cannot
support an exactness proof.

An independent ChatGPT Pro literature report may be supplied after local
diagnosis. Retain its prompt/output under `build/`, verify material claims from
primary sources, and map proposals to current engine invariants.

### Gate 4 — Coupled Prototype

Run only if Gate 3 identifies a compatible pair or one dominant candidate.
Build the smallest isolated prototype that can measure bounded utility while
keeping mechanics, prices, action scope, public caps, Bellman meanings, and
certificates unchanged.

Require small-case exact parity, adversarial bound checks, deterministic
repetition, equal-budget portfolio curves, complete work attribution, and
headless release-WASM confirmation when native evidence predicts browser
relevance. A failed prototype is an accepted result. No prototype becomes
production source.

### Gate 5 — Decision And Handoff

Deliver a short report containing the pinned portfolio, complete
baseline/candidate table, bound/work/memory trends, dominant causes, rejected
ideas, counterexamples, proof/repricing/WASM obligations, and a ranked
candidate list. Recommend one production milestone with acceptance cases and
commands.

Archive this plan/report only after Oliver accepts the research conclusion.
Update stable docs only with verified durable findings. Commit diagnostic
source/evidence separately from final documentation, update `HANDOFF.md`, and
leave the branch clean.

## Acceptance

The research is complete when:

- fresh evidence covers the pinned three-/four-T1 portfolio;
- every material claim traces to raw reports and hashes;
- upper, lower, and graph-generation causes are separated;
- the two leading candidates have measured or counterexample-backed outcomes;
- any recommended bound or partial-action method has explicit proof
  obligations and exact controls;
- the recommendation is expressed in time-to-certified-gap terms;
- all watchdogs report no survivors;
- documentation links/reachability pass; and
- no production/default/cap, tracked-fixture, or economy change remains.

Oliver selects, rejects, or revises the recommended implementation milestone
in a later task.
