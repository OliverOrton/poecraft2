## Yes—the statewise lower estimates are our admissible heuristics

More precisely, **a certified estimate of optimal remaining cost is an admissible heuristic**:

\[
h(s)\le V^*(s),
\]

where \(V^*(s)\) is the minimum **expected** cost of finishing the requested craft from state \(s\). The eligible goal-cover, clean-carrier, and retention contributions serve that role. The solver combines compatible contributions by taking their maximum. 

There are three related but different quantities:

| Quantity | Meaning |
|---|---|
| **Heuristic \(h(s)\)** | An optimistic estimate of remaining expected cost at a state |
| **Maintained lower \(L_t(s)\)** | The currently justified lower after combining heuristics and any additional complete proof work |
| **Verified upper \(U_t(s)\)** | The expected cost of an executable, properly evaluated policy from the relevant entry |

The displayed lower is the maintained bound **at the requested start**. It may equal one dominant heuristic contribution, but it is not necessarily just the original initialization value. This lower/upper distinction is standard in bounded heuristic search for stochastic shortest-path problems.  citeturn479224search0

Two qualifications matter here.

**Admissible does not mean strong.** Zero is admissible for a nonnegative-cost problem. A better heuristic helps only when the relevant proof/search path consumes it effectively. Also, it bounds *expected* cost: a lucky individual craft costing less than \(h(s)\) does not contradict admissibility.

**Not every number named `values` in the solver is an admissible heuristic.** Coarse estimates, fixed-policy values, ordering scores, and certified lowers have different owners and guarantees. The current documentation explicitly preserves that distinction. 

So yes: **we have been improving the admissible heuristic machinery. The next question is whether the exact-proof process gets enough useful work done with it.**

---

# New plan

### :chatgpt-content-reference{index="21"}

:chatgpt-content-reference{index="22"} · :chatgpt-content-reference{index="23"} · :chatgpt-content-reference{index="24"}

**Selected boundary: Pre-Finish Exact-Proof Progress and Heuristic Consumption v1**

It is self-contained for a fresh Codex session. It does not require the previously inaccessible package, and it preserves the current **no-subagents** policy.

## 1. What is now on `main`

I checked again at the end. Remote `main` is:

```text
317438392c88c0151f41e48b4b0323b06b1d0037
Honor bounded finish during strict publication
```

There are three commits beyond the previous reviewed checkpoint, covering continuation completion, preparation improvements, rejected strict-extension experiments, and bounded-finish handling.  

### The partial-item work now demonstrably helps the empty-start policy

The combined continuation change services missing selected-policy and publication dependencies through the existing ladder. In the recorded comparison, it reduced the empty-five verified upper from approximately **17 million to 85,558.71 Chaos**, with the lower unchanged at **405.3694**. That is actual partial-policy integration—not simply a successful lower lookup. 

The preparation work also improved substantially. Constructing the supported retention relations eagerly reduced the recorded empty-start setup from approximately **27.35 seconds to 8.28 seconds**, while preserving the same checked lower and improved policy. Rebuilt WASM then reached that policy within the existing user-path test. 

The latest recorded native endpoints are:

| Case | Certified lower | Verified policy upper |
|---|---:|---:|
| Empty-start five-goal reference | **405.3694** | **85,558.7062** |
| Empty-start two-sided four-goal bridge | **198.8335** | **5,218.0409** |
| Partial three-to-five control | **405.1116** | **794,067.4530** |
| Existing exact three-suffix anchor | **1,101.1565** | **1,101.1565** |

These remain separately scoped requests. The four- and five-goal cases are **not exact**, and the empty-five fixture is still the authorized repository reference rather than your unavailable exact browser export. 

**That is real recovery from the recent poor results, but not evidence of a new four/five-goal exact-closure breakthrough.**

## 2. The latest fix improves delivery, not proof strength

Earlier four-goal experiments continued strict work after the host had requested a finish, eventually timing out despite having a verified policy available.

The new implementation keeps the finish request latched and returns the cheapest compatible verified artifact at a safe strict-work suspension. It also avoids discarding a cheaper strict result that has already been verified but has not yet reached ordinary publication. 

The matched four-goal run now returns its existing policy in **62.203 seconds rather than 86.477 seconds**. However:

