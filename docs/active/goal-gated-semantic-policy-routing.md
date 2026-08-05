# Goal-Gated Semantic Policy Routing

**Status: Gates 0-3 are complete; Gate 4 is active.**

Owner: Oliver

Parent: [Active work](README.md)

Starting commit: `cf67fd982e649193821424a0d8b85ea614027cf3`.

Branch: `codex/goal-gated-semantic-policy-routing`.

## Objective

Make normal Calculator solves explicitly request Oliver's selected
zero-goal-progress reforge model and compile solved policies by executable
policy region rather than by irrelevant junk-carrier identity.

The intended simple policy is derived from the solved policy:

```text
goal satisfied -> Goal
otherwise      -> selected reforge
selected reforge -> router
```

No rule may be hardcoded for Chaos, one-mod goals, a particular base,
modifier, fixture, or action family.

## Preserved boundaries

- Do not change crafting mechanics, action filtering, prices, Bellman
  comparisons, V3 evaluator semantics, state abstraction, quotient proof,
  caps, or the engine-wide unrestricted default.
- Calculator requests `goal_progress_gated_reforges: true`; other callers keep
  their explicit scope.
- Zero-satisfied-goal reforge outcomes may enter the existing retry basin.
  Partial-progress outcomes remain exact with their complete filtered action
  envelope.
- Ordinary, Essence, Harvest, and Fossil alternatives remain discoverable.
- Native compiled strategy output must shrink; frontend-only hiding is not an
  implementation.
- Broader routing is refused unless certified; existing off-policy or bounded
  Restart behavior remains the fallback.
- Add no condition vocabulary unless the existing vocabulary is proved
  insufficient.
- Rare-renewal reconciliation, five-goal finalization, checkpoint/replay, and
  general solver scaling are out of scope.
- Do not raise caps, push, merge, run rendered visual review, or rerun the
  standalone five-goal watchdog.

## Gate 0 — boundary, retained artifact, telemetry, and baselines

**Completed.** The retained condition and deterministic U/J/M/S definitions,
A/B measurements, policy and strategy hashes, 10,000-run results, and
old/corrected memory columns are recorded in
[`goal-gated-semantic-policy-routing-gate0.json`](../../fixtures/solver-reliability/v1/evidence/goal-gated-semantic-policy-routing-gate0.json).
The exact supplied condition is retained verbatim beside it, and the uniform
case input is frozen separately. U and J are not the same solve. Point C stays
empty until the new compiler exists, preserving A-to-B gating attribution and
B-to-C compiler attribution.

Retain the supplied 435,441-byte condition verbatim in milestone evidence.
Its required original SHA-256 is
`5D639CAF69E699186475B823BC86D24DB5643F24E3D2719183319806E5109520`.

Freeze four attributable cases:

- **U:** uniform one-operation destructive-renewal policy.
- **J:** reproduced approximately 435 KiB junk-route case.
- **M:** genuine multi-operation policy.
- **S:** state-local or observation-owned option that must not merge.

Record whether U and J are the same solve. If so, both acceptance thresholds
apply and U's 8 KiB JSON ceiling is stricter.

Capture the primary comparison at three points:

- **A:** ungated solve plus current compiler — product bug reproduction.
- **B:** gated solve plus current compiler — gating attribution.
- **C:** gated solve plus new compiler — compiler attribution.

For each point record solve options/scope; status, bounds, value and hashes;
renewal witness; discovered, expanded and policy-reachable states;
behavioural classes and final operation regions; nodes, edges, JSON bytes and
compile duration; total/maximum condition bytes; exact-state fallback count;
junk predicate count; exact evaluation; and simulation/off-policy result when
compilation succeeds.

Add deterministic observational compiler-memory telemetry with two columns:

- memory under the pre-milestone accounting; and
- audited complete compiler-owned memory, including condition strings,
  quotient members, feature indexes, route caches/nodes, and the growing JSON
  document.

At Gate 0 the audited view is observational only: it must not affect cap
decisions, strategy JSON, values, hashes, or policy behavior. Measure portfolio
headroom before corrected enforcement. Commit the boundary, retained artifact,
telemetry, and baselines separately.

## Gate 1 — Calculator scope and structured provenance

**Completed.** `calculatorSolveOptions` is the normal Calculator's single
option builder and always requests gated reforges, independently of optional
gap targets. Native and WASM defaults remain optional/unrestricted. Compiled
strategies now own optional `solver_policy_scope` metadata using the existing
telemetry spelling, while authored legacy strategies may omit it. Focused web
model/presentation tests and native compile/solve suites pass; those native
tests also preserve absent and below-tier retry aggregation, exact
partial-progress states, admitted alternatives, and cancellation/progress
contracts.

Centralize Calculator solve-option construction and make normal Calculator
solves send `goal_progress_gated_reforges: true`, including when both gap
targets are disabled. Keep the engine/WASM field optional and the native
default false. Diagnostic, benchmark, and unrestricted callers keep their
chosen scope.

Add optional, non-executable strategy metadata following existing naming
conventions, provisionally:

```json
"solver_policy_scope": "unrestricted" | "zero_progress_reroll_policy_restriction"
```

Preserve it through the native result, WASM/TypeScript strategy model,
cloning, persistence, and comparisons. Authored legacy strategies may omit it.
Prove Calculator gated behavior with and without gap targets; unrestricted
callers; absent and present-below-tier zero-progress retry; exact genuine
multi-goal partial progress; admitted alternative reforge families; matching
result/strategy provenance; and unchanged progress/cancellation behavior.

Commit Gate 1 separately.

## Gate 2 — compact certified uniform-region compilation

