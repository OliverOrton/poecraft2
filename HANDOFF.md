# Session Handoff - S7.0 Complete, Approved Corpus, S7.1 Next

Updated 2026-07-15 after the S7.0 native/worker-WASM benchmark, telemetry,
unoptimized baseline, structural comparison tooling, and owner corpus approval.
Read
[AGENTS.md](AGENTS.md), [docs/direction.md](docs/direction.md), this file, then
[docs/solver-depth-and-performance-plan.md](docs/solver-depth-and-performance-plan.md).

## Exact next boundary

Oliver approved the permanent oracle, ordinary, advanced, endgame, and stress
corpus plus the numeric performance, responsiveness, cap, and simulator
criteria on 2026-07-15. All artifact-backed cases are enabled. Implement
**S7.1 only: one-item correctness and state substrate**:

1. fixture the approved tied/no-Eldritch-dominance ordinary-currency behavior;
2. add remove-crafted-modifiers as a primitive costing one Scour;
3. add exact Harvest resistance conversion and carrier-aware Fracture
   calculation evaluators;
4. add only the crafted/fractured/side-specific state and compiler distinctions
   required by the approved crafts;
5. keep the resulting policies compatible with the ordinary strategy compiler
   and simulator; defer plan-level correctness verification to S7.6 unless a
   broken path requires a narrowly targeted simulator diagnostic.

No approved corpus case currently exposes an unsettled metamod interaction. Ask
Oliver only if implementation reveals a new mechanic ambiguity.

Stop before S7.2. Do not optimize the solver, action registry, value iteration,
transition storage, or worker scheduling in S7.1. The saved S7.0 baseline must
remain the comparison authority for the later performance pass.

## S7.0 result

One entry point now builds and runs both paths:

```powershell
powershell -File scripts/benchmark-solver.ps1
```

It writes the committed, otherwise ignored reports under `build/performance`:

- `native-solver-s7.0-unoptimized-v1.json`
- `wasm-worker-solver-s7.0-unoptimized-v1.json`
- `solver-s7.0-unoptimized-v1-comparison.json`

The comparison recorded 421 checks across all eight enabled cases with zero
mismatches. Those fields establish comparable artifact identity, outcome
status, action/abstraction/state/work/cache/compilation structure, `V(start)`,
and compiled graph size. The compiled-strategy simulator runs are the
correctness evidence. Timing and memory are reported rather than forced equal
across native and WASM.

Baseline highlights:

| Case | Native solve | Worker/WASM solve | States / rows / transitions | Sweeps | `V(start)` |
| --- | ---: | ---: | ---: | ---: | ---: |
| One T1 ES mod | 1.168 ms | 17,605.552 ms | 8 / 12 / 48 | 2,297 | 8.020144334354436 |
| Pre-approval any-tier ES + fire resistance | 0.955 ms | 9,234.950 ms | 9 / 20 / 80 | 1,184 | 4.145081724989235 |

Both compiled and completed 10,000-run deterministic simulator checks with 100%
success. Oliver has now set 10,000 runs as the permanent compiled-strategy
verification count. Compiled graph sizes are 10 nodes / 15 edges /
27,861 bytes and 12 / 19 / 36,496 bytes respectively. The full-registry stress
case records 15,604 candidates, 15,597 evaluator-supported actions, 11 priced
actions, 42 discriminating tags, 122 junk classes, 14,909 discovered states
from one expansion, and 14,909 transitions.

The maximum worker slice was 11.822 ms against the approved 50 ms budget.
Cancellation acknowledgement was 8.149 ms against the approved 250 ms budget.
No solve-time or memory completion ceiling was introduced. The per-case
state/row/transition/byte/graph values in the permanent corpus are approved
operational caps.

## Telemetry and harness map

- Public C telemetry: `pc_solver_telemetry` in
  `engine/include/poecraft/solver.h` and `engine/src/solver_api.cpp`.
- Core counters/snapshots: `engine/src/solver_solve.cpp`,
  `engine/src/solver_calc.cpp`, `engine/src/solver_reforge.cpp`, and
  `engine/src/solver_internal.hpp`.
- Native harness: `engine/benchmarks/solver_benchmark.cpp`.
- WASM export: `pcw_solver_telemetry` in `bindings/wasm/wasm_api.cpp`.
- Worker/client path: `apps/web/src/app/engine-{wasm,worker,client,protocol}.ts`.
- Worker harness/corpus validation: `apps/web/test/solver-benchmark*.ts`.
- Cross-backend report comparator: `scripts/compare-solver-benchmarks.py`.
- Versioned corpus: `fixtures/solver-benchmarks/v1`.