> **It returns before strict rows start.**

The missing continuation at the historical parent-4741 boundary is therefore still unresolved. Returning the old policy reliably is valuable, but it is not advancement of the exact proof. 

This changes the next task.

**We should not undo the finish fix to allow more proof work after the user asked to stop. We should establish a useful opportunity for proof work before that point.**

---

## 3. Why I selected this next chunk

The current code already has a cooperative `PolicyExactLiftWork` interface and a persistent strict quotient session. The session retains exact states, selected closure, partitions, proof obligations, dependencies, and verified evidence. We should reuse those owners rather than introduce another solver.  

The proposed change is:

```text
A suitable candidate becomes available
    → retain compatible candidate/proof ownership
    → advance bounded exact-proof work during the solve
    → preserve that work across cooperative slices
    → stop promptly when requested
    → return the best verified result
```

This is an **implementation hypothesis**, not a guarantee that scheduling alone solves the problem.

The first measurement is important:

> **When does a sufficiently useful, compatible candidate actually become available?**

If it only becomes available during finalization, moving an existing proof call earlier is not enough. Codex must identify whether an already-complete candidate could be verified earlier through the existing path—or whether candidate construction itself remains the limiting cost.

The plan explicitly prevents starting a huge proof against the worst early fallback and starving the ladder that finds the useful policy.

## 4. What the implementation will cover

### A. Make proof work possible before finish

Create the smallest integration of the existing cooperative strict work into an appropriate existing work phase.

Initially, retain **one active proof attempt**. Its source policy, selected rows, prices, observations, and scope must remain valid while ordinary work continues. A suspended C++ object holding references is not sufficient evidence that those references still describe the same mathematical problem.

The solver must continue making ordinary discovery progress. The plan does not reintroduce the failed “resume after every newly completed dependency” trigger, which produced excessive attempts and worse policies in the C9 experiment. 

### B. Resolve a real missing continuation—not just the array extent

The known four-goal boundary has a structurally matching coarse state outside the saved selected-policy table.

That does not establish that the old candidate owned a decision there. The next implementation must distinguish:

```text
Existing compatible selected/fallback evidence
Newer evidence belonging to a different candidate
A genuinely unfinished continuation
Unsupported correspondence
```

Use existing evidence when it actually covers the entry. Otherwise, construct a **new candidate continuation** through the native row and properness machinery. Do not pad the old policy vector with current greedy choices.

The D4/D5 attempts already explored new-row construction and failed their ordinary watchdog qualification. Their fixtures and negative evidence are useful; replaying their patches unchanged is not the new task. 

### C. Check that the admissible heuristic reaches the exact-proof consumer

This directly addresses your question.

Before inventing a stronger heuristic, inspect what the strict process actually receives at its selected states, frontiers, and alternatives:

```text
Which lower is available?
Is the stronger prepared contribution eligible?
Is it actually consumed?
Is there a compatible statewise upper for pruning?
Which constraint remains competitive?
```

If the existing prepared heuristic is valid but missing from that path, connect it through the existing typed provider.

If it is already consumed and remains weak, identify the actual limiting action or frontier. **Do not count more heuristic lookups as proof progress.**

The existing scope protections stay: a root upper is not an upper at every successor, and an ordinary physical-state lower cannot simply be attached to a compressed retry marker. 

### D. Continue until there is a checked root effect

A repaired mapping or an increased strict-state count is a checkpoint.

The task continues toward a stronger root interval, better verified policy, earlier achievement of a fixed proof target, or exact closure. Any new lower still needs the complete original action/family scope and the existing numerical/native acceptance conditions. 

---

## 5. Why the four-goal bridge is useful without abandoning 0→5

The four-goal case already reaches a concrete strict dependency and has a verified policy around **5,218 Chaos**. It is a smaller end-to-end proof target than empty-five, while still being empty-start and two-sided—not another isolated anchored microcase. 

The plan therefore makes it the first exact-proof implementation case, while keeping the **empty-five result as a required non-regression target**.

Success cannot come from making the four-goal test better while breaking the five-goal ladder.

The preferred result is a new exact closure within the existing envelope. If it remains bounded, the material goal is a predeclared improvement such as **20% smaller certified root gap on the four-goal bridge**, with no worse verified upper and unchanged scope/resources.

