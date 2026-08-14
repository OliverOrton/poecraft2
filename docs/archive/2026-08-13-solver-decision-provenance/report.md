# Solver Decision Provenance And Result-Truth Hardening Report

**Outcome: complete on 2026-08-14.**

The selected hardening work now makes the action envelope, policy decision,
and published proof claims agree across native, release WASM, and the web
contract. Calculator odds inspection no longer mutates ordinary Solve action
scope. Sparse decisions use one Q authority. A smaller finite expected cost is
the canonical winner, while a separately named stability tolerance prevents
near-equal policy-iteration cycling and exact publication reconciles the
canonical strict winner.

Coarse termination, precise stop cause, proper executable publication, global
exactness, and lower-bound certification remain distinct claims. Legal
`termination = ExactClosed` plus `policy_status = BoundedFeasible` results are
preserved when coarse discovery actually closed. Open envelopes and unclosed
strict refinements cannot manufacture a globally certified lower.

## Delivered contracts

- Exact selected-action odds may materialize a requested Fossil, but the
  product action envelope and the priced candidate IDs derived from it do not
  depend on which odds action the user last inspected.
- Incremental sparse-row evaluation delegates to the same probability,
  denominator, dead-mass, finiteness, and observed-choice semantics used by
  the authoritative sparse row evaluator.
- Strict finite argmin, deterministic exact-equality tie-breaking, and the
  tolerance-guarded policy-change decision are separate operations.
- A tolerance-suppressed strict winner cannot carry exact publication until
  it is reconciled with the canonical winner.
- Synthesis of coarse `ExactClosed` from `None` or `NoExecutablePolicy`
  requires explicit evidence that coarse discovery completed.
- Public lower-bound telemetry states whether the lower is globally certified
  and identifies its proof family without changing the stable C ABI v2.
- Optional strict-policy reuse failure no longer blocks the existing certified
  admitted-row fallback.
- If strict refinement ran but did not close and the result is not exact, the
  public lower uses the universal certified zero with provenance
  `unclosed_strict_refinement_universal_zero`. Valid positive closed-envelope
  lower bounds remain legal.

## Runtime qualification

The four frozen Goal Realignment controls remain exact and native/release-WASM
equal. The primary remains exact at `3745.7309340083884` chaos and completed in
92.854 seconds natively and 147.292 seconds in release WASM, both below five
minutes and materially below the frozen 218.703/277.906-second baselines.
Warlord, Eldritch, and Imprint retain their exact values; all four compiled
strategy comparisons pass.

The current 49-case native and release-WASM breadth runs each published all 49
policies: 2 exact, 29 bounded feasible, and 18 cap-stopped policies. Their
semantic comparison passed 3,729 checks with zero mismatches. That breadth run
exposed one soundness witness before finalization: case
`natural-t1-breadth-two-f2efa568ca5e` claimed lower
`278.9524669786507` while its executable strategy exact-evaluated to the
cheaper `274.52189255967016`. The optional strict reuse probe had failed before
the certified fallback could run, and the unclosed strict result retained a
coarse lower without global authority.

The repaired case now publishes a truthful bounded result with lower `0`,
evaluated upper `453.49800766071047`, a visible
`refused_resource_cap` stop, and the new universal-zero provenance. Its native
and final release-WASM semantics match. As in the accepted Goal Realignment
portfolio precedent, the full breadth run was not repeated after the
evidence-selected finalizer correction; affected current reports and the
focused native/WASM overlay are pinned in the
[Gate 5 record](evidence/gate5-final-acceptance.md).

Seven emitted release-WASM strategies whose prepared graph changed from the
frozen Goal Realignment corpus were each run with the required 10,000 samples.
Six completed 10,000/10,000 successes. The very large
`natural-t1-deep-four-dense-a64d3d932590` graph exact-evaluates with success
probability one and zero off-policy mass, but its sample completed only 16
runs; the other 9,984 reached the unchanged 100,000-action Simulator ceiling.
Across all seven reports there were zero off-policy failures. The exact graph
evaluation, rather than the finite sampled horizon, remains policy authority.

## Qualification limits

The historical Fossil-to-Chaos request remains unavailable, so its symptom is
not localized and no current repair is presented as its cause. The controlled
Calculator request-scope defect is independently reproduced and fixed.

The benchmark harness writes the requested 10,000-run reports correctly but
returns a nonzero process status because its ordinary corpus assertion still
expects the default 100-run fixture count when `--verification-runs 10000` is
explicitly supplied. The tracked result inspection confirms 10,000 requested
runs per changed strategy. This reporting mismatch is disclosed rather than
expanded into an unrelated harness change.

No cap or Simulator action limit was raised, no mechanic ruling changed, and
no rendered or visual review was performed. The deferred mixed-action
seven-step follow-up was not started.

The first attempted full-pipeline invocation reached 3,464,781 native checks
and exposed one stale S8.3 exact-tie assertion. That test still expected a
lower-variance blocker to win, contrary to Gate 2's accepted strict objective:
exactly equal costs use stable row identity and do not add variance as a
second objective. The assertion was corrected, the focused S8.3 suite passed
525 checks, and the post-fix final pipeline then passed end to end. This record
discloses both invocations rather than presenting the first as a passing
acceptance run.

The accepted pipeline passes 18 ingest tests, 12 economy tests, 17 fixture
tests, canonical data and compiled-artifact validation, 3,464,781 native
engine checks, 12 solver benchmark specifications, 28/28 release-WASM smoke
checks, and the complete non-visual web suite.

## Final state

Implementation checkpoints run from `405b3af` through `970e133`, followed by
the final acceptance/archive checkpoint. The completion contract passes, the
milestone is archived, and `HANDOFF.md` has no active implementation boundary.
