# Change Impact Map

**Integrated reference.** Authored from the repository contracts at `f3e7c0fa7bd827064a41c48a53c4db372840cf0f`. Reconciled with local `215654f`; importing this mechanism reference supplies no new runtime authority.

Use this map when an edit crosses an actual boundary. It is not universal startup reading or a requirement to run every downstream suite. The shared operating and validation policy has one owner: `AGENTS.md`.

## Purpose

A local change can leave a derived artifact, parser, binding, fixture, or documented contract inconsistent. Identify what consumes the changed contract and validate that path. Do not turn a mathematical documentation update into a new native acceptance milestone, and do not call a native semantic change documentation-only because the ABI stayed stable.

Mechanics remain native/owner-defined. A disagreement between code and a mathematical argument creates an explicit discrepancy; neither is silently rewritten to hide it.

## Dependency Overview

```text
ingest/schema → canonical game SQLite → compiled runtime artifact
             → native loader/session → engine and bindings

native source/public C ABI → Python subset and WASM facade
                         → worker/client → browser product

economy inputs → validated immutable snapshot → pinned native prices
```

The mathematical reference records assumptions and arguments. It does not replace these data and execution boundaries.

## Rules Before Editing

Start at the actual owner. Preserve canonical versus derived distinctions, relevant local work, and the existing semantic scope. Inspect only applicable bindings: Python and WASM expose different subsets.

A focused check is useful when it resolves uncertainty or validates a retained change. Intermediate phases are not routine test gates. Final validation is selected by actual impact; no universal simulation or full-pipeline requirement is introduced here.

## Change Matrix

| Change | Consumers and evidence to inspect | Appropriate validation scope |
|---|---|---|
| Prose/navigation only | Changed links, canonical owner, incoming historical anchors | Edited-link/diff review; claim lint when installed; no native build |
| Mathematical claim or premise | Referenced argument, affected native producer/checker/consumer, counterexample | Substantive review and the smallest useful proof/example check; no automatic whole-repository reread |
| Research/report tooling | Existing CLI compatibility, input schemas, identity/failure semantics, generated outputs | Focused parser/report fixtures and deterministic output checks |
| Ingest or canonical schema | Writers/queries, validators, compiled projection, loader and fixture expectations | Regenerate through owning tools when authorized; validate affected canonical and derived data |
| Compiled artifact format | Compiler/validator, native loader, web data-bundle generator | Artifact/loader and actual downstream consumers |
| Internal native semantics | Exact and sampled mechanics, solver/evaluator, exposing bindings | Focused native contracts; rebuild affected release artifact before claiming browser parity |
| Public C ABI or lifetime | Header smoke, native clients, exposing Python/WASM bindings, worker types | Relevant ABI/binding and downstream lifetime checks |
| Primitive action or request shape | Owner ruling, enum/parser, registry, Simulator, exact evaluator, product pickers | Native/exact/sampled parity as applicable; regenerate affected WASM |
| Strategy vocabulary | Native parser/simulator, compiler, exact evaluator, authored shape, persistence | Complete changed-vocabulary path; sampled qualification only when genuinely required |
| Solver algorithm, stable I/O | Scope/proof assumptions, policy/evaluation, benchmark identities, cooperative behavior | Affected focused proof/behavior controls and justified matched experiment |
| Solver result/progress field | Typed owner, collection/serialization, C ABI/facade, worker/client and presentation | Field semantics, missing/non-finite handling, and exposed consumers |
| WASM facade/export/marshalling | Native declaration, export inventory, generated module, bindings and worker | Rebuild affected module and exercise matching interface/transfer checks |
| Python binding | Actual exposed C ABI subset, ctypes/lifetime, package assumptions | Shared-library and binding checks |
| Worker RPC or persistence | Message/structured-clone types, cancellation, request versions, document migrations | Typecheck and focused nonvisual web tests; native rebuild only when its input changed |
| Economy schema/pricing envelope | Provider/validation, snapshots, browser cache, native price parser | Affected economy and price-consumer checks; do not change game data unnecessarily |
| Source removal/build isolation | All consumers, inventories, optional oracle/reproduction role | Smallest affected compile/test; keep old evidence retrievable |
| Workflow/packaging | Actual source inputs, required-check behavior, manifests | Narrow workflow/packaging checks; skipped work must remain honestly labelled |

## Rebuild Triggers

