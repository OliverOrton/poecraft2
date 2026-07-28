# True First-Frontier Successor Census

**Status: completed measurement-only solver research.**

Owner: Oliver

Branch: `codex/true-successor-frontier-census`

Starting source: `b7740b8` (`main`)

## Decision To Make

Measure the complete exact Chaos successor support of the four frozen
natural-T1 hard cases that currently stop at 200,000 discovered states during
their first ordinary broad row.

The existing reports describe only cap-censored prefixes. This milestone must
answer:

1. how many distinct projected `AbstractState` successors the complete first
   Chaos row actually has;
2. how much exact reforge work and temporary memory completion requires;
3. which state features account for the support; and
4. whether the complete population exposes a credible exact reduction that
   was invisible at the 200,000-state prefix.

This is a census, not authorization to raise product caps or ship a compact
representation.

## Frozen Inputs

Use the four established hard representatives:

- `natural-t1-full-three-24920b3b28de`;
- `natural-t1-deep-three-low-probability-af4719c816f3`;
- `natural-t1-full-four-47d8b909aa88`; and
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

Pin the source commit, case hashes, artifact manifest, natural-T1
generator-config hash, economy hash, compiler, machine, and diagnostic
executable hash. Wall time is machine/compiler-bound; deterministic work is
the portable comparison.

The measured action is the exact primitive `chaos` distribution from each
case's unchanged empty rare start. Do not substitute a Harvest/Fossil action,
the entire admitted action envelope, or an approximate roll calculation.

## Non-Negotiable Semantics

- Count the exact collision-checked projected successors produced by the live
  reforge evaluator.
- Complete the action kernel. A resource-capped prefix is not a true count.
- Preserve the existing goal, state projection, junk-class, group-conflict,
  side-cap, and probability semantics.
- Verify total probability and deterministic rerun identity.
- Keep exploratory instrumentation outside public ABI and product behavior.
- Restore measurement-only source unless a small, generally useful regression
  is justified independently.

## Gates

### Gate 0 — boundary and identity

- Land the completed exact-quotient audit on local `main`.
- Create this branch and publish this plan plus matching `HANDOFF.md`.
- Record the four frozen cases and the exact Chaos action boundary.

Exit: the experiment is reproducible and cannot be confused with the
cap-censored historical runs.

### Gate 1 — measurement oracle

- Locate the live exact reforge completion point.
- Add the smallest isolated diagnostic needed to observe a completed Chaos
  distribution without expanding its successor states.
- Record at minimum:
  - distinct successor count;
  - probability sum;
  - reforge frontier work;
  - roll-bucket/frontier peaks;
  - goal-status histogram;
  - prefix/suffix count histogram;
  - blocker/junk occupancy summaries; and
  - collision/hash integrity.
- Prove on a small native oracle that the census count and probability match
  the ordinary exact `OutcomeDistribution`.

Exit: the diagnostic is exact, action-local, and cannot silently report a
partial kernel as complete.

### Gate 2 — complete four-case census

- Run cases serially under an outer watchdog.
- Increase only diagnostic allowances needed to complete the one action.
- Record deterministic work, wall time, peak memory, and all composition
  fields.
- Repeat each completed measurement once and require identical deterministic
  output and hashes.

Exit: all four cases complete, or the milestone reports the precise resource
boundary that prevents a true count without presenting an estimate as fact.

### Gate 3 — exact-reduction interpretation

Evaluate the complete supports against collision-checked diagnostic
projections only where they answer a concrete question:

- prefix-side core;
- suffix-side core;
- goal/status core;
- affix-count and blocker partitions; and
- any complete preserved-boundary or action-observation signature already
  defined by the solver.

These are ceilings and composition measurements, not Bellman equivalence.
Do not revive simple side convolution, goal/failure aggregation, cleanup
compression, or cap-stopped quotient claims rejected by prior milestones.

Exit: select one narrowly stated next hypothesis only if the complete census
materially supports it. Otherwise close with a negative result.

### Gate 4 — restoration, evidence, and handoff

- Restore exploratory engine changes unless independently justified.
- Retain tracked JSON evidence and durable documentation.
- Run focused native checks needed by any retained regression plus link,
  JSON, whitespace, and clean-tree checks.
- Do not run WASM, bindings, web, SQLite, or full-repository acceptance when
  no corresponding behavior changed.
- Archive this plan, update stable solver/evidence/roadmap docs, close
  `HANDOFF.md`, and commit locally with the required co-author line.

## Stop Rules

Stop and close the milestone without production integration if:

- a complete exact count cannot be obtained inside a deliberately generous
  diagnostic budget;
- the complete support only confirms that the current state explosion is
  intrinsic to the observation contract;
- a proposed reduction depends on erased information observable by any
  admitted continuation action; or
- the only apparent improvement is a larger cap, approximate probability, or
  restricted-action answer presented as the complete solver result.

## Out Of Scope

- Product cap changes.
- Mechanics, goals, or condition semantics.
- A new compact Bellman kernel.
- GPU work.
- ML guidance.
- Benchmark analytics unrelated to this census.
- WASM or web behavior.
