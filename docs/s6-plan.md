# S6 Completion Plan

Execution plan for the remaining solver S6 work, written 2026-07-12 as a
handoff to Codex. Read [AGENTS.md](../AGENTS.md) and [HANDOFF.md](../HANDOFF.md)
first; design authority for the solver is
[crafting-solver-plan.md](crafting-solver-plan.md) and for the workspace
[desktop-workspace-ui.md](desktop-workspace-ui.md). Phases are in priority
order; each ends test-green with one commit and an updated HANDOFF.md.

Status 2026-07-13: Phase 0 is complete. Oliver approved Calculator Variant E,
the shared item display, and the refined goal/outcome presentation. The
success definition is now native solver input (`min_satisfied_slots`), the C
ABI/WASM result includes the combined `success_probability`, and compiled
policies preserve partial thresholds with `at_least`. The Odds inspector shows
goal-coverage buckets and overlapping miss signals; raw abstract classes are
collapsed under Technical distribution.

Status 2026-07-14: Oliver inserted the Strategy Builder calculator mode
([strategy-calculator-mode-plan.md](strategy-calculator-mode-plan.md))
ahead of Phase 1. Phase 1 resumes after it lands; its compiled-policy
node badges should reuse that plan's board-annotation mechanism.

Status 2026-07-14 (later): Strategy Builder calculator mode Phases A-C are
complete. Oliver then scheduled the loop-acceleration/progress pass in
`strategy-calculator-mode-plan.md` Phase C.1 before this plan resumes. Phase 1
is next only after C.1 completes. Reuse
`strategy-eval-presentation.ts` / the `PcStrategyNode` annotation input for
compiled-policy expected-cost badges instead of adding a second badge system.

Status 2026-07-14 (latest): Phase C.1 and its rare-reforge exact-evaluation
OOM hardening are complete and fully gated. Phase 1 below is now the next task.
The board-annotation reuse ruling above still applies; Phase D remains
unscheduled.

Status 2026-07-14 (newest): pre-S6 polish P1 and the exact-evaluation loop
repair are complete. Oliver skipped P2 and narrowed P3 to presentation only:
Calculator remains one input item, one v1 goal item, one selected action, and
engine-returned odds. Oliver approved P3a Goal Item Variant A; P3b shared
item-frame implementation is next. Resume S6 Phase 1 only after P3b. The
earlier multi-goal/OR engine contract and shared predicate editor are no longer
scheduled. The board-annotation reuse ruling and unscheduled Strategy Builder
Calculator Phase D boundary still stand.

Status 2026-07-14 (current): pre-S6 P3b is complete. Calculator still uses one
concrete input item, one v1 goal item, and one selected action, but Input and
Goal now share the explicit concrete/target `pc-mod-list` frame. The
implementation, full gates, browser recovery smoke, and comparison captures
are recorded in `design/specs/calculator-goal-item.md`. Phase 1 below is now
the next task; do not begin Phase 2 in the same milestone.

Status 2026-07-15: Phase 1 is complete. Oliver approved the Calculator's
natural lower center/right workspace, and the implemented panel spans exactly
from the Modifier Pool's left edge to the Odds column's right edge. It reuses
the shared action-price rows, exposes a real `Start solve` action, scopes a
temporary Solve-only solver to the fully priced native action ids, compiles and
auto-layouts the policy, opens an ordinary copied Strategy Board document,
renders native `expected_cost` through the existing annotation channel, and
verifies with 5,000 simulator runs. Vocabulary-gap refusals retain the native
message. Phase 2 is the next boundary.

Status 2026-07-15 (later): Phase 2 is complete. The synchronous and stepped C
ABI paths share one native `SolveWork` state machine; WASM and the worker drive
bounded expansion/sweep chunks with structured progress and AbortSignal
cancellation; Calculator shows only live states, sweeps, residual, V(start)
bound, and Cancel while work is active.

Status 2026-07-15 (Phase 4): Oliver explicitly selected Phase 4 before Phase 3,
so Phase 3 was not begun at that boundary. Phase 4 is complete. Veiled chaos/
exalt/unveil and all five eldritch actions now have exact evaluators,
including policy-owned choice among sampled unveil offers. The policy compiler
now represents tag-sensitive junk identities, exact item flags/influence/
eldritch tiers, group tier thresholds, and unveil-option routing with native
strategy conditions. Synthetic and Vaal Regalia Monte Carlo gates, all-T1 and
unveil solve -> compile -> simulate gates, rebuilt WASM/web checks, and the full
repository pipeline pass.

