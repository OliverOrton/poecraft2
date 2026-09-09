## Review complete—and Codex was right to stop at this boundary

Current `main` is:

```text
19a41d88234be4495fbe86cc4cc502ed0b2f8e89
Enable checked empty-start retention lowers and qualify WASM
```

It remained unchanged at my final check. I read Codex’s `astra-review.md`, the experiment record, the rejected snapshot patch, and the relevant native producer and lower-consumer code. 

### :chatgpt-content-reference{index="25"}

:chatgpt-content-reference{index="26"} · :chatgpt-content-reference{index="27"}

**My decision: keep the retry-carrier safety check. Transfer lower-bound evidence through the producing action’s actual outcomes instead, and connect that result to a genuinely complete root/action consumer.**

There is also an important second finding: **improving the scratch calculation Codex identified would not, by itself, improve the returned root lower.** That calculation is currently diagnostic rather than part of the final bound.

## 1. First, the empty-start integration has genuinely advanced

The previous restriction—requiring the actual starting item to contain a fracture—is no longer the situation on `main`.

The implementation now separates the **certificate’s construction frame** from the **real starting item**. It can prepare the existing coupled model for the empty request without pretending the request starts fractured. 

The retained results are:

| Observation | Root lower | Verified upper |
|---|---:|---:|
| B0 baseline | 36.48853 | 470,485,191.44 |
| B2: same preparation, lower not consumed | 36.48853 | 16,997,812.20 |
| B6: retained implementation | **405.36940** | **16,997,812.20** |

WASM reports the same lower and a numerically matching independently evaluated upper. However, B2 and B6 emit the same strategy, and increasing the number of partial-state consumers did not change the interval. **The stronger empty-source lower is demonstrated; a policy improvement caused by lower consumption is not.** 

That distinction is useful rather than discouraging: we can now investigate the missing **partial → predecessor → root** connection instead of debating whether the bound is available at all.

---

## 2. Answer to Codex’s main question: the retry carrier is not a physical empty item

Codex’s concern is correct.

The reforge producer constructs `retry_base` **before** adding direct Essence/Fossil modifiers. Later, it can aggregate zero-progress results **before** Veiled Chaos inserts its placeholder. It then creates the retry carrier by projecting that earlier base and setting:

```cpp
goal_progress_retry_basin = 1;
```

Consequently, a retry carrier’s apparent empty slots do not necessarily describe the completed physical item.  

The important producer differences are:

| Producer | Information the retry representation can omit |
|---|---|
| Ordinary reforge | Rolled non-goal/below-tier affixes, their occupancy and exclusions |
| Harvest reforge | Unsuccessful guaranteed and ordinary picks |
| Essence/Fossil | Direct non-goal modifiers as well as subsequent rolls |
| Veiled Chaos | The subsequently inserted veiled placeholder and its side/state |

The zero-progress test means **zero satisfied goals at that point in the producer**, not merely “no improvement over the incoming item.” A direct modifier that already satisfies a requested goal prevents that branch from entering this zero-progress class. Fossil special flags are also explicitly propagated; they cannot be discarded when defining the proof domain.  

### There is a separate control restriction

The retry expansion path retains primitive renewal operators, excludes non-primitives and Eldritch Chaos, and returns before ordinary automatic generation. The documentation explicitly treats goal-progress gating as a restriction of the optimization problem.  

We therefore need to distinguish:

```text
A real item with ordinary permitted choices
A real item under the retry restriction
The compressed representation of that restricted continuation
```

A proof valid for one of these does not automatically transfer to the other two.

**Keep `project_native_retention_lower()` refusing the compressed retry marker.** That is not an assertion that no useful retry-related bound exists. It means this particular physical-state lookup does not establish one. 

## 3. How partial-item knowledge can cross this boundary safely

The simplest route does **not** require assigning a generic value to the retry carrier.

Instead, attach the proof to the **action that produced the outcomes**.

Suppose action \(a\) from actual source \(s\) has a complete partition of physical outcomes \(E_i\), with probabilities \(p_i\). Suppose \(b_i\) is a valid completion lower for **every item in \(E_i\)**, under compatible target and control semantics.

Then:

\[
B(s,a)=c(s,a)+\sum_i p_i b_i
\;\le\;
Q^*(s,a).
\]

The argument is direct: whatever proper continuation is chosen after the action, its expected remaining cost at each realized outcome is at least the corresponding lower. Taking expectations preserves the inequality.

**The outcome classes do not need identical native kernels. Their supplied lower must be uniformly valid.**

