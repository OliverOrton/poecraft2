# Post-B6 Reconnaissance: Oracle Regression Hypotheses And Docs Drift

**Status: read-only reconnaissance, 2026-07-22. No source was changed, no
build or solve was run, and no measurement in this report is new.** Every
number is read from the two pinned reports named below or from source at
`edc03a6`. Nothing here selects work or authorizes a fix.

Parent: [Active work](README.md)

Scope: (1) hypotheses for the open two-natural-T1 oracle regression recorded in
[HANDOFF](../../../HANDOFF.md), with a proposed measurement plan for the deferred
profiling chunk; (2) a drift survey of live documentation against post-B6 code.
The [mechanical solver split](../2026-07-22-mechanical-solver-split/plan.md) remains the only selected
boundary and this report does not interleave with it.

Sources compared throughout:

- new — `build/diagnostics/b1-two-t1-oracle-1800.json` (B1 oracle, retained as
  the B6 evidence, report SHA-256 `9501b055…`);
- old — `fixtures/solver-scaling/v1/evidence/q5-two-t1-product.json`
  (`7b11b34`-era timing report, tracked and never overwritten).

---

## Part 1 — Regression Reconnaissance

### 1.1 The regression is pure cost, not a changed problem

Both runs solve the same case to the same answer:

| Fact | New | `7b11b34` |
|---|---|---|
| `value.start` | `230.26738656962243` | `230.26738656962243` |
| `solve_summary.expanded_states` | 57,182 | 57,233 |
| `compiled_graph.nodes` / `edges` | 6,391 / 9,607 | 6,391 / 9,607 |
| `phase_wall_ms.solve` | 1,083,087.349 | 50,712.495 |

Search work per unit of answer actually **improved**: `work.bellman_backups`
457,864 → 228,728 (0.50x), `work.bellman_action_evaluations` 7,231,480 →
2,952,556 (0.41x), `work.state_action_rows` 903,935 → 738,139 (0.82x),
`states.discovered` 189,946 → 116,041 (0.61x), `skipped_action_count` 28,244 →
154. The pruning milestone did what it claimed. The 21.36x is spent somewhere
else entirely.

### 1.2 The three HANDOFF-ranked phases are not three regressions

`HANDOFF.md` ranks the profiling order as focused optimization (+812,444.694
ms), constructive policy (121,995.350 ms, new phase), and strict clean-goal
cover (13,221.741 ms, new phase). All three are charged **once per focused
round**, in the same function:

- [`solver_solve.cpp:11650`](../../../engine/src/solver_solve_focused.cpp:598) —
  `focused_expansion_ns += optimization_ns`
- [`solver_solve.cpp:11652-11660`](../../../engine/src/solver_solve_focused.cpp:603) —
  `constructive_policy_ns +=` around `acquire_focused_fallback()`
- [`solver_solve.cpp:11661-11669`](../../../engine/src/solver_solve_focused.cpp:612) —
  `strict_clean_goal_cover_ns +=` around `prepare_strict_clean_goal_cover()`

`optimization_ns` is zeroed each round by `reset_focused_optimization_state()`
([`solver_solve.cpp:11470`](../../../engine/src/solver_solve_focused.cpp:424)), so
`focused_expansion.duration_ns` is a genuine cumulative sum of per-round
optimization time, not a double-counting artifact. Per-round cost:

| Phase | New total | ÷ 326 rounds | Old total | ÷ 22 rounds |
|---|---:|---:|---:|---:|
| focused optimization | 841,222 ms | 2,580 ms | 28,778 ms | 1,308 ms |
| constructive policy | 121,995 ms | 374 ms | — | — |
| strict clean-goal cover | 13,222 ms | 41 ms | — | — |

Per-round focused optimization grew only 1.97x. **The round count grew 14.8x.**
That is the shared multiplier behind all three ranked items, and it is the
thing worth measuring first.

### H1 — Primary: the focused expansion batch was capped, multiplying rounds 14.8x

**Confidence: high. Arithmetic closes to within 0.2%.**

At `7b11b34` a focused round had no global batch cap. The fringe was filtered
only per coarse class, at `kStrictMembersPerFringeClass = 4096`
(`git show 7b11b34:engine/src/solver_solve.cpp`, `schedule_next_focused_expansion`).
Current code enforces a hard global cap and a 64x tighter per-class cap:

