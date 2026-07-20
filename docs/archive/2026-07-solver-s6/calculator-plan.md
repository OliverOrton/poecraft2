# Strategy Builder Calculator Mode — Implementation Plan

> Archived milestone record. Phases A-C.1 and OOM hardening are complete;
> Phase D remains unscheduled historical backlog. See
> [Project Direction](../../direction.md) and [HANDOFF](../../../HANDOFF.md)
> for current work.

Execution plan for the Strategy Builder simulator/calculator mode switch,
written 2026-07-14 as a handoff for implementation. Read
[AGENTS.md](../../../AGENTS.md) and [HANDOFF.md](../../../HANDOFF.md) first; design
authority for the solver machinery is
[solver architecture](../../solver/README.md) and for the workspace
[workspace reference](../../product/workspace.md). **This work is scheduled
before [the S6 plan](plan.md) Phase 1** (Oliver's call, 2026-07-14); the
s6 phases resume afterwards and are renumbered only in prose, not in files.

Decisions already made by Oliver — do not relitigate:

1. **Semantics: whole-graph exact odds.** Calculator mode evaluates the
   authored strategy graph exactly, engine-side: every edge gets its true
   traversal probability, terminals get exact success/failure/stop odds,
   plus expected actions and expected cost. This is a new engine capability
   (propagating exact outcome distributions through the graph, including
   restart loops), not a per-node approximation.
2. **Surfaces: all four.** An exact summary panel replacing the Monte Carlo
   runner, edge probability labels on the board, node visit badges, and a
   per-node outcome drill-down.
3. **Gaps: refuse first, bounds later.** Graphs using actions or conditions
   the exact machinery cannot evaluate are refused with a precise,
   element-level reason (the `solver_compile.cpp` gap-message convention).
   An "unknown bucket" bounds mode is specced as a follow-up (Phase D), not
   built now.
4. **Sequencing: next up, before s6 Phase 1.** The plan assumes the current
   working tree (Calculator Variant E milestone).

Status 2026-07-14: **Phases A, B, and C are complete.** The Strategy Builder
now switches between its Monte Carlo runner and the exact whole-graph
Calculator inspector; node/edge annotations, selected-node state classes,
live-priced expected consumption, persistence, refusals, and stale/evaluating
states are implemented. Oliver selected a hybrid of mock Variants A and B:
Variant B's node/result treatment in the existing bottom runner boundary, with
the right panel retained for actual graph-node inspection.

Status 2026-07-14 (later): **Phase C.1 is complete.** Exact evaluation now
discovers the reachable pair graph once, solves its SCC condensation directly
with geometric/rank-one/dense paths plus a bounded local fallback, exposes a
stepped ABI with real worker progress and cancellation, and retains the
synchronous API as a wrapper. Phase D remains unscheduled; `s6-plan.md` Phase 1
resumes next.

Status 2026-07-14 (latest): **The Calculator exact-evaluation OOM hardening is
complete.** Base-shared reforge distributions and routed rows are shared,
edge ids are interned, SCC discovery no longer copies the dense edge relation,
state/transition capacity failures are explicit, WASM has a 4 GB ceiling, and
worker/UI errors retain their real code without double-prefixing. The pinned
rare Alchemy/Chaos loop now completes exactly under default options; Phase D
is still unscheduled and `s6-plan.md` Phase 1 is next.

Two standing rules gate everything below, same as every solver phase:

- **The engine is the only crafting-rule authority.** Condition evaluation
  over abstract states, distribution propagation, and legality all happen
  engine-side. The UI renders returned numbers; it may do presentation
  arithmetic (percent shares, price dot-products) but never probability or
  legality logic.
- **Mechanic questions go to Oliver.** Ask him directly; never research
  online or guess.

## What Calculator Mode Is

The Strategy Builder's bottom runner section today hosts `pc-simulator` +
`pc-run-trace` (`pc-strategy-editor.ts` `renderShell`, the
`.pc-strategy-runner` section) and estimates a strategy's behavior by
compiling it and running Monte Carlo simulations. Calculator mode answers
the same question exactly: given the start item, what are the odds of every
path through the graph?

The math: a strategy run is a walk over (graph node, item state). Project
item states onto the solver's abstract state (`AbstractState`,
`engine/src/solver_internal.hpp:205`) and the walk becomes a finite
absorbing Markov chain over (node, abstract state id) pairs, because:

