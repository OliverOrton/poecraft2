# Solver Improvement Plan — Performance Restoration + Action-Space Redesign

Prepared 2026-07-19 from two read-only audits of the solver (performance
structure, then action-space trimming). Reviewed by Oliver; his per-finding
decisions are embedded below and are authoritative. This document is for
Codex review and implementation planning. No code was changed while producing
it.

Standing rules that bind every item:

- PoE mechanic questions are decided by Oliver directly — never researched
  online or inferred.
- Resource-cap values do not change anywhere in this plan. Caps are enforced
  via the incremental byte ledger, never weakened.
- `docs/*.md` are load-bearing design specs; doc edits ship with the change
  that makes them true, and go through Oliver.

---

## Status and sequencing — updated 2026-07-19 (evening)

This table is the authoritative order *and* the current position. Update the
Status column as work lands.

| Order | Items | Status |
|---|---|---|
| 1 | A1 byte-audit repair | **Done** — `d1e928e` "Use incremental selected-byte accounting". Verified: the previously blocked 4,096-state diagnostic now completes expansion in 11.55 s |
| 2 | A3 WASM step cap | Not started — still the cheap, independent win |
| 3 | A2 admission context | **In progress** — Codex's current R3A-closure work (protected-side admission boundary, `3d2cb8e`) is this item under the active plan's naming |
| 4 | B3, B4, B5, B8, A7 | Not started — small hash-identical / fail-fast / diagnostics fixes |
| 5 | A4, A6, A8 | Not started — envelope gating + telemetry level/classification |
| 6 | B2 fracture discovery | Not started — build the pinned fracture case first |
| 7 | B1 veiled/eldritch, B9 start-item semantics | Not started — both need Oliver rulings |
| 8 | B6, B7, A5 | Not started — measured investigations; **B7 is the item that addresses the live capacity failure below** |

### The live capacity failure this plan orbits

The motivating failure, observed in the Calculator on the Mirage craft (the
pinned `conquest-lamellar-mirage-r3f-product` gate case): the solve hits
`max_discovered_states` (100k discovered, 0 sweeps), refuses before Bellman
entry, and no strategy is compiled — so nothing can be verified or loaded
into the Simulator. Be precise about what addresses it:

- **Part A makes each state cheaper; it does not reduce the discovered state
  count.** After all of Part A this goal still refuses — just in seconds
  instead of minutes.
- **B7 (compact outer partition) is the only item in this plan that shrinks
  the state space itself.**
- The remaining lever is raising `max_discovered_states`, which this plan
  deliberately does not do (no-cap-changes guardrail). That is an Oliver
  decision owned by the active B1/S8 plan's normal-cap gate, made affordable
  by Part A.
- Until then, the only user-side lever is goal shape: one fewer goal slot or
  a relaxed tier threshold is what actually moves discovered-state count.

---

## Answers to open questions

### Q1 — strict junk partition: would switching affect correctness?

Switching the outer DP back to the compact partition does **not** break the
algorithm, the caps, or the verification machinery. What it does is
reintroduce the *documented* approximate-soundness drift, specifically where
mechanics act on existing junk identity: a fractured/crafted junk mod's group
footprint determines later reforge pools, and compact classes merge mods with
different footprints, so `materialize` may pick a representative whose group
differs from the true carrier. Strict mode is force-enabled whenever
Fracture / Unveil / HarvestResist / RemoveCraftedModifiers are in the layout
(`engine/src/solver_calc.cpp:322`) precisely to make those evaluators exact.

The trade is: compact = smaller state space, but V(start) and the S8
preservation/accounting claims downgrade from "exact up to the partition" to
"approximate, empirically gated by the simulate-vs-V(start) check." Whether
that trade is acceptable is a measurement question — item B7 is therefore
*measure first, Oliver decides*, not a switch.

### Disposition of the action-space audit findings (Oliver's review)

