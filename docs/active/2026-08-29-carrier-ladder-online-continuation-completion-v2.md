# Carrier-Ladder Online Continuation Completion v2

**Status: active.** Selected on 2026-08-29 from the diagnosis committed after
`60e33bd8c681ee8a0f06da3c46e452c8482a5ab7`.

Parent: [Active work](README.md)

## Objective

Turn one concrete missing continuation discovered by current joint-policy
assembly into ordinary exact row service early enough for a later joint retry,
using the existing solve-local graph, mechanics, action envelope, scheduler,
compiler, evaluator, incumbent, and publication authorities.

The ladder remains the planner. V2 does not store reusable fragments, items,
bases, or craft-specific advice. It does not add a second planner or global
comparison path.

## Starting Evidence

Frozen-graph attribution proves:

- terminal assembly sees every completed alternative;
- checkpoint two first misses state 1780 / goal mask 8;
- terminal assembly first misses state 4489 / goal mask 0;
- both are unexpanded non-carrier states with no owner row;
- two missing-frontier obligations remain open, with zero priority offers and
  zero service completions; and
- the historical Eldritch-heavy policy remains current-valid.

The existing feedback path already records those state IDs and
`schedule_incremental_refinement()` can prioritize their ordinary exact
expansion. The carrier-local second stage only counts a priority offer when
the named state enters the carrier set, which neither witness did. V2 must
first distinguish late discovery from failed consumption; it may repair only
the demonstrated handoff.

## Execution

### 1. Freeze the online obligation lifecycle

Retain one benchmark-private bounded lifecycle record from joint-attempt
failure through feedback enqueue, refinement offer, ordinary expansion, row
completion, joint retry, and closure/refusal. Bind graph generation, state,
goal mask, row identities, action envelope, budget, and stop owner. Recording
must remain observational.

### 2. Prove the first failed handoff

Using the cumulative-10 witness, determine whether state 1780 is discovered
only after the host's requested bounded finish is latched, whether the existing
post-upper scheduler sees it, and whether it is expanded or still queued at
terminal publication. Do not infer service from aggregate counts.

### 3. Narrow online service

If the obligation is available while ordinary work may continue, feed it into
the existing exact refinement/service lifecycle ahead of unrelated broad
fringe work. Reuse current scheduler and action-envelope ownership. Do not
continue beyond a latched requested finish, increase caps, materialize a
representative item, or run a private mechanics copy.

If the obligation is first discovered only after bounded finish, adjust the
existing joint-attempt cadence only when a source-backed earlier checkpoint
can expose the same obligation within the unchanged work budget. Do not add a
terminal extra-work pass.

### 4. Qualification and promotion

Run one immutable diagnostic case to show the complete lifecycle. A retained
behavior repair must prove at unchanged solve limits:

- at least one named continuation is offered and receives a completed valid
  ordinary row;
- a later joint attempt consumes the new row or records the next exact cascade
  member;
- all candidates still pass existing properness, compilation, independent
  exact evaluation, incumbent comparison, and publication authority; and
- ordinary fallback, cancellation, resource accounting, and requested-finish
  truthfulness remain intact.

Exact policy quality decides usefulness. Run Simulator only if a materially
changed compiled strategy is retained; otherwise it is not applicable. Use
focused native verification and one matched CLI comparison before considering
WASM or the full pipeline.

## Stop Conditions

Close diagnosis-only if the obligation is available only after the immutable
bounded finish, if existing exact service cannot act without inventing an
entry/exit or representative state, or if the local work merely displaces
more valuable ordinary service without closing a continuation.

Stop before any change to mechanics, probability, state identity,
lower-bound/proof authority, public ABI, product behavior, permanent
fragment/item/base storage, global planning, action filtering, or caps.

## Living Log

### 2026-08-29 — Activation

- Frozen-row outcome B is established by revision
  `case-rev-33ceac3cbb5a476ad183a28f47966566`, job
  `job-cbf1b9bc-e10b-4782-8b1b-34feb639c29d`, and attempt
  `attempt-4113e966-7616-4feb-9c61-63c75b17ceed`.
- Terminal row visibility is complete; missing-frontier counters are
  discovered/open `2/2`, priority offers `0`, service completions `0`.
- Existing source priority-expands named missing states, then offers
  carrier-local automatic service only if the state qualifies as a carrier.
- Next: add bounded lifecycle timestamps/generation facts around checkpoint
  failure and post-upper scheduling, then rerun only the pinned case.
