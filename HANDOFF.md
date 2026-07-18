# Session Handoff - B1.5 Bestiary Acceptance Is Next

Updated 2026-07-17 after B1.4. Read [AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[the active B1/S8 plan](docs/active/bestiary-and-solver-capability-plan.md).

## Current State

B1.0-B1.4 are complete. Only `bestiary:imprint` is selected, classified as
checkpoint/restore. Prefix to Suffix and Suffix to Prefix remain explicit
unsupported B1 recipe rows and have no action descriptors or behavior.

The data path is canonical SQLite schema 2 to derived artifact schema 4.
Recipe/action descriptors preserve approved legality, checkpoint effects, and
the exact repeated price-key vector. Craicic Chimeral resolves through the
existing Beast provider; `beast:rare` is explicit manual-only pricing.

Native `BestiaryCraftState` is one live item plus an optional full-state saved
copy bound to a stable live identity. Creation/restoration and deterministic
calculation share one atomic transition implementation. Strategy execution
supports the exact two action ids and keeps checkpoint state per run; restart
discards the old item and its checkpoint.

Solver support is the specific `imprint_retry` fixed option. It accepts one to
three exact ordinary primitive actions, only for a complete magic-item goal.
Every attempt pays the four beast keys plus program costs. Non-goal outcomes
compile through explicit free restore and retry; goal outcomes terminate with
the active checkpoint still bound to the successful item. Raw create/restore
are not ordinary one-item DP primitives.

The additive public C ABI uses `pc_bestiary_craft_state` for the live item and
its bound checkpoint without widening `pc_item_state`. Stable presentation,
action results, deterministic calculations, refusal keys/reasons, declared
cost keys, actual consumption, and the specific solver-option metadata are
available through Python and WASM. WASM item export/import preserves the bound
checkpoint while a fresh item/restart begins without one.

Emulator and Calculator consume the same engine-owned Bestiary presentation.
Calculator renders engine calculation/refusal results and exact repeated beast
costs, and exposes only the complete-magic-goal `imprint_retry` option. Strategy
Builder emits the two existing ordinary Bestiary operations. The parked
conversions remain absent from bindings and all workspace surfaces.

Local checkpoint commits:

- `f0c4461` - B1.0 authoritative Imprint contract.
- `534dac4` - B1.1 data, price, and registry substrate.
- `a6adfb7` - B1.2 native checkpoint state and deterministic calculation.
- `4f89e23` - B1.3 solver and strategy integration.
- The B1.4 bindings/workspace checkpoint is this handoff's commit.

## Exact Next Boundary

Execute **B1.5 only - Bestiary Acceptance**:

1. Run the complete relevant ingest, artifact, native, binding, WASM, and web
   acceptance once.
2. Verify engine/calculation parity for the selected Imprint recipe on its
   approved fixtures, including all checkpoint/refusal semantics.
3. Solve and compile the selected solver-visible recipe, then run the required
   strategy verification exactly 10,000 times.
4. Deliver the resulting ordinary strategy and Calculator outputs for Oliver's
   visual and mechanic review, recording the shipped recipe, parked recipes,
   and any remaining state-model constraints.

Stop after B1.5. Do not begin S8 until B1 acceptance and Oliver's review are
complete.

## Gotchas

- Simulator trace/action-distribution values for the two Bestiary operations
  use internal operation codes outside `pc_action_type`. Bindings and UI must
  continue using the explicit Bestiary presentation rather than casting those
  values to ordinary public action enums.
- Refusals preserve item, checkpoint, and costs. Identical-state restore still
  applies and consumes the checkpoint.
- UI has no mechanic authority and must not reimplement legality or checkpoint
  rules in TypeScript.
- `beast:rare` may be missing until the user supplies a manual price; never
  treat that as free.
- Do not expose parked conversions or infer mechanics.
- Oliver owns rendered/visual review; no screenshots or browser visual smoke.

## Validation At B1.4

- Native release/fallback build completed; focused Bestiary suite passed 76
  checks with 0 failures, including the public C ABI.
- Focused Python binding file passed all 14 tests.
- Release `poecraft_engine.mjs` and `.wasm` rebuilt successfully from `C:\emsdk`.
- Narrow non-visual WASM/worker binding and workspace contract test passed.
- Web TypeScript check passed with `npx tsc --noEmit`.
- No B1.5 complete acceptance suite, 10,000-run verification, rendered browser
  review, screenshot, or visual smoke was performed.

B1.5 is the sole next boundary. S8 follows only after B1 closes.
