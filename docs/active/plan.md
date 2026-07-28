# Harvest Natural Pools And Shared Exact Reforge Frontier

**Status: active (2026-07-28).**

Owner: Oliver

Branch: `codex/harvest-shared-reforge-frontier`

Starting source: `94fb013` (`main`)

Progress: Gate 1 is complete. The shared targeted-natural Harvest correction
passed artifact-backed native acceptance (`515,093` checks, zero failures).
Gate 2 is the current boundary.

## Objective

First implement Oliver's 2026-07-28 Harvest mechanic ruling across sampled,
exact, solver, and debug paths. Then test and, only if qualified, integrate an
action-independent exact structural frontier shared by support-compatible
destructive reforges while preserving separate action probabilities, costs,
and Bellman choices.

The unrestricted exact solver remains the default. Goal-progress gating,
action filtering, mechanics outside the selected Harvest correction, and
product caps remain unchanged during the initial measurement.

## Owner Mechanic Ruling

Harvest reforge, Harvest augment, and Harvest resistance conversion select
targeted modifiers from the ordinary naturally rollable pool:

1. positive spawn weight;
2. positive generation weight;
3. the requested target tag; and
4. ordinary final roll weight.

Zero ordinary generation weight always makes a modifier unavailable. Harvest
must not substitute generation percentage `100` or revive such a modifier.
No other mechanic behavior may be inferred during this milestone.

## Gate 0 — Boundary And Baseline

Before source edits:

1. map the mechanic change through sampled actions, exact calculation, solver
   registration/heuristics, pool/debug reporting, WASM-visible behavior,
   tests, and documentation;
2. pin the starting commit, compiled artifact, frozen corpus, caps, compiler,
   and machine;
3. identify one shared targeted-natural pool authority and every former
   `HarvestSpawnOnly` caller;
4. capture sequential Chaos, Lucent, Jagged, and retained goal-relevant
   Harvest reforge evidence for the two frozen four-mod cases; and
5. define deterministic parity fields before changing work accounting.

## Gate 1 — Harvest Correction

Implement one shared targeted-natural pool mode used by:

- sampled and exact Harvest reforge;
- sampled and exact Harvest augment;
- sampled and exact Harvest resistance conversion;
- solver feasibility and optimistic helpers that model those pools; and
- pool/debug reporting.

Add a focused fixture containing a modifier with positive spawn weight and
zero generation weight. Prove it is absent from every affected operation in
sampled and exact paths, while positive-generation controls remain reachable.

Exit: focused Harvest regression and sampled/exact parity pass. Update the
mechanic contract before any structural-frontier source work begins.

## Gate 2 — Shared-Frontier Prototype

Refactor the exact destructive-reforge evaluator around an
action-independent structural DAG/frontier. A compatible preserved-base and
support signature owns:

1. roll states and target-count stages;
2. eligible structural edges;
3. exclusion-group occupancy and blocker effects;
4. goal/junk classification;
5. successor identities and exact state interning; and
6. explicit topology/seed deltas.

Every action retains a separate probability lane, cost, final distribution,
and Bellman row. Each lane computes its own remaining eligible weight and
normalized edge probability at every node. Chaos probabilities are never
reused or rescaled after the fact.

Initially cover:

- Chaos;
- every goal-filtered Fossil with a compatible signature; and
- every retained Harvest reforge.

Fossil weight changes and disabled mods are lane weights. Added mods are
topology extensions. Forced mods are deterministic seeds that consume
capacity, occupy groups, and may satisfy or block goals. Special flags and
preserved-base differences remain in compatibility. Unsupported shapes use
the existing evaluator unchanged.

Essence, Veiled, Eldritch, Harvest augment/resistance, and all other
non-reforge actions stay outside the shared frontier.

## Gate 3 — Exactness And Resource Qualification

Compare shared and sequential evaluation for:

- Chaos plus Lucent on
  `natural-t1-full-four-47d8b909aa88`;
- Chaos plus Jagged on
  `natural-t1-deep-four-low-probability-1a1102b0e06b`; and
- retained goal-relevant Harvest reforges after Gate 1.

Required proof:

1. exact per-action outcomes, probability bits, deterministic hashes, and
   total mass agree;
2. each action remains a distinct Bellman option;
3. shared successors never collapse future-distinguishable states;
4. topology construction, lane propagation, projection/interning, wall time,
   and memory are measured separately; and
5. savings reflect real computation/allocation rather than hidden or
   undercounted per-lane probability updates.

Instrument structural nodes/edges, cache hits/misses, lanes, scalar
lane/edge updates, successor projections/interner hits, attributed work,
peak solver-owned memory, and group/lane wall time.

Keep current caps for initial measurement. Continue to production integration
only if parity holds and real resource use materially improves. If faithful
lane work still exceeds a cap, propose only the smallest evidence-backed
accounting or cap adjustment and record it explicitly.

## Gate 4 — Integration Or Restoration

If qualified, integrate shared evaluation for all filtered compatible Fossils
and Harvest reforges, retain the sequential fallback, and measure whether the
complete filtered root envelope finishes and partial-state expansion begins.

If propagation remains effectively additive, exact parity fails, or memory is
unacceptable, restore experimental production changes, retain the Harvest
correction and measurements, archive a clear falsification result, and stop.

## Gate 5 — Closure

Run the appropriate native acceptance once after the complete selected work.
Because Gate 1 changes browser-visible mechanic behavior, rebuild release
WASM and run affected non-visual downstream acceptance. Rebuild any other
derived artifact required by the implementation rather than editing it.

Update stable Harvest, engine/solver, evidence, active/archive, and handoff
documentation. Commit locally with:

`Co-authored-by: Codex <codex@openai.com>`

Do not push.

## Stop Conditions

Stop only for:

- a genuine mechanic ambiguity not answered by Oliver's ruling;
- a hard exactness/resource falsification in Gates 2–4; or
- a consequential action outside this boundary.