Telemetry is read-only. It reports action availability honestly:
`evaluator_supported` is diagnostic, not a filter claim;
`priced_scanned` is what the current solve scans; missing or unsupported
requested actions qualify a converged value as
`exact_supported_priced_subset` with
`full_request_status: incomplete_action_subset`. Cancellation preserves a
frozen `abandoned` partial snapshot. Do not weaken these labels.

## Owner-approved permanent corpus

- **Oracle:** clean ilvl 86 Vaal Regalia to one T1 ES mod, and clean Regalia to
  T1 flat ES plus T1 fire resistance.
- **Ordinary:** empty rare Regalia to T1 flat ES, T1 increased ES, and the
  selected cold-resistance bench finish using ordinary currency, bench, and
  Restart choices.
- **Advanced:** empty rare Regalia to T1 flat ES, T1 increased ES, T2-or-better
  fire resistance, and the selected cold-resistance bench finish using ordinary
  currency only; no Eldritch side-intent setup/crafting.
- **Endgame:** rare Regalia with fractured T1 flat ES to T1 increased ES, T1
  ES/stun recovery (`LocalIncreasedEnergyShieldPercentAndStunRecovery6`), T1
  fire and cold resistance, and the selected Intelligence bench finish using
  ordinary, Harvest defence, Dense Fossil, bench, and Restart choices.
- **Stress/refusal:** missing price, state cap, unsupported Fracture,
  unreachable goal, cancellation, and full-registry cases.

Exact mod keys, frozen prices, starts, approved per-case caps, and simulation
tolerances are in the JSON cases. The second oracle changed after the S7.0
report from any-tier to T1/T1. Preserve the old row as historical evidence and
capture a fresh unoptimized T1/T1 baseline before S7.2 optimization; this is a
benchmark measurement, not an intermediate test-suite run.

## Recorded owner decisions

1. Tied or absent Eldritch dominance behaves like the corresponding ordinary
   currency.
2. Prefix/suffix Eldritch intent is a separate explicit setup-and-craft solver
   option.
3. Remove-crafted-modifiers costs one Scour.
4. Make strategies as optimal as practical and report exact, bounded, or
   heuristic status honestly.
5. There is no fixed solve-time or memory completion ceiling. Push performance
   as far as practical and keep all measurements visible; the approved 5x speed
   and 2x memory directional targets are minimums, not stopping points.
6. The approved responsiveness budgets are 50 ms worker steps and 250 ms
   cancellation acknowledgement. The approved operational/safety caps are the
   per-case values in the permanent corpus.
7. Oliver evaluates the performance gains and resulting strategies.
8. Plan-level correctness is measured by compiling the produced strategies and
   running them in the native simulator exactly 10,000 times at the end of the
   complete S7 plan using the existing per-case tolerances. Separate
   exact-evaluator or oracle matrices are not required acceptance gates.
9. Do not run routine test suites after intermediate S7 phases. Run a narrowly
   relevant test only when something is broken and the test is needed to
   diagnose or fix it; run the complete acceptance suite at the end of S7.
10. Oliver owns rendered and visual UI review. Do not run browser visual checks
    unless he explicitly asks.
11. Later, an owner-controlled readability pass may trim choices with negligible
    value impact if it reports the discarded delta and honest optimality status.
    This is not S7.1 work and can wait until after the exact S7 work.

Mechanic rulings come from Oliver. Do not research or guess them.

## Validation completed for S7.0

- Fallback native build and benchmark target.
- Native engine suite: 370,607 checks, zero failures.
- Twelve corpus specifications validate against the pinned compiled artifact.
- Native and WASM aggregate reports pass all enabled expectations.
- Cross-backend comparison: 421 checks, zero mismatches.
- `npx tsc --noEmit`, full `npm test`, production web build, Python binding
  tests, and the full repository test pipeline are deferred until the end of
  the complete S7 plan. Run one earlier only to diagnose or fix a broken path.

Rendered and visual UI review belongs to Oliver. Do not run a browser visual
gate in a later UI-facing phase unless he explicitly asks for one.

## Scope that remains parked

S6 Phase 3 ambient Emulator odds was skipped entirely and must not reappear.
Economy E0-E7 is complete except external production activation. Phase 12
accounts, publishing/community, mechanic track M1-M5, Phase 18 recombinators,
and ML remain deferred, blocked, parked, or later as documented.