Status 2026-07-15 (final): Oliver skipped Phase 3 entirely. It is not deferred
work and must not be reintroduced as a future milestone unless Oliver explicitly
creates a new scope for it. With Phases 1, 2, and 4 complete and Phase 3 skipped,
S6 is complete.

Two standing rules bear repeating because they gate everything below:

- **The engine is the only crafting-rule authority.** UI code renders what
  `pc_calc_action_outcomes` / `pc_solver_solve` return; it never computes
  odds, weights, or legality itself.
- **Mechanic questions go to Oliver.** If an evaluator's behavior or a
  condition's semantics is ambiguous, ask him; never research online or
  guess.

## The image-model UI design loop (applies to every UI phase)

Oliver wants UI designed by an image model first, then implemented to
match. Oliver was not happy with the current Calculator look, so treat
this loop as the required path for new UI, not an optional garnish. The
loop, per surface:

1. **Design brief** — write `design/briefs/<surface>.md` containing:
   - purpose of the surface, one paragraph;
   - a complete content inventory with *realistic values* (every number,
     label, and state the surface must show — pull real values from the
     running app, e.g. "+(91-100) to maximum Energy Shield · T1 · 3.99%");
   - required states: empty, loading, error/illegal, dense (8 goal slots,
     40 outcome rows), and any hover/expanded states;
   - interactions (what is clickable and what it does);
   - hard constraints: dark theme, desktop-first, lives inside a dockview
     tab, plain CSS + Web Components (no React), and the existing token
     palette from `apps/web/src/styles/app.css` `:root` (list the actual
     hex values in the brief so the image model can use them).
2. **Reference screenshots** — capture the current state of the surface
   (and the Emulator, whose look Oliver accepts as the baseline aesthetic)
   into `design/refs/`. Give these to the image model as input images.
3. **Generate mockups** — prompt the image model for a flat desktop UI
   mockup, exact text labels from the brief, 2–4 *structurally different*
   variants (different information hierarchies, not recolors). Save to
   `design/mockups/<surface>/`. If the agent has no image-generation
   tool, write the finished prompt to `design/mockups/<surface>/PROMPT.md`
   and hand it to Oliver to run; he returns the images.
4. **Oliver reviews** — he picks a variant and annotates changes. Do not
   implement before this sign-off.
5. **Implementation spec** — translate the chosen mock into
   `design/specs/<surface>.md`: layout regions and grid, spacing/type
   scale, component inventory, and a mock-color → `--pc-*` token mapping.
   If the mock implies palette changes, change the *tokens* so the whole
   app stays coherent — never fork one surface's colors inline.
6. **Implement and compare** — build it, then screenshot the result next
   to the mock and note deviations in the spec file. Engine truth always
   wins over mock fiction: image models invent plausible numbers and
   typos; layout and hierarchy are the signal, pixel text is not.

Keep `design/` in git. It is the record of what Oliver approved.

### Phase 0 (small, recommended first): pilot the loop on the Calculator

The Calculator works but Oliver dislikes its look — and it is the
smallest real surface, so use it to calibrate the design loop before
Phase 1 depends on it. Scope: restyle/re-arrange only — behavior,
engine calls, and tests must not change (`pc-calculator.ts` render
methods and `app.css` only). Deliverables: brief, mocks, Oliver's pick,
implementation, before/after screenshots. If Oliver would rather not
block Phase 1 on this, he can defer it — ask him when presenting mocks.

## Phase 1 — Solve in the workspace (solve → Strategy Board) — complete

**Goal.** A user authors a goal, clicks Solve, and gets the optimal
strategy opened as a Strategy Board document, nodes annotated with
expected remaining cost, plus a one-click verification run.

