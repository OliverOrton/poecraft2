# Documentation Restructure Completion Report

**Status: historical completion evidence for the 2026-07-19 documentation-only
restructure.** It records what changed and what was verified; it has no current
execution authority.

Parent: [Documentation cleanup archive](README.md)

## Resulting Knowledge Map

[Documentation](../../README.md) is the single top-level map. It composes:

- [Foundation](../../foundation/README.md) for architecture and ownership;
- [Mechanics](../../mechanics/README.md) for implemented crafting behavior;
- [Engine](../../engine/README.md), [Solver](../../solver/README.md),
  [Product](../../product/README.md), and [Economy](../../economy/README.md) for
  stable subsystem contracts;
- [Decisions](../../decisions.md), [evidence](../../evidence.md), and the
  [glossary](../../glossary.md) for durable cross-cutting knowledge;
- [Notes](../../notes/ruling-needed.md) for unresolved rulings and raw capture;
- [Future](../../future/README.md) for unscheduled designs; and
- the [archive](../README.md) for completed plans and point-in-time evidence.

[Project direction](../../direction.md) is an 88-line orientation rather than a
phase ledger. [Active](../../active/README.md) contains only its empty-state
contract, and root [HANDOFF](../../../HANDOFF.md) says that Oliver must select
the next implementation boundary.

## Moves And Archives

The [starting inventory](inventory.md) classifies all 60 Markdown files present
at the baseline and records each permanent destination and authority. The main
structural changes were:

| Former material | Permanent home |
| --- | --- |
| Foundation architecture and codebase plans | Consolidated [foundation reference](../../foundation/README.md) |
| Engine data/bitset/item/pool/weight plans | Short [engine references](../../engine/README.md) |
| Solver architecture/phasing plan | Extracted [stable solver reference](../../solver/README.md); full source in the [B1/S8 archive](../2026-07-19-bestiary-solver-s8/README.md) |
| Workspace and strategy UI plans | Split [product references](../../product/README.md) |
| Economy implementation/deployment plan | Stable [economy references](../../economy/README.md) plus the [dated plan](../2026-07-15-economy/README.md) |
| Portfolio implementation plan | [Project-roadmap archive](../2026-07-19-project-roadmap/README.md) |
| Active B1/S8 plan, final handoff, and solver audit | [B1/S8 archive](../2026-07-19-bestiary-solver-s8/README.md) |
| Root solver-improvement audit | B1/S8 archive `audit.md` |
| Product briefs, specifications, prompts, and images | [Product-design archive](../2026-07-product-design/README.md) |
| Earlier S6, S7, and engine-performance files | Short filenames and folder indexes under the [archive](../README.md) |

The supplied [execution directive](directive.md) and
[original parked plan](original-plan.md) are preserved verbatim in this folder.

## Preserved Mechanic Authority

Ten mechanic-family pages now cover the complete 26-kind serialized action
enum, the `Restart` and condition-only strategy vocabulary, and Bestiary
Imprint/restore behavior. Each page distinguishes engine, Emulator, Solver, and
Calculator support and links checked implementation paths. No mechanic rule was
researched or inferred during this work.

The following questions remain explicitly owned by Oliver:

- double-side-lock Scour behavior and raw normal-item Annul legality;
- whether raw engine requests must reject corruption-only Essence rows;
- permanent Fossil support boundaries for lucky rolls, mirroring, partial
  specials, and flag-only exact projection;
- raw Harvest resistance-pair validation and missing dated provenance for the
  add-then-remove ruling; and
- target-side Eldritch lock behavior and raw Eldritch Annul rarity legality.

[Rulings needed](../../notes/ruling-needed.md) also separates non-mechanic owner
choices about authored Unveil and Restart/exact-evaluator coverage.

## WASM Boundary Preserved

The [WASM reference](../../engine/wasm.md) records the verified release shape:
C++20 at `-O3`, no pthreads, memory growth from 128 MiB with a configured 4 GiB
maximum and 64 MiB stack, a single Web Worker, and cooperative stepped
solve/evaluate cancellation. It documents selected native evaluator/solver
caps separately from whole-WASM/browser memory.

**Correction recorded 2026-07-20:** the completion report previously described
release-and-rebuild repricing as implemented. The checked product instead
retains its keyed solve handle and transition data after strategy transfer.
Release after transfer and rebuild on repricing is an approved, deferred
decision recorded in [Decisions](../../decisions.md#2026-07-18--browser-repricing-uses-rebuild-by-default)
and the [solver roadmap](../../future/solver-roadmap.md), not current behavior.

Unknowns remain explicit: real-browser/device throughput and peak memory,
worst-step cancellation latency, practical 4 GiB availability, full JavaScript
and structured-clone memory, heap reuse after hard workloads, a product-wide
live-memory budget, and native/Node-WASM/browser performance equivalence. The
build script's explicit export list also omits four stepped solver names that
are currently retained by `EMSCRIPTEN_KEEPALIVE` and present in the tracked
release module.

## Verification Scope

- Starting code baseline: `d5e38e34ef3b5feb288e64e35a1a3e857aed5eaa`.
- Stable code-dependent references were checked by source inspection against
  that unchanged product baseline and carry `2026-07-19 @ d5e38e3` stamps.
- The final one-off audit checked 114 repository Markdown files and found no
  unresolved relative link target. All 109 non-template Markdown files were
  reachable from `docs/README.md` through area/archive indexes.
- All 25 area/archive directory READMEs link every Markdown document they
  directly own.
- No Markdown link target uses a removed filename, and stable references do not
  present archived sequencing as current.
- Product, engine, web, ingest, and fixture test suites were not run, as this
  phase changed documentation and process material only.

## Local Commit Sequence

1. `a8a0ae5d17e3ff6b551117cfeed7965cb0d7abb3` — archive current plans and scaffold documentation.
2. `47f74d229e32e215fb4e712be9926a1b03fa1a83` — document implemented mechanic authority.
3. `52f516c2d101103cf9c2f1433043bb516209ea93` — verify and organize stable subsystem references.
4. `784dcb7b0a9159e97673b1490e00037094df9749` — extract durable knowledge and deferred work.
5. `964294dcc717b45db0f2d593179378859f6103f0` — make the documentation map the primary entry point.
6. The enclosing local commit repairs historical links, completes archive
   indexes, and adds this report; its hash is intentionally not embedded in its
   own content.