| # | Finding | Decision |
|---|---|---|
| 1 | Strict junk partition forced on all API solves | Measure, then decide (B7) |
| 2 | Bench catalog retained in registry (automatic mode) | **Deferred** — mostly neutralized once A2 removes the per-state registry copy; revisit after A2 |
| 3 | Per-solve re-pricing scan of accumulated operators | Fix (B3) |
| 4 | Illegal operators burning full work items | Fix (B4) |
| 5 | Essence relevance excludes essence-as-cheap-reroll | **Declined** — no action, keep current prune |
| 6 | Veiled/eldritch unconditional prune with over-broad justification | **Flagship** item (B1) |
| 7 | No post-prune goal-producibility check | Fix, low priority (B5) |
| 8 | Fossil beam price-blind and final | Investigate (B6) |
| 9 | Fracture prepare authored-only / single-carrier | Directive: fracture routes discovered by the solver, not forced (B2) |

---

## Part A — Performance restoration

### A1. Finish the owned-byte audit repair (in flight — completeness checklist)

The incremental ledger (`fast_estimated_owned_bytes`,
`engine/src/solver_calc.cpp:422`) exists; the repair is done only when the
deep walker (`engine/src/solver_calc.cpp:1052`) is out of every loop. Per
HANDOFF.md's reconciled timers, deep audits are 76–84% of expansion time and
grow ~4x when the carrier bound doubles. Call sites that must all be
converted or removed:

1. `prepare_state_expansion` — the direct call at
   `engine/src/solver_solve.cpp:1266` **plus** the second deep audit hidden
   inside `Impl::estimated_owned_bytes()` on the next line
   (`engine/src/solver_solve.cpp:5318` calls `calc.estimated_owned_bytes()`
   again). Two deep walks per expanded state today.
2. The state-local admission context's constructor ledger init —
   `initialize_owned_bytes_ledger()` (`engine/src/solver_calc.cpp:350`) runs
   a full deep audit per expanded state (it is the ~third audit call per
   carrier that hides inside the admission timer). Give fresh contexts a
   computed-constant base instead, or skip reconciliation for them.
3. The every-64-states `check_solver_byte_cap()`
   (`engine/src/solver_solve.cpp:3022`).
4. Extraction's two **full-variant** cap checks in the unveil branches
   (`engine/src/solver_solve.cpp:5064`, `engine/src/solver_solve.cpp:5100`)
   — convert to `check_solver_byte_cap_from` with the precomputed base, the
   same batching that already fixed the variance-scratch path. Left as-is,
   unveil-heavy endgame policies partially reintroduce the 285-second
   extraction pathology.
5. The per-automatic-row `transition_cache->estimated_owned_bytes()`
   before/after pairs (`engine/src/solver_solve.cpp:2884`,
   `engine/src/solver_solve.cpp:2916`) — replace with an incremental delta
   computed by the append path.
6. Make the always-on ledger reconciliation
   (`engine/src/solver_calc.cpp:1277`, which re-runs the ledger inside every
   deep audit and throws on undercount) debug/test-only; production keeps
   ledger-only accounting.

Acceptance:

- `owned_byte_audit_ns ≈ 0` in expansion/extraction telemetry on the pinned
  1,024/2,048 s8.4r cases; per-state expansion time flat when the state
  bound doubles.
- The byte cap still refuses at the same thresholds — add a test that
  inflates a cache and confirms `max_solver_owned_bytes` still fires.
- The blocked 4,096 / normal-cap `conquest-lamellar-mirage-r3f-product` run
  proceeds. No cap values change.

### A2. Stop rebuilding the world per expanded state in automatic admission

`admit_state_local_automatic_candidates` constructs a complete `CalcContext`
per expanded state (`engine/src/solver_options.cpp:2141`). Each construction
pays:

- a deep copy of the entire `ActionRegistry` — the constructor takes it by
  value (`engine/src/solver_internal.hpp:920`): hundreds of descriptors with
  id/display/cost-key strings plus the string-keyed `index_by_id` map,
  allocated and freed per state;
- a full `build_abstract_layout` with the strict exclusion-effect partition —
  a session-width bitset is allocated per reachable junk mod and used inside
  a `std::map` key (`engine/src/solver_abstract.cpp:367`), thousands of
  transient allocations per state;
