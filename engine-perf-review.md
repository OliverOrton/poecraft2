# Engine Performance Review — Follow-up (current working tree)

**Date:** 2026-06-30 · **Reviewer:** Claude (read-only) · **Audience:** codex (decide + implement)

**Scope:** Same workload as the original audit — 100k-run chaos/alchemy/reforge sims on a
rare body armour at iLvl 86. This is a *re-review of the current source*, because the engine
`.cpp` files are all modified-but-uncommitted (` M` in git) and the original
[`engine-hotpath-findings.md`](engine-hotpath-findings.md) line numbers are now stale.

**Headline:** Most of the original audit's high-ROI items are **already implemented** in this
working tree. The hot path is in good shape. What remains is (1) a missing determinism
safeguard, (2) residual rejection-sampling cost, (3) a couple of free micro-cleanups, and
(4) the still-large item struct. None are as large as the original P1/P2/P5.

**Measured (see §0):** the simulator mod-pool path is **not** allocation-bound — it runs at a
**98.9% pool-cache hit rate**, ~0 heap allocs per reforge, and **1M chaos reforges in 346 ms**.
The dominant remaining mod-pool cost is the **weighted sampling** (`lower_bound` over
`prefix_sums`), not pool building or allocation. The biggest open lever in "mod-pool stuff" is
O(1) **alias-table sampling** cached with the pool (§G), plus the fact that the **emulator /
`apply_batch` path doesn't get the fast sampler at all** (§H).

---

## A. Already landed — do NOT redo

Verified by reading the current source. The original work-order items map as follows:

| Orig item | Status | Evidence (current source) |
|---|---|---|
| **P1** guard per-step trace entry + 1.1 KB item copy | ✅ done | Trace entry build now wrapped in `if (trace != nullptr && options.max_trace_entries != 0)` at [simulator.cpp:702](engine/src/simulator.cpp:702), [:737](engine/src/simulator.cpp:737), [:824](engine/src/simulator.cpp:824). `entry.item = result.item` only inside the guard. |
| **P2** skip `SimulationExample` build when caps full | ✅ done | Build gated by `retain_success || retain_failure` at [simulator.cpp:1095-1117](engine/src/simulator.cpp:1095); `example.item = result.item` only inside ([:1111](engine/src/simulator.cpp:1111)). |
| **P3** reserve pool vectors; drop dead zero-fills | ✅ done | `pool.entries.reserve(candidate_count)` / `prefix_sums.reserve` at [session_builder.cpp:1263](engine/src/session_builder.cpp:1263). Candidate scratch is now a direct copy-assign (`candidate = session.normal_random_roll_mask`, [:1158](engine/src/session_builder.cpp:1158)) with no preceding `assign(words,0)`. |
| **P4.1** flatten `group_masks` from hash map → vector | ✅ done | `session.group_masks` is now indexed by dense group id: `if (group < session.group_masks.size()) … group_masks[group]` ([actions_basic.cpp:225](engine/src/actions_basic.cpp:225), [session_builder.cpp:151](engine/src/session_builder.cpp:151)). Same for `implicit_tag_masks`. No `unordered_map` lookup in the per-pick block-mask build. |
| **P4.2 / P5** incremental refill (avoid full pool rebuild per pick) | ✅ done (different design) | Two mechanisms: (a) a **refill pool cache** keyed on the group-block mask + signature + side + influence + metamod hints (`RefillPoolCacheKey`, [engine_internal.hpp:397](engine/src/engine_internal.hpp:397); lookup at [session_builder.cpp:1088-1119](engine/src/session_builder.cpp:1088)), with a single-entry `last_refill_pool` fast path; (b) a **rejection sampler over a once-built superset** (`add_random_mod_from_superset`, [actions_basic.cpp:352](engine/src/actions_basic.cpp:352)), used for headless fills of 2+ mods. So a 4–6 mod chaos/alch builds the weighted pool ~**once** (cache-warm) instead of 4–6×. |
| **P6** structural pre-key so a hit skips candidate build | ◑ partial / largely moot | The refill cache hit at [:1102-1118](engine/src/session_builder.cpp:1102) returns **before** the candidate-mask passes for the refill loop. The *first* pool build per signature still builds the full mask (acceptable — once per run, then cached). The full P6 "cheap structural key for the general `pool_cache`" was not done, but the refill cache covers the hot loop. |
| **D1** elide double item copy per action | ✅ done for hot path | Single rollback snapshot, only when needed: `action_needs_rollback(type)` returns **false** for Transmute/Alteration/Alchemy/Chaos ([simulator.cpp:633-646](engine/src/simulator.cpp:633)); no copy-back on success. The old `working` + copy-back pair is gone. |

