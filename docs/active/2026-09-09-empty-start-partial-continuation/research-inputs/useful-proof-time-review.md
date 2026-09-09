**Yes—the solver’s statewise certified completion lower is its admissible heuristic.** And I agree that representative development solves should now get **four to five minutes**, rather than forcing every investigation into the old one-minute window.

The important qualification is that the extra time must reach useful search and proof work. The newest change on `main` makes that distinction particularly important.

## New plan

### :chatgpt-content-reference{index="16"}

:chatgpt-content-reference{index="17"} · :chatgpt-content-reference{index="18"} · :chatgpt-content-reference{index="19"}

**Selected task: Useful Proof Time and Demand-Driven Exact Continuation v1**

This is a substantial implementation chunk for **one fresh Codex session, without subagents**. It preserves the working short-run behavior, introduces separately identified longer experiments, and follows the remaining exact-continuation obstruction through a real proof window.

---

## 1. Is our lower bound the admissible heuristic?

**At the statewise level, yes.**

For a state \(s\), an admissible heuristic satisfies:

\[
h(s)\le V^*(s),
\]

where \(V^*(s)\) is the minimum expected remaining cost of completing the requested craft under the declared action and policy scope. In ordinary search terminology, admissibility means never overestimating the optimal cost to the goal; here the cost-to-go is stochastic and measured in crafting cost. citeturn520328search12 

There are several related objects in your implementation:

| Object | Meaning |
|---|---|
| **Statewise completion lower \(h(s)\)** | The admissible heuristic for that compatible state |
| **Displayed root lower \(L(s_0)\)** | The currently certified lower at the requested start, potentially combining heuristic values with additional proof work |
| **Verified policy upper \(U(s_0)\)** | The cost of an actual proper executable strategy |
| **Scheduling score or coarse estimate** | Guidance or provisional information—not automatically an admissible value |

The native-retention table is **one heuristic component**, not the entire lower system. The proof manager combines compatible independent components by **maximum**, while unsupported domains retain valid fallback values. 

So the answer is not “we have a lower bound instead of a heuristic.” **We have an admissible lower-bound heuristic, plus machinery that can strengthen and use it.**

### Why a stronger heuristic does not automatically produce a better policy

A stronger \(h(s)\) can eliminate alternatives, tighten the root guarantee, or improve decisions about where to spend proof work. But it has to reach the relevant consumer.

For example, suppose the complete lower calculation is:

```text
Action A lower: 400
Action B lower: 40
```

Improving A to 800 leaves the minimum at 40. Likewise, a stronger state lower does not help a continuation that the compiler cannot represent or that the proof system never gets time to examine.

Your documentation now correctly separates **admissibility**, **consistency for a particular Bellman model**, and **search performance**. A valid native lower is not automatically a feasible starting vector for every truncated auxiliary model, and an ordering score does not acquire proof authority merely by being useful. 

**We should continue improving the heuristic—but not assume it is the only remaining bottleneck.**

---

## 2. What is now on `main`

I rechecked at the end. Remote `main` is:

```text
317438392c88c0151f41e48b4b0323b06b1d0037
Honor bounded finish during strict publication
```

The latest commit repairs how a finish request is handled while optional strict proof is running. 

The retained results are:

| Case | Certified lower | Verified upper | Exact? |
|---|---:|---:|---|
| Empty five-goal Conquest | **405.3694** | **85,558.7062** | No |
| Empty two-sided four-goal Conquest | **198.8335** | **5,218.0409** | No |
| Existing three-suffix anchor | **1,101.1565** | **1,101.1565** | Yes, existing regression |

These are committed measurements, not native runs I repeated here. 

### What improved

The earlier continuation work successfully recovered useful partial policies and assembled the approximately **85,559-Chaos** empty-start strategy. Eager construction of the joint lower relations then reduced empty-five setup from approximately **27.35 to 8.28 seconds**, preserving that result. 

The latest finish repair reduced the four-goal observation from **86.477 to 62.203 seconds**, returning the **same verified strategy** instead of continuing optional proof after the caller had asked it to finish. That is better bounded delivery—not a new bound or exact closure. 

