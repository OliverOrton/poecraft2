# Session Handoff

**Status: Oliver selected
[R4 browser transfer and solver lifetime](docs/active/browser-transfer-lifetime-r4.md)
on 2026-07-26. Gates 0–5 execute in order on branch
`codex/browser-transfer-lifetime-r4` from source boundary
`42e03152f871067e828b595c30950b46de9dd1de`.**

## Main Reconciliation

Local `main` was fast-forwarded from `b55e61a` through the completed
gap-directed, exact automatic-action, and broad-action research lineage to
`42e0315`. The entire delta is documentation: archived plans/reports, evidence
indexing, roadmap updates, and the clean no-active-boundary handoff. No
diagnostic engine, binding, web, data, fixture, or script change landed on
`main`.

## Objective

Replace the Calculator's large solve-to-strategy copy chain with raw
transferable bytes, then release the scoped native solver and transition
closure after result/telemetry/strategy handoff. A later solve or repricing
rebuilds from the goal and pinned economy.

The ordinary Calculator odds handle remains separate. Solver algorithms,
mechanics, action scope, public caps, compiler vocabulary, and strategy
semantics do not change.

## Starting Boundary

The current product path:

1. compiles strategy JSON into a native string;
2. escapes that entire document as a string inside a second JSON response;
3. parses the response and then the embedded document in the worker;
4. structured-clones the parsed graph to the main thread;
5. immediately clones it again during Strategy Board preparation; and
6. retains the solved native handle/transition closure for potential repricing.

Native default compiler limits already match the accepted product corpus:
100,000 nodes, 400,000 edges, and 64 MiB of strategy JSON. R4 preserves those
truthful native boundaries rather than inventing an inaccurate frontend size
estimate.

## Acceptance

The final gate rebuilds release WASM, then runs `npm test` and
`npx tsc --noEmit` once. Real Node worker/WASM evidence must cover byte
transfer, strategy validity, handle release, selected live bytes, linear-memory
high-water disclosure, and responsiveness. No rendered review is authorized.

Commits are local-only and end with:

`Co-authored-by: Codex <codex@openai.com>`
