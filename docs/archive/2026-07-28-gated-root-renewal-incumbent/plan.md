# Gated Root Renewal Incumbent

**Status: complete (2026-07-28), implementation retained.**

Owner: Oliver

Branch: `codex/gated-root-renewal-incumbent`

Starting source: `c8109d1` (`main`)

## Objective

Turn a completed goal-progress-gated root destructive-reforge row into an
immediate finite executable incumbent:

> apply the same destructive reforge until the goal is satisfied.

The incumbent is a fixed policy, not an optimality result. It supplies
`J_pi = U` while every unresolved admitted action remains in the lower-bound
problem. The solver continues ordinary discovery until its existing stopping
condition fires.

This milestone does not replace the unrestricted exact solver, weaken the
zero-progress-reroll restriction, raise a product cap, or admit bounded Pareto
state aggregation.

## Gate 0 - Corrected Boundary And Contract

The prior report correctly measured the first gated Chaos rows but
misidentified the immediate next work owner. Both frozen runs have
`expanded_states = 1`. The first Chaos row consumes 2,807,580 of 3,000,000
reforge-work units; the next root Fossil request consumes the remainder before
any partial state is expanded.

The incumbent qualifies only when:

1. the completed row belongs to the solve start state;
2. the selected operator is one priced primitive destructive reforge;
3. the gated kernel has positive terminal probability;
4. every positive-probability non-goal exit can legally repeat the same
   action and has the same complete engine-owned reforge-kernel signature;
5. the scalar immediate action cost is finite and nonnegative; and
6. the recurrence `J_pi = c + (1 - p) J_pi`, hence `J_pi = c / p`, is finite
   below the solver value ceiling.

Retry-basin and retained partial exits are not merged as states. The signature
proof establishes only that this one selected action has the same next kernel
from each exit.

Exit: pin the corrected root-action work evidence and the exact fixed-policy
proof boundary before changing solver behavior.

## Gate 1 - Small Exact Falsification

Extend the native destructive-renewal oracle to the gated kernel:

1. terminal, retry, and partial mass sum to the original row mass;
2. every non-goal exit repeats the same exact action-local kernel;
3. the native value equals `c / p`;
4. changing the preserved boundary rejects the witness;
5. an illegal retry action rejects the witness; and
6. unrestricted and non-gated solves retain their existing behavior.

Candidate synthesis must not request or enumerate another reforge row.
Record validation states and a deterministic witness hash.

Exit: the positive oracle compiles and evaluates exactly; the negative
controls refuse the candidate.

## Gate 2 - Early Bounded Incumbent

After a qualifying gated root row is committed, install the best deterministic
fixed-renewal incumbent seen so far before continuing root action expansion.

Requirements:

1. preserve the complete unresolved action envelope and ordinary expansion
   order;
2. never use the incumbent as a pruning certificate by itself;
3. retain `L <= J_pi <= U` through later graph appends and resource-cap
   finalization;
4. mark the result `bounded_feasible` within the
   zero-progress-reroll restriction, never exact or globally optimal;
5. keep only the actually policy-reachable fixed-renewal domain authoritative;
   unrelated interned diagnostic states need no fabricated policy; and
6. deterministically choose the lowest finite upper, breaking ties by stable
   planner-operator order.

Exit: a cap-stopped native solve returns the executable incumbent and finite
upper without changing its lower bound, cap, action scope, or reforge work.

## Gate 3 - Compact Exact Compilation

Compile the witnessed policy as one destructive-reforge loop:

```text
apply selected reforge
  -> goal satisfied: success
  -> otherwise: repeat selected reforge
```

The compact form is allowed only after the compiler independently rechecks the
same legality and complete-kernel-signature witness. It must not emit one
strict-state node per retained partial state, broaden a partial state to a
different action, or make the fixed policy a Bellman equivalence claim.

Verify the compact policy on the small oracle with exact strategy evaluation
and 10,000 seeded simulator runs. Refuse compilation if the witness is absent,
stale, illegal, or not expressible.

Exit: the toy compiled value matches the native incumbent and all 10,000 runs
succeed, with a negative compilation control.

## Gate 4 - Frozen Four-Mod Acceptance

Run only:

- `natural-t1-full-four-47d8b909aa88`;
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

Use the committed product action envelope, prices, artifact,
generator-config hash, one worker, watchdog policy, gated mode, and unchanged
caps.

Acceptance:

1. both runs retain their first Chaos row mass and deterministic kernel hash;
2. both stop at the same root-side reforge-work boundary unless later work
   genuinely improves it;
3. both return a finite executable fixed-renewal policy and upper bound;
4. the lower bound remains admissible and the result remains bounded rather
   than exact;
5. compact compilation stays within product node/edge/JSON caps; and
6. an unrestricted control retains its prior scope, result, work, and hashes.

Frozen wall time is machine/compiler-bound. Reforge work, states, rows,
transitions, mass, hashes, policy status, value bracket, and compiled size are
the portable evidence. The tiny terminal probabilities make 10,000 frozen
simulation runs computationally inappropriate; the required 10,000-run
compiled-strategy verification is owned by the exact small oracle in Gate 3.

## Gate 5 - Retention Or Rejection

If Gates 1 through 4 pass:

1. retain the gated-only early incumbent and compact compiler path;
2. correct the prior partial-state-bottleneck wording in stable and archived
   documentation without rewriting the measured first-row result;
3. record that competing root broad rows are the immediate next exact-search
   wall and partial-state expansion remains a later, unmeasured wall;
4. run the native solver acceptance and only the downstream checks affected by
   retained source; and
5. archive this plan, update evidence and `HANDOFF.md`, and commit locally with
   the required co-author line.

If any proof or compilation gate fails, restore diagnostic source and retain
only the corrected evidence and rejection report.

## Stop Rules

Stop and document rather than weaken the result if:

- any non-goal exit cannot legally repeat the action with the same exact
  kernel signature;
- the recurrence needs probability deletion, renormalization, or sampled
  evidence;
- publishing the incumbent removes or defers a competing action from the
  lower-bound problem;
- bounded finalization requires assigning actions to unreachable states;
- compact compilation accepts a carrier outside the proved fixed-policy
  domain; or
- unrestricted behavior changes.

## Out Of Scope

- Raising `max_reforge_work` or another product cap.
- Partial-state merging, bounded Pareto admission, or goal-count quotienting.
- Claiming the fixed repeat policy is optimal.
- Using the upper bound to prune without complete competing lowers.
- Mechanics, economy, ingest, data, UI, GPU, or ML changes.
