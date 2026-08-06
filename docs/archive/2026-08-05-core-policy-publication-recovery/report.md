# Core Policy Preservation And Direct Certification Report

**Status: implementation complete; requested acceptance incomplete because one
input was unavailable and the single pipeline-wrapper invocation did not
finish green.**

## Outcome

The regression hypothesis was correct. On the real Runic Gauntlets five-goal
case, the current core solver still produced the historical policy with the
same value and hashes, but the pre-repair publication path discarded it after
core closure when strict refinement exhausted its remaining reforge budget.

The repaired pipeline retains that candidate before publication work, runs the
existing compiler and exact assertion directly, and preserves stage-specific
failure evidence. Direct certification proved the five-goal coarse graph
improper, so the existing strict proper-policy path repaired only the
demonstrated incompatibility and published a bounded executable strategy. The
recovered two-goal candidate compiled to a proper, completely priced,
zero-off-policy strategy whose exact cost differed from its coarse estimate;
that artifact remained executable and strict refinement published the same
selected policy semantics at the exact evaluated upper.

## Gate 0 Localization

The historical pre-2026-07-31 WASM result for
`runic-gauntlets-partial-five` was exact at `984705.7127622988`, with transition
hash `d3300c001dda774c`, policy hash `d030150696da20a8`, and a 936-node,
2,140-edge, 2,999,738-byte compiled graph. Its captured frontend request did
not contain `goal_progress_gated_reforges`; the current Calculator explicitly
sets it to `true`.

Before this repair, the current native engine reproduced core value
`984705.71276229876`, the same two hashes, 937 expanded states, and six policy
sweeps. It then exhausted the strict reforge budget at `19,051,550` logical
work and returned `refused_resource_cap` with no public policy. Thus core
search had not regressed; candidate retention/publication had.

## Implemented Contract

- `BoundedPolicyIncumbent` now retains the complete selected policy and
  preferences, reachable closure, values/bounds, goal/economy/artifact/action/
  graph/generation identities, hashes, certification state, and owned memory.
- Telemetry reports `core_policy`, `direct_certification`, `strict_lift`, and
  `publication` independently. A later failure cannot rewrite “core policy
  found” as “no solution found.”
- The retained core candidate is compiled, parsed, and exact-asserted before
  strict lift. Executability requires proper absorption, complete exact cost,
  and zero off-policy mass. Cost mismatch retains an honest bounded upper.
- Demonstrated improperness, routing/off-policy witnesses, or mismatch seed
  the existing strict quotient/proper-policy path. Direct work reduces the
  remaining unchanged budget.
- Final publication rejects finite certified uppers without retained artifact
  bytes and rejects equal certified bounds without a certified compiled graph.
  `exact` additionally requires global lower-bound closure.
- Compilation returns the retained asserted artifact. Compiler route
  compaction and exact-evaluator behavioral partitioning avoid reconstructing
  full repeated carrier graphs. Shared exact rows are solved once and raw
  occupancy is reconstructed and residual-checked.
- Calculator presentation consumes native stage fields. It contains no new
  mechanic, transition, policy, or exactness authority.

## Final Qualifications

### Real five-goal Runic Gauntlets

Native and release WASM agreed on the core value and historical hashes. Direct
certification recorded `improper_policy`, exact partial cost
`121.66891655289717`, and off-policy probability
`0.83735237141173724`; it did not publish that graph. Strict refinement
completed with 19,503 exact states, 19,501 classes, 350,906 transitions, and
19,035 kernels. The retained public artifact is bounded because the final
global lower bound remains open.

Both runtimes compiled the same 195-node/723-edge/358,092-byte graph and
exact-evaluated it at `923509.244023527`, success probability `1`, and zero
off-policy mass. The native certified upper is `923509.24399211595`; the
`0.000031411` difference is within the frozen relative exact-evaluation
tolerance and is not presented as global exactness.

Each runtime executed exactly 10,000 simulations with seed `20260806` and the
unchanged 100,000-action cap: 2,521 succeeded, 7,479 reached the action cap,
and zero failed off-policy. The frozen `minimum_success_rate: 1.0` expectation
therefore remains an honest Monte Carlo miss; the cap was not raised. Exact
evaluation, not this action-capped sample, proves the graph proper.

### Recovered two-goal Dire Pelt

Native core search selected value `240.49226451842537`. Direct certification
proved the candidate proper, completely priced, and zero-off-policy at exact
cost `232.02487923415782`; the mismatch correctly blocked an exact label but
retained the executable candidate. Strict lift completed and published a
bounded upper of `232.02487923416101`; final exact evaluation matched at
`232.02487923415782` with success probability
`0.9999999999991992` and zero off-policy mass. The graph has 18 nodes and 50
edges. The fixed-seed native sample completed 10,000/10,000 successes with no
off-policy failure.

Release WASM independently published a numerically equivalent bounded policy:
upper `232.02487923461015`, exact cost `232.02487923461`, success probability
`1.000000000000828`, and zero off-policy mass. Its floating-point tie path
retained 4,051 strict states and compiled 25 nodes/77 edges rather than native's
4,027 states and 18/50 graph; both exact evaluations and both 10,000-run samples
passed. This platform-structural difference is disclosed rather than described
as byte-identical strategy output.

## Scope And Limitation

No crafting mechanic, price, objective, action filter, goal meaning, state
abstraction, strategy vocabulary, resource cap, or watchdog changed. The
historical/current frontend option difference is recorded separately.

The exact serialized three-goal request named by the milestone was not present
in either supplied attachment, saved workspace data, or the repository. It
could not be replayed without inventing its start, goal, prices, or action
scope, which the milestone forbids. No synthetic replacement was used. The
implementation has focused native coverage for cheaper certified-candidate
preservation across later strict failure, but the requested real three-goal
qualification remains unproven until Oliver supplies that serialized request.

## Acceptance

- Native release build: passed.
- Focused refinement: 4,875 checks, 0 failures.
- Focused compiler: 791 checks, 0 failures.
- Focused exact evaluator: 16,761 checks, 0 failures.
- Focused quotient proof: 357 checks, 0 failures.
- Focused solve: 98,247 checks, 0 failures.
- Release WASM rebuild: passed; SHA-256
  `5cd3fa531abb4c0bc0afc094b9831d6a6c1bbf93db5317ef19d2d5899dfab2ad`.
- `npm test`: passed, including 27/27 worker smoke checks.
- `npx tsc --noEmit`: passed.
- Full `scripts/test.ps1`: invoked exactly once. Ingest, economy, canonical
  database/fixture/artifact validation, and Python binding stages passed. The
  wrapper stopped at five stale solver-API assertions which still coupled
  exact solve scope to coarse per-state queries and expected compilation to
  duplicate an already retained artifact. After correcting only those test
  expectations, focused API acceptance passed 549/549 checks and all 12 CTest
  targets passed. The wrapper was not invoked a second time; benchmark-corpus
  validation passed in CTest and the web/WASM and TypeScript commands above
  passed after the correction.
- Rendered browser review: intentionally not run.
