# poecraft2 — the next milestones after verified delivery

**Decision package · 19 September 2026 · read-only review, proposed implementation**

**Reviewed main:** `d13c9b835186ce35cb51aef0de6d0027f8b8a1a7`  
**Predecessor audit:** `4594e44b6b851511ae5471e52f232bce57d16856`  
**Programme:** Reliable validation and retained-artifact ownership

## Decision

Complete a bounded structural-consolidation interval now: **M0 baseline lock → M1 trustworthy validation → M2 one retained-artifact ownership boundary → M3 qualification and closeout.** Then stop and review. The next performance programme is coordinated cooperative setup across the two measured synchronous owners, with an initial Conquest-four runtime-gap triage; it is queued, not silently included in this implementation.

The completed programme delivered request-scoped Finish, frozen request exports, complete private-artifact transfer and final sealing. It did not make startup cooperative, close the Conquest-four WASM gap, or repair the hosted validation failures. Its two saved-Ring research probes selected neither a representation adapter nor a response cache. This package preserves those results rather than restarting them. [Current review](01_MAIN_REVIEW.md).

The immediate purpose is to make a local change require a smaller amount of reasoning and a reproducible validation path. A lower line count, another manager, or a successful build by itself is not an acceptance criterion.

## Use this package

Give the implementation session **[CODEX_PROMPT.md](CODEX_PROMPT.md)** and this directory. That prompt is self-contained and selects only M0–M3. Start with the prompt and [milestones](02_MILESTONES.md); consult the ownership, validation and mathematics documents when working on those boundaries. Do not load the whole background archive as routine startup context.

| Document | Purpose |
|---|---|
| [01_MAIN_REVIEW.md](01_MAIN_REVIEW.md) | What landed, what is still open, current CI, and reconciliation with the previous audit |
| [02_MILESTONES.md](02_MILESTONES.md) | Selected scope, implementation gates, deliverables and stopping rules |
| [03_ARTIFACT_OWNERSHIP_DESIGN.md](03_ARTIFACT_OWNERSHIP_DESIGN.md) | Concrete retained-pool design, mixed evidence stages, transactions, passive reads and memory hazards |
| [04_VALIDATION_AND_EXPERIMENTS.md](04_VALIDATION_AND_EXPERIMENTS.md) | Existing test entry points, prerequisite repair, negative controls and proportionate qualification |
| [05_RESEARCH_AND_MATHEMATICS.md](05_RESEARCH_AND_MATHEMATICS.md) | Research dispositions and the precise invariants the refactor must preserve |
| [06_CANONICAL_DOC_MAINTENANCE.md](06_CANONICAL_DOC_MAINTENANCE.md) | Canonical source/math/claim destinations and context-recovery contract |
| [07_NEXT_PERFORMANCE_PROGRAMME.md](07_NEXT_PERFORMANCE_PROGRAMME.md) | Queued two-owner setup programme and conditional research priorities |
| [SOURCES.md](SOURCES.md) | Pinned evidence and current primary technical references, with review coverage |

The `evidence/` directory contains the baseline, CI observations, decision/qualification records and a fresh exact-byte check. `checks/` contains standalone package/audit checks, **not a replacement repository test harness**. `background/` preserves the previous audit as historical context; its recommendations are not a second active plan.

## Evidence and limits

Repository inspection used the connected GitHub reader. Current source excerpts, programme dispositions, build identities and both new-main hosted job logs were examined. No repository files were edited and no native build, WASM solve, simulation, CPU profile, full AST census or exhaustive diff review was run in this review.

Two small checks were executed locally: the current-pin Git blob reproduces one Windows hash failure solely through LF-to-CRLF conversion; an isolated mixed-style test file shows why unittest can pass while a top-level pytest test is uncollected. Results and exact limitations are in [reference_checks.json](evidence/reference_checks.json). Recorded solver timings remain recorded, not remeasured.

Full game-data inputs and historical native executables are not included in this ZIP. A recorded build hash identifies an object; it does not supply its bytes. M1 must resolve a genuinely reproducible prerequisite route rather than invent a dataset or silently skip required tests. The executor should preserve the existing protected root path `0`, work sequentially, and keep commits local unless Oliver separately requests a push. [Operating rules](SOURCES.md#r02).

## Success looks like

The intended tests are actually collected and can run from the declared prerequisites. The retained candidate pool has one mutation owner and no mutable-vector escape hatch for the selected lifecycle. Reading availability does not prune or resequence work. Finish still returns a graph, certificate, context and cost that belong together. Memory accounting still charges real overlapping ownership correctly. The remaining startup, step-latency, total-delivery, WASM-quality and exactness gaps stay visibly open until separately qualified.

## Verify the packet

Run `python checks/verify_package.py` to validate the included SHA-256 manifest.
Run `python checks/reproduce_audit_checks.py` to print fresh byte/collection-check results without changing this packet. An optional `--output` should point outside this directory when keeping the manifest unchanged. These checks do not execute the native solver.

The final remote recheck is preserved in [final_remote_recheck.json](evidence/final_remote_recheck.json). Main remained at the reviewed commit; both inspected push jobs still reported failure.
