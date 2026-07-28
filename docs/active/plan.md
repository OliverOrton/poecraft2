# Root Broad-Row Falsification

**Status: active (2026-07-28).**

Owner: Oliver

Branch: `codex/root-broad-row-falsification`

Starting source: `0b72110` (`main`)

## Objective

Determine whether the broad root reforge that follows the completed gated
Chaos row can produce useful exact information without materializing its full
successor row.

The first candidate is an exact streaming fixed-policy bound:

> enumerate the action's exact roll frontier, accumulate only proved terminal
> goal mass, and use `cost / proved_success_mass` as a conservative executable
> upper for repeating that same action until the goal.

Unprocessed mass remains unresolved. It is never deleted, classified as
failure, or renormalized. A partial success sum is a lower bound on the true
per-attempt success probability, so its reciprocal is an upper bound on the
fixed policy's expected cost.

This milestone is falsification-first. It retains production behavior only if
the candidate improves an executable result within the unchanged product
budget and passes its proof obligations. Otherwise it retains only generally
useful cap-owner telemetry and the negative evidence.

## Gate 0 — Interrupted-Row Ownership

The existing action-cost telemetry is updated only after a row returns. When
`max_reforge_work` throws during `CalcContext::outcomes`, the consuming action
is absent from `action_analysis.search_cost`.

Add exception-safe diagnostic attribution that records:

1. carrier state and root/non-root ownership;
2. complete planner/action identity;
3. work consumed before interruption;
4. wall time and cache requests/hits;
5. the cap name; and
6. the stable operator cursor/order.

It must not swallow the exception, change ordering, or convert incomplete work
into a retained row.

Exit: the two frozen gated cases directly identify their cap-owning action and
work delta; unrestricted controls keep their prior deterministic result.

## Gate 1 — Exact Streaming Success Oracle

Refactor the existing reforge frontier rather than duplicate crafting
mechanics. Add an internal evaluation mode that:

1. uses the same pools, weights, target-count distribution, exclusions,
   guarantees, and frontier order as the materialized reforge evaluator;
2. accumulates raw mass only when the configured goal is irrevocably
   satisfied;
3. reports processed work, proved success mass, unresolved mass, completion,
   and deterministic hash;
4. interns no nonterminal outcome and does not populate a Bellman row;
5. remains resumable or stops at an explicit work boundary without
   renormalization; and
6. cannot be consumed as an admissible lower bound or pruning certificate.

The action-local repeat witness additionally requires a structural proof that
the same action stays legal and its complete engine-owned preserved-boundary
kernel is unchanged after every non-goal outcome. Unsupported action shapes
must refuse rather than infer the mechanic.

Validate on small exact oracles:

- completed streaming success mass equals the materialized gated row;
- an interrupted stream never exceeds the completed mass and produces a
  conservative `cost / p_lower` policy upper when `p_lower > 0`;
- zero proved mass produces no incumbent;
- a state-dependent legality or preserved boundary refuses the witness; and
- unrestricted ordinary outcome evaluation is unchanged.

## Gate 2 — Frozen Cap-Owner Census

Use the two frozen four-mod gated cases and unchanged caps:

- `natural-t1-full-four-47d8b909aa88`;
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

First run the streaming evaluator only in shadow measurement at the exact
point where the ordinary row would begin. Measure:

- observed cap-owner action, price, and remaining product work;
- proved terminal mass at the remaining-work boundary;
- conservative fixed-policy upper, if finite;
- exact completed mass and true work under a separate diagnostic ceiling;
- comparison with the retained Chaos incumbent;
- states and bytes avoided by not materializing failures; and
- whether any later solver discovery would actually become reachable.

Wall time is machine/compiler-bound. Work, mass bits, hashes, action identity,
state counts, and bounds are the portable evidence.

Exit: decide from real data whether streaming evaluation is useful under the
product budget. Do not qualify a candidate merely because it becomes finite
under the diagnostic ceiling.

## Gate 3 — Production Qualification

Retain a gated-only streaming incumbent path only if all of the following
hold:

1. it produces a strictly better executable upper on at least one frozen case
   before the existing work cap;
2. it does not discard the already materialized Chaos partial states or remove
   any competing action from the proof-bearing lower envelope;
3. its bounded result and compiled strategy remain explicitly exact only
   within the zero-progress-reroll restriction;
4. the compiler independently rechecks the structural repeat witness;
5. the other frozen case is no worse in deterministic work, policy
   availability, or memory; and
6. unrestricted mode remains behaviorally unchanged.

If the candidate fails, restore its production path. Exception-safe cap-owner
telemetry may remain because it corrects an observational blind spot without
changing solver decisions.

## Gate 4 — Deferral Ruling

Record, but do not implement, broad-row deferral unless an exact unresolved
action representation exists. A sound deferral must:

1. retain an admissible action-conditioned lower placeholder in every Bellman
   minimum;
2. keep the action eligible for later expansion;
3. prevent a bounded result from being called exact while any placeholder is
   unresolved; and
4. demonstrate additional executable-policy discovery before the same fixed
   cap on both frozen cases.

Changing only row order, silently omitting the action, or treating non-use as
a pruning certificate is insufficient.

## Gate 5 — Closure

Run the native solver acceptance and only downstream checks affected by
retained code. Rebuild release WASM only if retained source changes its
runtime. Pin the frozen evidence and machine/compiler qualification, update
stable solver documentation, archive this plan, update `HANDOFF.md`, and
commit locally with the required co-author line.

## Stop Rules

Stop and reject the production candidate if:

- streaming uses a different roll frontier from materialized evaluation;
- any unprocessed probability is classified or renormalized;
- repeat-policy properness depends on sampled evidence;
- the candidate cannot beat the current Chaos incumbent within the remaining
  192,420 work units;
- preserving the unresolved action lower requires a broad solver rewrite
  unsupported by measured benefit;
- any existing Chaos partial state is lost; or
- unrestricted hashes or behavior change.

## Out Of Scope

- Raising product work, state, transition, memory, or compiled-output caps.
- Partial-state merging, Pareto admission, or goal-count quotienting.
- Treating a fixed-policy upper as a Bellman lower or optimality proof.
- Mechanics, economy, ingest, corpus, UI, GPU, or ML changes.
