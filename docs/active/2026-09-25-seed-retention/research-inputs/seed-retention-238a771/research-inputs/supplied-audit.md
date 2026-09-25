# User-supplied audit of the J0–J4 run

This is a transcription of the audit pasted in the user's request, with Markdown/line wrapping normalized. It is advisory input, not an independently produced native receipt. REVIEW.md records the verification and the exclusive-strata qualification; the original claims below have not been silently corrected.

---

I checked current `main` at **`238a771`**. The narrow **O experiment was implemented and qualified carefully**, but I would **not** consider the broader dirty-intermediate-state investigation fully completed or resolved.

The useful result is real: neutralizing `unrelated_occupancy` **inside the same satisfied-goal mask** changed the work order a lot—2,612 carrier positions moved, 1,412 moved earlier—but the matched A5 run returned the **exact same independently evaluated 85,558.706c controller**, and the strategy JSONs were byte-identical. So we learned something fairly strong:

> **The simple “cleaner carrier first within a goal mask” ordering is not what is preventing A5 from finding a better policy.**

That part was done well. The treatment is private/default-off, preserves the goal, actions, prices, caps, mask order, fracture/protection precedence and exact evaluation, and the baseline/treatment used the same executable, corpus and artifact. They also correctly did **not** claim the ~0.65 s verification difference or ~31 MB lower peak as a speed/memory win from one pair. The test caught a cross-mask mode leak before the economic run and they corrected it. Local build, 87,635 solver checks and 29 corpus-runner tests passed.

Where I think the programme fell short is **J1 branch selection**.

The plan said O should be selected when a dirty carrier is delayed by raw occupancy **and has a useful already-supported continuation**. PLAN But the resulting living record explicitly admits that the observed rank inversions did **not** establish a complete native route or economic saving. In other words, they established:

```
junk changes ordering
```

but not the stronger causal chain:

```
valuable dirty carrier
→ delayed because of junk
→ useful continuation already exists
→ delay prevents/badly postpones that continuation
```

They then selected O anyway and used the full matched run to see whether it mattered. That's still a valid ablation, but it doesn't satisfy the strongest version of the predeclared O gate.

More importantly, **S was not actually instrumented the way the plan requested**. The plan specifically called for comparing completed first-policy seed rows against the acquisition-credit counterfactual and logging whether the selected row changes. PLAN `solver_solve_constructive.cpp` is unchanged between `b4ea87d` and `238a771`; I cannot find a new S counterfactual logger. The living record says “no completed-row, gate-active selection reversal was logged,” but that is weaker than actually performing the requested S census.

And the suspicious code is **still there unchanged**:

```
if (first_policy)
    advances =
        joint_policy_terminal_debt(successor) <
        joint_policy_terminal_debt(state);
```

with:

```
debt = max(1, missing_goals + extra_affixes + rarity_mismatch);
```

So the particular thing that originally worried us—

```
4-goal problem:

empty Rare
D = 4

3/4 goals + 3 junk
D = 4
```

—**still exists in Current's first-policy construction** whenever:

```
high_impact_executable_uppers
&& incremental_action_generation
&& !output_incumbent
```

The O result does not test that mechanism.

There is another gap. The requested observation design wanted enough information to distinguish **actual obstruction from harmless/deferred junk**: selected operation, missing continuation burden, cleanup-evidence grade, complete preserving law/tail, etc. NATIVE_DESIGN The retained J1 samples mostly contain rank, goal mask, occupancy, capacity obstruction, blocked goals, admission time and rows-at-finish. That's useful, but not enough to answer:

> “Was this 3/4-goal dirty item actually a fantastic state because its junk was cheap to clean later?”

That economic distinction was the core of your hypothesis.

The numbers themselves reinforce this limitation. Of the 2,612 changed positions:

- **2,534 were classified side-obstructed**
- only **50 were high-progress dirty**
- **0 were full-coverage-with-extras**

So the experiment mostly tested whether ignoring occupancy changes ordering among already-obstructed states. It did **not** get much exposure to the particular “valuable goals held + harmless junk” population we care about.

The first-acquisition/post-acquisition cost diagnostic also wasn't obtained because the saved donor lacked safe decision-boundary provenance. I think that was handled correctly—the plan explicitly says not to fake such a boundary—but it means another major part of the hypothesis remains unmeasured.

Finally, **G wasn't tested at all**. Current main's own closeout says the finder still lacks the requested protected retention/dependency-only programme. So we still don't know whether the real problem is:

```
search undervalues dirty progress
```

versus

```
search understands the state,
but nobody has constructed the good
protect / finish / clean-later controller.
```

### My verdict

I would **keep `238a771`**. I don't see a reason to revert it. The diagnostic is isolated, default-off and the O negative result is valuable.

But I would interpret the result narrowly:

> **Ruled down:** raw `unrelated_occupancy` as a same-goal-mask ordering term is not responsible for A5's current 85.6k policy quality under this matched experiment.

I would **not** record:

> “dirty intermediate states aren't a problem.”

And I definitely would not record:

> “the dirty-state issue was fixed.”

The two most interesting mechanisms from our original discovery remain open:

1. **S / first-policy acquisition credit.** The exact debt formula that can make 3/4 goals + junk tie an empty item is still active and wasn't properly counterfactually measured.
2. **G / policy grammar.** The solver/finder may simply lack the preserve-progress → finish → cleanup-later controller that makes those dirty states economically valuable.

So the experiment gave us a useful negative result, but **I think Codex stopped one level too early if the objective was to answer the major breakthrough hypothesis**. The next Pro planning session should start from `238a771` and treat O as closed-negative, while revisiting **S and G with actual causal witnesses**, rather than rerunning another occupancy-ordering variation.