**Everything below the UI already works.** The exact call sequence is the
last test in `apps/web/test/engine-smoke.test.ts` ("solver runs in the
browser runtime"): `openSolver` → `solverSolve(solver, item, economy)` →
`solverCompileStrategy(solver)` → `compileStrategy(session, json)` →
`createSimulator` → `runStrategy`, with empirical mean cost matching
`solve.start_value`. `EngineClient` (apps/web/src/app/engine-client.ts)
has typed methods for all of it; `loadEconomy({version:"v1", prices})`
takes the price map from `apps/web/src/app/workspace/prices.ts`.

**Placement.** Recommended: a "Solve" section in the Calculator tab — it
already owns the goal editor, the item, the solver handle, and the price
table. [crafting-solver-plan.md](crafting-solver-plan.md) §Workspace
Integration frames this as Simulator integration instead; put the
placement question in the Phase 1 design brief and let Oliver decide on
the mock. Design the solve panel (price checklist, solve button,
progress, result summary, verify affordance) through the image-model
loop.

**Implementation steps.**

1. Price readiness UI: candidate cost keys come from
   `solverActions(solver, {omitFossilCombos:true})` (already fetched once
   per session in `pc-calculator.ts`). List keys with no price in
   `prices.ts` and offer inline inputs. Unpriced actions are *excluded*
   from the solve — `SolveSummary.skipped_actions` reports how many;
   surface that number prominently after solving.
2. Solve: `solverSolve(solver, item, economy, {epsilon?, max_states?,
   max_sweeps?})` (engine defaults 1e-9 / 100k / 100k; expose as an
   "advanced" disclosure). Until Phase 2 lands this call blocks the
   worker — run it via the existing `guard()` busy pattern and say so in
   the UI ("solving — may take a while on big goals").
3. Result summary: `converged`, `start_value` (headline: expected cost),
   `expanded_states`, `sweeps`, `residual`, `skipped_actions`.
4. Open in board: `solverCompileStrategy(solver)` returns ordinary
   strategy JSON (`isStrategyDocument` in
   `apps/web/src/app/strategy-model.ts` already accepts it — verified).
   Two integration gaps to close:
   - **Positions**: compiled nodes have no `position`. The strategy
     editor already has a BFS auto-layout (`pc-strategy-editor.ts`,
     `autoLayout` around line 1140). On import, detect missing positions
     and run that layout. Compiled policies are a hub (master router)
     with many prioritized edges — if the BFS grid is unreadable, a
     router-as-hub column layout is acceptable follow-up polish.
   - **Annotation display**: compiled operation nodes carry
     `expected_cost` (V(s), emitted by `engine/src/solver_compile.cpp`).
     Add `expected_cost?: number` to `StrategyNode` and render a badge on
     `pc-strategy-node` when present ("~6.9c to go"). It must survive
     save/load untouched.
   - Open via `workspace().openStrategy(compiled, "copy")`.
5. Verification run (the end-to-end gate as a button): compile + simulate
   5,000 runs with the same economy (`runStrategy` with progress +
   cancel already works), then show `known_total_cost / completed_runs`
   next to `start_value` with the delta. Tolerance expectation: within
   ~0.5 chaos at 5k runs on toy goals (see the smoke test).
6. Optional, cheap: "Download solve log" — `solverLog(solver)` returns
   JSONL (the ML corpus records); offer it as a file download.

**Vocabulary-gap failures are expected.** `pc_solver_compile_strategy`
returns `PC_RESULT_UNSUPPORTED_FEATURE` with a precise message when the
policy needs condition types that don't exist yet (tag-discriminating
junk layouts, flagged states, group slots with tier thresholds). Until
Phase 4, show that message verbatim with "this goal needs condition
types that are not implemented yet" framing. Choose the built-in
verification goals (and the web test's goal) from family-slot goals that
compile today.

**Gate.** `npx tsc --noEmit` and `npm test` green in `apps/web`; a new
web test that drives solve → compile → auto-layout → validate through
the worker (assert positions were assigned and `expected_cost` present
on operation nodes); manual preview verification of the full flow with
screenshots.

**Completion record (2026-07-15).** `npx tsc --noEmit`, `npm test`, and
`npm run build` pass. The worker smoke now prepares the compiled policy through
the shared board layout and asserts finite positions, board validity, and
operation-node `expected_cost`. A separate headless Chrome flow exercised
ready → solve → 5,000-run verify → open Strategy Board with zero console
errors. Its compiler-safe fixture returned exact `5.4351c`, empirical
`5.4084c`, delta `-0.026712c`, and a `~5.4351c to go` node annotation. The
measured Solve surface was `375.265625..1440px`, exactly matching the Modifier
Pool left edge and Odds right edge. The UI also exercised a real compiler
vocabulary refusal before the success fixture and preserved its native detail.

## Phase 2 — Chunked solve with progress and cancel — complete

**Goal.** Long solves report progress and honor cancellation instead of
blocking the worker; UI from Phase 1 gets a live progress readout
(states expanded, residual, current V(start) bound) and a Cancel button.

**This is an engine + C ABI + WASM change.** Pattern to mirror: the
simulator's chunked run — `pc_simulator_run_chunk`-style stepping driven
by the worker loop in `apps/web/src/app/engine-worker.ts` (`runStrategy`:
bounded chunks, `yieldToEventLoop()` between chunks, a `cancelled` set
checked per chunk, `progress` messages posted to the client;
`EngineClient.runStrategy` already exposes `onProgress` + `AbortSignal`).

**Implementation steps.**

1. C ABI (`engine/include/poecraft/solver.h`, `engine/src/solver_api.cpp`):
   add an explicit stepped solve alongside the existing synchronous
   `pc_solver_solve` (keep that; native tests and Python use it):
   - `pc_solver_solve_begin(solver, start_item, economy, options)` —
     validates, snapshots prices, resets solve state;
   - `pc_solver_solve_step(solver, budget, out_progress)` — does up to
     `budget` units of work (states expanded during the expansion phase,
     sweeps during iteration) and reports a progress struct: phase
     (expanding / iterating / done), expanded_states, sweeps, residual,
     current start-value bound;
   - `pc_solver_solve_finish(solver, out_summary)` — policy extraction +
     the same `pc_solve_summary` as the sync path;
   - `pc_solver_solve_abandon(solver)` — discard partial state.
   Follow the existing struct_size/abi_version conventions in the header.
   The internal solve loop in `engine/src/solver_solve.cpp` already has
   the two phases; restructure it so both the sync entry point and the
   stepped one drive the same code.
2. Engine test (`engine/tests/`): chunked solve with assorted budgets
   produces *identical* summary, values, and policy to the synchronous
   solve on the synthetic session and the Vaal Regalia toy goal
   (determinism is already a solver invariant — tie-breaks are
   deterministic). Remember new engine test files must be registered in
   three places: engine CMakeLists, `scripts/build.ps1` TestSources, and
   `tests.hpp`/`test_main.cpp`.
3. WASM facade (`bindings/wasm/wasm_api.cpp`): `pcw_solver_solve_begin/
   step/finish/abandon` following the existing `pcw_solver_*` JSON
   conventions. **Rebuild WASM** (`scripts/build-wasm.ps1`, self-activates
   emsdk from `C:\emsdk`) and re-run web tests after.
4. Worker: reimplement the `solverSolve` method as the chunked loop
   (begin → step until done → finish), yielding between steps, honoring
   the `cancelled` set (cancel → abandon → respond `{cancelled:true}`-
   style result — mirror how `runStrategy` reports cancellation), and
   posting progress. The protocol's `ProgressMessage` is `{done,total}`;
   totals are unknown for solves, so extend the message with optional
   fields (e.g. `phase`, `residual`, `bound`) — the protocol is
   project-internal (`engine-protocol.ts`), just keep `EngineClient`
   backward-compatible for `runStrategy`.
5. Client: `EngineClient.solverSolve` gains `{onProgress, signal}` like
   `runStrategy`. Phase 1's solve panel switches to live progress +
   Cancel.

**Gate.** Engine suite green (chunked==sync equivalence test); web test:
a solve on a multi-slot goal reports ≥2 progress events and cancels
promptly via AbortSignal (mirror the two existing cancellation tests);
full `scripts/test.ps1` once before commit since this touches engine,
bindings, and web.

**Completion record (2026-07-15).** `SolveWork` now advances one reachable
state per expansion unit and one deterministic Bellman sweep per iteration
unit; synchronous `pc_solver_solve` drives that same work to completion. The
new begin/step/finish/abandon C ABI and WASM facade preserve summary, values,
and policy exactly across assorted budgets, including the synthetic and Vaal
Regalia fixtures. The worker adapts expansion chunks, yields between every
iteration sweep, posts `SolveProgress`, and abandons native partial state on
AbortSignal. Calculator's approved lower panel replaces Start with a live
Cancel action only while solving and renders states, sweeps, residual, and the
current V(start) bound; cancelled solves return to a restartable minimal idle
state. Worker tests cover multi-slot progress plus prompt cancellation.
Separate headless Chrome exercised progress -> cancel -> restart -> converged
solve with zero console errors; its progress capture showed `109` states,
`5,450` sweeps, residual `2.47e-2`, and bound `733.2208c`. The measured outer
Solve surface still matched Modifier Pool left and Odds right exactly. Native,
WASM, web, build, and full repository gates passed.

## Phase 3 — Emulator ambient odds ("odds before you click") — skipped

**Final status:** skipped entirely by Oliver on 2026-07-15. The material below
is retained only as historical planning context; it is not scheduled work.

**Goal.** Per [desktop-workspace-ui.md](desktop-workspace-ui.md)
§"Watched Modifiers And Action Odds": the Emulator gets a watched-
modifier tray (families + tier thresholds); once non-empty, craft
controls show the chance that action hits the watched mods on the
current item, with a hover expansion showing the full outcome breakdown.

**Mechanism.** Exactly the Calculator's math: open a solver on the
emulator's session with goal slots = watched mods (goal `rarity:"rare"`
default — confirm with Oliver), then `solverCalc(solver, emulatorItem,
actionId)` per craft control. Distributions are cached per abstract
state inside the engine, so re-hover and repeat clicks are cheap; the
cache invalidates naturally because the item's abstract state changes.