Alternatively, the producer can establish a lower on the expected continuation directly, using the existing native-containing probability envelopes and checked minimization.

### Why the result must remain action-specific

Consider a synthetic row with two exits whose completion costs are 2 and 20, each with probability one-half.

The expected continuation is 11. That is useful information for the producing row. But 11 is **not** a lower for every member of a shared retry class—the first item costs only 2.

So the correct object is:

```text
this source/action’s expected continuation lower
```

not:

```text
a universal scalar assigned to retry_state_id
```

This is the smallest connection that respects your original intuition: **use what we know about completing partial items in the calculation for reaching those items.**

### Start with ordinary Chaos, not every retry producer

For the unchanged empty primary, ordinary Chaos avoids the additional direct-modifier and veiled-placeholder complications. That makes it a suitable first architecture witness—not a permanent preference for Chaos.

The producer may be able to use a small set of conservative post-action potential coordinates and their minimum. It does not necessarily need to materialize every physical successor.

However, **all retry short-circuit paths must be covered**. The code can call `commit_retry` before final successor construction when later progress is impossible, as well as through factored terminal calculations. A callback only at the final successor would miss that mass. Unsupported portions must retain a valid fallback with their full probability, not be renormalized away. 

---

## 4. The second obstruction: the scratch calculation is not the returned root bound

I traced the exact distinction Codex asked about.

### The two paths use different lower sources

The cached/main-context path calls `completion_proof_lower_value()` on represented successors.

The scratch calculator instead uses its own `projected_lower`, combining older prepared tables. It does not query the newer retention contribution. Its state IDs also belong to a different calculator, so passing them directly to the main-context lookup would be incorrect.  

### More importantly, scratch `selected.lower` is not included in final `best`

The scratch loop updates its exact-action values and emits a trace such as:

```text
start_action=chaos,exact=37.425208735976348
```

But the final bound combines the common independent lower, other independent contributions, and the existing materialized-operator minimum. It **does not include the scratch-selected minimum**. 

The identified call site is in finalization, with a comment explicitly separating this independent maximum from scheduling, pruning, selected rows, and the incumbent. 

Therefore:

> Adding a stronger lookup to the scratch path could produce a more impressive diagnostic while leaving the published lower and search behavior unchanged.

This is **already documented in the September 4 ownership audit**. I confirmed it against current source; it should be reused as existing project knowledge, not recorded as another newly discovered architecture. 

### Do not fix that by simply adding `selected.lower` to `best`

The local “complete materialized envelope” test counts finite admitted rows. That is not, by itself, a proof that every caller-authorized action or residual family is covered.

The new result must enter an existing **canonical, action-complete lower query or action-lower consumer** with the required provenance. A list of successfully built rows is not enough.

---

## 5. There is also a mathematical trap in the old row helper

The envelope helper is not a safe universal template for the new action lower.

It contains self-loop elimination and a known observed-choice shortcut. Those have to be interpreted under the correct semantics, not copied because they already return a number. The older audit records the observed-choice counterexample explicitly.  

A simple example shows the first-action distinction:

```text
Action A:
    pay 1
    50% finish
    50% return to s

Action B:
    finish for 0.1
```

Repeating A until success costs 2.

Choosing A **once**, then using B after failure, costs:

\[
1+\tfrac12(0.1)=1.05.
\]

So the repeat-policy value of 2 is not a lower on the unrestricted first-action \(Q^*(s,A)\).

Self-elimination can be appropriate inside a correctly constructed simultaneous Bellman problem or a verified repeat-policy calculation. **It cannot be assumed to produce a valid independent action lower when switching remains permitted.**

The same warning applies to optional cleanup: a chosen cleanup-and-rebuild strategy is one feasible continuation, not a lower on all possible continuations.

The options literature makes the same structural separation between a temporally extended policy and the underlying primitive decisions. It supports keeping the commitment and termination semantics explicit—not introducing another option library. citeturn813992search0

---

## 6. Answer to “which unchanged action caps the improvement?”

**The supplied evidence does not establish a particular unchanged native action’s true value as the ceiling. I would not invent one.**

In particular, the scratch Chaos value of 37.425 is neither a native optimum nor a demonstrated ceiling on the new root lower.

But there is a decisive **local-model ceiling test**:

If any required action or residual family remains represented by

\[
x(s_0)\le405.3694021063399,
\]

then that unchanged placeholder caps this particular query at 405.3694.

For example:

```text
Refined Chaos floor:       600
Unchanged family floor:    405.3694

Complete query minimum:   405.3694
```

