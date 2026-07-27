# Streaming Broad-Lower Fold Falsification

**Status: active.** Oliver selected this falsification-first solver pass on
2026-07-27. Execute Gates 0 through 4 in order, including exactly one
conditional Gate 3 path.

Parent: [Active work](README.md)

## Objective

Test the one remaining novel broad-kernel hypothesis before creating another
solver architecture:

```text
Can c(s,a) + E[H_coarse(X)] be computed before successor interning,
inside the existing work cap, strongly enough that the broad row does not
immediately require ordinary exact materialization?
```

The first implementation is measurement-only. It computes and reports a
shadow scalar without changing Bellman values, scheduling, action admission,
upper policies, caps, or termination. The experiment is designed to reject
the direction quickly.

## Fixed boundaries

- The native engine remains the sole authority for transitions, modifier
  persistence, canonical abstract states, legal actions, and goal status.
- Intercept only a fully constructed final `AbstractState` immediately before
  the existing `intern_state` call. Intermediate roll states are not outcomes.
- Use only the graph-independent coarse optimistic completion lower. The
  strict graph-derived table, materialized rows, state IDs, focused values,
  learned estimates, and restricted action subsets are forbidden inputs.
- The fold is lower-only. It cannot create an incumbent, executable policy,
  properness certificate, pruning certificate, or selected product action.
- The measurement path retains no successor state, hash, deduplication table,
  row, cache, or promotion structure.
- Incomplete enumeration publishes no scalar. The current coarse action lower
  remains authoritative.
- Use the existing authoritative transition probability semantics. The shadow
  report must expose emitted mass and the exact normalization/arithmetic path
  it used; production integration requires a separate one-sided arithmetic
  proof if the gate passes.
- Diagnostic work is reported separately and does not alter production work
  counters, caps, graph contents, or Bellman behavior.
- No cap increase, mechanic change, action-scope change, ABI change, GPU work,
  ML guidance, persistent cache, decision DAG, or detached successor table.
- No adaptive racing. Every candidate and baseline run uses fixed cases,
  limits, environment, and stopping policy.
- The exact natural two-T1 oracle does not run.
- Oliver owns rendered review; this pass has no visual surface.
- Commits remain local-only and end with
  `Co-authored-by: Codex <codex@openai.com>`.

## Gate 0 - Contract, cases, and ownership

1. Pin the source, executable, artifact, corpus, economy, generator config,
   selected cases, caps, compiler, and machine.
2. Map the current reforge cache, final-outcome commit point, coarse/strict
   lower split, action lower/ranking logic, work accounting, and telemetry
   ownership.
3. Freeze the shadow quantities:
   - existing broad action lower `q0`;
   - streamed broad lower `qF = c + E[H_coarse(X)]`;
   - next-smallest other action lower `q_other`;
   - local Bellman lower before and in shadow mode;
   - action rank before and in shadow mode; and
   - whether the broad row remains the next required refinement.
4. Pin one smoke case and the existing full/deep three-/four-target hard
   cases before running the candidate.

## Gate 1 - Measurement-only streaming fold

1. Refactor the existing coarse optimistic completion initializer to accept a
   complete `AbstractState` without a state ID or graph access. Preserve the
   existing state-ID entry point and strict-table behavior for production.
2. Add a diagnostic-only exact reforge traversal that:
   - observes final successors before interning;
   - accumulates emitted mass and weighted coarse lower;
   - retains no successor support;
   - uses an isolated 11,000,000-unit work meter; and
   - publishes only after complete traversal and mass validation.
3. Invoke it in shadow mode for the selected broad action without changing the
   ordinary solve.
4. Report at least:
   - completion, work, wall/CPU time, emissions, lower-evaluation work, and
     emitted mass;
   - `q0`, `qF`, `q_other`, incumbent `U`;
   - local Bellman lower and action rank before/shadow;
   - incumbent domination and next-refinement decisions; and
   - estimated or observed ordinary materialization work.
5. Add narrow deterministic tests for:
   - final-successor interception;
   - standalone coarse-lower equivalence;
   - no graph/state/counter mutation;
   - complete versus interrupted publication; and
   - unchanged ordinary solver results with diagnostics enabled.

## Gate 2 - Falsification portfolio

Run the fixed portfolio with identical production caps and no tuning after the
first candidate result.

The broad-lower direction passes only if all three gates pass:

1. **Computability:** the complete fold finishes before 11,000,000 reforge
   work. Near-cap completion is conditional on avoiding later materialization.
2. **Selectivity:** the shadow scalar either makes the broad action
   incumbent-dominated, moves it above another action so it no longer controls
   the Bellman minimum, or materially improves the matched-work certified root
   lower trajectory.
3. **Refinement pressure:** shadow scheduling does not immediately require the
   same exact row when fold plus ordinary materialization would exceed the cap
   or baseline work to the same certified point.

Abandon on the first failed criterion. A raw increase in `qF - q0` is not a
pass when the action remains controlling and immediately selected.

## Gate 3 - Exactly one conditional path

### Gate 3A - Scalar integration, only after a complete pass

If every Gate 2 criterion passes:

1. add the smallest lower-only unresolved broad-action record containing the
   existing kernel identity, full problem epoch, certified scalar, and fold
   work;
2. retain the action in every proof-bearing lower minimum;
3. keep it ineligible for upper-policy construction;
4. call the ordinary exact expansion from scratch only when refinement later
   requires it; and
5. accept duplicate enumeration only if the fixed traces prove it is rare and
   cheaper at matched certified progress.

Do not introduce detached support or incremental promotion.

### Gate 3B - Properness-proof reuse after any failure

If any Gate 2 criterion fails:

1. record the broad-lower rejection and restore all measurement-only solver
   changes that have no continuing diagnostic value;
2. do not attempt another broad-kernel representation;
3. implement versioned reuse of constructive/fallback start-properness proofs
   behind complete graph, policy, economy, action-vocabulary, transition, and
   relevant solver-version identity;
4. prove reuse behavior-identical and invalidate on every dependency change;
   and
5. measure the retained speedup against the known post-incumbent properness
   owner without claiming improvement to the hard pre-bound failure.

## Gate 4 - Acceptance, evidence, and handoff

1. Run one appropriate affected native/benchmark acceptance suite after the
   selected Gate 3 path. Do not run routine suites after intermediate gates.
2. Rebuild WASM and run downstream non-visual checks only if retained native
   behavior reaches the browser solver path.
3. Run exact compiled-policy evaluation and 10,000 simulator executions only
   if a retained candidate changes or newly qualifies a policy.
4. Pin wall and deterministic-work evidence, compiler/machine limits,
   timeouts, survivors, and every negative result.
5. Extract durable facts into solver, decision, evidence, and future
   documentation; archive this plan; clear the active boundary; update
   `HANDOFF.md`; and create the local completion commit.

## Completion criteria

This pass completes with one honest result:

- a qualified lower-only scalar integration that improves matched certified
  progress under unchanged caps; or
- a pinned rejection of broad-lower streaming plus a measured,
  behavior-identical properness-proof reuse result.

It does not complete by retaining measurement scaffolding, reporting a raw
action-bound lift without selectivity, laundering semantic states through a
new name, or weakening proof and policy authority.
