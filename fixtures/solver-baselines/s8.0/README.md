# S8.0 exact solver before-state

This directory is the versioned S8.0 reproducibility and review-contract
checkpoint. It records existing behavior; it does not change solver decisions,
action generation, pruning, Bellman logic, policy compression, strategy
execution, or crafting mechanics. The raw ordinary strategy remains the only
execution authority.

## Selected cases

The baseline reuses all five S7 benchmark cases without changing their
fixtures:

- `oracle-real-one-mod` and `oracle-real-two-mod` retain the small exact S7
  oracle coverage and make basic before-state changes easy to detect.
- `ordinary-es-bench` retains ordinary currency plus bench-action coverage.
- `advanced-es-resist-bench` retains the broader resistance/bench policy and
  action-control diagnostic surface.
- `endgame-fractured-es` retains the archived S7.6 endgame graph and its
  Fracture-adjacent starting state. It was not rerun after long captures were
  abandoned at Oliver's direction.

Three narrow, mechanical cases use existing S7 option contracts rather than a
new subjective strategy-quality corpus:

- `s8-fracture-prepare` captures the existing `fracture_prepare` option.
- `s8-temporary-bench-blocker` records the current bench-blocker envelope's
  truthful `not_converged` result; it produced no strategy or Simulator run.
- `s8-protected-metamod-reforge` records the attempted existing
  `protected_repeat` option. The long Chaos and Harvest Fire attempts were
  abandoned before a report was written, so no solver result, strategy,
  evaluator result, or Simulator run is claimed.

`manifest.json` locates the normalized case records. Each case record contains
the repository revision; source fixture and hash; complete session, start,
goal, threshold, solver option, action envelope, economy, price-source,
limit, tolerance, seed, and command configuration; solver result and value;
ordinary graph identity where one exists; exact evaluator outcome; Simulator
aggregate and run count; and action inclusion/deferred/pruned/unpriced/
unsupported diagnostics where the solver produced them.

## Captured identities and results

Current captures use before-state commit
`b9e426b7e45feb4b254a56cbf3fbf43239a51261`, native ABI 1 under GCC 14.2.0,
artifact schema 4/database schema 2, source-data hash
`93c97d879e11b2022fc272b4d51c6336c3656151cd758ec1c2427d8f74bfc615`,
game-data hash
`af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5`,
and strings hash
`654a55489b47740672aea5a27ef52f01e23efc3bf5ce337be5a58756b72f955c`.
The canonical SQLite and compiled-manifest hashes are recorded separately in
every current case.

The archived endgame case uses S7.6 commit
`f2678b137f9fceff6b37b6ba5c080f8f6b686f66`, artifact schema 3/database
schema 1, and its historical artifact hashes. This distinction is deliberate:
the old S7 corpus pins pre-Bestiary game/strings hashes and cannot be presented
as a current B1.4-artifact capture.

| Case | Solver value | Graph nodes/edges | Exact evaluator | Simulator |
| --- | ---: | ---: | --- | --- |
| `oracle-real-one-mod` | 8.02014428412885 | 15 / 94 | unsupported (`mod_count`) | 10,000; 100% success; mean 8.14047 |
| `oracle-real-two-mod` | 356.45917785234406 | 19 / 116 | unsupported (`mod_count`) | 10,000; 100% success; mean 357.11403 |
| `ordinary-es-bench` | 741.5018555381404 | 3,457 / 19,437 | unsupported (`mod_count`) | 10,000; 100% success; mean 753.02685 |
| `advanced-es-resist-bench` | 4911.464629420442 | 2,917 / 22,465 | unsupported (`mod_count`) | 10,000; 100% success; mean 4878.5372 |
| `endgame-fractured-es` | 132353.19529787666 | 619 / 5,124 | not requested in archived S7 run | archived 10,000; 99.42% success; mean 130724.1208 |
| `s8-fracture-prepare` | 44.915464317950615 | 7 / 10,369 | unsupported (`mod_count`) | 10,000; 100% success; mean 44.7843 |
| `s8-temporary-bench-blocker` | unavailable (`not_converged`) | none | not completed | 0 |
| `s8-protected-metamod-reforge` | unavailable (abandoned) | none | not run | 0 |

Thus, newly captured compiled-strategy verification totals 50,000 Simulator
executions. The referenced archived endgame sample contributes another 10,000,
for 60,000 executions represented by the baseline. No B1.5 Imprint verification
or unrelated acceptance run was performed.

## Versioned contracts

- `contracts/review-projection.schema.json` defines non-executable derived
  sections and descriptive roles. Every section and entry maps explicitly to
  raw node or edge ids, while the raw graph hash and
  `raw_strategy_only` execution authority pin the executable document.
- `contracts/action-accounting.schema.json` defines native action descriptors,
  price key/quantity/unit price/cost contribution, raw-node and optional review
  section mapping, totals/reconciliation, and the retry, restart, setup,
  cleanup, protection, temporary blocker, Fracture, and finishing
  classifications. It is a contract/example only; S8.4 behavior is absent.