- transition distributions of every candidate action are exact functions of
  the abstract state (the S1–S3 invariant; `CalcContext::outcomes`,
  `solver_internal.hpp:294`), and
- every edge condition is mapped to a pure predicate over the abstract
  state — and any condition that *cannot* be so mapped is refused up front
  (decision 3), which is exactly what keeps the chain Markovian and the
  result exact rather than approximate.

Evaluation is then forward mass propagation from
(start node, project(start item)) until (nearly) all probability mass is
absorbed at terminals or failure buckets. Flows are price-independent —
prices never influence routing — so expected cost falls out as engine-
reported expected consumption per price key, dotted with the live workspace
price table in the UI (the same presentation arithmetic the Calculator's
cost panel already does).

Implementation note (Phase A): the ordinary DP solver deliberately keeps the
compact, approximately-sound junk partition described in
`crafting-solver-plan.md`. Whole-graph evaluation enables a strict layout mode
that additionally partitions junk modifiers by their complete exclusion-group
effect mask. Without that refinement, two items collapsed to one abstract
state can remove different weighted families from a later Regal/Exalt pool,
which is not exact enough for calculator mode.

## Simulator-Parity Semantics

Calculator mode predicts what the simulator does. The evaluator must mirror
the run loop (`engine/src/simulator.cpp`) exactly:

- **Edge selection** — edges are stable-sorted at compile by
  (priority, source order) (`simulator.cpp:1005`); the first *non-default*
  edge whose condition matches wins; the default edge is the fallback; no
  match and no default → the run fails with `no_matching_edge`
  (`select_edge`, `simulator.cpp:588`).
- **Illegal actions fail the run.** When `apply_action` reports
  `applied == false` the run fails with `action_not_applied`
  (`simulator.cpp:834`). ⚠ This is *different* from the calc engine's
  internal convention, where an illegal action is a supported self-loop
  (`solver_calc.cpp:319`). The evaluator must check `action_legal`
  *before* calling `outcomes()` and route illegal mass to an
  `action_not_applied` bucket — do not let the self-loop leak in.
- **Restart** resets the item to a fresh base with implicits, normal
  rarity, charges price key `"base"`, and counts as an action
  (`simulator.cpp:364,803`). The registry's synthetic restart descriptor
  and the calc's restart evaluator already model this transition.
- **Terminals absorb**; success/failure/stop are separate buckets, tracked
  per terminal node.
- **Run limits do not apply.** Action/step/cost limits and missing-price
  failure are simulation options; the evaluator's analog is a sweep cap
  with explicit unresolved-mass reporting (below). MC comparison tests must
  run the simulator with generous limits and no cost cap so limit failures
  are ~0.

