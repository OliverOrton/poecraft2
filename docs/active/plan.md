# Chaos-Anchored Incremental Action Generation

**Status: active (2026-07-28).**

Owner: Oliver

Branch: `codex/chaos-anchored-incremental-actions`

Starting source: `f37b4ef` (`main`)

## Objective

Replace atomic per-state all-operator expansion with an exact incremental
action-envelope schedule:

1. evaluate Chaos and ordinary inexpensive actions first;
2. release, intern, queue, and expand Chaos successors immediately;
3. optimize the currently admitted restricted graph;
4. evaluate filtered Fossil, corrected Harvest reforge, and goal-relevant
   Essence alternatives against usable state values;
5. admit improving exact rows and reoptimize; and
6. repeat until every filtered alternative is evaluated, safely ruled out, or
   honestly unresolved at a resource boundary.

This is not another stored structural DAG. Compatible action outcomes should
resolve to Chaos-created state IDs through the ordinary state interner and
exact reforge-kernel reuse. Added/forced/guaranteed support deltas remain exact
and may introduce additional states that must be expanded before an action can
be classified.

Veiled and Eldritch work remain outside this boundary. Mechanics, action
filtering, prices, and current product caps are unchanged for the initial
scheduling test.

## Status And Exactness Contract

Each reached state/action pair has an explicit lifecycle:

- `unevaluated`;
- `evaluating` or resumable;
- `completed_admitted`;
- `completed_non_improving`; or
- `unresolved_resource_limit`.

A completed Chaos row releases successors immediately. Bellman optimization
may run over the admitted subset while alternatives remain open.

A usable executable bounded policy may be returned from that subset. Global
exactness is forbidden while any filtered legal action is unevaluated,
evaluating, or unresolved. An alternative is non-improving only after its exact
row and every support-delta successor needed by that row have usable final
restricted values. Incomplete values produce `unresolved`, never rejection.

Kernel reuse is permitted only for collision-checked exact reforge-kernel
signatures. Shared retry/self-loop behavior must be validated for every
carrier using the kernel.

## Gate 0 — Ownership And Baseline

Before solver behavior changes:

1. map the current state-level atomic expansion cursor, queue release, sparse
   row ownership, focused/Bellman phases, transition-cache persistence,
   compilation, result status, and telemetry;
2. pin the two frozen four-mod gated controls and their current caps, hashes,
   interrupted root owners, states, work, wall, and memory;
3. define which actions are anchors, inexpensive ordinary actions, and delayed
   alternatives without changing goal-relevant filtering;
4. define exact row/state readiness invariants and the final exactness gate;
5. identify how action progress survives bounded solve steps and resource
   exceptions; and
6. add no scheduling source until the active plan and HANDOFF are committed.

## Gate 1 — Incremental Expansion Core

Refactor expansion ownership so a state can publish completed anchor rows and
successors before its full filtered action envelope closes.

Required behavior:

- Chaos and inexpensive ordinary actions run before delayed alternatives.
- Completed anchor successors are interned and queued immediately.
- A state remains envelope-open until every filtered legal action reaches a
  terminal lifecycle status.
- Resuming never duplicates a completed row, transition, work charge, or
  successor enqueue.
- Kernel-equivalent carriers reuse one exact distribution only when complete
  observation and retry/self-loop checks agree.
- Existing unrestricted complete solves retain their exact policies/hashes.

## Gate 2 — Alternative Evaluation And Bellman Cycles

Optimize the admitted restricted graph, then evaluate delayed alternatives
with exact

`Q(a,s) = cost(a) + sum(P(a,s -> s') * V(s'))`.

An improving row is admitted and triggers another Bellman optimization cycle.
A completed row is marked non-improving only against final usable restricted
values for all of its successors. A row that adds states stays pending until
those states are queued, expanded, and solved.

Telemetry must report:

- admitted, evaluated-non-improving, unevaluated, evaluating, and unresolved
  actions;
- unique kernel evaluations and carrier reuse;
- alternative Q values and improvement margins;
- states added outside compatible Chaos support;
- Bellman reoptimizations; and
- the remaining action envelope.

## Gate 3 — Exact Toy Oracles

Add focused native cases proving:

1. Chaos successors expand before all root actions finish.
2. A better alternative is detected and admitted.
3. A worse alternative is safely rejected.
4. An admitted row changes values and causes another evaluation/reoptimization
   cycle.
5. An exceptional-support action adds, queues, expands, and solves missing
   states before classification.
6. An incomplete envelope returns bounded/incomplete and never exact.
7. Different partial states may select different admitted actions.
8. Kernel reuse preserves carrier-specific retry/self-loop semantics.

## Gate 4 — Frozen Qualification

Run the gated frozen cases:

- `natural-t1-full-four-47d8b909aa88`;
- `natural-t1-deep-four-low-probability-1a1102b0e06b`.

Keep current caps initially. Required evidence:

- partial states expand after the Chaos row rather than stopping on the first
  root Fossil;
- Lucent/Jagged plus reached corrected-Harvest alternatives receive exact
  evaluated or honest unresolved statuses against solved state values;
- compatible outcomes reuse Chaos-created IDs;
- delta states, if any, are counted and expanded;
- solver policies may vary by reached state;
- the open action envelope blocks global exactness;
- deterministic work, wall, memory, reoptimizations, and envelope progress are
  reported without discounted probability work; and
- unrestricted controls and action filtering remain correct.

If a current cap blocks meaningful incremental progress, change only the
smallest evidenced resumability or cap boundary. Do not alter probability-work
accounting to make the result pass.

## Gate 5 — Integration Or Restoration

Continue to production integration only if the toy exactness gates pass and
the frozen runs demonstrate meaningful successor expansion plus honest
alternative progress.

If row/value dependencies make safe classification impossible, the scheduler
cannot progress under the product boundary, or exactness/status invariants
fail, restore experimental production changes. Retain measurements and a
precise falsification report.

## Gate 6 — Closure

Run the appropriate native solver acceptance once after the selected
implementation is complete. Rebuild release WASM and run downstream
non-visual web acceptance if retained native behavior is browser-visible.
Run 10,000 simulator verification when a newly returned compiled policy
requires it.

Update stable solver/flow documentation, evidence, active/archive indexes, and
HANDOFF. Commit locally with:

`Co-authored-by: Codex <codex@openai.com>`

Do not push.

## Stop Conditions

Stop only for:

- a genuine mechanic ambiguity;
- a hard exactness or architecture falsification under Gates 1–5; or
- a consequential action outside this boundary.
