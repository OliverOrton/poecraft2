# Core Policy Preservation And Direct Certification

**Status: active implementation boundary selected 2026-08-05.**

Owner: Oliver

Parent: [Active work](README.md)

Starting commit: `9b2bcfb82c694df0a77858c8c63fdae20d8b4f68`.

Branch: `codex/core-policy-publication-recovery`.

## Objective

Prove and repair the suspected publication regression in which core solving
selects a finite policy, and may close its coarse lower and upper bounds, but
strict refinement, compilation, or final publication later discards that
policy or replaces it with a generic renewal fallback.

Preserve the current core search unless the frozen evidence disproves that
hypothesis. A selected core policy becomes public only after its compiled
strategy parses, is proper, exact-evaluates completely from the authored
concrete start, and has zero off-policy probability. Equal scalar bounds alone
do not establish executable exactness.

## Preserved boundaries

- Do not change mechanics, prices, objectives, action filtering, goals, state
  abstraction, strategy vocabulary, or the requested action envelope.
- Do not raise memory, reforge-work, transition, state, compiler, or watchdog
  limits.
- Do not weaken exactness or publish an unverified strategy.
- Reuse `BoundedPolicyIncumbent`, `RetainedCompiledPolicyArtifact`,
  `assert_compiled_policy_exact()`, `lift_policy_quotient()`, the existing
  compiler, exact evaluator, proper-policy machinery, and fallback portfolio.
- Keep native code authoritative. WASM and Calculator layers transport and
  present the native result.
- Preserve resource accounting across direct certification and any later
  targeted strict refinement.
- Run no rendered browser review and keep all commits local.

## Gate 0 — freeze and localize the regression

Freeze deterministic Calculator-equivalent requests with the concrete start,
goal, economy, admitted actions, solve options, artifact identity, and limits:

- a real four- or five-goal case that the pre-2026-07-31 WASM returns exact;
- the current two-goal case that returns no policy; and
- the current three-goal case that returns only the Chaos loop.

Recover the original five-goal request from saved workspace/session data when
possible. Otherwise use an isolated historical worktree and bounded
deterministic search for a real historical-exact/current-degraded case. Replay
with the matching historical frontend/WASM before comparing historical and
current engines with byte-identical serialized requests. Record frontend
option differences separately, especially
`goal_progress_gated_reforges`.

Capture the core convergence/termination, bounds and evaluated value; selected
policy and transition hashes; reachable policy states/actions; compatibility
triggers; compilation and exact-evaluation outcomes; strict-refinement work,
carriers, transitions and cap; and final public status/artifact identity. Add
only bounded deterministic telemetry needed to distinguish:

```text
core search -> selected candidate -> direct assertion -> strict lift -> publication
```

Before implementation, prove the current primary case retains its existing
hashes, bounds, work, and classification. Gate 0 must answer whether the core
policy still exists and where it is lost.

## Gate 1 — retain the core-selected candidate

Materialize the selected policy at the end of core solving before strict lift
or publication revocation. Reuse incumbent/provenance structures and retain
the complete policy/preferences, policy closure, core value/bounds, goal,
economy, artifact, vocabulary, graph and generation identities, policy and
transition hashes, certification state, and owned memory.

The candidate is not executable merely because it is retained. A later
failure may make it unpublishable, but cannot erase its identity or rewrite
the outcome as generic “no solution found.” If fallback wins publication,
telemetry must preserve the selected candidate and its exact refusal reason.

## Gate 2 — direct candidate certification

Before `lift_policy_quotient()`, compile the retained core policy through the
existing semantic executable-region compiler even when compatibility triggers
exist. Parse the exact compiled artifact, exact-evaluate it from the authored
start, and require complete pricing, proper absorption, and zero off-policy
mass.

- A cost-reconciled executable candidate is retained immediately.
- It is exact only when its exact cost also closes a proved admissible global
  lower bound over the complete requested action envelope.
- A proper executable cost mismatch remains an honest bounded policy whose
  upper is its exact evaluated cost.
- A concrete compiler/evaluator incompatibility retains its deterministic
  witness and proceeds to targeted strict refinement.
- Direct work is charged to the unchanged shared resource budget.

Once a core-selected candidate is certified executable, a later failed lift
must not erase it or cause compilation to reconstruct a different artifact.

## Gate 3 — target demonstrated incompatibilities

Use direct-certification failure witnesses to seed only the affected existing
refinement path: semantic compiler routing for an observation incompatibility,
carrier/router seeding for illegal or off-policy behavior, mismatch regions
for cost reconciliation, bottom-component witnesses for improperness, and the
owning phase for resource refusal. Do not eagerly construct the complete
strict carrier graph merely to certify the already selected strategy.

Alternatives remain necessary for global exactness, but not for proving that a
fixed selected policy is executable. Strict refinement remains the repair and
proof path for cases that demonstrate the need.

## Gate 4 — publication invariants

Enforce across native results, WASM transport, and Calculator presentation:

- every finite certified upper has a retained executable witness;
- equal certified lower/upper bounds have certified strategy JSON;
- `exact` requires executable certification plus global lower-bound closure;
- a proper exact-evaluated candidate with an open gap is
  `bounded_feasible` or `bounded_near_optimal`;
- post-core failure reports the precise certification stage instead of “no
  policy”;
- an uncertified preferred policy cannot erase an executable candidate; and
- compilation returns the exact retained artifact that was certified.

## Gate 5 — qualification and completion

The frozen historical four-/five-goal case must again produce an executable
current-WASM strategy; the two-goal and three-goal cases must publish their
core-selected policies whenever direct certification proves them, using Chaos
only when the evidence supports it. No failed candidate may become public and
no equal certified bounds may lack strategy JSON.

Add focused native regressions for successful direct certification,
cost-mismatch bounded retention, off-policy refusal, preservation across a
failed strict lift, and the equal-bounds/artifact invariant.

Run final acceptance once, after implementation:

1. build native and run focused compiler/solver/evaluator suites;
2. replay the frozen native cases;
3. exact-evaluate every published qualification artifact;
4. run exactly 10,000 simulator executions per required strategy;
5. rebuild release WASM and replay the frozen cases through the worker;
6. run `npm test` and `npx tsc --noEmit` in `apps/web`; and
7. run `powershell -File scripts/test.ps1` exactly once after all earlier
   gates pass.

Archive this plan with a final report and deterministic evidence, update the
stable solver/product references, clear `HANDOFF.md`, and leave the branch
clean. The implementation, evidence, archive, and generated WASM belong in one
final commit after this boundary commit.