Mass that never absorbs (e.g. a pure router cycle, or a scour loop that can
trap some outcome class forever) is reported, not hidden: the result carries
`unresolved` mass with a per-node breakdown ("2.3% of runs get stuck at
node X"). Implementation note: advance router hops one hop per sweep like
every other hop, so cycles stall detectably instead of hanging a sweep.

## Supported Vocabulary And Refusals

Condition mapping (engine condition vocabulary from `compile_condition`,
`simulator.cpp:193`) onto `AbstractState`:

| Condition | Abstract predicate |
| --- | --- |
| `always` | true |
| `has_mod_group(g)` | slot(g).status ≠ Absent |
| `has_mod_family(f, min_tier=0)` | slot(f).status ≠ Absent |
| `has_mod_family(f, min_tier=t>0)` | slot(f, t).status == Satisfied |
| `rarity_is` | `state.rarity` |
| `prefix_count_range` / `suffix_count_range` | `state.prefix_count` / `suffix_count` |
| `open_prefix_count` / `open_suffix_count` | `rarity_affix_cap(session, rarity) − count` (reuse `solver_calc.cpp:31`) |
| `all` / `any` / `not` / `at_least` | recurse |

Every family/group a condition references becomes a goal slot of the
evaluation's internal abstraction ("targets"); junk classes derive from the
graph's action set exactly as for a solve.

**v1 refusals** — all structural, all reported with the offending
node/edge/condition named, ideally every gap listed in one message (the
`PC_RESULT_UNSUPPORTED_FEATURE` + verbatim-message convention from
`pc_solver_compile_strategy`):

- An operation whose calc evaluator reports no exact support. As of today:
  veiled chaos/exalt, unveil, the five eldritch actions, fracture, and
  harvest resist. **Key off evaluator support, not a hardcoded list** — add
  a small `calc_supports(descriptor)` helper next to the evaluator dispatch
  so s6 Phase 4a landing extends calculator-mode coverage with zero eval
  changes.
- `has_mod_family` with `fractured: true` (the abstract state tracks only
  the item-level `kFlagFractured`, not per-slot fractured).
- The same family referenced with two *different* non-zero tier thresholds
  (one abstract slot can only answer one threshold), or a family and a
  group whose member masks overlap (`build_abstract_layout` rejects
  overlapping slots, `solver_internal.hpp:197`). Message should tell the
  user to align the tiers.
- More than 8 distinct condition targets (`kMaxGoalSlots`).
- If A2's layout relaxation proves invasive (see below): graphs with *no*
  family/group conditions at all.

Simulator mode always remains available for refused graphs.

## Phase A — Engine Evaluator + C ABI

New file `engine/src/solver_eval.cpp`, new tests
`engine/tests/test_solver_eval.cpp`. Remember new engine test files must be
registered in three places: engine CMakeLists, `scripts/build.ps1`
`TestSources`, and `tests.hpp`/`test_main.cpp`.

1. **Derivation: compiled graph → evaluation model.** Input is a compiled
   `pc_strategy_handle` (`pc_strategy_compile_json`,
   `poecraft/simulator.h:20`) — reuse its parsed nodes/edges/start item, do
   not re-parse JSON. Steps:
   - Resolve each operation node to one registry `ActionDescriptor`
     (`build_action_registry(session)`). Match on the node's compiled
     `ActionParameters` (or re-derive the canonical id string —
     implementer's choice, but a test must pin every operation type,
     including fossil loadouts, to the descriptor it resolves to). Restart
     nodes resolve to the synthetic restart descriptor. Unresolvable →
     refusal naming the node.
   - Walk every edge's `CompiledCondition` tree; collect targets
     (family_id / group_id, tier threshold) and detect the refusal cases
     above.
   - Build `GoalSpec{slots = targets}` (rarity and `min_satisfied_slots`
     are irrelevant — success is defined by success terminals only; the
     evaluator must never consult `is_goal_state`) and a
     `CalcContext(session, goal, registry, used_action_indices)` using the
     evaluator's strict exclusion-effect junk partition. The DP solver keeps
     its compact partition; calculator-mode evaluation does not.
   - Pre-check evaluator support per used descriptor via the new
     `calc_supports` helper → refusal before any propagation.
2. **Empty-target graphs.** `build_abstract_layout` currently rejects zero
   slots. Pure count/rarity strategies ("chaos until 3 prefixes") are
   legitimate, so relax the layout to accept an empty slot list for eval
   callers (junk classes and `project_item` remain well-defined) behind an
   explicit construction flag, keeping the `pc_solver_create` JSON path
   rejecting empty goals. If the relaxation ripples further than expected,
   fall back to a v1 refusal with a precise message and leave the
   relaxation to a follow-up.
3. **Condition predicates.** Implement the mapping table above as a
   compiled-predicate tree over `AbstractState`. Property gate: for every
   reachable abstract state and every leaf condition type, the predicate
   must agree with the simulator's `evaluate_condition`
   (`simulator.cpp:523`) applied to `CalcContext::materialize`'s
   representative item (`solver_internal.hpp:287`).
4. **Propagation.** Worklist/sweep propagation of mass over
   (node index, state id):
   - init: mass 1.0 at (start node, `intern(project(start_item))`);
   - operation nodes: illegal → `action_not_applied[node]`; legal → expand
     `outcomes(state, action)`, accumulate `expected_actions += mass` and
     per-price-key consumption from the descriptor's `cost_keys`;
   - routers/start: route the state unchanged; terminals: absorb into
     per-terminal buckets;
   - routing mirrors `select_edge` exactly (sorted order, default
     fallback, `no_matching_edge[node]` bucket);
   - accumulate per-node expected visits, per-edge expected traversals, and
     each node's incoming state mixture (top-K classes by mass + remainder,
     K an option, default ~16);
   - stop when the transient wave's total mass < epsilon (default ~1e-12)
     or at `max_sweeps`; leftover mass → `unresolved[node]`.
   - Invariant to assert in tests: absorbed + transient mass == 1 within
     FP tolerance after every sweep. Distribution caching per
     (state, action) makes loop re-visits free; state count is bounded the
     same way a solve is (`max_states` option guards memory).