The original §4 non-issues still hold; in particular `active_spawn_weight`'s per-call
`unordered_set` rebuild ([actions_basic.cpp:111-114](engine/src/actions_basic.cpp:111)) is still
there but remains **off** the chaos/alch/reforge path (only unveil/eldritch/bloodstained call it).

---

## B. Remaining / new findings

### F1 — Determinism safeguard is still missing, and two surfaces now diverge per-seed · **flag: HIGH** · risk to ship: low
- The original Constraint 0.1 required a **seeded golden-sequence test before any draw-order
  change**. That change has now shipped (rejection sampler) and **the test still does not
  exist** — `grep` for golden/seeded-sequence/`next_below` assertions in
  [test_actions.cpp](engine/tests/test_actions.cpp) finds nothing. A draw-order regression
  would pass CI silently.
- Two code paths now consume RNG differently for the *same* item+seed:
  - **Simulator** (`capture_action_trace = false`, set at [simulator.cpp:1037](engine/src/simulator.cpp:1037))
    → rejection sampler (`add_random_mod_from_superset`).
  - **Emulator / single actions** (`capture_action_trace = true`, the struct default at
    [engine_internal.hpp:530](engine/src/engine_internal.hpp:530)) → exact per-pick
    `add_random_mod` (which, with the incremental block-mask hint, reproduces the legacy
    filtered pool).
  By construction the rejection sampler is an **unbiased** draw from the same weighted
  distribution (accept iff side+group+metamod filters pass; bounded fallback to the exact
  sampler at [actions_basic.cpp:425](engine/src/actions_basic.cpp:425)), so **aggregate
  probabilities are unaffected** — but a given seed no longer produces the same item in the
  emulator and the simulator.
- **Recommendation for codex:** before any further sampler change, land (a) a **golden seeded
  sequence** test pinning the simulator path, and (b) a **statistical equivalence** test
  (e.g. χ² over mod-id frequencies, exact-vs-rejection, N≈1e6) to *prove* the rejection path is
  unbiased rather than asserting it. This unblocks the Fenwick option (F2) safely.

### F2 — Rejection efficiency degrades late in a fill, and is unmeasured · **impact: MEDIUM** · risk: low
- `add_random_mod_from_superset` retries up to `max_rejections = 4` (5 attempts, [actions_basic.cpp:463](engine/src/actions_basic.cpp:463)) then falls back to `add_random_mod`
  ([:425](engine/src/actions_basic.cpp:425)), which calls `get_weighted_pool` with the *current*
  block mask → a refill-cache **miss** (group mask changed) → a full candidate rebuild for that
  pick. Acceptance probability falls as groups fill and a side closes (5th/6th mod), so the tail
  of each reforge is where rejections + fallback rebuilds concentrate.
- For a body armour (hundreds of candidates, few blocked groups) this is likely cheap, **but
  there is no counter for it** — the perf block tracks `candidate_build_ns` / `sampling_ns` /
  cache hits ([engine_internal.hpp:522-527](engine/src/engine_internal.hpp:522)) but **not**
  rejection attempts or superset-fallback count.
- **Recommendation:** add a `rejection_attempts` / `superset_fallbacks` counter, measure on the
  chaos benchmark, and only then decide whether the original P5 **Fenwick/segment-tree
  sample-and-delete** (O(log n), zero rejections, can be made bit-identical) is worth the
  complexity. Don't build the Fenwick blind — measure first.

### F3 — Dead metamod re-check inside the rejection loop · **impact: LOW (free)** · risk: none
- The superset is built with `block_attack`/`block_caster` already applied: `get_weighted_pool`
  runs `apply_metamod_pool_blocks(...)` at [session_builder.cpp:1212](engine/src/session_builder.cpp:1212)
  using `session.implicit_tag_masks["attack"/"caster"]`. The rejection loop's `metamod_allowed`
  check ([actions_basic.cpp:395-401](engine/src/actions_basic.cpp:395)) uses the **same** masks
  via its `tag_mask` lambda ([:360-374](engine/src/actions_basic.cpp:360)). So any metamod-blocked
  mod is already absent from the superset → `metamod_allowed` can never reject → the check (and
  the two `tag_mask` lookups per fill) is **dead work**.