### Native engine

`scripts/build.ps1` is the normal repository build entry. `scripts/dev-engine.ps1` exposes narrower development workflows; inspect its current help/selector before running it. Do not substitute a direct full recompile for an available incremental target without a reason.

`engine/engine-sources.txt` is the shared native translation-unit inventory consumed by the builds. Add, move, or remove an engine translation unit there once and validate the inventory. Private headers and benchmark-only sources have different build ownership.

### Compiled game data

Canonical SQLite and compiled data are regenerated through their owning tools, never hand-edited. A data-format change also affects `data_loader.cpp` and the web data-bundle generator. Timestamps can suggest what to inspect; identity/content validation establishes compatibility.

### WASM

`scripts/build-wasm.ps1` owns the committed release module. It activates the configured Emscripten environment; a fresh shell without `emcc` does not establish that rebuilding is impossible.

Rebuild before browser acceptance when browser-visible native semantics or the facade changes. C ABI and strategy-vocabulary changes require the corresponding binding/export review. Inspect the actual shared export inventory and native/facade call sites rather than copying a stale list from this page.

A genuinely native-only private option or benchmark-only change does not imply a changed public feature. State which source/artifact was validated and which was not; do not call an old release module current by assumption.

### Web and public artifacts

`npm test` does not replace TypeScript checking. Use `npx tsc --noEmit` for affected TypeScript contracts; use the existing product build/data hooks when packaging is in scope. Rendered review belongs to Oliver unless explicitly requested.

## Verification Selection

The full `scripts/test.ps1` pipeline remains available for a finished change whose impact warrants it. Read the actual script for current phases; do not maintain a second brittle copy of its ordering here.

A documentation or metadata edit needs no Simulator. A source removal may need a narrow compile. A changed executable strategy may need fresh sampled qualification under `AGENTS.md`, but an identical already-qualified artifact does not require another sample for ceremony.

Do not execute a test selector solely because its name sounds proof-only: inspect whether it also launches costly simulation or downstream work. Preserve failed, unrun, skipped, and unavailable results separately.

## Documentation Consequences

| What changed | Canonical destination |
|---|---|
| Mathematical proposition or counterexample | Relevant mathematics chapter and claim history |
| Implementation correspondence | Owning mechanism page and selected source references |
| Native mechanic/ruling | Existing mechanics page and ruling owner |
| Source/lifetime structure | Existing source/flow map |
| Owner engineering decision | Append or supersede in `docs/decisions.md` |
| Experiment | Existing immutable evidence plus relevant research question/series |
| Current sequencing | HANDOFF only, when continuation needs it |
| Substantive external research | Original report once; canonical dispositions and ordinary final-response receipt |

Do not mirror every change into the documentation map, evidence index, active index, and HANDOFF. Historical numeric narratives remain at their existing addresses. Claim/reference tools and generated research views are only described as available after their implementation is integrated.

## Worked Change Traces

### A lower producer changes

Follow its model/scope claim, native probability/projection evidence, numerical checker, and actual public/pruning consumers. Validate the changed relations with the smallest relevant fixture. Report local bound gain separately from complete-model and end-to-end performance. Do not widen a member-domain guard because one representative passes.

### A telemetry field changes

Define unit, population, ownership, and whether diagnostics are inside the solver cap. Update the actual collection/serialization and exposed bindings. A post-authority diagnostic must not become an input to scheduling or proof.

### Documentation is reorganized

Preserve historical evidence and anchors. Move arguments to their canonical chapter, not merely to another chronological file. Validate the changed links and claim references when tools are available. No standing requirement for a whole-repository link audit, fresh verification stamps, or a product suite is created by prose edits.

## Source basis

This rewrite uses the [preceding reference](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/foundation/change-impact.md) and [AGENTS.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/AGENTS.md), [CMakeLists.txt](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/engine/CMakeLists.txt), [benchmarking.md](https://github.com/OliverOrton/poecraft2/blob/f3e7c0fa7bd827064a41c48a53c4db372840cf0f/docs/solver/benchmarking.md). Claim IDs are registered in [the ledger](../solver/claims.md); each history states its acceptance basis. The [backbone integration](../archive/2026-09-06-solver-mathematical-backbone-v2/README.md) is complete. Remaining native correspondence is scoped in [research](../solver/research.md#open-obligations); an argument link does not confer runtime authority.