- [`solver_internal.hpp:1581-1583`](../../../engine/src/solver_internal.hpp:1581) —
  `focused_members_per_fringe_class = 64`,
  `focused_expansion_batch_states = 256`, `focused_lower_batch_states = 64`;
- [`solver_solve.cpp:11530-11533`](../../../engine/src/solver_solve_focused.cpp:501) —
  the loop breaks at `focused_expansion_batch_states`;
- [`solver_solve.cpp:11540`](../../../engine/src/solver_solve_focused.cpp:510) —
  per-class admission stops at `members_per_class`.

Observed mean expansion per round: 57,233 / 22 = **2,602 states** then;
57,182 / 326 = **175 states** now. Ratio 14.9x — matching the round ratio
14.82x (`focused_expansion.rounds` 22 → 326).

Because every round re-solves the whole monotonically-grown expanded graph,
total policy-evaluation work is roughly `O(rounds x mean graph size)`. The full
decomposition against pinned counters:

| Factor | Old | New | Ratio |
|---|---:|---:|---:|
| `focused_expansion.rounds` | 22 | 326 | **14.82x** |
| `optimization.policy_evaluation_calls` per round | 3.09 | 8.90 | **2.88x** |
| `optimization.policy_states_collapsed` per call | 34,487 | 23,653 | **0.686x** |
| product | | | **29.28x** |
| measured `focused_expansion.duration_ns` | 28,778 ms | 841,222 ms | **29.23x** |

Each individual policy evaluation got *cheaper*. There are simply 42.7x more of
them (`policy_evaluation_calls` 68 → 2,903).