5. **C ABI** (`engine/include/poecraft/solver.h`,
   `engine/src/solver_api.cpp`):
   ```c
   pc_result pc_strategy_evaluate(
       pc_strategy_handle strategy,
       const pc_strategy_eval_options* options, /* struct_size/abi_version,
           epsilon, max_sweeps, max_states, top_classes_per_node */
       char* buffer, size_t capacity, size_t* out_length,
       pc_error_info* out_error);
   ```
   JSON result via the query-required-count buffer pattern
   (`pc_solver_compile_strategy` precedent, `solver.h:209`). Vocabulary
   refusals return `PC_RESULT_UNSUPPORTED_FEATURE` with the full gap list
   in the message. Result shape (v1 — field names final at implementation,
   but keep this structure):
   ```json
   {
     "version": "v1",
     "converged": true, "sweeps": 37, "residual_mass": 0.0,
     "terminals": {
       "success": 0.61, "failure": 0.02, "stop": 0.0,
       "action_not_applied": 0.0, "no_matching_edge": 0.37,
       "unresolved": 0.0,
       "by_node": [{"node_id": "success_1", "kind": "success", "p": 0.61}]
     },
     "unresolved_by_node": [{"node_id": "alt_loop", "mass": 0.0}],
     "failures_by_node": [{"node_id": "exalt_1",
                            "reason": "action_not_applied", "p": 0.0}],
     "expected_actions": 41.2,
     "expected_consumption": [{"key": "chaos", "quantity": 38.1}],
     "targets": [{"kind": "family", "family_id": 512, "min_tier": 1},
                 {"kind": "group", "group_id": 41}],
     "nodes": [{"id": "chaos_1", "expected_visits": 38.1,
                 "classes": [{"share": 0.31, "rarity": 2, "prefixes": 3,
                              "suffixes": 2, "flags": 0, "blocked": 0,
                              "slots": [2, 0]}],
                 "classes_truncated_share": 0.05}],
     "edges": [{"id": "edge_4", "expected_traversals": 12.3}]
   }
   ```
   The UI resolves `family_id` via its cached `ModInfo.family_id` and
   `group_id` via the catalog, exactly as existing surfaces do. Costs are
   deliberately absent: the UI prices `expected_consumption` with the live
   price table.
6. **Tests** (`test_solver_eval.cpp`), on both the synthetic session and
   the Vaal Regalia fixture:
   - hand-computed closed forms: a chaos-until-family-hit loop (success
     mass → 1, expected actions = 1/p, edge flows geometric) and a
     straight-line deterministic graph, asserted to ~1e-9;
   - exact-vs-MC gate: a richer graph (alt/regal/exalt with conditions, a
     scour recovery loop, a restart path) evaluated exactly, then simulated
     20–50k runs with generous limits and fixed seed: per-terminal
     probabilities within 5σ + slack, expected actions and per-key
     consumption within CLT tolerance;
   - abstraction regression: the richer graph includes a post-Regal branch
     where differently weighted non-goal exclusion groups would drift if
     collapsed; strict evaluation must remain inside the same MC tolerance;
   - semantics parity: illegal-action mass lands in `action_not_applied`
     exactly where the simulator fails runs; `no_matching_edge` parity;
     priority/default-edge ordering parity on a graph with overlapping
     conditions;
   - **price-key parity**: for every operation type, the registry
     descriptor's `cost_keys` must match the simulator node's
     `price_keys` (both include `resonator:<n>` for fossils —
     `simulator.cpp:424` vs. the registry's fossil descriptors; pin this
     so they cannot drift, since the MC cost gate depends on it);
   - refusal messages: veiled/eldritch/fracture ops, fractured condition,
     two tiers on one family, >8 targets — each names the offending
     element;
   - unresolved mass: a deliberate router cycle reports its mass by node;
   - determinism: identical JSON in → byte-identical JSON out;
   - mass conservation invariant.