- `build_planner_operators`, plus the ledger-init deep audit (A1 item 2);
- when imprint is legal and priced, `discover_automatic_imprint_options`
  runs up to `max_imprint_program_work` (default 256) program evaluations
  per state (`engine/src/solver_options.cpp:913`).

Measured: admission was 1.54 s at 1,024 states growing to 5.25 s at 2,048 —
superlinear even excluding audit time.

Change: share the immutable parts. Registry behind
`shared_ptr<const ActionRegistry>` (or a non-owning view for local
contexts); cache the strict local layout once per parent context — it does
not depend on the carrier; only the synthesized fixed-option *specs* differ
per state, and those do not feed layout derivation unless their dependency
actions change, which is a small precomputable set. Pool/reset one reusable
local context instead of construct/destroy. Keep per-state state tables
isolated.

Acceptance: `expansion_prepare_admission_ns` per state flat as discovered
states grow; identical `transition_bits_hash` / `policy_bits_hash` on all
pinned s8.4r cases; admission decisions byte-identical in the evidence JSONs.

### A3. Remove the WASM worker's 4-item step cap

`apps/web/src/app/engine-worker.ts:332` clamps `workItems` to
`Math.min(4, ...)` while one expansion work item is a single
(state, operator) row (`engine/src/solver_solve.cpp:2593`). Every ≤4 rows
the browser pays a JS↔WASM crossing, a C++ JSON progress serialization
(`bindings/wasm/wasm_api.cpp:2404`), a JS JSON parse, and a macrotask yield
every ~8 ms of accumulated step time (`yieldToTimerTask` is subject to
setTimeout clamping). The adaptive controller targets a 12 ms slice but the
cap prevents it from ever getting there. Native `solve()` steps with 4096.

Change: let the controller reach its target (e.g., cap at 4096, scale by
measured ms); keep the phase-change reset to 1 item, the ~100 ms progress
cadence, and the cancellation yields.

Acceptance: on a pinned browser case, step count down by >10x and wall time
materially down with an identical solve summary; cancellation latency stays
< ~50 ms (the 8 ms yield rule is retained).

### A4. Gate the per-row preservation envelope work

`append_sparse_row` computes `carrier_effect` / `carrier_successor_envelope`
for every new row (`engine/src/solver_solve.cpp:2350`), but only rows whose
action has `preservation.destructive_renewal == true` ever read the result
(`engine/src/solver_solve.cpp:1572`) — a handful of actions. The envelope
loop also calls `carrier_facts` per unique successor, which unconditionally
computes `abstract_state_hash` (`engine/src/solver_solve.cpp:274`) that the
envelope never reads — for reforge rows that is thousands of discarded
full-state hashes per row. Some shared-kernel paths build the envelope twice
(`engine/src/solver_solve.cpp:2375` vs `engine/src/solver_solve.cpp:2490`).

Change: compute the envelope only for destructive-renewal rows (or when
`preservation_control` demands it for witnesses); drop the unconditional
hash from `carrier_facts` and compute it only at the witness/JSON sites that
use it; deduplicate the double envelope construction.

Acceptance: identical pruning decisions and witnesses on pinned cases;
`expansion_sparse_row_ns` drops on reforge-heavy cases.

### A5. Focused-expansion round cost (measured, conservative)

Each fringe round re-runs full policy iteration over all discovered states:
4 Gauss–Seidel seed sweeps (`engine/src/solver_solve.cpp:4415`), the
kernel-quotient preparation (per-state `full_kernel` edge vectors, hashing,
Tarjan, SCC solves) rebuilt from scratch (`engine/src/solver_solve.cpp:3541`),
and the O(state-count) `probability_by_state` kernel caches reallocated
(`engine/src/solver_solve.cpp:3338`). In focused mode successors are not
enqueued during row append (`engine/src/solver_solve.cpp:2573`), so the graph
advances roughly one policy layer per round — deep crafts mean many rounds.

