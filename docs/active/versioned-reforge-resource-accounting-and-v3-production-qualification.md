# Versioned Reforge Resource Accounting And V3 Production Qualification

**Status: Gates 0-1 complete; Gate 2 observational baseline and matched
calibration is the active implementation boundary.**

Owner: Oliver

Parent: [Active work](README.md)

Starting commit: `00718908c866dc71a2e7e3864ebc5c5015ff063f`.

Branch: `codex/reforge-resource-accounting-v3-qualification`.

Source milestone:
[Certified Fallback Publication And Final-Depth Reforge Accumulation](../archive/2026-08-03-certified-fallback-and-terminal-reforge-factorization/README.md).

## Objective

Replace the heterogeneous interpretation of `reforge_work` with three
separate, explainable resource dimensions:

1. an evaluator-independent logical search envelope;
2. versioned evaluator-specific implementation effort; and
3. measured wall time, solver-owned memory, and cooperative-step latency.

Then reconsider exact V3 terminal factorization on those dimensions and make
it the production strict-row evaluator only if it satisfies every frozen
native and release-WASM qualification criterion. V1 remains the independent
exact oracle and rollback path. V2 remains diagnostic unless matched timing
unexpectedly reverses its existing negative result.

No universal weighted score may combine bucket probes, subset checks, hash
insertions, recurrence terms, time, or memory.

## Preserved boundaries and non-goals

This milestone does not change crafting mechanics, action filtering, goals,
state abstraction, quotient proof, partition or Bellman semantics, policy
compilation, strategy vocabulary, economy, or frontend crafting authority. It
does not address rare-renewal `1e-9` reconciliation, five-goal finalization,
or deferred checkpoint/replay.

No production or fixture cap may be raised to force a pass. Temporary
diagnostic caps must be named and isolated. A stopped or interrupted run is
never exact, and a partially completed row cannot publish certified output.
The certified executable Chaos-renewal fallback remains independently owned:
it may supply an upper, but cannot alter the lower or resolve alternatives.

## Gate 0 — frozen boundary and decision rules

**Status: complete.** Tracked evidence is
[`reforge-resource-accounting-v3-qualification-gate0.json`](../../fixtures/solver-reliability/v1/evidence/reforge-resource-accounting-v3-qualification-gate0.json).

The branch begins from clean source commit `0071890`. Gate 0 freezes the source
tree, native binaries, release WASM, compiled artifact, Mirage economy,
canonical reliability corpus and hard fixture, complete 27-action order, V1/
V2/V3 binding evidence, 20M hard evidence, selected-only 100M evidence, the
medium case, the ordinary/Essence/Harvest/Fossil equivalence matrix, empty and
fractured carriers, the identity-sensitive exclusion witness, and the existing
transition, policy, and compiled-strategy hashes.

The cap inventory deliberately distinguishes:

- `SolveOptions` and benchmark omission default to 50,000,000;
- a null C options pointer, a zero C field, or an omitted/zero WASM JSON field
  falls back to that native 50M default;
- the production TypeScript contract makes the field optional and adds no
  independent web default;
- the reliability generator and its canonical product cases explicitly use
  20,000,000, including the hard fixture;
- the frozen selected-only and Fracture qualification diagnostics use 100M;
- prior V2/V3 binding diagnostics used temporary 18M copies; and
- tracked benchmark, scaling, regression, baseline, natural-T1, reliability,
  web-smoke, and unit-test overrides remain inventory evidence, not product
  defaults.

Frozen V3 qualification criteria are:

- target maps, probabilities, Bellman values, and selected semantic actions
  match V1 exactly within the existing family-specific numerical tolerances;
- median native binding-row wall improves by at least 25% over V1;
- aggregate runtime across every eligible measured row does not regress;
- no nontrivial row regresses by more than 10% unless a simple deterministic
  pre-enumeration dispatch rule keeps that class on V1;
- peak solver-owned memory and longest cooperative step do not regress by
  more than 10%;
- release WASM retains cancellation and responsiveness headroom;
- no partial/interrupted row is certified; and
- the canonical fallback remains independently executable and does not affect
  selected closure, lower-bound, or unresolved-alternative accounting.

No test or benchmark is run at Gate 0. Commit this documentation/evidence
boundary separately before implementation.

## Gate 1 — versioned component accounting