**Gate.** `scripts/build.ps1` then
`build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`
green.

## Phase B — WASM, Worker, Client Plumbing

1. `bindings/wasm/wasm_api.cpp`: `pcw_strategy_evaluate(strategy, options
   json)` following the existing `pcw_*` JSON conventions. Remember
   `pcw_*` doubles serialize via `std::to_string` (6 decimals): web
   assertions on probability sums get 1 ± 1e-3. **Rebuild WASM**
   (`scripts/build-wasm.ps1`, self-activates emsdk from `C:\emsdk`).
2. Protocol (`engine-protocol.ts`): `StrategyEvalOptions`,
   `StrategyEvalResult` (mirroring the JSON above). Worker
   (`engine-worker.ts`): a `strategyEvaluate` method that compiles the
   strategy JSON, evaluates, and destroys the compiled handle in one
   request. Client (`engine-client.ts`):
   `strategyEvaluate(session, strategy, options?)`.
3. `apps/web/test/engine-smoke.test.ts` additions: a happy-path eval
   (terminal probabilities sum ≈ 1, edge flows present, expected actions
   > 0, consumption keys present, spot-agreement with a 5k-run
   `runStrategy` at loose tolerance) and a refusal path (a veiled-op
   strategy → error message names the operation).

**Gate.** `npx tsc --noEmit`, `npm test`, `npm run build` in `apps/web`;
one full `powershell -File scripts/test.ps1` before the phase commit since
engine + bindings + web all changed.

## Phase C — Strategy Builder UI (design loop required)

Oliver requires new UI to go through the image-model design loop
([the S6 plan](plan.md) §The image-model UI design loop) **before
implementation**. This phase has real surface area: the mode switch, the
summary panel, board overlays, and the drill-down.

1. **Design brief** `design/briefs/strategy-calculator-mode.md`. Content
   inventory with *real values* from a Phase B eval run (actual
   percentages, consumption rows, a real refusal message). Required
   states: valid + converged; refusal (verbatim gap list); unresolved-mass
   warning; missing-price cost rows; stale (graph edited since last eval);
   evaluating/busy; dense graph (annotations readable at 30+ nodes).
   Interactions: the mode switch itself; evaluate trigger (auto on change
   vs. explicit button — pose in the brief); node select → drill-down;
   edge hover → absolute expected traversals; live price edits.
   Brief questions for Oliver's mock review: edge label quantity
   (conditional share of the node's exits vs. absolute traversal count)
   and node badge quantity (expected visits vs. reach chance — v1 engine
   provides expected visits; reach probability is Phase D). Constraints:
   dark theme, dockview tab, plain CSS + Web Components, existing `--pc-*`
   tokens, Emulator as baseline aesthetic. Reference screenshots of the
   current builder and Calculator into `design/refs/`; 2–4 structurally
   different mockups into `design/mockups/strategy-calculator-mode/`;
   Oliver picks; spec lands in `design/specs/strategy-calculator-mode.md`.
2. **Mode state.** `mode: "simulator" | "calculator"` on
   `PcStrategyEditor`, persisted as an optional `builderMode` field on
   `StrategyDraftRecord` (`workspace/persistence.ts:71`; old drafts load as
   simulator). The switch swaps the runner section between
   `pc-simulator`/`pc-run-trace` and the new odds panel, and toggles board
   annotations. Dockview detaches inactive panels — mode and the last eval
   result must survive reconnect from retained state (see `pc-mod-pool`),
   and tests must activate panels via `panel.api.setActive()`.
3. **Eval driver** in `pc-strategy-editor.ts`. Trigger on entering
   calculator mode and on *structural* graph change (nodes, edges,
   conditions, operations, base state — not positions, viewport, labels,
   or names; note `markChanged(false)` already exists for viewport-only
   changes). Debounce ~300 ms; gate on `validateStrategy` having no
   errors, same as `run()`; call through the existing busy/guard pattern.
   The call blocks the sequential worker like the Calculator's
   `solverCalc` does today — say so in the UI ("evaluating…"); if real
   graphs get slow, the escape hatch is s6 Phase 2's chunked-call pattern,
   not a UI workaround. Engine refusals render verbatim in the odds panel
   with "this strategy uses actions or conditions calculator mode cannot
   evaluate exactly yet" framing.
4. **Summary panel** (new `pc-strategy-odds` element or per the chosen
   mock): success headline (`formatProbabilityExact` /
   `formatRawProbability` from `odds-presentation.ts`), failure/stop,
   `action_not_applied` / `no_matching_edge` / unresolved as miss-signal
   rows with node attribution, expected actions, and the cost section:
   `expected_consumption` rows with inline price inputs
   (`getPrice`/`setPrice`/`onPricesChange` from `workspace/prices.ts`),
   total expected cost as the dot product, missing prices explicit.
   Extract/reuse the Calculator's cost-row rendering into
   `odds-presentation.ts` rather than duplicating it.
5. **Board overlays.** Extend `pc-strategy-board`/`pc-edge-layer`/
   `pc-strategy-node` with an annotations input (e.g.
   `setAnnotations({nodeBadges, edgeLabels})`): edge labels show the
   share of the source node's outflow (UI arithmetic:
   traversals ÷ Σ sibling traversals) with absolute expected traversals on
   hover; operation nodes badge expected visits; terminal nodes badge
   their absorb probability. Annotations clear in simulator mode and dim
   with a "stale" chip when the graph has changed since the last eval.
   Note for s6 Phase 1: its compiled-policy `expected_cost` badge should
   reuse this same badge mechanism when it lands.