### What remains unresolved

The four-goal strict path encountered an exact state whose coarse parent was present structurally but outside the saved selected-policy table. Attempts to construct a new continuation passed a focused fixture but failed the old runtime qualification and were removed.

The new finish fix does not resolve that state. In the latest four-goal observation, it returns the existing artifact **before strict rows start**. 

That makes a longer, properly allocated proof window a sensible next experiment.

---

## 3. Yes: use longer solves, but keep unit tests and short regressions short

I would use this new development profile:

| Control | New setting |
|---|---:|
| Requested bounded finish | **240 seconds** |
| Native watchdog | **300 seconds** |
| Outer process-cleanup safeguard | **315 seconds** |
| Memory | Existing **1 GiB** |
| Other state, row, transition and work caps | Unchanged initially |

This means **four minutes of allowed work**, with up to another minute for bounded completion and evaluation. The extra outer allowance is for terminating and cleaning up a stuck process—not hidden solving time.

The original 60/90-second cases remain unchanged as regression evidence. The longer cases get new experiment identities through the existing runner conventions.

### Why both timing controls matter

Increasing only the watchdog would mean:

```text
ask it to finish at 60 seconds
→ permit it to overrun for several more minutes
```

That is not the experiment you want.

We need:

```text
allow useful work until 240 seconds
→ request bounded completion
→ enforce the 300-second native watchdog
```

The existing benchmark system already distinguishes finish requests, resource stops, watchdog failures, and usable partial trajectories. The plan reuses that system. 

### How to keep the longer runs efficient

**One long trajectory should answer several timing questions.** Record the available observations around 60, 120, 180 and 240 seconds rather than running separate solves at all four durations.

But only record what was actually known then. A policy independently evaluated at the end cannot be backdated as a verified upper at 60 seconds.

The plan starts with two long controls—four-goal and five-goal—then uses longer treatment runs only after focused fixtures establish the relevant change. It limits the initial experimental block rather than authorizing a large cross-product of settings.

And the comparison must stay honest:

> A new implementation at 240 seconds is compared with the baseline at 240 seconds. A 240-second result beating a 60-second result establishes the value of more compute, not an algorithmic speedup.

That follows the current experiment-comparability contract. 

---

## 4. The crucial issue: more discovery time is not necessarily more proof time

The latest stop behavior is correct: when the user requests a result, the solver can stop optional strict work and return its cheapest compatible verified artifact.

But this creates a question we must measure:

> **Does strict proof get meaningful time before that request, or does it only begin when the request has already told it to stop?**

The current code checks `requested_bounded_finish` inside the strict loop. The main work loop also uses that request to enter publication/finalization. Therefore, moving the request from 60 to 240 seconds could simply move the same “return immediately without strict work” behavior to 240 seconds.  

There are additional direct-publication gates when discovery remains open or a candidate’s coarse estimate does not support the desired proof. So this is not necessarily solved by one timer change. 

### The plan’s approach

First run current `main` under the longer profile and observe where the time goes.

**Only if needed**, add a small diagnostic distinction between:

```text
Stop expanding this candidate and start checking/proving it
```

and:

```text
The user wants the result now
```

A first controlled allocation could be:

```text
Around 60 seconds:
    hand a compatible candidate to the existing proof pipeline

Until 240 seconds:
    continue useful strict/alternative work

At 240 seconds:
    request final bounded delivery
```

This would reuse the existing publication and strict owners. **It is not a new general scheduler.** It must not clear a real finish request, pretend the action envelope is closed, or discard the last valid artifact.

The allocation is a proposed experiment, not a new permanent 60-second constant or public default.

---

## 5. Then resolve the missing continuation under conditions where it can qualify

The next implementation must keep three things separate:

1. The exact state maps to a known coarse parent.
2. The old candidate actually owns a selected decision at that parent.
3. A valid new continuation is constructed now.

The current adapter checks the selected table’s extent before some bootstrap handling. That guard is appropriate for interpreting an old fixed policy; it is not a proof that a new continuation can never be constructed. 

