# Session Handoff - S8.1 Derived Review Sections Is Next

Updated 2026-07-17 after S8.0. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4 are complete. Oliver explicitly waived B1.5 as a separate acceptance
checkpoint based on the focused B1.3/B1.4 validation already completed. B1.5 is
waived/deferred, not complete: its full acceptance suite, 10,000-run Imprint
verification, and rendered Bestiary review were not run or backfilled.

S8.0 is complete. Its manifest, normalized case records, ordinary strategies,
contracts, examples, and evidence notes are under
[fixtures/solver-baselines/s8.0](fixtures/solver-baselines/s8.0/). This is a
reproducible before-state and review-contract checkpoint, not a new subjective
strategy-quality corpus.

The selected baseline contains all five existing S7 cases plus narrow cases for
Fracture, temporary bench blocking, and protected metamod reforging:

- `oracle-real-one-mod`, `oracle-real-two-mod`, `ordinary-es-bench`, and
  `advanced-es-resist-bench` were recaptured at pre-S8 commit `b9e426b` against
  the exact current B1.4 artifact.
- `endgame-fractured-es` reuses the archived S7.6 graph and 10,000-run sample;
  it was not rerun after Oliver directed abandoning long runs.
- `s8-fracture-prepare` converged, compiled, and completed 10,000 Simulator
  runs.
- `s8-temporary-bench-blocker` records `not_converged`; no graph, evaluator
  completion, or Simulator run is claimed.
- `s8-protected-metamod-reforge` records both abandoned long attempts; no
  solver report, graph, evaluator result, or Simulator run is claimed.

Five newly captured compiled strategies each completed exactly 10,000
Simulator executions (50,000 total). The archived endgame sample contributes
another 10,000, so the baseline represents 60,000 runs. No unrelated
repository-wide acceptance, B1.5 Imprint run, browser review, screenshot, or
rendered smoke was performed.

The S8.0 contracts are definition/example only:

- `review-projection.schema.json` makes derived sections non-executable, gives
  every section/entry explicit raw node/edge references, and pins the raw graph
  as the only execution authority.
- `action-accounting.schema.json` versions native descriptor, price,
  contribution, raw-node/review-section, and all planned classification fields;
  it does not implement S8.4 accounting.
- `trimming-provenance.schema.json` requires parent hash, empirical marker,
  discovery and independent-validation parameters/seeds, threshold, removed
  raw ids, visitation mass, exact impact result, sampled confidence, and a
  nonzero upper bound for unvisited branches. Removed entries are pinned to
  serialized Restart with `explicit_user_choice_required`; trimming is not
  implemented.

## Exact Next Boundary

Execute **S8.1 only - Derived Review Sections**.

Build a display-only projection over the chosen exact policy using the S8.0
review schema. Representative projections should group exact graph/evaluator
facts for restart-equivalent/disposable carriers, satisfied goal carriers and
crafted/fractured state, preserved affix side or goal subset, protection/setup
intent, deterministic finishing readiness, and recovery paths. Retry SCCs stay
inside one section; restart or genuine carrier loss may return to an earlier
section. Labels remain descriptive metadata and every projected entry retains
raw node/edge links.

Prove that projected and unprojected inputs remain the same executable ordinary
strategy and compile/evaluate identically. Deliver representative projections
to Oliver for usefulness review. Stop after S8.1. Do not begin S8.2 action
control, S8.3 candidate generation, S8.4 accounting behavior, S8.5 trimming,
or S8.6 acceptance.

## Gotchas

- The archived S7 corpus manifest pins pre-Bestiary game/strings hashes. The
  S8.0 corpus pins the current B1.4 artifact while reusing unchanged S7 case
  files. Do not rewrite the archived corpus as if it were current.
- Compiler-produced ordinary graphs contain compiler-only `mod_count` routing,
  which Calculator exact evaluation currently refuses even in the oracle
  cases. S8.0 records that pre-existing mismatch verbatim. Do not turn S8.1
  into an evaluator or compiler fix.
- The archived endgame sample remains 0.9942 success against its historical
  0.995 threshold. It is disclosed, not a passing numeric gate.
- The temporary blocker case's current fixed policy-improvement path does not
  converge. Automatic blocker assembly belongs to S8.3, not S8.1.
- The protected-repeat exact captures were abandoned because they ran too
  long. Do not restart them as part of S8.1.
- Raw ordinary strategy nodes/edges remain the only execution, routing,
  legality, and evaluation authority. Projection metadata must not acquire
  solver semantics.
- Oliver owns rendered/visual review. Do not perform browser visual checks,
  screenshots, or rendered UI smoke unless explicitly asked.

## Validation At S8.0

- Native corpus validation accepted all eight exact S8.0 case specifications.
- Five focused baseline tests passed: every JSON and gzip strategy reloads;
  manifest and graph hashes/counts match; every review reference resolves;
  the accounting example reconciles to the existing exact Restart evaluator;
  and trimming provenance requires the explicit Restart fallback plus discovery,
  impact, independent-validation, confidence, and upper-bound fields.
- Newly captured Simulator verification: 5 strategies x 10,000 = 50,000 runs.
- Reused archived S7.6 Simulator evidence: 1 strategy x 10,000 = 10,000 runs.
- No full test pipeline, browser review, B1.5 suite, or later S8 implementation
  was performed.

S8.1 is the sole next boundary.