A smaller sound gain can be retained as partial progress. **Returning the same artifact faster cannot satisfy this new goal—the latest finish fix already achieved that.**

## 6. The strongest objection to this plan

**Starting strict work earlier could simply steal time from finding good policies.**

That is a real risk. The candidate may arrive too late, the proof may remain too expensive, or interleaving may make both processes worse.

The plan addresses it by requiring candidate-ready timing first, a single bounded persistent attempt, preserved ordinary discovery, and matched four/five-goal comparisons. It does not prescribe a large scheduler rewrite or a sweep of arbitrary time windows.

A longer diagnostic may be useful to understand one specific obstruction, but it cannot replace the original acceptance budget or be reported as a speedup.

---

## 7. What happens to the docs

The package includes an authored section explaining:

> **Admissible heuristics versus maintained root lowers versus policy/coarse values.**

Codex should incorporate it into the existing mathematical chapter, rather than creating another parallel document of assertions.

It should also update the actual strict-work lifecycle, heuristic consumer mapping, research evidence, and HANDOFF. The current HANDOFF’s references to an inaccessible older plan and “nothing pushed” are no longer useful instructions for this new session; the historical receipts should remain unchanged. 

No new documentation programme, model-checking framework, heuristic portfolio, or experiment database is part of this chunk.

## Send this to the fresh Codex session

Attach the package and paste the full `start_prompt.txt`. This shorter version captures the governing direction:

```text
/goal Make exact-proof progress before requested finish on the existing
empty-start two-sided four-goal bridge, while preserving the current
five-goal result and reliable bounded publication.

You are a fresh Codex session in OliverOrton/poecraft2.

Reviewed main:
317438392c88c0151f41e48b4b0323b06b1d0037

Use the attached implementation_plan.md and heuristic_and_review.md.
This is self-contained; do not wait for an older inaccessible ZIP or thread.
Reconcile actual local state and preserve newer/unrelated work.

The C3 continuation integration, eager retention preparation, and bounded-finish
repair are DONE. Keep them. Current five-goal result is
405.3694021063399–85558.70618560436. Four-goal is
198.8334996747695–5218.040949685988. Neither is exact.

The new task is useful pre-finish proof work, not another early return of
the same artifact. First establish when a compatible candidate is ready
and whether existing cooperative owners can serve it earlier.

Reuse PolicyExactLiftWork and PersistentQuotientSession. Initially retain
one bounded candidate-owned attempt. Preserve valid row/choice/price/scope
ownership, memory accounting, ordinary discovery, and prompt finish.

Resolve the exact out-of-snapshot dependency using owned evidence or explicit
new-candidate construction. Do not pad the old policy, borrow current greedy
actions, restore D4/D5 unchanged, or rebuild the complete graph.

Our certified statewise lowers are admissible expected-cost heuristics.
Audit their actual use in strict frontiers and alternative screening.
Connect an already-valid missing contribution before inventing another
heuristic family. Preserve complete action coverage and retry-domain refusal.

Continue through a checked root-level comparison, not merely more strict states.
Preferred completion is new exact closure; otherwise meet the plan's material
root-gap/target-time criterion without five-goal regression. Preserve the
original target semantics, prices, finish/watchdog and memory settings.

Honor bounded finish throughout proof. Keep the best compatible verified
artifact, including any cheaper verified strict result awaiting publication.
An unverified estimate cannot delay finish or become an upper.

Use focused fixtures, decisive native comparisons and affected WASM checks.
No routine Simulator, full census or broad intermediate suites.

Integrate the mathematical explanation and actual findings into the existing
backbone. One sequential session, no subagents, no inherited deadline or
automatic restart. Protect path 0 without inspecting it. Do not push.

Write one concise living record and continue into implementation.
```

Goal mode is appropriate for this sequence of implementation and evidence-driven follow-through, but it remains a scoped, budget-limited objective rather than a guarantee of unlimited unattended execution. citeturn394075view0

**Bottom line:** we already have admissible heuristics, and partial continuations are now helping the empty-start policy. The next step is to make the exact-proof machinery use that work while the solve is still running—not reserve all the difficult proof work for after the user requests a result.
