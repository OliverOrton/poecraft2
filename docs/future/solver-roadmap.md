# Solver Roadmap

**Status: deferred, non-executable roadmap.** This preserves unresolved work;
it does not select the next chunk, authorize implementation, or retain the old
S8 sequence as current momentum.

Parent: [Future work](README.md)

## Current Boundary Record

The 2026-07-20 [Exact Solver State Scaling archive](../archive/2026-07-20-solver-state-scaling/README.md)
supersedes the former normal-cap failure. Two accepted complete product cases
now close, compile, evaluate exactly, and pass their required 10,000-run
verification. The measured smallest sufficient defaults are 200,000
state/search entries, 1,215,000 rows, and 11,000,000 reforge work; unrelated
resource and compiler caps stayed fixed.

Exact quotienting is useful but action-scope dependent: the bounded Chaos
oracle reduces 57,722 strict states to 3, while the complete product envelopes
permit no merge. Approximate compact mode remains forbidden. The completed
[focused-round performance attribution](../archive/2026-07-23-focused-round-performance/README.md)
confirmed that the 256-state global batch causes repeated whole-graph work, but
accepted no default change because every tested tuple exceeded the fixed
5-second solve-step maximum. Oliver has now selected
[gap-directed natural-T1 solver research](../active/gap-directed-natural-t1-research.md)
to diagnose certified bounded/near-optimal behavior on three-/four-T1 cases.
That research does not authorize a production solver change.

The 2026-07-21
[action/state pruning archive](../archive/2026-07-21-solver-action-state-pruning/README.md)
supersedes the three-slot discovery bottleneck: exact producibility filtering
and a price-bound constructive finish certificate reduce that accepted case to
two strict states without changing caps. The natural two-T1 product remains a
broad exact control. Broader certificates are optional future work, not an
active continuation.

The 2026-07-21
[real three-T1 diagnostic](../archive/2026-07-21-real-three-t1-diagnostic/README.md)
separates the seeded carrier from a genuine empty-rare start. The real case
hits 200,000 discovered states with no finite executable upper and no exact
quotient merge. If Oliver selects its optimization, the measured order of
attack is: construct an exact compositional renewal/finish policy to supply a
finite upper; use that upper for exact frontier/state dominance; and reduce
temporary-bench synthesis through carrier-signature reuse or earlier exact
rejection. A cap-only continuation is not supported by the evidence.

Measurements and the former optimization boundary remain in
[the evidence index](../evidence.md) and the
[B1/S8 archive](../archive/2026-07-19-bestiary-solver-s8/README.md). They are
evidence, not a selected next task.

## Unfinished S8 Delivery

If Oliver selects another one-item solver chunk, the surviving product work is:

- **R4 — browser transfer and lifetime.** Avoid nested giant JSON transfer and
  unnecessary full clones, align or preflight compile limits, release the
  solved native handle and transition closure after successful strategy
  transfer, and rebuild on repricing. Retained-cache mode remains deferred
  until live-byte telemetry can enforce a product budget.
- **R5 — verification presentation.** Show the pinned economy and sampled
  uncertainty in product review, keep per-run and independent-retry-normalized
  costs distinct, and decide the remaining authored Unveil-offer evaluator
  boundary. Compiler-emitted count conditions and the selected product
  verification gate are complete.
- **S8.5 — compact review and optional empirical trim.** Focus view remains
  presentation-only. Trimming creates a separate derived strategy with parent
  hash, explicit fallback, discovery provenance, exact impact evaluation, and
  independent sampled validation.
- **S8.6 — broader solver acceptance.** A fresh checkpoint is needed only if a
  later plan expands one-item scope beyond the accepted scaling corpus.

The waived B1.5 checkpoint was not completed: no separate full Bestiary suite,
10,000-run Imprint verification, or rendered review was backfilled. Any future
plan decides whether one of those gates is still required; this roadmap does
not silently revive it.

## Unselected Investigations

The archived solver audit also contains ideas that are neither approved work
nor current findings. A future plan may reconsider them only from fresh code
inspection and measurement:

- cooperative subdivision of the approximately 11.5-second solve step before
  reconsidering a larger focused global batch; the batch matrix reduced the 2k
  median from 22.54 seconds to as low as 13.15 seconds but made the long step
  the p95;
- behavior-identical reuse or memoization around fallback start-properness
  validation, which owned 99.93% of measured fallback-validation component
  time; economy-identity work was only 0.06%, and the economy pipeline remains
  outside solver optimization;
- veiled/Eldritch product action scope and goal-producibility diagnostics;
- WASM solve-step batching as part of browser delivery work;
- Fossil candidate selection with price-aware evidence;
- candidate/skip diagnostic unit cleanup; and
- remaining preservation, focused-expansion, telemetry, repricing, legality,
  and small performance proposals from the archived audit.

Completed, superseded, or false audit items are not backlog: both incremental
owned-byte ledgers, exact kernel reuse, exact all-action quotienting, shared
policy compilation, compiler-emitted count-condition evaluation, selected
product verification, primitive Fracture planning, and concrete compiled start
items are implemented.

## Later Solver Directions

Recombinator outcome support, feeder/recombination graphs, and automatic
spec-pyramid planning remain separately deferred in
[Mechanics and Recombinators](mechanics-and-recombinators.md). ML remains later
in [ML Strategy Planning](ml.md). Accounts and publishing remain separately
deferred in [Accounts](accounts.md).
