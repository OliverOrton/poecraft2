# Goal-Progress-Gated Reforge Mode

**Status: complete (2026-07-27).**

Owner: Oliver

Branch: `codex/goal-progress-gated-reforge`

Starting source: `f843a9d` (`main`)

Final result: Gates 0 through 2 and the focused controls passed. Gate 3 put
both first Chaos rows below 200,000 states, then measured retained exact
partial states as the next bottleneck at the unchanged reforge-work cap.
Gate 4 retained the opt-in implementation, stable contract, evidence, and
future bounded-Pareto design.

## Objective

Add an opt-in exact solver mode that preserves every partial-progress state
but prevents terminal and zero-progress reforge compositions from consuming
the state graph. The existing unrestricted exact mode remains the default and
must retain identical behavior and hashes.

The new result is exact only within this executable restriction:

> After a reforge produces zero satisfied goal modifiers, discard its
> unpreserved affixes and choose the next action only from legal primitive
> destructive reforges whose next exact kernel ignores those discarded
> affixes.

It is not globally optimal over Annul, Exalt, Bench, removal, protection, or
other salvage routes from those discarded zero-progress items.

## Gate 0 - Representation And Mass Contract

1. Add an opt-in solver flag. Do not change the default.
2. Give retry basins an internal state identity distinct from an ordinary
   physical carrier with the same preserved boundary.
3. A basin materializes to its exact preserved boundary, but its Bellman
   action envelope contains only legal primitive destructive reforges proved
   independent of the discarded affixes. Exclude Eldritch Chaos because its
   preserved side can observe discarded affixes.
4. For each gated primitive reforge row, before interning a complete outcome:
   - route goal outcomes to one terminal state;
   - route zero-satisfied-goal outcomes to the basin for that preserved
     boundary; and
   - intern every partial-progress outcome as its complete exact abstract
     state.
5. Preserve raw probability mass. Gated distributions must prove their
   committed mass is one within the existing floating-point tolerance and
   must not renormalize terminal, retry, or partial mass.
6. Keep fixed-option kernels on their existing exact semantics. They already
   describe explicit restricted programs and do not expose their internal
   retry carriers as ordinary salvage states.

## Gate 1 - Exact Enumeration Short-Circuits

Add only monotone proofs inside the reforge frontier:

1. Once the base plus current roll satisfies the configured goal threshold,
   route all target-count mass remaining on that branch to terminal mass.
   Additional rolls cannot remove a modifier within the same reforge.
2. When the branch has zero satisfied goal modifiers and every currently
   eligible bucket capable of producing a satisfying modifier has zero
   weight, route all remaining target-count mass to retry. Eligibility can
   only decrease as the roll consumes side capacity and exclusion groups.
3. Never apply the retry short-circuit to a partial-progress branch.
4. Record terminal, retry, and partial mass plus short-circuit counts for
   deterministic evidence.

Exit: small exact controls match an unrestricted distribution after grouping
its goal and zero-progress states, probability sums to one, repeated runs
produce identical grouped kernels, and unrestricted controls remain
bit-identical.

Outcome: passed.

## Gate 2 - Restricted Bellman And Compilation

1. Expansion of a retry-basin state skips state-local automatic admission and
   retains only the legal primitive destructive-reforge envelope.
2. All ordinary partial states retain the complete solver action envelope,
   including protection, removal, addition, Bench, finishing, switching
   reforges, and staged prefix/suffix routes.
3. Transition-cache compatibility includes the mode.
4. Bellman lookups, row release, quotient identity, hashing, and deterministic
   telemetry distinguish gated from unrestricted kernels.
5. Compiled gated policies route post-reforge goal outcomes to success,
   zero-progress outcomes to the selected basin reforge, and partial outcomes
   back through the exact policy router. If compilation cannot represent an
   accepted closed policy exactly, fail rather than emit a broader policy.
6. Native/WASM telemetry and compiled-policy metadata label the result
   `exact_within_zero_progress_reroll_restriction`.

Exit: a native toy solve demonstrates that a basin cannot select a cheaper
salvage action, while an ordinary retained partial state can; its compiled
strategy agrees with the restricted native policy under 10,000 seeded runs
when compilation is reached.

Outcome: passed.

## Gate 3 - Frozen Four-Mod Acceptance

Run only:

- `natural-t1-full-four-47d8b909aa88`;
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

Use the committed product action envelope, prices, caps, one worker, and
watchdog policy. Enable only the new mode.

Acceptance:

1. the first Chaos row remains below 200,000 discovered states;
2. probability conservation and deterministic transition/policy hashes pass;
3. mechanics, action prices, and product caps are unchanged;
4. an executable restricted policy is produced, or the next measured
   deterministic bottleneck is documented; and
5. an unrestricted control retains its prior behavior and hashes.

Wall times are machine/compiler-bound. Discovered states, rows, transitions,
reforge work, grouped mass, and hashes are the portable comparison.

Outcome: first-row acceptance passed for both frozen cases. No frozen policy
was produced; exact follow-up rows from the retained partial states reached
`max_reforge_work` before Bellman optimization.

## Gate 4 - Retention And Handoff

1. Update the stable solver contract, decisions, evidence, and corpus index.
2. If partial-progress states remain the bottleneck, separately describe a
   future bounded Pareto admission design. Do not merge partial states by goal
   count in this milestone.
3. Rebuild native and release WASM because the opt-in solver flag crosses the
   existing solve-options surface.
4. Run the native solver acceptance, required 10,000-run compiled-strategy
   verification if a policy compiles, then the non-visual WASM/web checks
   affected by the new option.
5. Archive this plan, clear `HANDOFF.md`, and commit locally with the required
   co-author line. Do not push.

Outcome: passed.

## Stop Rules

Stop and document rather than weaken the restriction if:

- any routed zero-progress mass can retain an affix observed by an admitted
  basin action;
- mass conservation requires dropping or rescaling a semantic outcome;
- terminal or retry routing depends on sampled evidence;
- a partial-progress outcome would need count-only merging to meet the cap;
- compilation cannot express the restricted policy exactly; or
- unrestricted mode changes.

## Out Of Scope

- Replacing or changing default unrestricted exact solving.
- Treating gated results as globally optimal.
- Partial-progress quotienting by satisfied-goal count.
- Bounded Pareto admission implementation.
- Raising product caps.
- Mechanic changes, GPU work, or ML guidance.
