# Strategy-finder review and recommended completion programme

Repository: OliverOrton/poecraft2  
Reviewed main: `a7ea53c3ea4ea230c4eb099bb8800a9d5c86e0ec`  
Compared with: `28a955056a41699d1fa9996ee345b70678937f21`

**Verdict:** retain the independent experimental finder, shared backend and product lifecycle. Do not promote it, train a ranker, or interpret the reported losses as a test of the full conditional/macro-controller architecture. The current generator exhausts a very small one/two-primitive grammar. Before enlarging it, repair the mismatch between raw success-edge validation and effective compiled default-edge semantics.

Both current-head Windows and solver-knowledge workflows completed successfully.

This packet is a read-only review and a **recommended next programme**, not a repository modification or a claim that it has been dispatched. Native runs and benchmarks quoted here are recorded repository evidence. The supplied Python tests are abstract models; the supplied C++ regression is an unexecuted integration fixture.

## Read in this order

1. [Review](REVIEW.md): landed work, economic results, plan correspondence and source findings.
2. [Recommended next milestones](NEXT_PLAN.md): H0–H4, with a hard safety gate and a concrete search-coverage gate.
3. [Native design and mathematical contracts](DESIGN_AND_MATHEMATICS.md): effective routing, controller structure, macro scope, search bookkeeping and ownership.
4. [Validation and missing data](VALIDATION_AND_DATA.md): exact recorded paths, what to extract, and targeted checks.
5. [Documentation maintenance](DOC_MAINTENANCE.md): canonical owners and historical records.
6. [Self-contained implementation prompt](CODEX_PROMPT.md): the runnable handoff for a separately authorized implementation session.

[Sources](SOURCES.md) distinguish current source, recorded evidence, the previous plan, and primary research. [Baseline evidence](evidence/baseline.json) keeps approximate reported figures and unresolved controls explicit. [CI evidence](evidence/ci.json) carries the actual observation rather than assuming success.

The first native test is supplied in [finder_default_success_regression.inc](tests/finder_default_success_regression.inc). Integrate it into the existing native compile-test owner; it is not a standalone build target. [Abstract checks](tests/check_contracts.py) exercise the logical examples and can run without the engine.

The selected recommendation is **completion of the missing conditional-controller capability**, not a new model, new solver, broad source reorganization, evaluator replacement, or arbitrary extension of the previous micro-optimization programme.