6. **Drill-down.** Selecting an operation node in calculator mode shows
   its incoming state mixture: the targets legend (family/group labels
   resolved through the modifier options cache and catalog, like the
   Calculator's slot labels) and a top-K class table reusing the
   Calculator's technical-distribution table style (rarity, P/S counts,
   per-target status, flags, share), plus the truncated remainder share.
7. **Web tests**: mode persists across draft reload; annotations render
   from a canned eval result; refusal message displays verbatim; eval
   re-runs on a structural edit but not on a node drag.

**Gate.** `npx tsc --noEmit` + `npm test` green in `apps/web`; manual
preview pass with screenshots compared against the chosen mock (deviations
noted in the spec file); the Phase B smoke additions still green.

## Phase C.1 — Loop Acceleration + Real Progress (complete)

Implementation status (2026-07-14): the native SCC evaluator, stepped C ABI,
WASM/worker/client progress and cancellation, Strategy Builder abort lifecycle,
reference/MC/closed-SCC/fallback/cap/determinism gates, and opt-in pinned worker
benchmark are complete. On the same Vaal Regalia graphs used for the baseline,
the final warmed worker medians are 31.639 ms for the T1 flat Energy Shield
loop (45.32× faster than 1,434 ms) and 31.243 ms for the lower-hit-rate T1
hybrid family (112.35× faster than 3,510 ms). The callback-enabled median delta
was -0.200 ms (measurement noise, no observed overhead) for the first graph and
+0.136 ms for the second. The result JSON remains v1-compatible; its `sweeps`
field now counts only local fallback sweeps, so a direct SCC solve reports 0.

### Why this pass exists

The Phase A evaluator is exact but advances probability one graph hop per
sweep until transient mass falls below `epsilon` (default `1e-12`). Action
distributions are cached, but a loop with exit probability `p` still takes
approximately `log(epsilon) / log(1 - p)` sweeps to discharge its numerical
tail. Measured through the same WASM worker path used by the UI on 2026-07-14:

- the Phase C T1 Energy Shield Alteration loop took **1,434 ms**, 2,219
  sweeps, and reported 81.7014428412 expected actions;
- the same graph shape targeting a lower-hit-rate T1 family took
  **3.51–3.64 s**, 4,448 sweeps, and reported 162.4028856824 expected actions;
- session construction took about 15 ms, so the propagation tail—not setup—is
  the bottleneck.

Simple loops do not need numerical revisits. For `hit -> exit` with probability
`p` and `miss -> self` with probability `q`, expected visits are
`1 / (1 - q)` and all outgoing flows follow directly from that value. General
multi-state loops are the same problem over a transient transition matrix:
expected pair visits solve `(I - Q^T)x = incoming`.