**Status: complete.** Tracked evidence is
[`reforge-resource-accounting-v3-qualification-gate1.json`](../../fixtures/solver-reliability/v1/evidence/reforge-resource-accounting-v3-qualification-gate1.json).

Add a reusable saturating `ReforgeEffortBreakdown` whose fields are deterministic
physical counts, not a manufactured additive total. Retain the legacy V1, V2,
V3, active, and serialized counters for historical comparison.

The breakdown records:

- rows begun, completed, interrupted, and cache-reused;
- pool entries scanned, physical families, and roll buckets built;
- exclusion/group checks and availability construction;
- frontier nodes/states, dense bucket probes, availability words, eligible
  nonterminal edges, and terminal contributions;
- canonical terminal successors and duplicate contributions;
- raw-choice entries and identity-tree nodes;
- successor publication attempts, unique insertions, duplicate merges, and
  state interning;
- V3 predecessor indexing, denominator edges, subset checks, candidate sets,
  recurrence terms, and commits; and
- actual nested automatic-child work.

Every retained row sample has provenance for owner (`coarse`, `strict_selected`,
`strict_alternative`, or `exact_evaluation`), family (`ordinary`, `essence`,
`harvest`, `fossil`, or `automatic_option`), evaluator version, cache hit/miss,
and disposition (`completed`, `interrupted`, `discarded`, or `published`).
Exact aggregates survive sample truncation.

Hot-loop counts accumulate locally and merge transactionally into solve
telemetry. Counter arithmetic saturates deterministically. Samples and JSON
remain bounded, their selected capacities enter solver-owned memory, and no
counter participates in solver decisions.

Nested child contexts receive the parent's remaining relevant budget before
work begins. All executed child effort merges even if the child or parent later
refuses/discards the row; post-hoc parent saturation is not enforcement.

Focused contract coverage owns saturation, overflow, nested propagation,
interruption, forward/reverse enumeration, cache reuse, and completed-row
publication. Commit Gate 1 separately.

Gate 1 added one reusable saturating component breakdown without defining an
additive score. The existing active ledger and parallel V1/V2/V3 ledgers remain
available. Bounded row samples identify coarse, strict-selected,
strict-alternative, and exact-evaluation ownership; ordinary, Essence,
Harvest, Fossil, and automatic-option family; evaluator; cache reuse; and
transactional disposition. Exact aggregates remain authoritative when samples
truncate, and selected sample capacities are included in calculator, strict
adapter/result, and exact-evaluator owned-memory accounting.

Automatic admission and its protected-repeat comparison child now receive the
parent's remaining budget before work. Executed child effort merges on success
or refusal, and grandchild subtotals are not counted twice. This repairs nested
enforcement while leaving the interpretation and defaults of
`max_reforge_work`, production V1 selection, diagnostic V2, and disabled V3
production selection unchanged.

The focused calculator, S8.3 automatic, solve, and exact-evaluator entry points
pass with `125077`, `365`, `6083`, and `1176` checks respectively and zero
failures. Optional artifact sub-suites without supplied paths were disclosed as
skipped. No benchmark, canonical hard run, release-WASM build, or full
acceptance suite ran in Gate 1.

## Gate 2 — observational baseline and matched calibration

**Status: active.**

Keep production evaluator selection and cap behavior unchanged while capturing
representative accounting. Require unchanged legacy work, bounds, stop reasons,
transitions, policy hashes, and compiled artifacts. Measure the binding V1 row
with accounting disabled/enabled; instrumentation overhead must be at most 5%.
Reduce hot-loop overhead before proceeding if it exceeds that threshold.

Run V1, V2, and V3 from the same binary and identical input with at least three
native repetitions and report medians. Exercise a bounded release-WASM subset
where practical. The matched table must include every component count, row and
total wall, live/peak owned memory, longest cooperative step, transitions,
canonical successors, and policy/artifact hashes.

Explicitly demonstrate the historical inconsistencies without changing the
legacy ledger:

- Harvest guaranteed support is traversed more than once while charged as one
  scan;
- V1 charges all buckets even when control flow short-circuits;
- V2 and V3 count different operations; and
- nested automatic children can currently execute before parent saturation.

Commit the baseline/calibration evidence separately.

## Gate 3 — resource-cap contract

Prefer the following contract if the implementation proves V3 effort safely
bounded under the logical envelope plus existing memory/cancellation limits:

