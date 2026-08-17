# Replayable Exact Operation-Row Recovery

**Status: selected by Oliver on 2026-08-17. Gate 0 is active.**

Parent: [Five-T1 Restart-Monotone Strategy Recovery](README.md)

## Objective

Make the priced-base five-natural-T1 certificate reach exact evaluator closure
under the existing one-GiB solver/WASM budget by replacing retained full
24-byte routed operation transitions with an exact replayable/shared row
authority. The authority must feed closed-partition refinement, quotient-row
construction, component solving, exact raw attribution, and final route-flow
reporting without changing mechanics, probabilities, observation authority,
or publication truth.

This plan resumes only the stopped streamed-closure boundary. Parent successor
Gates 4-8 remain closed until Witness B is independently materializable.

## Binding Evidence And Prior-Work Audit

1. Exact online deterministic routing reduced Witness B to 35,828 raw pairs,
   but the sole 20-million probe retained 19,972,223 transitions in 1,007
   distinct rows. Another 7,233 expanded pairs already shared those rows.
2. Row payload was 479,377,920 bytes and route-trace payload was 80,202,800
   bytes. Comparable expansion projects raw closure near 84.5 million
   transitions, after the measured byte slope crosses one GiB.
3. Exact-kernel lookup/build used 156.6 ms in that probe; pair discovery used
   183.2 seconds and skipped 124.5 million deterministic router edges. A
   representation that blindly reroutes every outcome on every downstream
   pass can fit memory while still being unusably slow.
4. `OutcomeDistribution.entries` are sorted exact `(state, probability)`
   authorities. Stable reforge kernels are already shared by collision-checked
   observation signatures, and evaluator rows are already shared by immutable
   distribution identity where that contract applies.
5. `refine_closed_probabilistic_partition_replay` is the retained split-only,
   collision-safe fixed-point and lumpability authority. It supports one-level
   shared arc sources. Reuse it; do not create a second quotient algorithm.
6. Exact attribution currently retains the raw graph and constructs a full
   transpose. Removing discovery rows without replacing this consumer is not a
   complete solution.
7. Previously rejected or completed work is not retried: reconstruct-then-
   merge beyond the cap, open-graph aggregation, lossy observation keys,
   another pair/index compaction, cap-only growth, broad shared reforge
   frontiers, derived transition-state authority, or whole-run checkpoint and
   replay.

## Invariants

1. Every replayed probability, selected source edge, deterministic-route
   trace, successor pair, absorption, and concrete policy state remains exact.
   Hashes select candidate buckets only; full equality is authoritative.
2. Replay order and aggregation use the existing sorted outcome order and
   `WideFloat` accumulation rules. No epsilon grouping, probability truncation,
   optimistic pruning, or unfinished-graph quotient receives proof authority.
3. A replay recipe must name enough immutable authority to regenerate the same
   row after state-local outcome caches are released. Pointer identity alone
   is valid only for a retained stable shared kernel.
4. Discovery, partition, quotient conversion, component solve, attribution,
   and final reporting share one row vocabulary. No phase may silently fall
   back to materializing the complete raw graph.
5. All retained and transient recipe, cache, replay, partition, and solve
   storage participates in the existing owned-byte cap.
6. Certification remains fail closed. The six-node Chaos fallback remains the
   publication until the preferred candidate completes independent exact
   evaluation and reconciliation.
7. Do not change caps, action admission, prices, Bellman ordering, compiler
   graph semantics, or mechanics. Do not build release WASM or run the full
   acceptance pipeline before the native candidate closes.

## Gate 0 - Exact Row Ownership And Replay Census

Add behavior-neutral, cap-accounted aggregate telemetry for unique evaluator
rows, split by operation action/family and by stable-shared versus replayable
state-local kernel. Record:

- unique rows and pair reuses;
- exact outcome entries, routed transitions, and absorptions;
- retained distribution bytes, routed-row bytes, and route-trace bytes;
- repeated `(route root, state)` lookups and distinct results, so a route cache
  is selected only if it has measured reuse;
- exact-kernel, source-edge selection, deterministic routing, pair lookup, and
  row aggregation active time; and
- the projected bytes of a minimal replay token plus immutable route authority.

Add focused native tests for stable-shared, state-local, absorption, and
resource-stopped rows. Existing values, hashes, counts, cap classifications,
and output must remain unchanged apart from the new diagnostics.

Exit with exactly one selected representation. It must attack both the
projected 84.5-million-entry closure and downstream replay cost. If no exact
representation projects below the byte and time boundaries, stop here with a
measured handoff rather than layering speculative carriers.

## Gate 1 - Replayable Shared Row Authority

Implement the Gate 0 selection as one exact row authority. The expected shape
is a small immutable recipe plus a compact per-outcome routing token or another
equally proved carrier; this is not binding until the census measures it.

The implementation must:

- discover every successor and absorption exactly once;
- share recipes only under complete equality/immutable-kernel authority;
- release the 24-byte routed payload after its last owning use;
- replay without inserting unseen states or pairs after raw closure;
- validate replayed mass and exact target/edge/route parity against the legacy
  materialized row on bounded oracles; and
- remain cooperatively step-able, with no suspension in exception handlers.

Keep a materialized reference mode for small native tests. Prove byte-for-byte
row parity on direct, long-router, shared-kernel, modifier-offer, checkpoint,
terminal, no-matching-edge, and deterministic-cycle cases.

## Gate 2 - Partition And Quotient Conversion

Feed replay recipes into the existing replay partition. Preserve observation,
immediate, label, and one-level arc-source semantics. The completed partition's
final lumpability proof remains authoritative.

Construct only quotient rows after the partition is complete. Compare the
materialized and replayed paths on bounded graphs for class assignment,
rounds, lumpability checks, quotient rows, values, terminal mass, edge flow,
and hashes. A resource-stopped open graph remains unevaluated.

## Gate 3 - Component Solve And Exact Attribution

Replace raw-row consumers that still require the complete carrier, including
pass-through analysis, SCC/component construction where it precedes the
quotient, shared-row exact attribution, transpose construction, reconstructed
raw flow checks, and final route replay. Prefer streamed mat-vec/transpose
construction from shared row authorities; do not retain a second full edge
graph.

Acceptance requires exact raw-pair and shared-row occupancy equations,
quotient-flow reconciliation, action/material accounting, terminal and
failure mass, top classes, edge traversals, and route-trace flow to match the
materialized reference within the existing numeric contracts.

## Gate 4 - Bounded Real Decision

After focused native build, evaluator, and solver tests, run Witness A once and
Witness B once at the checked ten-million cap. Use Gate 0 measurements to
choose at most one higher probe whose projected memory and five-minute runtime
fit. Restore checked fixture caps immediately afterward.

Witness A must remain independently exact at `624800.9519118543`, success one,
zero off-policy mass, and preserve its paired-default contract. For Witness B,
record closure state, raw/refined pairs, recipes, replay tokens, partition
rounds/classes, component sizes, exact cost, success/off-policy mass, owned and
peak bytes, stage timings, and largest cooperative step.

If Witness B becomes independently materializable, return to successor Gates
4-8. If it does not, stop with the precise new owner. Do not run release WASM,
web acceptance, or the full repository pipeline at this gate.

## Checkpoints

Use coherent local commits for the selected plan, Gate 0 evidence, row
authority, partition integration, attribution integration, and the real-case
decision. Every commit ends with the required Codex co-author line. Nothing is
pushed unless Oliver asks.