### Scope decisions (final)

1. **Do not loosen exactness.** Keep the existing result contract, default
   epsilon, simulator-parity failure semantics, strict exclusion partition,
   and deterministic output. This is an algorithm change, not a cheaper odds
   mode.
2. **Discover transitions once.** Build the reachable graph of
   `(compiled node, abstract state)` pairs. Each legal action distribution,
   condition route, price-key effect, and absorbing/failure transition is
   derived once and reused for solving and result reconstruction.
3. **Solve loops by SCC.** Condense the reachable pair graph into strongly
   connected components:
   - acyclic singleton: direct forward flow;
   - singleton with a self-loop: geometric closed form;
   - small cyclic component: solve `(I - Q^T)x = incoming` with a
     pivoted dense solve assembled from sparse transitions;
   - large or numerically ill-conditioned component: retain a bounded local
     iterative fallback, not whole-graph sweeps.
4. **Closed recurrent components are unresolved.** Detect an SCC with no
   positive-probability exit or absorption without waiting for `max_sweeps`;
   route the probability entering it to the existing unresolved result and
   provide deterministic node attribution. Do not hide or renormalize it.
5. **Add honest progress and cancellation in this pass.** Progress reports
   real phases/counts (transition discovery, SCC solving, fallback residual,
   finalization), not a fabricated time percentage. A new structural edit,
   leaving Calculator mode, or document disposal cancels/abandons the obsolete
   evaluation before the newest one begins.
6. **Preserve the synchronous API.** Existing native/Python callers of
   `pc_strategy_evaluate` continue to work; it drives the new stepped engine to
   completion internally. WASM/web use the stepped API so progress messages and
   cancellation can flush between chunks.
7. **No new image-model loop.** The approved Phase C design already specifies
   the evaluating state. Extend its existing status line with progress text;
   do not redesign the panel or move the right graph inspector.
8. **Stop after C.1.** Phase D is still unscheduled. Resume `s6-plan.md`
   Phase 1 only after this pass is committed and handed off.

### Implementation

1. **Pinned benchmark and reference path.** Add an opt-in benchmark that runs
   the two measured Vaal Regalia loop graphs through the WASM worker, records
   warmed median wall time, and asserts the exact result fields independently
   of timing. Keep a test-only high-precision forward propagator for numerical
   parity on small graphs; do not keep the old production evaluator.
2. **Reachable transition graph** (`engine/src/solver_eval.cpp`, split into a
   helper file only if it improves clarity):
   - worklist from `(start node, project(start item))`;
   - one record per reachable pair with sparse transient transitions plus
     terminal/failure absorption entries and edge ids;
   - reuse `CalcContext::outcomes` and its `(state, action)` cache;
   - guard abstract states with `max_states` and add a pair-count guard so
     `node_count × state_count` cannot exhaust memory;
   - no mass threshold during discovery: every positive-probability reachable
     transition is part of the exact finite model.
3. **Component solver:** Tarjan/Kosaraju SCC decomposition, condensation DAG
   in topological order, direct/geometric/small-linear/fallback paths from the
   scope decisions above. Use partial pivoting, finite/conditioning checks,
   and clamp only tolerance-sized negative FP noise. Any fallback retains mass
   conservation checks after each local batch.
4. **Result reconstruction:** derive terminal/failure/unresolved probability,
   per-node expected visits and incoming normalized classes, per-edge expected
   traversals, expected actions, and price-key consumption from solved pair
   visits. Existing converged fixture results must agree with the reference
   propagator to `1e-9`; identical input remains byte-deterministic under the
   new implementation.
5. **Stepped C ABI** (`solver.h`, `solver_api.cpp`): add an opaque evaluation
   work handle and begin/step/finish/destroy calls. The progress struct reports
   phase, done flag, discovered/pending pair counts, solved/total SCC counts,
   fallback sweeps, and current residual where meaningful. Keep the existing
   query-required-count JSON finish and `pc_strategy_evaluate` wrapper.
6. **WASM/worker/client:** expose the stepped calls, rebuild WASM, and drive
   adaptive chunks targeting about 16 ms (mirror `runStrategy`). Yield between
   chunks, emit at most about 10 progress messages/second, and accept an
   `AbortSignal`. Destroy both evaluation and temporary compiled-strategy
   handles in every success/error/cancel path.