The plan allows the narrow new-continuation hypothesis to be reconsidered because **the premises have changed**: finish delivery is now repaired, and you have authorized a longer proof window.

It does not restore the rejected patches indiscriminately.

The implementation should request the specifically missing row through the existing exact/quotient bootstrap, preserve every successor and observation, and qualify a **new candidate** before replacing the old verified artifact. No greedy padding, broad snapshot copying, or assigning a root upper to arbitrary entries.

Once that selected continuation works, the session follows the actual surviving alternative-proof obstruction. That is where the admissible heuristic should be used to retire or tighten relevant obligations.

**The goal is a complete four-goal proof as a bridge toward five-goal closure—not merely a successful state lookup.**

---

## 6. What the new chunk must deliver

The plan continues through:

```text
Current long-window controls
→ actual phase/time attribution
→ useful pre-finish proof allocation, if missing
→ targeted new-continuation repair
→ one measured remaining proof obstruction
→ matched four/five-goal results
→ focused short-run and WASM regression qualification
```

The best outcome is new four- or five-goal exact closure.

Otherwise, the session must predeclare a material matched improvement—such as a 20% smaller root gap with no worse verified upper, or less time to the same verified target. A correct but smaller result can be retained, but must not be called full completion.

The package also supplies written mathematical and benchmark-policy updates for the existing docs. It does not ask Codex to create another documentation system.

I reviewed the source, current evidence, and relevant heuristic/proof references. **I did not run the native solver or repository tests, and I am not predicting that four minutes will be enough for closure.** The longer controls are intended to distinguish inadequate time allocation from a genuine mathematical, state-coverage, or work-cap barrier.

## Send this with the package

Use the full `start_prompt.txt` for the fresh session. This entry message captures the main changes:

```text
Implement the attached Useful Proof Time and Demand-Driven Exact
Continuation v1 plan in OliverOrton/poecraft2.

Reviewed main:
317438392c88c0151f41e48b4b0323b06b1d0037

You are a fresh session. This is authorization to proceed; do not wait on
old threads, missing earlier ZIPs, or historical review pauses.
Preserve newer local work. No subagents, inherited deadline or push.

I explicitly authorize efficient longer native development solves:
240-second requested finish, 300-second native watchdog, and a 315-second
outer cleanup safeguard. Preserve the old short fixtures and all other
target/resource controls. Create separately identified long comparisons.

First measure what the extra time actually buys on the existing empty-four
and empty-five cases. Do not merely increase the watchdog while retaining
a 60-second finish request. Use existing trajectories instead of separate
runs at every timestamp.

Keep the completed C3 continuation service, eager retention preparation,
empty-source certificate, numerical reuse and bounded-finish repair.

If strict proof only starts after the terminal finish request, establish a
real pre-finish proof window through the existing owners. Prefer an existing
checkpoint; otherwise implement the smallest optional diagnostic handoff.
Never clear a real finish request or claim that paused discovery is closed.

Resolve the actual out-of-snapshot continuation through compatible owned
evidence or newly certified exact rows. Keep fixed-policy ownership separate
from a new candidate. No greedy padding, broad snapshot capture, historical
strategy seed, or retry-to-empty cast.

Preserve the last verified artifact during optional proof, including failures
and caps. Continue through the actual remaining alternative-proof obstruction
and a matched end-to-end result—not merely a passing mapping fixture.

The statewise completion lower is the admissible heuristic. Clarify its
relationship to the displayed root lower, scheduling scores and policy upper
in the existing mathematical docs. Do not assume a stronger heuristic alone
guarantees faster search or exact closure.

Use focused tests for iteration and longer runs only for decisive comparisons.
No routine Simulator, full alternative census or broad intermediate suites.
Qualify the affected short native/WASM paths after retained changes.

Aim for new exact closure. Otherwise use the plan's predeclared same-budget
material improvement criterion. Report unmet goals honestly, preserve the
research findings, and do not continue a no-progress loop.
```

**The extra time is reasonable. The important change is to make it available to the work that can close the proof—not simply let the same one-minute execution run late.**