Attribution: the caps were introduced in `13e8723` ("Checkpoint exact
constructive solver search", 2026-07-21, +4,287 lines in `solver_solve.cpp`) —
the action/state pruning milestone, **not B1**. Values are byte-identical at
`13e8723`, `60500ef`, `58aa5ea`, and `HEAD`. B1-B6 inherited this regression;
they did not cause it.

### H2 — Contributing: a second full Howard pass per round

**Confidence: high for existence, medium for magnitude.**

`7b11b34` had exactly one `focused_expansion_ns` accumulation site (old line
6999). Current code has two — the focused *upper* solve is a second complete
policy-iteration pass, added in the same `13e8723` commit:

- [`solver_solve.cpp:11680`](../../../engine/src/solver_solve_focused.cpp:628) —
  `if (begin_focused_upper_solve()) return;`
- [`solver_solve.cpp:11615-11619`](../../../engine/src/solver_solve_focused.cpp:585) —
  sets `focused_upper_mode`, `focus_optimizing`, and re-enters
  `PolicyUnitStage::InitialSelect`
- [`solver_solve.cpp:11936-11937`](../../../engine/src/solver_solve_focused.cpp:884) —
  the second `focused_expansion_ns += optimization_ns`

Corroborating counters: `focused_expansion.partial_policy_rounds` = 241 (no old
equivalent); `policy_evaluation_calls` per round 3.09 → 8.90. This is the 2.88x
middle factor in H1's table. It buys the certified upper bound that makes
bounded results possible, so it is a deliberate cost — but it is charged on
every one of 326 rounds rather than 22.

### H3 — Contributing: the balanced quota halves lower-fringe throughput

**Confidence: medium. Mechanism is certain; magnitude is not separated in the
pinned telemetry.**

[`solver_solve.cpp:11713-11746`](../../../engine/src/solver_solve_focused.cpp:666)
reserves `lower_quota = min(256, 64) = 64` of each 256-state batch for the
lower fringe and 192 for the upper fringe. Whenever the upper fringe is
non-empty, lower-bound progress advances at 64 states per round, not 256. The
comment records why (either bound could starve the other), and the union stays
complete — but it compounds H1 by up to a further 4x on lower-bound closure
specifically. Rounds that reach `focused_closure_proved` or leave the upper
fringe empty skip this path, so the effective factor is between 1x and 4x and
is not directly readable from the report.

### H4 — Secondary: per-round constructive-witness revalidation, 122 s

**Confidence: medium. Cost centre is bounded to one function; the dominant term
inside it is not yet identified.**

`constructive_policy` = 121,995 ms over 326 calls = 374 ms each, all inside
`acquire_focused_fallback()`
([`solver_solve.cpp:1678-1718`](../../../engine/src/solver_solve_constructive.cpp:183)).
Counters confirm 3 syntheses + 323 reuses = 326 = rounds, and 0 refreshes — so
323 of 326 calls only *revalidated* an unchanged witness via
`retained_fallback_invalid_reason()`
([`solver_solve.cpp:1585-1676`](../../../engine/src/solver_solve_constructive.cpp:90)).

Candidate cost centres inside that validation, in rough order of suspicion:

1. [`economy_identity()` at `:1516-1526`](../../../engine/src/solver_solve_constructive.cpp:34)
   — copies the entire `prices` map into a vector (**string copies**), sorts
   it, and hashes every key byte-by-byte, on every call. Nothing is memoized.
2. [`action_vocabulary_prefix_identity()` at `:1528-1540`](../../../engine/src/solver_solve_constructive.cpp:46)
   — walks all 2,166 operators and byte-hashes each `planner.id` per call.
3. [`goal_identity()` at `:1502-1514`](../../../engine/src/solver_solve_constructive.cpp:20)
   — walks all layout slots and satisfying-mask words per call.
4. The two properness probes at `:1666` and `:1672`
   (`fallback_terminal_upper`, `focused_start_upper_bound`).

All four recompute from scratch what the witness-reuse rule already asserts is
unchanged. Note the adjacent
[`graph_identity()` at `:1546-1560`](../../../engine/src/solver_solve_constructive.cpp:64)
iterates all 738,139 transition rows, but is reached only from
`stamp_fallback_provenance()` on the 3 syntheses — it is *not* on the
revalidation path. Do not assume it is the culprit without measuring.

### H5 — Byte-ledger growth: 86x per-request cost from a 64-context fan-out

**Confidence: high.**

`calc_owned_byte_ledger_ns` grew 935.8x (9.087 ms → 8,503.782 ms) on only
10.88x more requests (357,521 → 3,890,362). Per request: **25.4 ns → 2,186 ns,
86.0x.** The request growth tracks the round growth from H1; the per-request
growth has an independent cause.

`fast_estimated_owned_bytes()` gained a recursive fan-out after `7b11b34`
(verified by diffing the function against `7b11b34:engine/src/solver_calc.cpp`
— this block is the only difference):

- [`solver_calc.cpp:463-471`](../../../engine/src/solver_calc.cpp:463) — iterates
  `automatic_admission_contexts_` and calls `fast_estimated_owned_bytes()` on
  each child context;
- [`solver_options.cpp:2485`](../../../engine/src/solver_options.cpp:2485) —
  `kRetainedAutomaticAdmissionContexts = 64`, the cache bound;
- [`solver_calc.cpp:370-439`](../../../engine/src/solver_calc.cpp:370) — each
  recursive call performs a full ~25-term `dynamic_shallow_owned_bytes()` walk.

So the "fast" ledger became `O(retained contexts)` — up to 65 shallow walks per
call, ~1,650 ns at the old 25.4 ns/walk rate, against 2,186 ns measured. The
residual is consistent with `automatic_comparison_context_` recursion at
[`:460-462`](../../../engine/src/solver_calc.cpp:460).

This is the *cost side* of a real win: the same retained-context cache dropped
the five `admission_phases.local_*` timers by a combined **7,651 ms** (led by
`local_context_ns` 3,954.730 → 129.181 ms and `local_planner_build_ns`
1,720.506 → 25.190 ms). Against that, the ledger cost grew **8,495 ms**. The
trade is therefore roughly break-even *today* — but asymmetric under H1: the
build saving is bounded by the number of distinct contexts, while the ledger
cost scales with request count and so with round count. Also new:
`calc_owned_byte_ledger_max_overestimate` 0 → 520, meaning the incremental
ledger now drifts from the audited truth.

At 8.5 s of 1,083 s (0.78%) this is a supporting lead, exactly as HANDOFF
states — but it is the one item whose *unit* cost regressed rather than its
count, so it is cheap to confirm and cheap to fix.

### H6 — Not a regression: audit growth is proportional

`calc_owned_byte_audit_requests` is **58 in both runs**. `calc_owned_byte_audit_ns`
grew 3.32x (1,485.834 → 4,934.798 ms), i.e. 25.6 ms → 85.1 ms per audit,
against `memory.solver_live_owned_bytes_estimate` growth of 2.54x
(156,407,069 → 396,659,207 bytes). The audit walks bigger structures the same
number of times. `periodic_cap_byte_audit` (+3,218.211 ms) and
`finalize_byte_audit` (+154.351 ms) are the same effect. Record it, do not
chase it.

### 1.3 Open anomalies — flagged, not explained

- **`states.goal` rose 3,225 → 5,851 (1.81x)** while discovered states fell to
  0.61x. Every other structural counter moved down. Worth one bisect; it may be
  benign (goal-state marking now applied to more of a smaller graph) or it may
  indicate a changed goal test on this case.
- **`optimization.largest_policy_component` rose 5,230 → 16,570 (3.17x)** while
  `max_sparse_policy_iterations` was flat (36 → 33). The header comment at
  [`solver_solve.cpp:50-53`](../../../engine/src/solver_solve_types.hpp:55) warns that
  pivoted elimination is cubic and sets `kDensePolicyComponentLimit = 96`, so
  16,570 is safely on the sparse path — but a 3.17x component means the focused
  quotient is merging less aggressively than it did.

### 1.4 Proposed measurement plan for the deferred profiling chunk

Ordered so that each step is cheap and can falsify the step after it. **Nothing
here is authorized; it is a plan for a chunk Oliver has not selected.**

**M0 — Establish that the timers partition the wall clock. Do this first.**
They currently do not, in either direction:

- new: 841.2 + 122.0 + 13.2 + 19.8 + 7.6 = 1,003.8 s against
  `phase_wall_ms.solve` 1,083.1 s — **79.3 s (7.3%) unattributed**;
- old: 28.8 + 21.4 + 5.6 = 55.8 s against 50.7 s — **over-sums by 5.1 s (10%),
  so old timers demonstrably overlap**.

Until the phase timers are proven disjoint, no ranking built from them is
trustworthy, including the one in HANDOFF. Add a single whole-solve reference
clock and assert the leaves sum to it within tolerance.

**M1 — Confirm H1 by round-count sweep, not by code reading.** Run the derived
bounded 2k case (already specified in
[mechanical-split Gate 2](../2026-07-22-mechanical-solver-split/plan.md), so it needs no new fixture) at
`focused_expansion_batch_states` ∈ {256, 512, 1024, 4096} with everything else
fixed. H1 predicts near-linear fall in `focused_expansion.rounds` and in
`policy_evaluation_calls`, with `policy_states_collapsed` per call roughly
flat. If rounds fall and wall time does not, H1 is wrong and the cost is
per-round-size rather than per-round-count.

**M2 — Separate H2 from H1.** Same sweep, comparing
`policy_evaluation_calls / focused_expansion.rounds`. If that ratio stays near
8.9 across batch sizes, the upper pass is a fixed per-round multiplier and H1
and H2 are cleanly separable. Only then is it meaningful to ask whether the
upper pass can run every *k*th round instead of every round.

**M3 — Attribute H4 with a scoped timer, not a profiler.** Four
`steady_clock` spans inside `retained_fallback_invalid_reason` around the three
identity helpers and the two properness probes. 374 ms per call is far too
large for the identity hashes as read; either one of them is unexpectedly
expensive (most likely `economy_identity`'s copy-and-sort) or the cost is in
the probes. This is a ~20-line diagnostic-only change and it answers the
question outright.

**M4 — Confirm H5 by counting, not timing.** Add a depth or fan-out counter to
`fast_estimated_owned_bytes()`. If the mean fan-out is ~65, H5 is confirmed and
the fix is a per-context cached subtotal invalidated on mutation, not a
restructure.

**M5 — Bisect the anomalies in 1.3.** `states.goal` and
`largest_policy_component` over `7b11b34 → 13e8723 → 60500ef → 58aa5ea`. Four
points, hash-only cases, no oracle.

**Do not run the exactly-once two-T1 oracle for any of this.** Every step above
is answerable on the bounded 2k derived case and the two existing chaos cases,
all of which complete well inside the 15-minute ceiling. The oracle's 1,083 s
solve is the *subject* of the investigation, not an instrument for it.

**Expected payoff, stated as a prediction to be falsified.** If H1 and H2 hold
and the round count returns to `7b11b34` scale while retaining the upper pass,
focused optimization falls from 841 s toward roughly 57-114 s and constructive
policy from 122 s toward roughly 8 s — an estimated whole-solve figure in the
150-250 s range rather than 1,083 s. That is a hypothesis derived from the
ratios above; it is not a target, a commitment, or evidence.

---

## Part 2 — Live Documentation Drift

Nothing under `docs/archive/` was read for correction or is proposed for
change. Findings are ordered by consequence.

### D1 — `future/solver-roadmap.md` denies the active boundary

[`future/solver-roadmap.md:21-22`](../../future/solver-roadmap.md) states "There is
no active solver implementation boundary". That contradicts
[`active/README.md:3-12`](README.md),
[`direction.md:45-46`](../../direction.md), and `HANDOFF.md`, all of which record
the mechanical solver split as selected on 2026-07-22.

The same page also has **no entry for the deferred profiling chunk**. The
largest known solver performance item in the repository exists only in
`HANDOFF.md`. `future/` is by policy where deferred work is preserved.

### D2 — `evidence.md` contradicts itself on the two-T1 control

[`evidence.md:131-135`](../../evidence.md) (pruning milestone) states the natural
two-T1 product "remained the broad control: … 189,946 strict states and 903,935
rows remained". [`evidence.md:200-203`](../../evidence.md) (B1-B6), on the same
case, records 57,182 expanded states and 738,139 rows; the report's
`exact_state_scaling.strict_states` is 116,041. Same page, ~70 lines apart, no
reconciliation.

Separately, **`evidence.md` nowhere records the 21.36x wall-clock regression**,
though it is the largest measured change to that control case. A reader
consulting the authoritative evidence index would conclude the control was
unaffected.

### D3 — Two load-bearing facts live only in `HANDOFF.md`

`HANDOFF.md` is boundary-scoped and is rewritten when a boundary closes. It
currently holds two facts that outlive the boundary:

1. **The economy publish hazard** (`HANDOFF.md:509-536`): `poecraft-economy
   publish` must not be run because the local DB was rebuilt from test fixtures
   on 2026-07-22 and its compiled snapshots carry fixture prices. Nothing in
   [`economy/NOTES.md`](../../economy/NOTES.md) or
   [`economy/README.md`](../../economy/README.md) records this. It is a
   product-data-safety hazard sitting in the most volatile file in the repo.
2. **The profiling order and 21x regression** (`HANDOFF.md:75-190`). Not in
   [`solver/NOTES.md`](../../solver/NOTES.md), not in `evidence.md`, not in the
   roadmap.

### D4 — The `harvest_reforge:defences` pricing gap is undocumented

[`mechanics/harvest.md:61`](../../mechanics/harvest.md) correctly records Oliver's
2026-07-15 ruling that "the canonical key is `defences`". The engine agrees.
Every published snapshot carries the stale singular `defence`, so the
Calculator cannot price that action. The docs are right and the data is wrong —
but no live document states that the product has this gap.
[`economy/README.md:3-6`](../../economy/README.md) still reads "Repository support
is complete for canonical ingest, immutable snapshots, browser
caching/selection, manual overrides, and price pinning."

### D5 — `foundation/change-impact.md` misses the CMake explicit source list

Stamped `2026-07-20 @ 8f6ea61` — before the pruning milestone and all of
B1-B6. Its "Rebuild Triggers" section
([`:111-131`](../../foundation/change-impact.md)) documents `scripts/build.ps1`
and `scripts/build-wasm.ps1` but never says that the three source-discovery
paths disagree:

| Path | Discovery | Adding a `.cpp` |
|---|---|---|
| [`engine/CMakeLists.txt:66`](../../../engine/CMakeLists.txt:66) | **explicit per-file list** | must be edited by hand |
| [`scripts/build.ps1:77`](../../../scripts/build.ps1:77) | `Get-ChildItem … -Filter *.cpp` | automatic |
| [`scripts/build-wasm.ps1:44`](../../../scripts/build-wasm.ps1:44) | `Get-ChildItem … -Filter *.cpp` | automatic |

`change-impact.md:127` says build-wasm "compiles every `engine/src/*.cpp`
file", which is true and reinforces the wrong generalization. A new source file
builds fine in two of three paths and silently vanishes from the third — which
is exactly the failure mode the mechanical split can hit, on a machine where
CMake was unavailable at selection time. The split plan catches this in its own
Motion Rules; the durable process reference does not.

### D6 — Verification stamps

- [`solver/README.md:8`](../../solver/README.md) ("2026-07-22 @ B6 boundary") and
  [`solver/flow.md:9`](../../solver/flow.md) ("2026-07-22 @ bounded-policy B6
  boundary") name a **milestone, not a commit**, contrary to the policy at
  [`README.md:67-69`](../../README.md). The commit is `3d76198`. Milestone labels
  stop resolving once the plan is archived.
- **20 of 24 stamped live pages still read `2026-07-19 @ d5e38e3`** — before
  the pruning milestone and the entire B-series. Only `engine/wasm.md`,
  `product/calculator.md`, `solver/README.md`, and `solver/flow.md` were
  re-verified at B6. Most are mechanics pages where that is probably fine, but
  it is unverified rather than known-fine.

### D7 — `product/NOTES.md` and the roadmap now disagree

[`product/NOTES.md:10-18`](../../product/NOTES.md) (2026-07-19) says the
Calculator's 10,000-run button "does not yet require a successful terminal
gate, zero failure/stop/limit/off-policy outcomes, or show sampled
variance/confidence", and marks itself "promoted to the future solver roadmap".
The roadmap's R5 entry
([`future/solver-roadmap.md:59-60`](../../future/solver-roadmap.md)) says "the
selected product verification gate are complete" (also a grammar defect).
B2 and B6 changed that surface and `product/calculator.md` was re-verified at
2026-07-22, but the note was not revisited. One of the two is stale; the code
decides which.

### D8 — `min_tier` semantics are documented nowhere

[`solver_options.cpp:210-211`](../../../engine/src/solver_options.cpp:210) and
[`solver_abstract.cpp:133`](../../../engine/src/solver_abstract.cpp:133) implement
`slot.min_tier == 0 || (tier != 0 && tier <= slot.min_tier)`. So `min_tier` is
the **worst acceptable tier** (tier numbers descend in quality) and `0` means
any tier. [`solver.h:30-31`](../../../engine/include/poecraft/solver.h:30) shows it
only by example, with no prose. No live document explains the direction; the
only occurrence in live docs is a speculative JSON block in
[`future/ml.md:221,226`](../../future/ml.md). Read naively, `"min_tier": 3` means
the opposite of what it does. This belongs in
[`solver/README.md`](../../solver/README.md) or
[`mechanics/strategy-and-solver-vocabulary.md`](../../mechanics/strategy-and-solver-vocabulary.md).

### D9 — `solver/NOTES.md` understates what is now known

[`solver/NOTES.md:43-54`](../../solver/NOTES.md) lists "focused-round reuse" among
"#idea — Reviewed optimization candidates … Status: open; no execution
authority". Since that note was written (2026-07-19), focused-round behaviour
has become the single highest-value known solver performance item — see H1/H2.
The note predates the measurement and does not point at it.

### D10 — Where docs are correct

Recorded so this is not read as a general indictment.
[`direction.md:45-54`](../../direction.md) accurately describes the selected split;
[`active/README.md`](README.md) accurately records deferrals;
[`solver/README.md:342-381`](../../solver/README.md) accurately describes the B4/B5
orchestration, analytics, stage funnel, and exit-code-2 semantics against the
shipped code; [`mechanics/harvest.md`](../../mechanics/harvest.md) correctly holds
the `defences` ruling. The drift above is concentrated in cross-references,
stamps, and notes — not in the stable behaviour descriptions.

---

## Part 3 — Open Decisions For Oliver

Ordered by cost of continued delay. Each names what is blocked and what the
decision changes. **None of these is started.**

### O1 — Is the profiling chunk scoped to the round cap?

Part 1 says the 21x is one root cause (batch cap → 14.8x rounds) plus one
deliberate cost charged per round (the upper pass), not the three independent
phases the current ranking implies. If that reframing is accepted, the chunk is
a bounded parameter/scheduling investigation on existing fast cases, not an
open-ended profile. **Blocks:** nothing today; the split proceeds regardless.
**Changes:** whether the chunk is scoped as "measure and tune the focused round
schedule" or as "profile the solver". The regression predates B1, so it is also
worth deciding whether it is a *regression* to reverse or a *price* knowingly
paid for bounded results.

### O2 — When does the economy database get rebuilt from live data?

`HANDOFF.md:520-536` requires this to run **between** chunks, and it changes
tracked files (a new snapshot JSON plus the league index). It also unblocks the
`harvest_reforge:defences` fix, which a from-scratch recipe seed was already
verified to produce correctly with zero code changes. **Blocks:** any economy
publish; the defences pricing fix. **Note:** the surviving content-addressed
raws in `data/economy/raw/poe-ninja/` mean the July 15 state is reconstructable,
but that window is only as durable as an untracked directory.

### O3 — The 120-case benchmark campaign has never run

Only the 2-case `smoke` stage has ever executed
(`build/reports/bounded-policy/b5-boundary/smoke/`). The `full_short` (120),
`deep` (12), `exact_evaluation`, and `acceptance_verification` stages in
`fixtures/solver-natural-t1/v1/benchmark-stages.json` are unrun. B6 accepted on
one explicit case. So the corpus's actual solvability distribution is
**unmeasured** — the only signal is a 2-case sample in which the three-T1 Dire
Pelt returned `refused_state_cap` / no-policy in 507 ms.
**Blocks:** any claim about how the solver performs across the corpus; also the
paired-comparison baseline that would make O1's before/after credible.
**Note:** the B4/B5 machinery to run it is built, tested, and resumable — this
is spend, not construction. Sequencing against O1 matters: running it before a
performance change gives a baseline; running it after gives an answer but no
comparison.

### O4 — Should the corpus generator support tier ranges?

`natural_t1_corpus.py:185` hardcodes `{"family_mod_key": key, "min_tier": 1}`,
so every one of the 146 pinned cases demands exact T1 on every goal slot. The
engine already accepts looser goals (D8: `min_tier: 3` admits T1-T3) — this is
a generator limitation, not an ABI one, so the change is Python-only.
**Blocks:** nothing. **Changes:** whether the benchmark corpus represents real
crafting targets. Real goals are usually stated as ranges; an all-T1 corpus is
an unrepresentatively hard slice, which interacts with O3 — measuring the hard
slice at 120 cases may be the wrong spend if the realistic slice is untested.

### O5 — What are the Calculator-split prerequisites?

Deferred by the split plan "because its private lifecycle wiring lacks direct
characterization coverage and this chunk adds no tests"
([mechanical-split.md:48-50](../2026-07-22-mechanical-solver-split/plan.md)).
`apps/web/src/app/components/pc-calculator.ts` is 2,606 lines with no
direct component test; the nearest coverage is
`apps/web/test/solver-result-presentation.test.ts` (5 focused DOM cases from
B2) and `strategy-calculator-mode.test.ts`. **Blocks:** the Calculator split
indefinitely, since nothing schedules the characterization tests it depends on.
**Changes:** whether a "write Calculator characterization tests" chunk exists at
all. The same reasoning applies to `solver_internal.hpp` (1,990 lines) and
`apps/web/test/engine-smoke.test.ts` (1,832 lines).

### O6 — Where do durable facts go when a boundary closes?

D3 is a process gap, not a content gap: `HANDOFF.md` is the only home for the
publish hazard and the profiling order, and it is rewritten at boundary close.
**Blocks:** nothing today. **Changes:** whether the mechanical split's Gate 8
handoff rewrite silently drops both. The existing lifecycle
([`README.md:42-50`](../../README.md)) routes notes to stable areas and plans to
archives, but has no rule for "operational hazard discovered outside chunk
scope". `economy/NOTES.md` and `solver/NOTES.md` already exist and are the
obvious destinations.

---

## What This Report Did Not Do

- No source, header, script, fixture, or artifact was modified.
- No build, test, solve, benchmark, or economy command was run.
- No Path of Exile mechanic was researched, inferred, or ruled on. D4 and D8
  report a data/documentation mismatch against Oliver's existing 2026-07-15
  ruling; they do not restate or reinterpret it.
- No document under `docs/archive/` was read for correction or changed.
- [`active/README.md`](README.md) was deliberately **not** updated to link this
  page, to avoid colliding with in-flight work on the mechanical split. That
  index link is outstanding.
