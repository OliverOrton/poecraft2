# Session Handoff - S8.2 Preservation-Aware Action Control Is Next

Updated 2026-07-17 after S8.1. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4 are complete. Oliver explicitly waived B1.5 as a separate acceptance
checkpoint based on the focused B1.3/B1.4 validation already completed. B1.5 is
waived/deferred, not complete: its full acceptance suite, 10,000-run Imprint
verification, and rendered Bestiary review were not run or backfilled.

S8.0 and S8.1 are complete. The frozen before-state, ordinary strategies, and
contracts remain under
[fixtures/solver-baselines/s8.0](fixtures/solver-baselines/s8.0/). S8.1 added
only display metadata over those exact files:

- `tools/ingest/poecraft_ingest/solver_review.py` implements deterministic
  generation, contract validation, source-hash verification, raw-reference
  resolution, full graph coverage, SCC grouping, and boundary checks.
- `fixtures/solver-baselines/s8.0/review-projections/manifest.json` records four
  representative projections, their purposes, projection hashes, exact raw
  strategy paths/hashes, evaluator statuses, and the preserved mismatches.
- The representatives are `oracle-real-one-mod` for a small oracle;
  `ordinary-es-bench` for ordinary rolling, Scour restart, Annul recovery, and
  deterministic bench finishing; `s8-fracture-prepare` for the existing exact
  Fracture option expansion; and `endgame-fractured-es` for the archived
  fractured-prefix carrier, Fossil rolling, recovery, and bench finishing.

The raw compiled policies route every reachable state through a central exact
state router. Their operation nodes and router therefore form one large retry
SCC. S8.1 keeps that SCC in one section and uses traceable entries inside it for
rolling, temporary blocking, protection, fracture, finishing, recovery,
restart, setup, and cleanup. Start, satisfied-goal, and failure exits remain
separate singleton sections. Every raw node and edge belongs to exactly one
generated section; cross-section routes have explicit edge entries.

Derivation is conservative:

- common success-route `has_mod_family` conditions define the goal families;
- incoming exact route conditions define satisfied goal subsets, crafted and
  fractured status, active lock/block flags, and crafted non-goal blockers;
- a goal bench is deterministic-finishing-ready only when every routed state
  already satisfies all other goals and lacks the bench target;
- fractured goal affixes and side-matching active locks identify preserved
  subsets; otherwise labels say only that a carrier has a subset;
- absence of a satisfied goal on every routed state permits the descriptive
  disposable-carrier label; and
- operation ids provide roles without becoming solver rules.

All projection semantics flags are false and execution authority is
`raw_strategy_only`. Projection labels and ordering never enter the engine API.
The raw gzip bytes and their recorded hashes did not change.

## Focused S8.1 Validation

The single focused acceptance run passed:

```text
py -3 -m poecraft_ingest.solver_review --check
  validated 4 display-only review projections

py -3 -m unittest discover -s tools/ingest/tests \
  -p test_solver_s8_baseline.py -t tools/ingest
  10 tests passed

py -3 -m unittest discover -s bindings/python/tests \
  -p test_solver_review_projection.py
  1 test passed
```

Validation covers successful reload, schema-v1 semantics, at least one raw
reference per section/entry, complete resolving node/edge coverage, raw hash
identity, deterministic byte output, retry SCC atomicity, cross-section and
backward recovery rules, unchanged execution input under relabelling/reordering,
native compile/execute identity on 32 oracle runs, and explicit rejection of
malformed hashes and unresolved references. The 32-run check used the existing
unchanged oracle graph solely as a narrow identity test; it is not a new
compiled-strategy verification sample. No 10,000-run verification was required
or performed.

No repository-wide suite, B1.5 acceptance, Imprint verification, browser
review, screenshot, rendered smoke, long protected-reforge capture, or endgame
recapture was performed.

## Exact Next Boundary

Execute **S8.2 only - Preservation-Aware Action Control**.

Add symbolic preserve/destroy/create/unreachable metadata and exact
restart-equivalent/disposable-carrier certification. Control destructive
rolling candidates only through certified dominance/equivalence or valid
bounds, retain uncertain actions, and preserve deterministic inclusion/defer/
prune diagnostics with witnesses. Validate value/policy-cost equality against
small exhaustive-oracle fixtures and show intended action reduction on the
existing real cases.

Do not begin S8.3 automatic Fracture/bench/metamod candidates, S8.4 accounting,
S8.5 focus/trimming, or S8.6 acceptance.

## Gotchas

- Review roles are descriptive outputs only. S8.2 must derive its own exact
  preservation and restart-equivalence facts; it must not consume an S8.1
  section or label as action-control authority.
- Raw ordinary strategies remain the only execution, routing, legality, and
  evaluation authority. Do not add projection fields to executable strategy
  JSON or compiler/engine inputs.
- Current compiled graphs contain compiler-only `mod_count` routing that
  Calculator exact evaluation refuses. S8.1 preserved the same documented
  unsupported result; do not widen S8.2 into an evaluator/compiler fix.
- The archived S7 corpus pins pre-Bestiary artifact hashes. The S8.0 current
  captures pin the B1.4 artifact while the archived endgame graph remains
  historical.
- The archived endgame Simulator sample remains 0.9942 against its former
  0.995 threshold. It is disclosed, not repaired.
- The temporary-blocker case did not converge. Automatic blocker assembly is
  S8.3, not S8.2.
- Protected-reforge captures were abandoned because they ran too long. Do not
  restart them in S8.2.
- B1.5 remains waived/deferred, not complete. Do not backfill its full suite,
  Imprint run, or rendered review.
- Follow the repository cadence: no routine intermediate suites, 10,000 runs
  only when compiled-strategy verification is actually required, local commits
  only, and no browser visual review unless Oliver explicitly requests it.

S8.2 is the sole next boundary.