- **Recommendation:** drop the `metamod_allowed` term and the `attack_mask`/`caster_mask` setup
  from `add_random_mod_from_superset`. Removing it does not change which entries exist or their
  order, so the draw is **bit-identical** — safe even before F1's golden test. (Keep a one-line
  comment noting the superset already applied the block.)

### F4 — Per-reforge fixed overhead · **impact: LOW** · risk: very low
- `build_refill_group_block_mask` does `block_mask.assign(session.words, 0)` every reforge
  ([actions_basic.cpp:240](engine/src/actions_basic.cpp:240)) — a full-width zero-fill even when
  nothing is preserved (the common chaos case: no fractured/locked slots ⇒ the loop ORs nothing).
  Similarly `collect_preserved` + `restore_slots` clear and rebuild both sides each reforge
  ([:577-578](engine/src/actions_basic.cpp:577)).
- These are O(words) / O(slots), tiny next to a pool build, but they're paid once per *run*.
  Low ROI; only worth touching if F2's measurement shows per-reforge fixed cost is material.

### F5 — Strategy-graph traversal is unprofiled and uses linear scans · **impact: LOW–MEDIUM, data-dependent** · risk: low
- The original audit only covered mod-picking. The *other* half of per-run cost is the graph
  walk: every step calls `select_edge` → `evaluate_condition` for each edge until one matches
  ([simulator.cpp:579-592](engine/src/simulator.cpp:579)). `HasModGroup`/`HasModFamily` are
  linear scans over occupied slots × group offsets (`has_group` [:471](engine/src/simulator.cpp:471),
  `has_family` [:490](engine/src/simulator.cpp:490)).
- For a simple chaos-spam strategy this is negligible (few edges, `Always` conditions). For a
  realistic editor strategy (many nodes, `All`/`Any`/`AtLeast` composites, repeated `has_mod_group`
  checks) it runs every step of every run and can rival mod-picking.
- **Recommendation:** include a *representative* multi-condition strategy in the benchmark, not
  just N chaos nodes, before deciding. If it shows up, the fix is an incrementally-maintained
  per-item occupied-group bitset so `has_group` is an O(1) bit test. Defer until measured.

### F6 — `pc_item_state` is still ~1.1 KB, ~94% cold (orig D2) · **impact: LOW now** · risk: ABI
- Layout unchanged ([item_state.h:58-94](engine/include/poecraft/item_state.h:58)): `rolls[8]`
  per slot, `implicits[8]`, `enchantments[4]`, sockets — none written on the chaos/alch path.
  Now that P1/P2/D1 removed most discarded copies, the main remaining copies are the **per-run
  start-item copy** (`result.item = strategy.start_item`, [simulator.cpp:689](engine/src/simulator.cpp:689))
  and the rollback snapshot for non-reforge actions. Slimming the struct (split a lean hot state
  from the cold `rolls`/sockets tail) would roughly halve those.
- Changes the ABI and `pc_data_check_capacities` contract → re-validate Python/WASM/UI readers.
  Treat as a deferred follow-up; lower priority than it was pre-P1/P2.

---

## C. Confirmed-correct (don't "fix") 