Change: first add `focused_expansion_rounds` and per-round optimization time
to the tracked budget metrics. If rounds are large on real cases: reuse
allocated buffers across rounds, warm-start policy rows alongside the
already-warm values, and consider fewer seed passes when the previous round
converged cleanly. Do not change the lower-bound semantics or the closure
proof.

Acceptance: identical final values/policies (hash equality); round count
unchanged; per-round setup cost drops.

### A6. Telemetry timer density

`expand_one_unit` takes ~8–12 `steady_clock::now()` samples per
(state, operator) unit; in WASM each goes through
`emscripten_get_now()` / JS `performance.now()` and is markedly more
expensive. Keep the coarse phase timers (they found the audit problem);
sample the per-row fine timers (e.g., 1-in-64 rows, scaled up) or gate them
behind a `SolveOptions` telemetry level defaulting to coarse in product/WASM
builds.

Acceptance: phase attribution still reconciles to within a few percent on
native evidence runs; WASM expansion throughput improves.

### A7. Small cleanups (bundle into one hash-identical PR)

- Delete dead `operator_q` (`engine/src/solver_solve.cpp:3252`). Nothing
  calls it, and if revived it would silently rebuild released distributions
  through `calc.outcomes()`.
- `abstract_state_hash`: hash the stored (index, value) entries of the four
  `CompactCountVector`s directly instead of element-wise `get()` scans
  (`engine/src/solver_abstract.cpp:526`); each `get()` is a linear scan, so
  today's hash costs ~5–10x a dense scan. Equality already compares entries.
- `CalcContext::evaluate`: replace the `std::map<uint32_t,double>`
  successor accumulator with sort-and-merge on a flat vector
  (`engine/src/solver_calc.cpp:1298`).
- `reforge_cache_`: `std::map` → `unordered_map`
  (`engine/src/solver_internal.hpp:1095`).
- Reuse extraction's `random_values` scratch across rows
  (`engine/src/solver_solve.cpp:4993`); stop constructing discarded reason
  strings past the diagnostic cap in `finalize_preservation_diagnostics`
  (`engine/src/solver_solve.cpp:2195`).

Acceptance: hash-identical solves on pinned cases.

### A8. Telemetry keep/toggle/retire classification (companion to A6)

Audited classification of the telemetry surface (~370 counters, 76 clock
sites, ~1,200 serialization lines in `solver_solve.cpp`):

