# Handoff

**Status: Carrier-Ladder Early Executable Closure v1 is active.**

## Current Boundary

[Carrier-Ladder Early Executable Closure v1](docs/active/2026-08-30-carrier-ladder-early-executable-closure-v1/README.md)
is the selected ladder-side follow-up. Attribute and close, only if supported,
the route preventing the near-optimal PDR direct core policy from becoming a
proper independently evaluated executable candidate before terminal strict
lift.

The authoritative PDR report is
`build/qualification/pdr-retention-capacity-operator-proof-pilot-v1/operator-consumer-shadow-r2/report.json`,
SHA-256
`219708AAB8194200482345243B1BDCAECFAAAAA38B2B7A18C182372A0741D3BF`.

Direct candidate `3153018158135326262` estimates
`3770.7208497712586` Chaos. Its exact evaluator reaches 21,736 raw pairs and
8,440 refined pairs with complete pricing, but reports success
`0.0289578150891359` and off-policy/failure `0.9710421849108641`. There is no
stop, action-not-applied, no-matching-edge, or unresolved mass. Exactly one
`certification_fail_closed` policy-router default edge owns the failure.

Terminal strict refinement later produces independently evaluated candidate
`12798937337675811455` at exact cost `3758.1244272552067`. The live question is
therefore not broad policy quality: it is which concrete state classes enter
that one default, which coarse route/row owns them, and whether existing
continuation/refinement service can close it earlier.

## Immediate Work

1. Retain a bounded observational sample of exact state classes reaching the
   direct assertion's off-policy terminal and identify their source router.
2. Run one fresh PDR diagnostic only after that evidence is available.
3. Implement no behavior change unless the sample names a general closure
   through existing service ownership.

Do not repeat the rejected whole-policy retry after every serviced state. Do
not add RC/fragment state, a new planner, fixed scheduling windows, learned
guidance, MCP, GUI/catalog work, or request-specific action logic.

## Promotion Gate

A retained repair must make the direct candidate independently proper with
success probability one, zero off-policy/unresolved mass, complete pricing, a
reconciled verified upper no higher than `5000` Chaos, and preserve the final
exact PDR result `3758.1244272552067` plus ordinary row/joint-policy work.

## Repository State

- Starting implementation HEAD: `71abdf633f8d0685dbefd08c57f6c3cca733b9b7`.
- Work is local-only. Do not push.
- Protected untracked `0` is user state. It was not read, modified, staged,
  moved, cleaned, or deleted. Preserve it exactly.
