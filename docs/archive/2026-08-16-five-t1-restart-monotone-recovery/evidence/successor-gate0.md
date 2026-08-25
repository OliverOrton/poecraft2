# Successor Gate 0 - Exact-Evaluator Phase Attribution

**Status: passed on 2026-08-16; Gate 1 is next.**

Parent: [selected successor plan](../successor-plan.md)

## Result

The compiled-strategy evaluator now retains active-work nanoseconds for model
setup, observation preparation, raw pair discovery, pair interning, exact
kernel lookup/construction, pair refinement, component construction, component
solve, and finalization. Pair interning and exact-kernel time are documented
inclusive subsets of pair discovery.

Resource-stopped diagnostic results retain the active subphase, raw/refined
pair counts, and the actual internal refined-pair limit. Direct Solve
certification copies those fields into `policy_refinement.direct_certification`
telemetry, so Witness B can attribute the failed candidate even though the
benchmark's separate external verification remains disabled.

Timing accumulates only while native `step()` work is active. Browser/event-loop
suspension between calls is excluded. The canonical standalone evaluator JSON
was deliberately left unchanged because it has a deterministic byte-equality
contract; solver telemetry already owns nondeterministic wall attribution.

## Focused Verification

- `powershell -File scripts/build.ps1` - passed.
- `powershell -File scripts/dev-engine.ps1 -Task Test -Suite eval` - 16,819
  checks, zero failures.
- `powershell -File scripts/dev-engine.ps1 -Task Test -Suite solve` - 96,083
  checks, zero failures.

Focused tests prove both a completed destructive-refinement graph and a
pair-refinement memory refusal. The complete result reaches `done`; the capped
result remains at `pair_refinement`, preserves 76 raw pairs, and reports the
configured refined-pair limit instead of the unrelated external benchmark
limit.

No real five-T1 witness, release-WASM build, web suite, Simulator verification,
or full acceptance pipeline was run at this intermediate gate.
