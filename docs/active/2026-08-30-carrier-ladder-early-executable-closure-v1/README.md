# Carrier-Ladder Early Executable Closure v1

**Status: active implementation boundary.** Selected by Oliver on 2026-08-30
with the instruction to keep working until a major improvement is found. Work
is local-only.

Parent: [Active work](../README.md)

## Objective

Make the existing near-optimal PDR direct core policy a proper independently
evaluated executable candidate materially earlier than terminal strict exact
closure, using the existing carrier-ladder, continuation, compiler, evaluator,
and strict-refinement authorities.

This is not fragment or retention/capacity work, a new planner, a permanent
item/base library, a broad whole-policy retry, or a fixed scheduling window.

## Starting Evidence

- Fresh PDR exact solve result: `3758.1244272552067` Chaos.
- Direct core policy identity: `3153018158135326262`, solver estimate
  `3770.7208497712586`.
- Independent direct evaluation: success `0.0289578150891359`, failure/off-
  policy `0.9710421849108641`, with complete pricing and no stop,
  action-not-applied, no-matching-edge, or unresolved mass.
- The entire failure is routed through exactly one
  `certification_fail_closed` policy-router default edge. The evaluator reached
  21,736 raw pairs and 8,440 refined pairs; the direct compiler emitted no
  separate product default in this assertion.
- Terminal strict refinement produces proper independently evaluated candidate
  `12798937337675811455` at `3758.1244272552067` Chaos.
- Prior ladder work proved that existing exact service can close named missing
  continuations, while retrying whole-policy assembly after every serviced
  state displaces ordinary work and worsens quality. That rejected design must
  not be repeated.

Authoritative report:
`build/qualification/pdr-retention-capacity-operator-proof-pilot-v1/operator-consumer-shadow-r2/report.json`,
SHA-256
`219708AAB8194200482345243B1BDCAECFAAAAA38B2B7A18C182372A0741D3BF`.

## Work Plan

1. Trace direct compilation, exact evaluation, and strict-lift ownership. Add
   only the smallest bounded observational evidence needed to identify the
   concrete state classes and source route entering the one fail-closed edge.
2. Reproduce that attribution in one fresh PDR diagnostic. Name the exact
   missing/default route, its probability mass, its coarse-policy owner, and
   whether an already materialized row or existing continuation service can
   close it.
3. Stop diagnosis-only if the default is an intentional terminal-only strict
   distinction, if closing it requires a new planner or broad retry, or if no
   existing service can make the direct candidate proper before terminal lift.
4. Otherwise implement the smallest general closure through existing
   continuation/refinement ownership. It must be incremental or amortized,
   preserve ordinary interleaving, and remain request-agnostic.
5. Use focused native fixtures while implementing. Run a fresh PDR solve only
   when changed attribution or behavior requires it. Retain production behavior
   only if the direct candidate becomes proper and independently evaluated
   earlier without degrading final PDR quality or ordinary work.
6. At a retained substantial boundary, run proportional native acceptance,
   rebuild WASM before web acceptance if browser-visible solver behavior
   changed, and use 1,000 Simulator trials only for a materially changed
   compiled strategy that genuinely requires new verification.

## Promotion Gates

- The direct candidate must independently evaluate with success probability
  one, zero off-policy/unresolved mass, complete pricing, and reconciled cost.
- Its verified upper must be materially useful: no higher than `5000` Chaos
  for this PDR witness and available before terminal strict-lift completion.
- Final PDR quality must remain no worse than the current exact
  `3758.1244272552067` result under the same request and prices.
- Ordinary row work, joint-policy attempts, candidate continuation, and strict
  fallback must remain available; no exclusive retry loop may consume them.
- Existing caps, cancellation, proof, incumbent, and publication authority
  remain unchanged unless direct evidence requires a separately selected
  contract boundary.

## Stop Conditions

- Do not retain behavior from attribution telemetry alone.
- Do not weaken fail-closed certification, properness, exact pricing, cost
  reconciliation, or independent evaluation.
- Do not add RC/fragment state, case-specific action logic, learned guidance,
  MCP, GUI/catalog work, or historical strategies as seeds/incumbents.
- Stop and archive diagnosis-only if the smallest supported closure fails the
  usefulness or ordinary-work gates.

## Work Log

- 2026-08-30: boundary selected after the RC pilot proved its prerequisite
  absent. Bounded report parsing narrowed the 97.1% off-policy result to one
  certification fail-closed policy-router default edge. Source attribution and
  exact reached-state evidence are the first live tasks; no behavior change is
  presumed.