That is a ceiling of the proposed optimistic query, **not the native optimal cost**.

Codex should identify the actual owner of every limiting placeholder **before** building an expensive exit fold. This is the safeguard against another long sequence of improved local calculations that never changes the complete result.

Constraint-generation research supports focusing expensive calculations on relevant constraints. It does not justify deleting the inconvenient ones or importing a new solver here. citeturn215667search9

## 7. What I recommend Codex do next

This remains part of the original empty-start goal:

| Step | Required outcome |
|---|---|
| **Inspect the real consumer** | Identify complete action/family coverage and the unchanged constraints limiting useful gain |
| **Reuse a context-aware projection** | Query the existing potential from compatible physical/member evidence without mixing calculator IDs |
| **Add one producer-scoped exit proof** | Cover real Chaos outcomes before compression, including every short-circuit contribution |
| **Connect it to existing proof ownership** | Produce an action lower and a complete predecessor query—not just a scratch diagnostic |
| **Run a causal comparison** | Hold the existing direct root lower and preparation constant; toggle only the new partial-exit consumption |
| **Follow one decisive remaining constraint** | Continue only where the same mechanism can change a relevant complete minimum |

The causal comparison is important. Disabling all retention at once changes both the direct root floor and partial-state evidence. To demonstrate the requested connection, **keep the current 405.3694 root contribution present in both arms** and isolate what the new predecessor transport adds.

The first focused fixture should demonstrate an actual predecessor increase—not merely successful acceptance of an exit value.

If the complete model remains pinned by unrelated families, the result should name those families and stop expanding this particular mechanism. A valid new capability is worth recording; it does not automatically justify another broad abstraction project.

## 8. What about the failed snapshot repair?

I read the rejected patch. It already attempted to retain completed decisions owned by the same immutable candidate outside its coarse reachable walk. It did not simply borrow current greedy actions. Its retention-disabled primary then hit the 90-second watchdog.  

So “preserve the snapshot” is no longer a sufficient next instruction.

If the lower-consumer experiment has no useful headroom, the upper-side alternative should be:

> **Complete the specifically missing candidate-owned retry routing/selected-row dependency, rather than broadly retaining every available decision.**

It needs a concrete missing entry, compatible evidence, and measured finalization cost. Preserve the last valid executable artifact while attempting the repair. A newly constructed continuation requires a newly checked candidate, not a silent extension of an old certificate.

The existing fixed-renewal witness may help when it proves the same selected action is legal and has the same complete kernel from every relevant exit. Its archived contract expressly limits that conclusion to the chosen policy, not state equivalence or optimality. 

**No source argument can promise that this finishes within the existing watchdog.** That remains an implementation measurement.

## 9. What I verified—and what remains open

I ran **eight exact rational counterexample checks** covering hidden-state transfer, row averages versus member lowers, complete event aggregation, repeat-policy versus first-action values, self choices, unchanged family ceilings, omitted probability mass, and causal predecessor transfer.

They are included in the package. They are **synthetic mathematical checks**, not native PoE experiments or a machine-checked proof of the implementation.

I did not modify the repository, run its native solver/tests, or run Simulator. The external certificate research reinforces preserving an independent acceptance boundary, but it does not establish the missing native member/control relationship for us. citeturn813992view0

No new mechanic decision from you is needed to pursue the scoped experiment. Changing the product to allow unrestricted salvage after every zero-progress result would be a separate action-scope decision; this response does not assume that change.

## Message to send with the package

```text
Read astra_response.md and continue the existing goal using codex_reply.txt.

Keep the retained empty-start certificate and the retry-carrier refusal.
The next experiment is producer-scoped physical-exit lower transport into
an existing complete root/action consumer—not casting the retry carrier
to a physical empty item.

First inspect unchanged limiting action/family constraints. The scratch
Chaos refinement is currently diagnostic; do not promote selected.lower
without a complete native coverage and numerical contract.

Hold the existing direct root lower and preparation fixed in the causal
comparison. Demonstrate additional partial-to-predecessor-to-root or proof
effect, rather than another successful lookup.

Do not repeat the removed broad snapshot patch. Any upper-side follow-through
must target a specific missing compatible continuation and its measured cost.

Import the arguments and counterexamples into the existing mathematical
owners, reuse the September 4 ownership findings, and record actual dispositions.
The original empty-start goal remains unmet. Preserve scope, caps, current
repository work policy and unrelated work; do not push.
```

**The useful next step is not to make the retry carrier pretend to contain more information. It is to carry the right proof around that compression—and ensure the solver actually consumes it where the root result is determined.**
