# Carrier-Ladder Released-Candidate Reclamation v1

**Status: completed — Outcome A.** The narrow lifecycle repair and production
activation qualified on 2026-08-30. Work is local-only and was not pushed.

Parent: [Documentation archive](../README.md)

Detailed chronology: [execution log](execution-log.md).

## Objective

Repair the concrete resumable-candidate lifecycle defect exposed by the matched
clean-five control before adding any new retention-capacity proof or planner
heuristic.

An active candidate may retain its existing one-attempt preference when it
yields to a named exact obligation. Once refused, it must release its owned
traversal payload and must not suppress the ordinary joint-policy attempt at
that checkpoint or any later checkpoint. The ordinary ladder, compiler,
evaluator, and incumbent portfolio remain the sole production authorities.

## Frozen Evidence

- Retained source checkpoint: `f85e026db160d28e5f300d21c34d2e4dbe4e0bf4`.
- Matched clean-five immutable revision:
  `case-rev-54343c2296afe4f624f622c16779498c`, SHA-256
  `54343c2296afe4f624f622c16779498cfc0f0d03f6170e323837a1a06c74edb0`.
- Current-main clean-five control:
  `build/regression/current-complete-interleave-fix-120s/report.json`, exact
  cost `85408.64362148782`, 24 ordinary joint-policy attempts, 23 missing-state
  services, and 301,858,199 peak native-owned bytes.
- Rejected production clean-five result:
  `build/qualification/resumable-joint-policy-continuation-v1/clean5-production-120/report.json`,
  primitive-fallback cost `470485191.442781`, 16 joint-policy attempts, 15
  missing-state services, and 550,263,386 peak native-owned bytes.
- New private causal witness:
  `build/qualification/resumable-joint-policy-nonexclusive-service-v1/clean5-diagnostic-before/report.json`.
  Candidate `798391a94c01cd51` captured once, resumed twice, yielded three
  times, grew to 304,701,928 retained bytes against its 268,435,456-byte cap,
  and ended `refused/resource_interrupted`. The refused object remained
  present with the complete payload. The call site also returned immediately
  on every `Yielded` or `Released` result instead of running the ordinary
  incumbent attempt.
- PDR positive control:
  `build/qualification/resumable-joint-policy-continuation-v1/pdr-diagnostic-r3/report.json`.
  Candidate `51d67b3219b70c43` stayed below the same cap at 169,227,180 bytes,
  resumed twice, yielded three times, and coexisted with an independently
  exact ordinary result at `3758.12442725521` Chaos.

## Diagnosis

The existing capacity limit already distinguishes the observed clean-five
candidate from PDR after bounded continuation growth. The failure is that
`release(ResourceInterrupted)` changes only the lifecycle enum: it does not
free selection values, boundary vectors, traversal nodes, or fixed decisions.
The wrapper remains engaged, and `try_install_reachable_incumbent` treats both
`Yielded` and `Released` as reasons to return before the ordinary attempt.

This violates the retained contract that ordinary work remains available
between candidate service events. It is an implementation/lifecycle defect,
not yet evidence that a new planner or admission score is required.

## Plan

1. Add a payload-release operation to the internal continuation. Preserve
   compact counts, identities, refusal reason, peak bytes, and sampled missing
   state identities, but reclaim all vectors needed only by an active or
   complete candidate.
2. Clear wrapper-owned boundary/fallback payload when a candidate is refused.
   Keep the refused tombstone only for bounded diagnostics and to prevent the
   same solve from repeatedly recapturing an already rejected candidate.
3. Preserve the existing bounded preference for an active yield, but make
   refusal non-exclusive: record it and continue the ordinary joint-policy
   attempt. A complete candidate may still enter the existing
   compiler/evaluator path, without publication authority.
4. Extend the mechanics-independent fixture to prove real reclamation and
   ordinary-work availability after both yield and resource refusal. Run only
   the focused native fixture while implementing.
5. Rebuild the native benchmark and rerun exactly one matched clean-five
   private diagnostic plus the PDR positive control. Require:
   - clean-five refused retained bytes become bounded and ordinary joint-policy
     attempts/services recover enough to retain the qualified executable upper;
   - PDR still preserves one candidate across multiple genuine resumes and the
     ordinary solver still reaches its exact result;
   - neither candidate publishes without the existing independent compiler and
     evaluator.
6. Only if both private controls pass, restore the previously tested cached
   fast-byte accounting, activate the mode through the existing high-impact
   profile, and run the production primaries. Do not invent a retention
   certificate unless the repaired lifecycle still fails with named evidence.
7. If production primaries pass, run the remaining controls, rebuild WASM, and
   perform the final affected-layer acceptance once. Simulator verification is
   10,000 runs only if a changed compiled strategy reaches qualification.

## Stop Conditions

- Stop production promotion if clean-five does not retain an independently
  evaluated executable upper within its `98220` Chaos and `10000` expected
  action gates.
- Stop and record Outcome B if the repaired lifecycle still admits a harmful
  active candidate; only then design the smallest evidence-backed admission or
  continuation predicate.
- Stop on invalid compilation/evaluation, off-policy mass, cap-accounting
  mismatch, or any change to ordinary authority.
- Do not broaden into fragment work, a new planner, mechanic changes, lab/MCP
  restoration, GUI work, or catalog edits.

## Acceptance

The boundary passes only when both primaries pass under production accounting,
the remaining written controls pass, release bytes are genuinely reclaimed,
ordinary work remains observable after candidate events, native/WASM artifacts
are current, and the final affected-layer suite is green. No soak result is
claimed by this boundary.

## Outcome

Outcome A qualified. Terminal refusal now reclaims candidate-owned traversal
payload and no longer suppresses the ordinary joint-policy attempt. Active
yield remains a bounded exclusive preference. Production enables the
resumable path only through the existing high-impact executable-upper profile,
and cached candidate ownership is reconciled against the audited estimator.

The retained implementation reproduced the exact PDR strategy at
`3758.1244272552067` Chaos and the clean-five strategy at
`85408.64362148782` Chaos. Partial four-to-five and non-armour exactly matched
their current-main controls. Native acceptance and rebuilt-WASM web acceptance
passed. A 10,000-run PDR simulation was also completed, but the strategy was
byte-identical to an already qualified artifact, so that rerun was unnecessary
and is not part of the minimum acceptance rationale.

The next sequenced boundary is the proof-only PDR Retention-Capacity
Operator-Proof Pilot v1. It must begin in shadow mode from measured live
obligations and may not change solving, scheduling, publication, or pruning.
