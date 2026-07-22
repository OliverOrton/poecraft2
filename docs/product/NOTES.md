# Product Notes

**Status: non-authoritative working notes.** Implemented contracts live in
[Product](README.md).

Parent: [Product](README.md)

## General

### 2026-07-19 — #debt — Calculator verification truth

Status: promoted to the
[future solver roadmap](../future/solver-roadmap.md).

The current 10,000-run button compares sampled mean cost with the returned
policy's exact `evaluated_policy_cost` after checking completed-run count and
cost completeness, but does not yet require a successful terminal gate, zero
failure/stop/limit/off-policy outcomes, or show sampled variance/confidence.

### 2026-07-19 — #debt — Aggregate Simulator graph flow

Status: promoted to the
[future solver roadmap](../future/solver-roadmap.md).

The Simulator reports operation action totals, not complete node-visit and
edge-traversal counters. Aggregate board overlays and empirical focus/trim need
those native/binding/WASM/product counters before they can be implemented.

### 2026-07-19 — #idea — Richer Simulator results

Status: open.

Median/percentile costs, a histogram, budget probability, and a complete
materials/shopping-list export were specified but are not present in the
current Simulator UI.

### 2026-07-19 — #idea — Emulator history tree

Status: open.

The current Emulator history is a linear append-only action list. Branching
history with Undo/Redo remains an unimplemented product idea.

### 2026-07-19 — #idea — Workspace and Stash fluency

Status: open.

Tab job-busy state, richer Stash search/folders/tags/sorting, explicit recovery
management, account storage, publishing/forks, and recombinator/feeder item
flow are future product designs. No item in this note is scheduled.
