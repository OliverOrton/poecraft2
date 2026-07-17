# Documentation Archive

**Status: historical index.** Nothing under `archive/` owns current sequencing.

Completed execution plans live here so the active documentation map stays
small without deleting implementation history. Archived files are evidence of
past decisions and acceptance gates; they are not current sequencing authority.

For current work, read in this order:

1. [Project Direction](../direction.md)
2. [HANDOFF](../../HANDOFF.md)
3. the active plan named by `HANDOFF.md`

## 2026-06 Engine Performance

[2026-06-engine-performance](2026-06-engine-performance/) contains the original
engine hot-path audit, its follow-up review, and the short optimization/cheat
decision menu. Phase 14 and later S7 work superseded their working-tree status;
they remain measurement and decision history only.

## 2026-07 Solver S7

[2026-07-solver-s7](2026-07-solver-s7/) contains:

- `solver-depth-and-performance-plan.md`: S7.0-S7.6 and S7.2R historical
  execution plan covering the real craft corpus, correctness substrate, action
  control, exact options, solver performance, native/WASM parity, compilation,
  and final simulator gate.
- `HANDOFF-s7.6-final.md`: full final solve/compile/simulator evidence and the
  pre-closure endgame success-gate caution. Oliver subsequently directed work
  to move forward; the recorded `0.9942` versus `0.995` miss remains intact.
- `solver-simulator-improvement-report.md`: point-in-time S7.6 engineering
  analysis. Its implemented and superseded recommendations are historical, not
  current sequencing.

## 2026-07 Solver S6

[2026-07-solver-s6](2026-07-solver-s6/) contains:

- `pre-s6-product-polish-plan.md`: P1 and P3 complete; P2 skipped by Oliver.
- `strategy-calculator-mode-plan.md`: exact Strategy Builder evaluation,
  loop acceleration, progress/cancellation, and OOM hardening complete;
  Phase D remains unscheduled historical backlog.
- `s6-plan.md`: S6 Phases 1, 2, and 4 complete; Phase 3 ambient Emulator odds
  skipped entirely and not deferred.
