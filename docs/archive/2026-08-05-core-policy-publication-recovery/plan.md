# Core Policy Preservation And Direct Certification Plan

**Status: implementation complete; acceptance incomplete.**

Starting commit: `9b2bcfb82c694df0a77858c8c63fdae20d8b4f68`.

Branch: `codex/core-policy-publication-recovery`.

## Objective

Prove and repair the path where core solving selects a finite policy but later
strict refinement, compilation, or publication discards it. Preserve core
search and correctness standards; do not publish a candidate until its exact
compiled graph parses, is proper, is completely priced, and has zero off-policy
mass.

## Boundaries

- Change no crafting mechanic, price, objective, action filtering, goal,
  abstraction, strategy vocabulary, or configured cap.
- Reuse the existing incumbent, retained-artifact, compiler, exact assertion,
  quotient lift, proper-policy repair, and fallback portfolio.
- Keep all publication authority native; WASM transports it and TypeScript
  presents it.
- Run exactly 10,000 fixed-seed simulator executions for each qualification.
- Run the complete acceptance pipeline once, last, and do no rendered review.

## Gates

1. Freeze a historical-exact/current-degraded real five-goal request and the
   recovered current two-goal request; preserve historical/current frontend
   option differences and locate the loss after core selection.
2. Retain the complete core-selected policy, reachable closure, bounds,
   identities, hashes, certification status, and memory before strict work.
3. Compile and exact-assert the retained core candidate first. Retain a proper,
   completely priced, zero-off-policy artifact even when its exact cost differs
   from the coarse estimate.
4. Use deterministic direct failure witnesses to target the existing strict
   refinement and proper-policy machinery. Charge all work to unchanged caps.
5. Enforce publication invariants: a finite certified upper owns an executable
   witness; equal certified bounds own certified JSON; only executable plus
   globally closed results are exact; final compilation returns asserted bytes.
6. Rebuild native and release WASM, run focused native suites and frozen cases,
   exact-evaluate and simulate every qualification, run web tests/typecheck,
   then run `scripts/test.ps1` once as the final command.

## Required evidence

Record the core, direct-certification, strict-lift, and publication stages;
core bounds and hashes; exact costs and off-policy mass; retained artifact
identity; strict work and caps; native/WASM parity; and the fixed-seed 10,000
simulation outcomes. Preserve unavailable requested inputs as limitations
rather than substituting synthetic cases.