**Implementation steps.**

1. Extract shared pieces first (no behavior change):
   - the craft-control → registry-action-id mapping
     (`derivedActionId`/`fossilComboId` logic in `pc-calculator.ts`) into
     `apps/web/src/app/craft-choices.ts` so Emulator and Calculator use
     one table;
   - `formatProbability` and the goal-coverage/miss-signal/cost renderers into
     a shared module (or a small
     `pc-odds-panel` element) so the hover panel and the Calculator
     results render identically.
2. Emulator state: add `watched: {familyModKey, minTier}[]` to
   `DraftRecord` (`workspace/persistence.ts`, optional field — old
   drafts must load unchanged). Manage a solver handle in `pc-emulator`
   (open when watched is non-empty; reopen on session rebuild or watched
   change; close in `disposeEngine`).
3. Watch gesture + tray + badges + hover panel go through the design
   loop (Phase 3 brief). Notes for the brief: the pool already uses
   click (craft) and right-click (fracture), so watching needs a third
   affordance (per-family star, or a tray fed by `pc-modifier-picker`);
   badges must not make the craft bar unreadable; the hover panel is
   "the Calculator rendered in place" per the design doc.
4. Badge computation: after each item change, compute for the *active*
   craft panel's controls only (sequential worker keeps this sane;
   guard against overlapping refreshes with the existing `busy` pattern).
   Badge value is the engine-returned `success_probability` for the watched
   goal's configured `min_satisfied_slots`, with per-mod odds in the hover
   panel. Do not recompute the combined predicate in TypeScript.