- **Refill cache ↔ pool cache pointer coupling.** `refill_pool_cache` stores raw
  `const WeightedPool*` into `pool_cache`'s mapped values. `std::unordered_map` does not
  invalidate element pointers on insert, and the overflow path clears **both** together
  ([session_builder.cpp:1324-1329](engine/src/session_builder.cpp:1324)) while the refill-only
  overflow ([:1122-1126](engine/src/session_builder.cpp:1122)) leaves pool_cache intact. Safe.
  (Worth a comment so a future edit doesn't clear `pool_cache` alone.)
- **`intern_item_tag_signature` per `get_weighted_pool` call** is an array lookup that returns
  immediately for the common (non-influenced) item: `signature_by_influence_bits[bits]`
  ([session_builder.cpp:1042-1043](engine/src/session_builder.cpp:1042)). Not a hotspot.
- **`kMaxPoolCacheEntries = 4096`** ([:29](engine/src/session_builder.cpp:29)) is far above the
  handful of distinct keys a single-action workload generates; eviction never triggers here.

---

## D. Measurement & build state (read before benchmarking)

- **Counters are reachable:** `pc_action_context_perf_timing_set` + a stats query
  ([api.cpp:1281-1308](engine/src/api.cpp:1281)) expose cache hits/misses, `candidate_build_ns`,
  `weighted_pool_build_ns`, `sampling_ns`. Enable on a chaos/reforge run to size each item.
- **Harness exists:** [`apps/web/test/performance-benchmark.ts`](apps/web/test/performance-benchmark.ts)
  (chaos on BodyInt17, 100k runs, env-configurable), plus `tools/benchmark_engine.py` and
  `tools/benchmark_wasm_module.mjs`.
- **Build-state caveat:** these engine `.cpp` changes are uncommitted and **no `.wasm` artifact is
  modified in git** — the committed WASM almost certainly predates this perf work, so the browser
  app may not yet be running these optimizations. Per project memory, `emcc` isn't assumed present;
  the *native* test/benchmark binaries can measure the C++ changes without a WASM rebuild, but
  validating the shipped app requires rebuilding WASM.

---

## E. Suggested order for codex

1. **F1 (safeguards first).** Land the golden seeded-sequence test + the exact-vs-rejection
   statistical equivalence test. Cheap, and it un-gates everything else.
2. **F3.** Delete the dead metamod re-check — free, bit-identical, no rebuild risk.
3. **F2 (measure, then maybe Fenwick).** Add rejection/fallback counters; benchmark; decide on
   the segment-tree only if the rejection tail is material.
4. **F5 (measure with a real strategy).** Add a multi-condition strategy to the bench; optimize
   `has_group` only if it shows up.
5. **F4, F6.** Per-reforge fixed-cost trims and the item-struct slim — last, lowest ROI now.

Net: the big wins are already in. The most *valuable* next step is not a new optimization — it's
the **determinism test net (F1)** that should have preceded the rejection-sampler change, plus the
free **F3** cleanup. Everything else is measure-first.

---

## §0. Measured baseline (2026-06-30, native g++ 14 ucrt64, `-O2`, current working tree)

Rebuilt `build/engine/poecraft_engine.dll` from the uncommitted source and ran
`tools/benchmark_engine.py` (BodyInt17, iLvl 86). Stable across 25k/100k runs:

| Metric | Value | Reading |
|---|---|---|
| **Strategy: chaos ×10/run, 100k runs** | **346 ms · 288,848 runs/s · 2,888,482 actions/s** | 1,000,000 chaos reforges in ⅓ s. This is the product workload (sim, `capture=false`). |
| Pool **cache hit rate** (reforge fills) | **0.989** | The refill/pool caches warm to ~99%. Pool *rebuilds* happen on ~1% of requests. |
| `candidate_build_ns` / request | **10.9 ns** | Amortized candidate-mask cost — near zero because hits skip it. |
| `weighted_pool_build_ns` / **miss** | **1118 ns** | A full pool build is ~1.1 µs, but it's paid on ~1% of requests. |
| `sampling_ns` / call | **56.2 ns** | `rng.next_below` + `lower_bound(prefix_sums)`, one per mod pick. |
| Single `alchemy` via `apply_batch` | 318,374 actions/s (~3.1 µs) | The **slow path** (see §H) + per-item ctypes marshalling — *not* representative of sim cost. |

**Verdict on the "allocations in the mod pool" hypothesis:** in the **simulator** path it's
already solved. Per reforge, after warmup: `collect_preserved` returns an empty vector (no
alloc), all bitset scratch is context-owned and reused (`candidate_mask_scratch`,
`block_mask_scratch`, `empty_group_mask`, `occupied_groups_scratch`), cache lookups are
pointer-based (`PoolCacheLookup`/`RefillPoolCacheLookup`, no key copy), and the superset pool is
built **once** for the whole 100k run. Heap allocation per reforge ≈ **0**. The only mod-pool
allocations left are on the **~1% miss path** (build `entries`/`prefix_sums` + copy the candidate
mask into the cache key) and on **off-chaos paths** (`pick_weighted_id`'s `candidates` vector +
`active_spawn_weight`'s per-call `unordered_set`, used only by unveil/eldritch/bloodstained;
`do_harvest_resist`'s `candidates`). Chasing allocations further will not move the sim.

**Where the time actually goes:** with candidate-build at ~11 ns and pool-build amortizing to
~11 ns (1118 ns × 1%), the per-pick cost is dominated by **sampling** (~56 ns × ~5 picks ≈ 280 ns
of the ~346 ns/action). If you want a real mod-pool speedup, target the sampler — §G.

## §G — O(1) alias-table sampling cached with the pool · **impact: MEDIUM–HIGH (the real lever)** · risk: medium (determinism)
- Today each pick is `rng.next_below(total)` + `std::lower_bound` over `prefix_sums` — **O(log n)**
  with a cache-unfriendly binary search over hundreds–thousands of entries
  ([actions_basic.cpp:309-312](engine/src/actions_basic.cpp:309), and the superset variant
  [:377-381](engine/src/actions_basic.cpp:377)). The rejection-superset path makes this *worse*
  per draw: it searches the **larger unfiltered** superset array and may draw several times
  (rejections) per accepted mod.
- **Fix:** add a **Walker/Vose alias table** to `WeightedPool` (built once in `get_weighted_pool`
  alongside `entries`/`prefix_sums`, [session_builder.cpp:1260-1313](engine/src/session_builder.cpp:1260)).
  Each draw becomes **O(1)**: one rng for the bin, one for the coin. Because the pool is cached at
  99% hit, the table is built ~once and reused for all 100k runs — it fits the existing cache
  architecture exactly. Expect a meaningful cut to the ~280 ns/action sampling component.
- **Determinism:** alias sampling consumes rng differently → a draw-order change (acceptable per
  Constraint 0.1, but **land F1's golden + statistical-equivalence tests first**). Keep the table
  keyed in ascending session-mod-id order for reviewability.
- **Bonus measurement:** since pool builds are now known-cheap (1.1 µs) and rare (1% miss), it is
  worth re-testing whether the **rejection-superset is still a net win** vs. sampling the exact
  (smaller) per-pick pool with an alias table. Add a `rejection_attempts` counter
  ([engine_internal.hpp:522](engine/src/engine_internal.hpp:522) area) and compare.

## §H — The emulator / `apply_batch` / direct-action path never gets the fast sampler · **impact: MEDIUM (UX/tools), HIGH clarity** · risk: low
- The rejection-superset fast path is gated on `!context.capture_action_trace`
  ([actions_basic.cpp:446](engine/src/actions_basic.cpp:446)), and `capture_action_trace`
  **defaults true** ([engine_internal.hpp:530](engine/src/engine_internal.hpp:530)). Only the
  *simulator* flips it false ([simulator.cpp:1037](engine/src/simulator.cpp:1037)). So every
  emulator action, Python `apply_batch`, and debug-pool call runs the **slow per-pick
  `add_random_mod`** path. Measured: ~9× slower per action than the sim (also inflated by ctypes
  marshalling, but the gating is real).
- This is *correct* today (capturing per-pick `ActionTraceStage`s requires the per-pick path), but
  it means the tool surfaces are needlessly slow and, per §F1, **diverge from the sim per seed**.
- **Options for codex:** (a) decouple "fast sampling" from "trace capture" — run the superset path
  even with capture on, recording the chosen mod *after* each accepted draw (one `ActionTraceStage`
  per pick, same data, just sourced from the rejection loop); or (b) accept the split and document
  it, but still let `apply_batch`/headless Python callers set `capture=false` for throughput. Either
  way, unify the sampler so emulator and simulator give the **same** draw for a seed.

## §I — Allocation map (chaos/alch reforge, headless) — for reference

| Site | When | Allocates? |
|---|---|---|
| `collect_preserved` → `vector<KeptSlot>` ([actions_basic.cpp:536](engine/src/actions_basic.cpp:536)) | per reforge | No (empty unless fractured/locked present) |
| `candidate_mask_scratch` / `block_mask_scratch` / `empty_group_mask` | per pool build / per reforge | No after warmup (context-owned, `.assign` reuses capacity) |
| `PoolCacheLookup` / `RefillPoolCacheLookup` | every lookup | No (pointer-based, transparent keys) |
| `pool.entries` + `pool.prefix_sums` (reserved) | **miss only (~1%)** | Yes — but reserved to exact size, then cached |
| `PoolCacheKey.candidate_mask` / `RefillPoolCacheKey.group_block_mask` copy | **miss only (~1%)** | Yes — full bitset copy into the key on insert |
| `PoolBuildRequest request = base_request` ([:295](engine/src/actions_basic.cpp:295), [:452](engine/src/actions_basic.cpp:452)) | per pick / per reforge | No for chaos (`fossil_indices` empty); **yes for fossil** |
| `pick_weighted_id` `candidates` + `active_spawn_weight` `unordered_set` | unveil/eldritch/bloodstained only | Yes — **off the chaos path** |

Only the 1%-miss rows and the off-path rows allocate. If fossil reforges become hot, hoist the
`fossil_indices` copy out of the per-pick `PoolBuildRequest`.