- `contracts/trimming-provenance.schema.json` defines the parent hash,
  discovery parameters/seeds/run count, threshold, removed raw ids, visitation
  mass, exact impact result, independent validation parameters/seeds/confidence,
  and a nonzero unvisited-branch upper bound. It requires the explicit
  `empirically_trimmed` heuristic marker.

The trimming fallback is pinned to serialized `{"type":"restart"}` with kind
`Restart` and `explicit_user_choice_required`. It cannot be chosen silently.
The example is a no-op provenance document; no trimming is implemented here.

## Documented pre-existing mismatches

- The archived S7 corpus manifest pins pre-Bestiary game/strings artifact
  hashes. The S8.0 current-capture corpus therefore versions the current exact
  B1.4 hashes while reusing the unchanged S7 case files.
- Current compiler-produced ordinary graphs use compiler-only `mod_count`
  routing. Calculator exact evaluation refuses that condition, including for
  the two oracle cases. The refusal is captured verbatim instead of changing
  strategy compilation or evaluator semantics.
- The temporary-blocker case does not converge under the current fixed
  policy-improvement limit. Automatic blocker assembly remains S8.3 work.
- The archived endgame Simulator sample has 0.9942 success against its
  historical 0.995 threshold. It is recorded, not repaired.

Focused validation lives in
`tools/ingest/tests/test_solver_s8_baseline.py`. It reloads every baseline JSON
and compressed ordinary graph, verifies hashes and graph identities, resolves
every projected reference, reconciles the accounting example to the existing
deterministic Restart evaluator totals, checks the contract classifications,
and requires complete trimming provenance plus the explicit Restart fallback.

## S8.1 derived review projections

S8.1 adds deterministic display-only documents under `review-projections/`.
They are derived over the frozen S8.0 strategy files; none of the compressed
ordinary strategies or their recorded hashes changed. Generate them with:

```text
py -3 -m poecraft_ingest.solver_review
py -3 -m poecraft_ingest.solver_review --check
```

The selected representatives reuse existing evidence rather than creating a
strategy-quality corpus:

- `oracle-real-one-mod` is the small exact oracle policy.
- `ordinary-es-bench` shows ordinary Alchemy/Scour rolling, Annul recovery,
  and a deterministic goal bench finish.
- `s8-fracture-prepare` shows the existing exact preparation/Fracture option
  expansion and retry routing.
- `endgame-fractured-es` shows a fractured prefix carrier, Fossil rolling,
  recovery actions, and deterministic bench finishing in the archived endgame
  graph.

Derivation is deliberately conservative and uses only serialized strategy
facts plus an exact evaluator result when one is available:

1. The source file's exact bytes are SHA-256 checked before decompression or
   projection. The projection records that source path/hash and
   `raw_strategy_only` authority.
2. Success-route `has_mod_family` conditions define the common goal subset.
   Incoming exact route conditions describe satisfied goal subsets, crafted or
   fractured goal state, active item flags, and crafted non-goal blockers.
   When a success route does not constrain crafted/fractured status, the label
   says that it is unconstrained instead of inferring a status.
3. Serialized operation vocabulary supplies descriptive roles: renewal rolls
   are `rolling`; Fracture is `fracture`; Scour/Restart are `restart`; Annul and
   restore are `recovery`; crafted-mod removal is `cleanup`; Exalt/Augment and
   similar preparation are `setup`. Bench goal mods are `finishing`, prefix or
   suffix locks are `protection`, cannot-roll metamods and non-goal bench mods
   are `temporary_blocking`, and Multimod is `setup`.
4. A goal bench operation is called deterministic-finishing-ready only when
   every exact incoming route already satisfies all other goal families and
   does not satisfy the bench target. Active prefix/suffix lock conditions may
   describe the goal subset being preserved. With no goal family on any exact
   incoming route, a rolling entry is labelled a disposable carrier.
5. Raw control-flow strongly connected components are atomic sections. The
   compiled policies use a central exact-state router, so the whole retry SCC
   stays in one section while entries expose its different operation/state
   roles. Cross-section raw routes receive their own edge reference; a backward
   boundary is accepted only as recovery, restart, or cleanup.
6. Every raw node and edge belongs to exactly one generated section, every
   entry has a raw reference, every cross-section route has an edge entry, and
   all references are re-resolved on reload. Stale hashes, unresolved ids,
   split retry SCCs, or unlabelled backward routes are refused.

Projection labels, entry order, and section order never enter the engine API.
Focused native binding validation compiles and executes the same verified raw
oracle bytes before and after relabelling/reordering projection metadata and
requires byte-identical deterministic results. The existing Calculator
`mod_count` unsupported result remains attached to the same raw strategy
identity; S8.1 does not change or reinterpret it.