7. **Strategy Builder:** replace the current uncancellable call with an
   `AbortController`. Show concise actual state such as
   `Discovering exact states · 143 pairs`,
   `Solving loops · 12/14 components`, or
   `Fallback · 320 sweeps · residual 2.4e-7`, plus elapsed time. Keep the
   previous stale result/annotations visible until the replacement completes.

### Tests and performance gates

- Closed forms: the existing geometric loop plus a two-state cyclic system
  have hand-computed terminal probabilities, expected visits, edge flows,
  actions, and consumption at about `1e-10`.
- Reference parity: representative straight-line, Alt/Regal, recovery/restart,
  priority/default, illegal-action, and no-matching-edge graphs agree with the
  test-only high-precision forward propagator at `1e-9` and retain the Phase A
  MC gates.
- Closed SCCs: self-loop and router-cycle cases report total unresolved mass
  immediately with deterministic node attribution; near-closed SCCs with a
  real exit still solve as converged.
- Scale/determinism: pair guard failure is explicit, large-SCC fallback
  conserves mass, and identical input remains byte-identical.
- Progress/cancel: a deliberately long evaluation emits at least two ordered
  progress events, cancel abandons promptly with no WASM handle growth, and a
  burst of structural edits runs only the newest graph after cancellation.
- Local performance: record before/after warmed medians for both pinned loops;
  target at least **5×** improvement on each. Reporting enabled versus the
  same stepped run with callbacks suppressed must add no more than **2% or
  2 ms**, whichever is larger. If either target is missed, profile and report
  the cause before committing rather than weakening the gate silently.

**Gate.** `powershell -File scripts/build.ps1`, direct native engine tests,
`powershell -File scripts/build-wasm.ps1`, `npx tsc --noEmit`, `npm test`, and
`npm run build` in `apps/web`, then one full
`powershell -File scripts/test.ps1`. Use a separate headless browser process
for the Calculator progress/cancel smoke; do not use Codex's built-in browser.
Rewrite `HANDOFF.md`, make one local commit, and do not push.

## Phase D — Specced Follow-Ups (not scheduled)

- **Bounds mode (Oliver's "refuse first, bounds later").** An eval option
  that converts structural refusals into absorbing "unknown" mass at the
  offending node/edge instead of failing: results become bounds
  (success ∈ [p, p + unknown]), rendered as such. Small delta once Phase A
  exists — each refusal site becomes a bucket redirect behind the flag.
- **Reach probability on demand.** P(ever visit) for the selected node via
  one extra absorb-on-first-visit propagation — cheap per node, wasteful
  for all nodes; wire to the drill-down if Oliver wants it after using
  expected visits.
- **Eval context reuse.** Cache the engine-side evaluation context keyed by
  the derived layout signature (targets + action set) so repeated evals
  across non-structural edits skip layout/DP rebuilds. Only if profiling
  says it matters.
- **Solver cross-check synergy.** Once s6 Phase 1 lands, a compiled policy
  opened on the board can be run through calculator mode: its exact
  expected cost should equal the solve's `start_value` — an exact-vs-exact
  verification stronger than the 5k-run Monte Carlo check. Worth adding to
  Phase 1's verification story when both exist.

## Cross-Phase Conventions

- Layer test commands: web = `npx tsc --noEmit` + `npm test` in
  `apps/web`; engine = build then
  `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`;
  full pipeline = `powershell -File scripts/test.ps1` (slow — required
  once per phase that touched engine or bindings).
- Rebuild WASM after any C ABI change (`scripts/build-wasm.ps1`), then
  re-run web tests.
- `pcw_*` JSON doubles have 6-decimal precision; write assertions
  accordingly.
- Commits: one per phase, local-only unless Oliver says push, agent
  co-author line at the end. Rewrite HANDOFF.md at each phase boundary.
- Doc updates at the end of Phase C: add calculator mode to
  [workspace reference](../../product/workspace.md) §Strategy Builder and
  a §Strategy Evaluation summary to
  [solver architecture](../../solver/README.md); run the doc-drift
  agent before editing.
