# Session Handoff - S7.2R Complete, S7.4 Next

Updated 2026-07-16 after the S7.2R product action/price repair and its Mirage
partial-price follow-ups. Read
[AGENTS.md](AGENTS.md),
[docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

S7.0-S7.3 and S7.2R are complete. S7.3 remains the earlier
out-of-sequence fixed-option commit; S7.2 did not reimplement or broaden those
options. S7.4 has not begun.

## Exact next boundary

Implement **S7.4 only: renewal and observation-aware options**.

1. Add fixed-exit repeat options for the approved rolling/reforge methods.
2. Add protected repeat loops that repay setup costs correctly.
3. Preserve observed Unveil choices inside option evaluation.
4. Add fracture preparation/retry only on the existing carrier-exact state.

Compile every option to ordinary primitive Strategy Board operations and retain
all expressible success, salvage, and brick exits. Stop before S7.5 cycle
acceleration/cache reuse or S7.6 acceptance work. Do not run routine native,
binding, WASM, web, simulator, full-repository, or visual-browser gates at this
intermediate checkpoint. Oliver owns visual review.

## S7.2R result

- Calculator now opens the native solver with `fossil_mode: goal_relevant`.
  Native generation advances a deterministic 96-entry beam through 1-4 fossil
  loadouts, preserves leaders for every goal slot, and emits only signatures
  that improve at least one goal target. This bounds generation instead of
  enumerating the 15,275-loadout powerset. `requested_fossil_actions` keeps a
  hand-selected loadout materialized for exact odds even when it is outside the
  automatic beam.
- Calculator refreshes its action/price envelope for every goal and uses that
  same envelope to create the priced solve. Multi-fossil entries therefore
  remain solver candidates rather than disappearing from the UI-side list.
  Telemetry distinguishes `goal_relevant`, `requested`, and `exhaustive`
  fossil modes and reports deferred signatures honestly.
- The Calculator keeps its exhaustive non-fossil handle for exact single-action
  odds, while Start Solve first asks native code for a pre-layout
  `action_mode: goal_relevant` product envelope and then opens the explicit
  priced subset. Missing prices exclude only those relevant actions; they do
  not block a solve while any priced route remains. Direct goal
  Essence/bench/Harvest/influence actions remain eligible, while unrelated
  Veiled/Eldritch actions are omitted. Standalone structural metamods,
  crafted-mod removal, and Fracture are also omitted from product solves: their
  unrestricted flag/tag combinations exhaust the state cap, while their
  bounded protected/retry assembly routes are explicitly S7.4 work. They remain
  available in the exhaustive Calculator registry. The omission count and
  reason are native telemetry.
- Non-converged or resource-capped results are no longer passed to the policy
  compiler, and the `1e12` internal infinity sentinel is displayed as
  unavailable rather than as an expected chaos cost.
- `fixtures/economy/harvest-recipes-v1.json` is now the owner-approved Harvest
  availability source for economy recipes, native generated allowlists, and
  the web catalog. The canonical defence tag/key is `defences`. Reforge and
  Augment expose their separate real tag sets; arbitrary tag-derived crafts are
  absent from the solver and rejected by direct action and strategy compilation
  paths.
- Published economy snapshots now pin each price as `quote`, `recipe`, or
  `zero`. Per-profile explicit overrides remain first priority. A visible
  strictly positive fallback can price only cost keys returned by the engine;
  it applies after snapshot quotes/certified recipes, never populates arbitrary
  keys, and is copied with per-key source into the pinned solve identity.
- Native fallback builds and the direct WASM build both generate the Harvest
  header from the checked-in recipe manifest, so non-CMake and Emscripten builds
  cannot drift from the CMake path.

## S7.2 result

- Explicit `actions` now form a conservative action envelope. Primitive
  dependencies referenced by S7.3 options are included and reported. Missing
  prices, unsupported requests, pruned actions, dependencies, deferred work,
  and equivalence decisions are exposed in `action_control.reasons`.
- Fossil loadouts are materialized lazily when a goal has an explicit action
  envelope. The one-mod oracle builds 330 registry entries instead of the
  15,605-entry exhaustive registry and reports all 15,275 unmaterialized fossil
  possibilities. Omitting `actions` preserves exhaustive diagnostic behavior.
- Legal state/action kernels are calculated once. Exact-equal successor and
  probability kernels collapse only when certification succeeds; the cheapest
  priced representative wins and equal-price ties remain disclosed.
- Solver-owned state/action data is stored as sparse contiguous columns.
  Identical kernels share one transition slice, and evaluator distributions are
  released after the row is copied. The four heap-backed junk-count vectors in
  each abstract state are now fixed-capacity sparse inline payloads.
- Independent caps cover discovered states, expanded states, state/action rows,
  transitions, reforge frontier work, peak solver-owned bytes, compiled nodes,
  compiled edges, and strategy JSON bytes. Cap names are reported explicitly;
  the public solve-options ABI remains compatible through `struct_size` checks.
- Bellman iteration now advances through bounded row/transition work units.
  The worker adapts the number of units toward a 12 ms slice instead of yielding
  once per whole-table sweep. Policy extraction and reachability consume the
  same stored sparse rows rather than recalculating kernels.
- Per-solve caches are intentionally released at the phase boundary. Reusing a
  compatible transition cache for price-only solves remains S7.5 work.
- The WASM stack reserve is 4 MiB. This is required for the existing S7.3
  primitive policy compiler to serialize the ordinary benchmark graph; no
  S7.3 operator semantics or strategy vocabulary changed.

## Benchmark evidence

The required clean pre-optimization command was started from commit `6964566`:

```powershell
powershell -File scripts/benchmark-solver.ps1 -Label pre-s7.2-unoptimized
```

It failed to finish even the native report. At Oliver's direction it was
stopped after 3,036 seconds (2,882 native CPU seconds); WASM had not started.
The ignored machine-readable record is
`build/performance/solver-pre-s7.2-unoptimized-timeout-v1.json`. Existing S7.0
reports were preserved.

Final pinned reduced-mode measurements are under `build/performance` with label
`s7.2-final`:

- One-mod oracle: native solve 0.365 ms and WASM solve 83.570 ms, versus the
  historical S7.0 1.168 ms native and 17,605.552 ms WASM. Both report 8 states,
  12 rows, 8 stored transition entries, `V(start) = 8.0201443343544`, and pass
  the benchmark's 10,000-run compiled-policy measurement. Solver-owned byte
  estimates are 308,128 native and 216,479 WASM. The cross-backend report passes
  89 checks.
- Ordinary ES: native solve 688.372 ms and WASM solve 1,193.949 ms. Both report
  241 states, 1,119 rows, 1,624 shared sparse transitions representing 25,557
  evaluated outcome entries, `V(start) = 780.177791988`, and a 10,000-run mean
  cost of 756.540375 with zero off-policy failures. Native/WASM comparison
  passes 89 checks; the WASM maximum worker slice was 17.218 ms.
- Cancellation acknowledgement was 18.115 ms in the worker, under the approved
  250 ms budget. The final exhaustive-registry stress refusal stayed under the
  50 ms slice budget at 34.876 ms. Native and WASM independently name
  `max_expanded_states` for the state-cap and stress refusals.

Two hard-case diagnostics remain visible rather than hidden:

- The advanced case completed during S7.2 with a 12.671 s native solve, but its
  10,000-run mean was 8.74% below the exact forecast, just outside the stored 8%
  tolerance. That final correctness investigation belongs to S7.6, not S7.2.
- After sparse storage and per-row cache release, the endgame case held roughly
  61 MB working set but remained CPU-bound and was stopped after 335 seconds.
  Its ignored timeout record is
  `build/performance/native-solver-s7.2-action-storage-v3-case-endgame-fractured-es-timeout-v1.json`.
  Algebraic self-loop elimination, SCC/policy iteration, and prioritized work
  are explicitly S7.5.

Bellman sweep/work-unit counts are reported but are no longer treated as
cross-backend structural equality: native and WASM can reach the same
tolerance a few sweeps apart and can discover sparse rows in a different order.
States, actions, stored transitions, compiled graph shape, status, cap hits,
`V(start)`, and simulator outcomes remain cross-backend gates.

## Verification performed

- For S7.2R, `powershell -File scripts/build.ps1` completed successfully. It
  compiled the native engine, test, and benchmark targets but executed no test
  binary. `npm run build` in `apps/web` also completed successfully, including
  its TypeScript check and production bundle build.
- For S7.2R, `powershell -File scripts/build-wasm.ps1` completed successfully
  and regenerated the optimized WASM artifact from the manifest-backed Harvest
  allowlist.
- After the Mirage partial-price repair, `powershell -File scripts/build.ps1`,
  `powershell -File scripts/build-wasm.ps1`, and `npx tsc --noEmit` completed
  successfully. A narrow headless replay used the pinned Mirage snapshot and a
  Vaal Regalia T1 energy-shield goal: 5 relevant actions had no price, the 13
  priced actions still converged in 49 states to `V(start) = 8.541424`, and the
  product envelope retained one useful fossil action at each socket count 1-4.
- A second narrow replay matched the reported Conquest Lamellar two-prefix T1
  goal against the same pinned Mirage snapshot. With 3 relevant prices missing,
  the remaining 13 actions retained useful 1-4 fossil loadouts and converged in
  109 states to `V(start) = 334.393381`. Before structural assembly primitives
  were deferred to S7.4, that exact case hit `max_discovered_states` after
  12,679 expansions and zero sweeps.
- Earlier targeted `s7.2-final` benchmark runs covered one-mod native/WASM agreement,
  ordinary native/WASM agreement, cancellation, explicit state-cap refusal,
  and exhaustive-registry stress refusal. These were measurements requested by
  the phase, not routine suite runs.
- No routine native, binding, WASM, web, simulator, or full-repository test
  suite was run. No rendered or visual check was performed.

## Scope that remains parked

S6 Phase 3 ambient Emulator odds was skipped entirely and must not reappear.
Economy E0-E7 is complete except external production activation. Phase 12
accounts, publishing/community, mechanic track M1-M5, Phase 18 recombinators,
and ML remain deferred, blocked, parked, or later as documented.