**Completed.** The existing primitive-renewal compiler now certifies both
exact and bounded executable policies directly from their complete reachable
closure. U compiles to the preferred 4-node/4-edge graph, 1,205-byte JSON,
154 condition bytes, and zero junk predicates with identical policy value and
hashes. Exact evaluation reconciles and 10,000 simulations have zero failures
and zero off-policy. Stale witness data, conflicting reachable operations,
and observation-owned policies remain on the general compiler. The complete
measurement is retained in
[`goal-gated-semantic-policy-routing-gate2.json`](../../fixtures/solver-reliability/v1/evidence/goal-gated-semantic-policy-routing-gate2.json).

Extend the existing primitive-renewal compiler and proof machinery. When all
policy-reachable non-goal carriers select one identical state-independent
destructive-renewal operation, verify action identity and legality on every
carrier, compatible engine kernel signatures, closure and properness from the
fixed start, and goal ownership of every terminal exit. Exact and bounded
executable policies are both eligible when proved.

The preferred output remains the existing four-node/four-edge shape. The hard
structural acceptance is one start path, one decision/router, one success
terminal, one renewal operation, one renewal back-edge, zero junk-fingerprint
predicates, and no duplicate renewal route. A small certified off-policy or
fallback structure is allowed only with a documented reason.

Any conflicting carrier, incompatible preserved boundary, state-local
observation, or unproved closure uses the general compiler. Commit Gate 2
separately.

## Gate 3 — route by final executable policy region

**Completed.** Generic behavioral-quotient and strict-policy routes now target
the final executable `policy_region_by_state`. Represented strict members are
grouped by final operation/continuation region, minimized only against other
regions and off-policy states, and emitted once per final region. A strict
decision-DAG partition stops as soon as it selects one region and invokes the
same quotient-feature minimizer; exact-state serialization remains its final
fallback. Structured observation-owned routes keep their prior independent
path. Focused compile and solve suites pass, and the measurements are retained
in
[`goal-gated-semantic-policy-routing-gate3.json`](../../fixtures/solver-reliability/v1/evidence/goal-gated-semantic-policy-routing-gate3.json).

Generalize the existing quotient-feature and policy-route DAG path so its
classification target is the final `policy_region_by_state`:

1. Map represented strict members through behavioural representatives to the
   final emitted operation/continuation region.
2. Group states by final region, not behavioural class.
3. Minimize a region only against states in a different region or off-policy.
4. Stop a classifier partition when all remaining members select one final
   region.
5. Split only on features distinguishing executable regions.
6. Route unknown, conflicting, or unproved values to the existing off-policy
   or bounded fallback.
7. Keep exact-state serialization as the final correctness fallback.

Conservatively preserve Unveil, Fracture, Imprint, gated retry basins,
observed choices, protected repeat, temporary-bench repeat, and all other
state-local fixed options. Merge only identical complete executable recipes
and continuations. Commit Gate 3 separately.

## Gate 4 — corrected compiler-owned memory contract

**Active.**

After routing lands, enforce the Gate 0 audited complete compiler-owned memory
accounting while retaining both old and corrected telemetry columns. If an
accepted portfolio case reaches the existing cap, reduce retained buffers,
duplication, or lifetimes. Do not raise the cap or restore under-counting.

Commit Gate 4 separately.

## Gate 5 — qualification

Case acceptance:

- **U:** structural goal-or-repeat loop, preferred 4/4 form, zero junk
  predicates, JSON below 8 KiB, and identical value for identical gated solve
  options.
- **J:** at least 95% reduction from gated/current-compiler condition or JSON
  size; if J is U, satisfy both and the stricter 8 KiB limit.
- **M:** preserve every genuine operation distinction and keep an explicit
  off-policy carrier off-policy.
- **S:** preserve state-local/observation-owned behavior, exact evaluation,
  and simulator choices.

For each changed compiled-strategy regression, exact-evaluate and reconcile
expected cost, then run exactly 10,000 simulations with zero off-policy and
zero unsuccessful runs while preserving terminal/goal semantics.

Run a focused V1/V3 equivalence subset covering zero progress,
present-below-tier zero progress, genuine partial progress, ordinary reforge,
forced Essence, targeted/exceptional Harvest, and positive/zero-weight/
additive/forced Fossil behavior. Require target-map, probability, and semantic
action agreement within existing family tolerances. Run the complete 252,997
matrix only if this subset exposes a discrepancy.

Run the representative reliability portfolio and compare nodes, edges,
condition bytes, JSON bytes, compile wall, old memory, and corrected memory.
No unaffected strategy may materially regress without explanation.

Final acceptance runs, once after implementation:

1. focused native compiler/solver tests;
2. targeted V1/V3 gated-outcome subset;
3. representative reliability workflow with 10,000 simulations per verified
   strategy;
4. release WASM rebuild;
5. WASM worker and Calculator regressions;
6. `npm test` and `npx tsc --noEmit` in `apps/web`; and
7. `powershell -File scripts/test.ps1` exactly once.

## Completion and report contract

Update stable solver, strategy, and Calculator documentation. Archive this
plan with retained evidence and a final report, clear HANDOFF to no active
boundary, and leave the branch clean.

The report separately answers what gating removed before compilation changes;
what region routing removed after gating; why the old compiler emitted the
435 KiB condition; which partial-progress states remain discoverable; when the
compact renewal loop is certified or refused; how multi-operation and
state-local policies are protected; old versus corrected memory; changes in
nodes, edges, conditions, JSON, compile wall, and evaluation performance; and
whether any value, mechanic, action filter, V3 result, cap, or unrestricted
default changed.
