# Session Handoff

**Status: no active implementation boundary. A proposed next plan is ready
for review.**

The
[Calculator WASM Scheduling And Selected-Policy Refinement](docs/archive/2026-08-14-calculator-wasm-scheduling-progress/README.md)
milestone completed and was archived on 2026-08-16. Oliver must choose the
next bounded plan before implementation resumes.

The proposed
[Five-T1 Restart-Monotone Strategy Recovery](docs/future/five-t1-restart-monotone-recovery-plan.md)
plan incorporates the priced-base five-natural-T1 Calculator failure and the
subsequent external review. It is deliberately not active: Oliver may review
or revise its fail-closed certification, Restart-monotonicity, automatic
admission, evaluator-scaling, responsiveness, and materiality gates before
selecting it. No source implementation from that plan has started.

The accepted behavioral source checkpoint is
`f3b908011490b39347e8fad6945b471d888b9845` on
`codex/solver-goal-realignment`. The final documentation/archive checkpoint is
the local commit containing this handoff. Nothing was pushed.

The representative from-empty five-natural-T1 Allflame Conquest Lamellar now
returns in about 14.25 seconds in the normal eight-item release-WASM profile.
It publishes a 184-node/666-edge non-Chaos strategy independently
exact-evaluated at `624800.9519118543`, about 59.7 times cheaper than the old
Chaos renewal. The eight-item and 1,024-work modes have identical termination,
bounds, evaluated cost, action IDs, and strategy hash. The result remains
honestly `bounded_feasible / numerical_stability` with lower zero because one
missing-price and one unsupported action leave the full envelope open.

A subsequent compiler audit found 170 bounded route defaults to
`bounded_default_restart` in that graph. Its exact evaluation still proves the
cost and executability of the emitted product graph, but it does not prove that
the intended selected-policy router has zero reachable misses because Restart
absorbs such misses. Treat the 624,800 result as provisional policy-coverage
evidence until the proposed plan's fail-closed certification gate passes.

Exact lift, compilation, and graph assertion now run cooperatively inside the
native stepped solve. ABI phase values 1-3 are unchanged; `refining = 4`,
`compiling = 5`, and `certifying = 6` are append-only through native, WASM,
worker, and Calculator surfaces. Native `Done` means `finish()` is packaging-
only, and cancellation/abandon remains available during retained finalization.

The exact three-prefix and three-prefix-plus-spell-suppression witnesses retain
their values and hashes with 164.756 ms and 238.509 ms maximum release-WASM
steps. The retained four-natural-T1, Eldritch, Warlord, Imprint, and three-
prefix controls each completed exactly 10,000 Simulator runs with zero
failures, cap hits, or off-policy executions. The five-T1 policy exceeds the
unchanged sampling action horizon, so its independent exact graph evaluation
is authoritative and 10,000-run sampling is not applicable.

The selected-candidate proof audit, retained-memory audit, and append-only ABI
audit passed. A release-WASM build under Emscripten 6.0.0 requires
`solver_solve_finish.cpp` to be compiled as an optimized non-LTO object before
the remaining full-LTO link because full-LTO coroutine lowering otherwise
produces invalid SSA. The qualified tracked WASM SHA-256 is
`c1b873930209fe86fae066ea85cf24130f0638eba0783c214bfa22ffcc5fd528`.

The one final `powershell -File scripts/test.ps1` invocation passed 3,462,490
native checks plus ingest, economy, fixtures, artifact validation, Python
bindings, benchmark validation, 28/28 release-WASM smoke checks, and the
complete non-visual web suite. Focused native tests, the release-WASM rebuild,
`npx tsc --noEmit`, and `git diff --check` also passed. Oliver retains rendered
and real-device/browser review; none was performed.

Detailed values, report hashes, sampling, and the disclosed pre-existing
benchmark expectation-vocabulary mismatch are in the
[final acceptance record](docs/archive/2026-08-14-calculator-wasm-scheduling-progress/evidence/final-acceptance.md).