- `max_reforge_work` limits the stable V1 legacy-equivalent logical search
  envelope for every evaluator;
- raw V1 receives exactly its historical envelope;
- evaluator-specific effort is versioned and reported separately; and
- qualification is based on measured time, memory, and cooperative latency.

Changing evaluator must not reduce the rows admitted by the same logical cap.
Nested contexts receive only the remaining logical budget. Interrupted work is
observable but cannot publish; cache reuse is explicit and tested.

If the safety relationship cannot be proved, add a separate versioned
evaluator-safety cap and stop reason, propagate it through C ABI, WASM,
TypeScript, tests, and documentation, and preserve all existing defaults. If
neither contract is safe, retain Gate 1 accounting, leave cap behavior and V1
production selection unchanged, and report the blocker. Do not invent weights.

Commit Gate 3 separately.

## Gate 4 — V3 qualification and integration

Only after Gates 1–3 pass, enable V3 for production strict selected rows and
competitively scheduled alternative rows. It must stay generic across ordinary
reforges, Essence, Harvest, and Fossils. Do not add it to the coarse oracle
without separate justification. V1 remains independently selectable and V2
diagnostic.

Preserve transactional row publication, proof-store and partition authority,
invalidation generations, unresolved-obligation honesty, Bellman/properness,
fallback independence, and compiler provenance. A resource-interrupted V3 row
is discarded, never resumed or certified.

If a meaningful eligible class is slower, use only a simple deterministic
pre-enumeration feature rule supported by Gate 2. Do not branch on action,
modifier, or fixture names. Commit the production decision separately.

## Gate 5 — scaling and product qualification

After integration, run the focused exact-equivalence matrix across ordinary,
Essence, Harvest, and Fossil rows, including positive/zero Fossil weights,
Fossil additions/forced modifiers, targeted/exceptional Harvest support,
empty/fractured carriers, prefix/suffix/mixed and present-below-tier goals,
identity-sensitive exclusions, and forward/reverse enumeration.

Run one temporary selected-only 100M diagnostic and compare it with preserved
V1 evidence. Report logical envelope, V3 components, wall, memory, latency,
and transitions separately. Failure to reach a partition is not V3 incorrectness.

Run the canonical hard 20M case once, with all 27 actions, 1 GiB owned memory,
and a 900-second watchdog. Preserve lower/unresolved status, require fallback
publication when selected refinement caps, compile and exact-evaluate, and run
10,000 simulations with zero off-policy failures.

Run the representative native reliability portfolio. Do not rerun the
standalone five-goal watchdog unless bounded earlier evidence predicts a
material change. Preserve known rare-renewal tolerance misses unless values or
classifications regress.

V3 qualifies only if exactness, native performance, WASM performance, memory,
and latency all pass. Otherwise restore V1 production selection while retaining
the accounting and exact V3 diagnostic.

## Gate 6 — historical interpretation and release

Add a tracked audit classifying prior reforge-work conclusions as unchanged,
evaluator-version-only, superseded/recovered, reopened, or unresolved. Preserve
history rather than editing archives into hindsight.

At minimum record that V3 is reopened because its prior rejection compared
heterogeneous counters despite better wall time; V2 remains rejected because
it was slower; the V1 20M/50M/100M slope remains V1-only; capped alternatives
remain unresolved; shared-frontier wall/memory and structural density results
remain valid; and certified fallback publication remains valid and independent.

If browser-visible native behavior changed, rebuild release WASM. Then run one
final acceptance gate: native build/focused solver tests, benchmark validation,
the required reliability workflow, release-WASM worker/cancellation checks,
`npm test`, `npx tsc --noEmit`, and `powershell -File scripts/test.ps1` exactly
once. No rendered visual review is authorized.

Archive this plan with the final report, update stable solver/evidence docs,
clear HANDOFF to no active boundary, and commit locally with no push or merge.

## Final report questions

The final report must answer:

1. What did legacy `reforge_work` measure?
2. Which operations were missing or inconsistently charged?
3. What limits logical search breadth now?
4. What separately limits evaluator safety?
5. Is nested cap enforcement honest?
6. Is V3 exact for every supported reforge family?
7. Is V3 faster natively and in release WASM?
8. Was V3 integrated, conditionally dispatched, or rejected?
9. Does the hard case still return its certified executable fallback?
10. Which historical conclusions changed?
11. What remains responsible for selected-closure and five-goal scaling?
