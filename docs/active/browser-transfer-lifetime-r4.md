# R4 Browser Transfer And Solver Lifetime

**Status: selected by Oliver on 2026-07-26; Gates 0–5 execute in order.**

Parent: [Active work](README.md)

Source boundary: `42e03152f871067e828b595c30950b46de9dd1de`

Branch: `codex/browser-transfer-lifetime-r4`

## Objective

Make the Calculator solve-to-strategy browser lifecycle explicit and
memory-conscious without changing solver behavior:

1. transfer compiled strategy JSON as one transferable byte buffer instead of
   nesting it as an escaped JSON string, parsing it in the worker, and
   structured-cloning the full graph;
2. release the product solve handle and transition closure after result
   diagnostics and strategy transfer, rebuilding on a later solve or reprice;
   and
3. prove the worker/WASM handle and selected-live-byte lifecycle with
   non-visual acceptance.

## Scope And Invariants

In scope:

- the browser-only WASM facade used to extract compiled solver strategies;
- worker/client strategy transfer and strategy-input marshalling;
- Calculator ownership of the product solve handle;
- removal of full graph clones that have no separate owner;
- existing compiler-limit alignment and truthful compilation refusals;
- non-visual Node worker/WASM lifecycle and memory evidence; and
- the stable WASM, Calculator, solver-flow, evidence, and handoff documents.

Out of scope:

- solver algorithms, action scope, state identity, prices, caps, mechanics,
  strategy vocabulary, C ABI, Python bindings, ingest, economy, or UI design;
- retained transition-cache mode;
- weakening compilation limits or silently raising WASM memory;
- rendered or visual review; and
- browser/device performance claims not exercised by the Node worker suite.

The transferred bytes must decode to the same v1 strategy document. Errors
must preserve their native result code and detail. Transfer changes ownership,
not content. The ordinary Calculator odds handle remains independent from the
scoped solve handle.

## Testing Contract

Follow the repository cadence: no routine suites between gates. Add focused
coverage while implementing, then run one final acceptance:

1. rebuild release WASM because the facade/export surface changes;
2. run `npm test` and `npx tsc --noEmit` in `apps/web`;
3. run a scoped transfer/lifetime measurement through the real Node worker and
   rebuilt WASM;
4. verify generated module/export drift and tracked changes; and
5. run the Markdown link audit after closure documentation.

Compiled-strategy behavior is unchanged, so this milestone does not require a
new 10,000-run strategy-quality campaign. Existing automated strategy
execution remains part of the web suite. Oliver owns rendered review.

## Gate 0 — Boundary And Baseline

- Fast-forward local `main` through the three completed research archives and
  verify that the lineage is documentation-only.
- Branch from updated `main`, record the selected boundary, and map current
  facade, worker, client, Calculator, compiler-cap, and memory owners.
- Record the starting copy/lifetime chain and the owner decision to rebuild on
  repricing rather than retaining a solve cache.

Exit: clean selected boundary and an implementation map.

## Gate 1 — Transferable Strategy Bytes

- Add a WASM facade path that exposes the native compiled strategy response as
  raw bytes with explicit result status and length.
- Copy those bytes once out of linear memory and transfer their `ArrayBuffer`
  from worker to main thread.
- Decode and parse once in `EngineClient`; preserve native error semantics.
- Keep the old facade path only if compatibility requires it, but remove it
  from the product call chain.

Exit: no escaped strategy-inside-JSON envelope, worker-side graph parse, or
full-graph response clone in the product path.

## Gate 2 — Strategy Input And Clone Ownership

- Encode main-thread strategy documents once and transfer bytes into the
  worker for native compile/evaluation rather than cloning a full graph into
  the worker and stringifying it there.
- Adopt the uniquely transferred solver document in preparation instead of
  immediately cloning it.
- Retain clones only where two independent product owners genuinely require
  them.

Exit: each remaining full graph copy has an explicit owner.

## Gate 3 — Solve Handle Lifetime And Repricing

- Open a fresh scoped solve handle for each product solve.
- Retrieve required summary, telemetry, and compiled policy before release.
- Close the solve handle in terminal cleanup and clear Calculator ownership;
  a later solve or price change rebuilds from the goal and pinned economy.
- Keep the ordinary odds/picker solver separate and live as before.
- Confirm native default compiler limits match the accepted product corpus;
  preserve truthful native compile-limit errors rather than adding a weaker
  frontend estimate.

Exit: no product transition closure survives merely for optional repricing.

## Gate 4 — Non-Visual Acceptance

- Add real-worker coverage for raw strategy transfer and round-trip validity.
- Prove scoped solver handles and selected live bytes return to the expected
  post-close baseline; disclose that WASM linear-memory high-water does not
  shrink.
- Rebuild release WASM and run the complete web test/typecheck gate once.
- Record response bytes, handle counts, live/peak selected bytes, worker
  responsiveness, and any limitations.

Exit: green rebuilt-WASM product acceptance with explicit memory evidence.

## Gate 5 — Closure

- Update stable WASM, Calculator, solver-flow, evidence, and decision records.
- Archive this plan with the final report and update `HANDOFF.md`.
- Run link, whitespace, generated-output, and clean-tree checks.
- Commit locally with the required Codex co-author line. Do not push.

Exit: completed R4 archive, clean branch, and no stale active boundary.