- **Functionally required — always-on, exact** (enforcement wearing
  telemetry clothes): the owned-bytes ledger and `peak_owned_bytes`;
  `reforge_frontier_work` (it *is* the `max_reforge_work` cap,
  `engine/src/solver_calc.cpp:990`); `state_action_rows` /
  `transition_entries` (cap-enforcement inputs in admission via
  `check_limits` / `merge_local_work`,
  `engine/src/solver_options.cpp:2198`); the cap flags
  (`resource_cap_hit` / `state_cap_hit` / `cap_hits`); progress fields; the
  skipped-action records (the web's incomplete-solve explainer reads them).
- **Keep always-on (cheap, contractual):** action-control summary, capped
  witnesses, structural counts, `transition_bits_hash` /
  `policy_bits_hash`, one-shot phase timers (`solve_setup_ns`,
  `layout_build_ns`, `registry_generation_ns`, `extraction_ns`), ledger
  health counters.
- **Toggleable (default off in product/WASM, on for evidence runs):** all
  per-row `_ns` timers, `AutomaticKindTelemetry` (22×8),
  `PrimitiveFamilyTelemetry` (8×8), per-carrier maxima
  (`automatic_carrier_work`), template-hit tracking, parent-context
  `telemetry_rows_` upkeep (keep the map only where it enforces caps — the
  admission-local contexts).
- **Retire once the current R-phase closes:** the four
  `expansion_*_byte_audit_ns` timers, `owned_byte_audit_requests/ns`, and
  the `shared_admission_ns` attribution machinery — no test, script, or web
  consumer asserts on any of them.

Mechanism: a `telemetry_level` on `SolveOptions` (minimal / coarse-default /
full-evidence) that controls *collection*, not just serialization; the
serializer omits sections and carries a schema-version key; coordinate in
one change with `apps/web/src/app/engine-protocol.ts`,
`scripts/compare-solver-benchmarks.py`, and
`scripts/package-s8-baseline.py`.

Acceptance: full level reproduces today's telemetry JSON on the pinned
cases; coarse level shows measurable expansion-throughput gain in WASM;
engine tests run at full level unchanged.

---

## Part B — Action-space work

### B1. Veiled/eldritch inclusion redesign (flagship)

Problem: product mode prunes all veiled and eldritch actions unconditionally
with a justification that is mechanically wrong for eldritch
exalt/chaos/annul — they add/reroll ordinary explicit mods with dominance
side-targeting (`engine/src/solver_registry.cpp:166`). The real cost is
state width (implicit tier fields and extra actions widening the
abstraction). Consequences today: a start item carrying veiled mods or
eldritch implicits keeps that projected state but loses every action that
interacts with it; unveil-reachable routes are invisible; goals requiring
these mechanics fail silently by burning the full budget (see B5).

Change, smallest viable first:

1. Fix the comment/diagnostic so the prune is labeled a state-width scope
   decision, not an impossibility claim.
2. Goal-level mechanic scope opt-in: extend goal JSON (parsed in
   `engine/src/solver_api.cpp` `registry_build_options` / `parse_goal`) with
   e.g. `"mechanics": {"eldritch": "auto"|"include"|"exclude", "veiled":
   ...}`. `include` retains the family in `action_is_goal_relevant`
   (`engine/src/solver_registry.cpp:124`); default `auto` = current behavior
   initially. This respects the create-time constraint that the registry is
   built before the start item is known.
3. Start-item-aware `auto` (design gate with Oliver): at solve begin, if the
   start item projects veiled/eldritch state — or B5's producibility check
   says a slot needs those mechanics — and the family is excluded, refuse
   fast with a named diagnostic telling the caller to opt in; or, if Oliver
   prefers, auto-rebuild the registry with the family included. The
   state-width cost of inclusion (ember/ichor tier combinations, dominance
   routing) must be measured on a pinned eldritch case before `auto` ever
   silently includes.

Acceptance: an eldritch-start pinned case solves with
`"eldritch": "include"` and its compiled policy passes the end-to-end gate;
the same case under `exclude` fails fast with the named diagnostic instead
of burning the budget; zero state-count regression on existing non-eldritch
pinned cases (the opt-in is pay-for-what-you-use).

### B2. Fracture routes discovered by the solver, not authored (Oliver's directive)

Current state: primitive Fracture (R3F) is admitted per state when a
relevant goal slot is Satisfied and unfractured and the exact distribution
actually fractures one (`engine/src/solver_solve.cpp:2740`; mask assignment
`engine/src/solver_options.cpp:1124`). `FracturePrepare` macros exist only
through *authored* fixed options, each hard-bound to a single
`carrier_goal_slot` (`engine/src/solver_internal.hpp:174`); automatic
synthesis never generates them. The intended strategy — roll a cheap
carrier, prep one or several goal mods, fracture, restart on miss — is
reachable through ordinary expansion + primitive fracture but is never
packaged or discovered as an option.

Change:

1. Make discovery the contract. Product/automatic solves rely on primitive
   fracture + the ordinary DP; authored `FracturePrepare` remains supported
   for explicit documents but is no longer the intended path and is not
   extended.
2. Pin a regression case where fracture-early-multi-prep is optimal (cheap
   base, several priced goal mods, fracture priced so early fracture beats
   late): assert the solver *discovers* the route with **no** authored
   options — the policy at the multi-satisfied cheap state selects fracture,
   and V(start) matches a 10k-run simulation of the compiled policy.
3. Only if (2) reveals a tractability gap (the roll-until-carrier loop makes
   expansion/iteration too deep without a macro), add *automatic*
   FracturePrepare synthesis in `synthesize_automatic_options` — multi-slot
   exit predicates (mask, not single slot), admission-gated like the other
   automatic kinds, kernel-exact, template-shared. Follow-up gated on the
   measurement, not part of the first change.
4. Verify the focused-expansion interaction in the pinned case: the
   optimistic lower bound should pull expansion toward the cheap-carrier
   states; if the route's states are starved by caps, surface that as a bug
   rather than forcing options.

Acceptance: the pinned case from (2) passes; existing s8.4r fracture
evidence unchanged; the recorded owner note that per-state fracture_prepare
admission contradicts the intended strategy can be retired or rewritten
after this lands.

### B3. Stop re-pricing the accumulated operator population per solve

`SolveWork`'s constructor prices and scans every entry of
`calc.candidate_operators()` — including all state-locally admitted
operators from prior solves on a reused context
(`engine/src/solver_solve.cpp:1057`; R3A evidence shows 519+ and growing).

Change: cache per-operator pricing keyed by an economy fingerprint
(id/version hash of the price table); on construction, re-price only new
operators or on fingerprint change. Keep diagnostics reflecting this solve's
effective set rather than re-emitting per solve.

Acceptance: a second solve on a reused context shows setup time independent
of accumulated operator count; identical priced sets and diagnostics
content.

### B4. Legality pre-bucketing for the static operator loop

Structurally illegal operators (rarity/flag-gated transmute/alt/alch/regal/
aug and friends) currently burn a full `expand_one_unit` work item each —
including its ~10 clock samples — before `action_legal` says no
(`engine/src/solver_solve.cpp:2756`). Compounded by WASM step granularity
until A3 lands.

Change: once per solve, bucket `static_operator_indices` by
`LegalityPredicate` structure (rarity mask, required/forbidden flags,
open/removable-affix needs). In `prepare_state_expansion`, assemble
`expansion_operator_indices` from the buckets matching the state's
rarity/flags/counts. `action_legal` remains the final per-row authority —
the buckets are a conservative prefilter, never a semantic change.

Acceptance: hash-identical solves; work items per expanded state drop
(measure on a pinned rare-goal case where normal/magic-only currency is dead
weight).

### B5. Goal-producibility diagnostic (small; do alongside B1)

Nothing verifies after pruning that each goal slot is still producible by
the retained action set, so a mis-scoped goal (unveil-only family, influence
mod with the influence exalt pruned, etc.) expands to the state cap and then
fails to converge with no diagnostic naming the cause.

Change: after layout construction, check that each goal slot's satisfying
mask intersects the union of retained candidate+dependency actions'
reachable masks — `action_reachable_mask` already computes the pieces during
layout build (`engine/src/solver_abstract.cpp:317`). On failure, refuse at
create/solve start with a structured diagnostic naming the slot and, by
scanning the *pre-prune* registry, the mechanic family that could produce it
("satisfiable only by pruned family: veiled/unveil — see mechanics
opt-in").

Acceptance: an unveil-only goal fails in milliseconds with the named
diagnostic instead of expanding to the state cap; all existing pinned goals
pass the check untouched.

### B6. Fossil beam price-awareness (investigation)

The beam scores combos purely by probability shares at registry build time,
before prices exist (`engine/src/solver_registry.cpp:240`), with fixed
constants (beam 96, ~1 emission per slot + 1 aggregate per socket count);
non-emitted combos are deferred forever
(`engine/src/solver_registry.cpp:849`). Fossil descriptors carry no
discriminating tags (`engine/src/solver_registry.cpp:792`), so widening the
fossil action set costs per-state rows only, not layout width — a fix is
cheap by construction.

Investigate/implement: retain the top-N (e.g., 64) scored beam candidates
with their scores in the registry (as data, not actions); at solve start,
with prices known, select the ≤K emitted loadouts by a score-per-cost
criterion (exact criterion is a design point for review); everything else
stays deferred with the existing diagnostics. Measure on a fossil-relevant
pinned goal whether the priced selection ever differs from today's and what
it does to V(start).

Acceptance: no change when prices are uniform; a constructed case with a
cheap near-optimal combo shows the priced selection choosing it and
V(start) improving; row-count growth bounded by K.

### B7. Strict vs compact junk partition — measure, then Oliver decides

Per Q1 above: machinery correctness is unaffected; exactness claims and
drift are the trade. The hardcoded `true` at
`engine/src/solver_api.cpp:704` and the mechanic-forced OR at
`engine/src/solver_calc.cpp:322` make strict unconditional for product
solves (automatic mode always includes Fracture), while
`docs/solver/crafting-solver-plan.md:257` still says the strict partition is
evaluator-only. Junk classes are the multiplier between the action set and
the abstract state count, so this is the largest single state-width lever in
the action pipeline.

Change: add an internal experiment toggle (SolveOptions or a debug goal
flag — not product-exposed) that builds the outer layout compact while local
admission contexts stay strict. On the pinned cases record: junk-class
count, discovered states, wall time, and end-to-end drift (10k-run simulated
mean vs V(start)) under both modes. Bring the table to Oliver: if drift is
within tolerance on all pinned goals, switching the outer DP is a large
state-space win; if not, keep strict and fix the plan doc to match reality.
Either outcome ends with code and doc agreeing.

Acceptance: the comparison table exists in the s8.4r evidence folder; a
decision is recorded; the doc is updated accordingly (through Oliver).

### B8. Skipped-action counters mix per-state events with static candidates (diagnostics bug)

Observed live in the Calculator: "21,060 of 21 priced candidates skipped by
the native solver". The numerator is `skipped_action_count` =
`skipped_missing_price_count + skipped_unsupported_count`
(`engine/src/solver_api.cpp:659`), and those counters increment once per
expanded state — every carrier's admission pass re-records the same handful
of unpriced automatic candidates (`engine/src/solver_solve.cpp:1326`). The
denominator is the static priced-candidate count
(`apps/web/src/app/components/pc-calculator.ts:1804`). Two different units
in one sentence.

Change: dedupe skip records by candidate id engine-side (report unique
candidates; keep a separate events counter if useful), or relabel the UI
line as events. Deduping also removes the small per-state re-record cost.

Acceptance: the panel shows a number ≤ the candidate count with consistent
units; engine tests asserting the skip counters updated to the deduped
semantics.

### B9. Compiled strategies forget the solve's start item (owner decision)

`compile_policy_strategy_json` emits `base_state` with only `base_key` and
`item_level` (`engine/src/solver_compile.cpp:694`); the simulator's
`parse_start_item` therefore starts every run from a fresh normal base
(`engine/src/simulator.cpp:155`) — not from the item the solve started from.
A mid-craft solve's 5,000-run verification and its Strategy Board runs both
simulate a different question than the solve answered; verification only
matches V(start) when the policy's restart region happens to cover the
fresh base.

Decision for Oliver: should the compiled document carry the concrete start
item (rarity/mods/flags in `base_state` — the simulator already parses all
of these), so simulation starts where the solve started? Or is
fresh-base-start the intended semantics — in which case the verification
delta should compare against V(fresh base) and the UI should say so?

Acceptance (either ruling): verification compares like against like on a
mid-craft-start pinned case; docs state the chosen semantics.

---

## Declined / deferred

Sequencing lives in the Status table at the top of this document.

Recorded so they are not re-litigated:

- Essence-as-reroll relevance widening — **declined** by Oliver.
- Bench-catalog registry retention in automatic mode — **deferred** until
  after A2, which removes the per-state registry copy that made it
  expensive.

## Guardrails

- Every hash-identical item (A2, A4, A5, A7, B4) must show equal
  `transition_bits_hash` and `policy_bits_hash` on the pinned s8.4r cases
  before/after.
- Every action-set-changing item (B1, B2, B6, B7) must pass the end-to-end
  gate (solve → compile → simulate; empirical mean within tolerance of
  V(start)) on its pinned case.
- Budget metrics tracked per pinned case in the evidence JSONs: deep-audit
  call count, admission ns/state, focused rounds, operators per carrier,
  expansion ns/state, peak/live selected bytes. Superlinear growth in any of
  them when the state bound doubles is the regression alarm.
- No resource-cap values change anywhere in this plan; caps are enforced via
  the ledger, never weakened.
- PoE mechanic questions surfacing during B1/B2/B6 go to Oliver directly.
