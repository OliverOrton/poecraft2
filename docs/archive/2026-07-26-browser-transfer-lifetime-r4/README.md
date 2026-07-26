# R4 Browser Transfer And Solver Lifetime

**Status: completed on 2026-07-26.** This folder is historical; it does not
select current work.

Parent: [Documentation archive](../README.md)

R4 replaced the Calculator's nested compiled-strategy JSON response with one
transferable UTF-8 byte buffer, moved strategy compile/evaluation inputs to the
same transferable-byte ownership model, and removed full-graph clones that had
no independent owner. Calculator now opens a fresh scoped solver for each
Solve and closes it after obtaining the summary, telemetry, and compiled
strategy. A later solve or reprice rebuilds.

The release WASM rebuild, complete web suite, TypeScript check, and focused
real-worker lifecycle check passed. The measured close released the scoped
handle and reduced selected native live bytes while the WASM linear-memory
high-water stayed unchanged, as expected. This is headless Node-worker/WASM
evidence, not browser/device RSS or throughput evidence.

Contents:

- [Plan](plan.md) — the completed Gates 0–5 contract.
- [Report](report.md) — implementation, acceptance, and limitations.

Durable behavior is extracted into
[End-To-End Solver Flow](../../solver/flow.md),
[Calculator](../../product/calculator.md),
[WASM](../../engine/wasm.md), [Decisions](../../decisions.md), and
[Evidence](../../evidence.md).