5. Illegal/unsupported actions show a muted badge ("—"), not an error.

**Gate.** Web tests green plus a new smoke test that iterates the shared
action-id table against a full-registry solver (every basic id plus one
representative parameterized id per mechanic resolves via `solverCalc`
without "unknown action") — this pins the UI↔registry id contract that
both Emulator badges and the Calculator depend on. Manual preview pass:
watch two mods, verify badges change after crafting, hover panel matches
the Calculator's numbers for the same action.

## Phase 4 — Veiled/eldritch evaluators + missing condition types — complete

Engine work; no UI beyond error-message removal. Two independent halves.

**4a. Calc evaluators** (`engine/src/solver_calc.cpp` `evaluate()`; the
unsupported list today: `veiled_chaos`, `veiled_exalt`, `unveil`, and the
eldritch actions).

- Ground truth is the engine's own action implementations (veiled and
  eldritch mechanics landed in engine Phase 13) — read those before
  writing evaluators, and gate every evaluator S3-style: exact DP vs
  engine Monte Carlo histograms in `engine/tests/test_solver_calc.cpp`,
  on both the synthetic 8-mod session and the Vaal Regalia fixture
  (eldritch-eligible: body armour). MC gate conventions: 20–50k samples,
  per-outcome 5σ + small slack, coverage ≥ 99.5% for reforges.
- `veiled_exalt` (single-slot) and `veiled_chaos` (reforge + guaranteed
  veiled slot) set `kFlagVeiledMod`; the abstract state does not track
  *which* veiled mod, so the evaluators mostly move affix counts and the
  flag. `unveil` is the bespoke one — the registry deliberately models it
  as one Special descriptor meaning "unveil, choosing the option the
  policy prefers" (see `solver_registry.cpp`), so its evaluator has to
  enumerate the option distribution and the solve must take the
  min-cost choice per outcome. Read the engine's unveil option rules
  first; ask Oliver where behavior is ambiguous.
- Eldritch ember/ichor are tiered implicit setters (flag + tier in
  legality); eldritch exalt/chaos/annul mirror the existing exalt/chaos/
  annul evaluators over the eldritch-modified pools. Check what
  `solver_abstract.cpp` tracks for eldritch before assuming.

**4b. Condition vocabulary** — close the compiler gaps so more policies
compile. `engine/src/solver_compile.cpp` `gap()` sites name exactly four:
tag-discriminating junk layouts, flagged states, group slots with tier
thresholds, ambiguous signatures. For each, the compiler knows precisely
which predicate it needs — derive the condition semantics from there
rather than inventing them. Likely additions: a junk-count-per-side (and
per-junk-class) condition, an item-flag condition (metamod locks, veiled,
influenced, eldritch), and a `min_tier` extension for `has_mod_group`.

- New condition types touch: `simulator.cpp` (`compile_condition` +
  evaluation), `strategy-model.ts` (`LEAF_CONDITION_TYPES` + the
  condition editor if user-authorable — flag conditions probably should
  be; junk-class conditions can stay compiler-only/advanced),
  [strategy-editor-ui.md](strategy-editor-ui.md) (the vocabulary is a
  documented spec), and — because strategy vocabulary changed — a **WASM
  rebuild** plus web tests.
- Gate: goals that previously threw vocabulary-gap errors now compile
  and pass the simulate-vs-V(start) end-to-end check in
  `engine/tests/test_solver_compile.cpp`. The S5 spec's outstanding
  target is the right headline test: **one all-T1 "perfect item" goal**
  solving, compiling, and verifying. Run the full `scripts/test.ps1`
  before the commit.

**Completion record (2026-07-15).** The special evaluators are exact against
the engine implementations. Abstract states now preserve the veiled side and
both eldritch tiers; unveil expands weighted three-option offers and Bellman
selection minimizes over the options actually offered. Compiler output uses
`mod_count`, `item_flag`, exact `influence_bits`, ranged `eldritch_tier`,
`has_unveil_option`, and `has_mod_group.min_tier`; the latter two plus exact
junk-class member keys close ambiguous and tag-discriminating policy routes.
Flag/tier conditions are visually authorable while solver-only predicates stay
valid advanced JSON. The synthetic and Vaal Regalia evaluator matrices each
use 20k engine samples. A dedicated tag-discriminating policy completes 20k
runs without off-policy routing, and compiled all-T1 and unveil policies both
complete 30k simulation runs with empirical cost matching `V(start)`. WASM was
rebuilt, web type/tests/build pass, and `scripts/test.ps1` passes end to end.

## Cross-phase conventions

- Layer test commands: web = `npx tsc --noEmit` + `npm test` in
  `apps/web`; engine = build then
  `build/engine/poecraft_engine_tests.exe data/compiled/current fixtures/spec`;
  full pipeline = `powershell -File scripts/test.ps1` (slow — use when a
  phase touched engine or bindings).
- WASM facade doubles serialize via `std::to_string` (6 decimals):
  probability sums over many outcome classes land at 1 ± 1e-3, and
  reforge evaluators have a ≥99.5% coverage budget — write web
  assertions accordingly.
- Dockview detaches inactive panels' DOM; components that rebuild DOM in
  `connectedCallback` must restore from retained state (see
  `pc-mod-pool`), and tests must activate panels via
  `panel.api.setActive()`.
- Commits: one per milestone, local-only unless Oliver says push, agent
  co-author line at the end. Rewrite HANDOFF.md at each phase boundary.
