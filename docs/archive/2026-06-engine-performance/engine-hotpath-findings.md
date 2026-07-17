# Engine Hot-Path Efficiency Findings — Run Start & Mod Selection

> **Archived 2026-07-17.** Historical read-only performance audit from
> 2026-06-30. It does not own current sequencing.

**Scope:** Runtime efficiency of the poecraft C++ engine for a 100,000-run strategy
simulation whose actions are chaos / alchemy / reforges (each reforge picks 4–6 mods)
on a rare body armour at item level 86. Focus: where runs start, and how modifiers are
picked.

**Status:** READ-ONLY audit. No code was changed. This document is a work order for an
implementing agent.

**How this was produced:** A 7-dimension multi-agent audit; every finding is grounded in
`file:line` evidence and most were adversarially verified. 19 of 30 findings received an
independent skeptic verdict (0 were rejected — all stood). 9 findings (the run-loop and
data-layout items, which include some of the most actionable wins) lost their independent
verifier to a session limit; those were re-read and re-verified first-hand by the
synthesizing agent. Provenance is flagged per finding: **[verified]** = independent
adversarial verdict; **[self-verified]** = analyst + synthesizer reading only.

---

## 0. Constraints the implementer MUST respect

1. **Determinism / draw-sequence parity.** Sampling is xoshiro256** (`engine/src/rng.cpp`).
   A single `rng.next_below(total)` per pick is mapped to a mod via `std::lower_bound`
   over `prefix_sums`, and `pc_bitset_for_each` yields candidates in **ascending
   session-mod-id order** (`engine/include/poecraft/bitset.h:74-88`), so `prefix_sums` are
   ordered by session mod id. Any change that reorders candidate iteration, or changes
   which bits survive into the pool, changes the sampled mod for a given seed.
   - Tests pin only **structural invariants** (mod counts 4–6, exclusivity-group
     distinctness, side caps, fractured preservation) plus one numeric weight
     (`final_weight == 1089`) and a trace identity — see `engine/tests/test_actions.cpp`.
     Python bindings pin pool **summaries/membership**, not exact draw sequences.
   - `engine/src/actions_basic.cpp:17` and the public API doc both state exact seeded
     replay across engine versions is **not** a compatibility requirement.
   - **Therefore:** algorithmic changes that alter which mod is drawn for a seed are
     acceptable across versions, AND the recommended incremental sampler (P5) can be made
     **bit-identical** to today if it keeps ascending-session-id order.
   - **Safeguard:** there is currently **no test that pins an exact seeded mod sequence**, so
     a draw-order regression would pass CI silently. Add a seeded golden-sequence test
     BEFORE changing any draw order.

2. **WASM rebuild required.** Everything except the one frontend item (P-Front) is
   engine-internal C++ and needs a WASM rebuild to take effect. The committed WASM is
   prebuilt and `emcc` is not assumed present — the implementer must have a working emscripten
   toolchain to build/validate.

3. **Measure before rebuilding.** The engine already has perf counters behind
   `perf_timing_enabled` (`engine/src/engine_internal.hpp:411-417`): `candidate_build_ns`,
   `weighted_pool_build_ns`, `sampling_ns`, `pool_cache_hits/misses`. A throughput harness
   exists at `apps/web/test/performance-benchmark.ts`. Enable the counters on a chaos/reforge
   run to size each change before committing.

---

## 1. How mod-picking works today (mechanism primer)

- A reforge (chaos/alchemy/Chaos/essence/fossil/harvest-reforge) preserves fractured/locked
  slots, then refills to a target of 4–6 mods by looping `add_random_mod`:
  `engine/src/actions_basic.cpp:344-386` (`reforge`), refill loop at `:375-380`.
- `add_random_mod` (`engine/src/actions_basic.cpp:176-247`) calls `get_weighted_pool` **once
  per pick** (`:200-201`), then draws one `rng.next_below(pool.total_weight)` and
  `lower_bound`s into `pool.prefix_sums` (`:210-213`).
- `get_weighted_pool` (`engine/src/session_builder.cpp:956-1127`) rebuilds the candidate
  bitmask from scratch every call (~6–9 full-width word passes), constructs a `PoolCacheKey`
  **from the full candidate mask**, looks it up, and on a miss walks the candidate set to
  build `entries` + `prefix_sums`.

**Key facts established by the audit:**
- Weight *gathering* for the common path is a precomputed array index
  (`session.base_roll_weight[s]`), table built **once per session** in `build_session`
  (`engine/src/session_builder.cpp:778-786`). There is **no per-action weight-table build**.
- The candidate-mask rebuild (the ~6–9 passes) runs **before** the cache lookup
  (`engine/src/session_builder.cpp:1052`), so a cache **hit still pays the full mask build**;
  only the entry/prefix-sum build is skipped.
- Within one reforge, the candidate mask changes every pick (group-blocking grows, a side
  closes when full), so the full-mask cache key differs each pick → picks 2…N essentially
  miss. The cache mainly helps pick-1 across runs.

---

## 2. Prioritized work items

Ordered by ROI (effort vs. payoff vs. risk). IDs are stable references.

### P1 — Guard the per-step trace entry (incl. ~1.1 KB item copy) when no trace is retained
- **Impact:** HIGH · **Risk:** very low · **Provenance:** [self-verified] (verifier lost to session limit)
- **Location:** `engine/src/simulator.cpp:800-812` (entry build; `entry.item = result.item` at
  `:811`), `:607-612` (`append_trace` null/cap check), `:1022-1026` (`trace_ptr` becomes null
  once `retained_trace_count` traces captured). Struct sizes: `engine/include/poecraft/item_state.h:58-94`.
- **What happens:** On every Operation/Router step, `run_one` fully populates a
  `TraceEntryInternal` — including `entry.node_id` (string), `entry.matched_edge_id` (string),
  and `entry.item = result.item`, a full **~1.1 KB `pc_item_state` copy** — *then* calls
  `append_trace`, which returns immediately when `trace == nullptr || max_trace_entries == 0`.
  `retained_trace_count` is small (≈10, or 0 headless), so most of 100k runs pass
  `trace_ptr == nullptr` and the entire entry (including the item copy) is built and discarded.
  `std::move(entry)` does NOT help: the trivially-copyable 1.1 KB item is copied at `:811`
  before the move. (`pc_mod_slot` ≈ 60 B; 18 slots ⇒ `pc_item_state` ≈ 1100 B; the `rolls[8]`
  int32 array per slot dominates and is never written on this path.)
- **Estimated waste:** ~200–400 MB of memcpy per 100k-run sim, plus the discarded struct work.
- **Fix:** Hoist the existing null/cap check to the call site — wrap the entry construction at
  `:800-812` (and the analogous terminal/`finish_failure` entries at `:687-698`, `:720-730`) in
  `if (trace != nullptr && options.max_trace_entries != 0) { ... }`, or pass a capture bool into
  `run_one`. Keep the guard condition identical to `append_trace`'s so retained traces are
  byte-for-byte unchanged.
- **Determinism:** None affected — pure observation-side bookkeeping; no `rng`, no candidate
  order, no `select_edge` impact.

### P2 — Skip building `SimulationExampleInternal` (incl. ~1.1 KB item copy) when retention caps are full
- **Impact:** MEDIUM · **Risk:** low · **Provenance:** [self-verified]
- **Location:** `engine/src/simulator.cpp:1069-1085`; `example.item = result.item` at `:1076`;
  cap checks at `:1077-1084`.
- **What happens:** After each run, `run_simulator_chunk` unconditionally builds an example —
  including the ~1.1 KB item copy and a `terminal_node_id` string copy — *then* checks
  `success_examples.size() < retained_success_count` / failure equivalent. `retained_*_count`
  are tiny (a handful), so once full (the common case) the whole example is built and thrown away.
- **Estimated waste:** ~100k discarded ~1.1 KB copies per sim.
- **Fix:** Compute `bool want = (result.terminal_kind == PC_TERMINAL_SUCCESS) ? success_examples.size() < retained_success_count : failure_examples.size() < retained_failure_count;`
  and skip the `:1069-1076` build when `!want`. Keep the success-vs-failure selection consistent
  with the existing `terminal_kind` branch so the same first-N runs are retained.
- **Determinism:** None — observation-only; the same runs are retained.

### P3 — Free micro-wins inside `get_weighted_pool`
- **Impact:** LOW (but free) · **Risk:** very low · **Provenance:** [verified]
- **Locations / fixes:**
  1. **Dead zero-fills.** `engine/src/session_builder.cpp:970`
     (`candidate_mask_scratch.assign(session.words, 0)`) is immediately overwritten by
     `candidate = session.normal_random_roll_mask` (full copy) at `:980`/`:973`. Same at
     `:1017` (`influence_mask_scratch.assign(...)`) → overwritten by copy at `:1019`. Remove the
     `assign`s. **Caveat:** keep a zero-fill (or skip the AND) for the `influence_masks` empty
     case at `:1018`, where the copy is skipped and the still-zeroed scratch is consumed at `:1030`.
  2. **No `reserve` on the miss-path pool vectors.** `engine/src/session_builder.cpp:1063-1112`
     push_backs into `pool.entries` / `pool.prefix_sums` with no reserve. Add
     `const std::size_t n = pc_bitset_count(candidate.data(), session.words); pool.entries.reserve(n); pool.prefix_sums.reserve(n);`
     before the `pc_bitset_for_each` at `:1064`. `n` is an exact upper bound (the loop only ever
     skips bits), so it never under-allocates.
- **Determinism:** None — capacity/scratch only; contents and order unchanged.

### P4 — Flatten `group_masks` + maintain the group-block mask incrementally
- **Impact:** MEDIUM · **Risk:** low · **Provenance:** [self-verified]
- **Location:** `engine/src/session_builder.cpp:145-175` (`build_current_group_block_mask`),
  `:262-264` (`group_masks`/`implicit_tag_masks` decls), `:364-381`
  (`apply_metamod_pool_blocks`). Built once in `build_session` at `:711-713`.
- **What happens:** On **every** `get_weighted_pool` call (before the cache check), for each
  occupied prefix/suffix slot, the code walks the mod's groups and does
  `session.group_masks.find(group_id)` — an `unordered_map<uint32_t, vector<uint64_t>>` hash
  lookup at `:156` — then `or_into` merges that group's word array. This is the densest
  hash-lookup site in the per-pick loop, and it re-ORs **all** live slots every pick.
- **Fix (two parts):**
  1. Replace `session.group_masks` and `session.implicit_tag_masks` with **flat
     `vector<vector<uint64_t>>` indexed by dense group/tag id**, sized to the max id in the
     session (most entries empty — acceptable memory). Inner loop becomes
     `const auto& m = group_masks_flat[g]; if (!m.empty()) or_into(...)` with no hashing.
  2. Maintain the block mask **incrementally** within a reforge: OR in only the newly added
     mod's groups per pick rather than re-scanning all slots. **Must** reset correctly at
     reforge start (`restore_slots`, `engine/src/actions_basic.cpp:328-339`) or it leaks stale
     group blocks and silently changes candidates.
- **Determinism:** Safe for part 1 (same groups, same order, different lookup). Part 2 must
  produce a byte-identical block mask to the from-scratch OR (validate against fixtures).

### P5 — ★ Incremental sample-and-delete for the 4–6 mod refill loop (highest leverage on the core question)
- **Impact:** HIGH · **Risk:** medium (determinism-critical, but achievable bit-identical) · **Provenance:** [verified]
- **Location:** `engine/src/actions_basic.cpp:375-380` (refill loop) →
  `engine/src/session_builder.cpp:956-1127` (`get_weighted_pool` rebuilt per pick).
- **What happens:** For a 4–6 mod reforge, `get_weighted_pool` is invoked 4–6× and each call
  rebuilds the full candidate mask (~6–9 full-width passes) and, on miss, re-walks the candidate
  set to rebuild `entries`/`prefix_sums`. The per-mod weights do **not** change between picks —
  only (a) newly occupied groups get blocked and (b) a side closes when it fills.
- **Fix:** Build the weighted candidate pool **once** at reforge start (for the open sides),
  then per pick: sample, then delete the chosen mod + all mods sharing its now-occupied group(s),
  and drop a whole side's entries when that side hits its cap. Use a **Fenwick/BIT (or segment
  tree) over the entries in ascending session-mod-id order with lazy zeroing of deleted leaves**
  for O(log n) sample-and-delete.
- **Payoff:** Collapses per-reforge cost from `O(picks × (~8 mask passes + full candidate walk))`
  to `O(candidates)` once + `O(picks × (group_size + log n))` — roughly a **4–6× cut** in the
  dominant per-pick work.
- **Determinism (critical):** A Fenwick keyed on the **same ascending session-mod-id order**
  with lazy deletion preserves both `total_weight` and the `lower_bound → entry` mapping that
  `rng.next_below` consumes, so it can be made **bit-identical** to today. Requirements:
  (a) per-pick total equals the sum of exactly the surviving entries; (b) cumulative ordering
  stays ascending session-mod-id; (c) the side-close transition removes exactly the same entries
  at the same pick. A group-**partitioned reorder** would change which mod a roll maps to — do
  NOT use that variant. Land the seeded golden-sequence test (Constraint 0.1) first.

### P6 — Structural pre-key so a cache hit skips the candidate build
- **Impact:** MEDIUM · **Risk:** medium (fingerprint correctness) · **Provenance:** [verified]
- **Location:** `engine/src/session_builder.cpp:1039-1057` (key build + lookup); key struct
  `engine/src/engine_internal.hpp:343-357`; hash `engine/src/session_builder.cpp:385-398`.
- **What happens:** `PoolCacheKey.candidate_mask` is the full uint64 vector, so the entire
  candidate mask must be built (the ~6–9 passes) before the key exists and the cache is checked.
  A hit at `:1053-1056` has already paid the full mask build + a full-width key copy (`:1040`)
  and per-word hash (`:385-398`).
- **Fix:** Compute a cheap structural key **before** the bitset work —
  `{tag_signature_id, weight_kind, target_tag_id, side_filter, influence_bits, fossil_indices,
  fingerprint(occupied-group set)}` — and look the cache up on it. On a hit, return the cached
  pool without building the candidate mask; on a miss, build as today.
- **Risk:** The fingerprint must be **injective** w.r.t. every input that changes the mask:
  occupied-group set (mirror `build_current_group_block_mask`'s `mod_id < mod_count ? group_offsets/group_ids : slot.group_id` logic exactly), metamod attack/caster blocks
  (`apply_metamod_pool_blocks`, `:377-380`), `item->generic_influence_bits`, `side_filter`,
  `tag_signature_id`, `weight_kind`, `target_tag_id`, `fossil_indices`. A wrong fingerprint
  silently serves the wrong pool and corrupts the sim. (Note this partially overlaps P5; if P5
  lands, picks 2…N become in-place deletions and this matters mainly for pick-1 reuse across runs.)

### P-Front — (frontend only) Cache `JSON.stringify(options)` across chunks
- **Impact:** negligible · **Risk:** none · **Provenance:** [verified] · **No rebuild needed.**
- **Location:** `apps/web/src/app/engine-wasm.ts:281-291` re-stringifies options every chunk;
  the C++ side re-parses + re-validates each chunk (`bindings/wasm/wasm_api.cpp:1221-1252`,
  `engine/src/api.cpp:1488-1545`). ~13–15 chunks per 100k-run sim, so this is tiny.
- **Fix:** Compute `JSON.stringify(options)` once per `runStrategy` and reuse the identical
  string across chunks. Bytes are identical (options are immutable mid-run, enforced by
  `options_equal`), so determinism is unaffected. Removing the C++-side re-parse would need a
  rebuild and isn't worth it at ~15 calls.

---

## 3. Deferred / lower-priority (same workload)

### D1 — Elide the double item copy per action (`working` + copy-back)
- **Impact:** MEDIUM · **Risk:** medium · **Provenance:** [self-verified]
- **Location:** `engine/src/simulator.cpp:775` (`pc_item_state working = result.item;`), `:776-777`
  (`apply_action(..., &working, ...)`), `:785` (`result.item = working;`).
- **What happens:** Two ~1.1 KB copies per action for rollback, even though chaos/alch/reforge
  almost always succeed. `apply_action`/`reforge` mutate in place; the failure path breaks
  without writing back.
- **Fix:** Apply directly to `result.item` and snapshot only for the rare `!applied` path — **but
  audit each action first.** Some actions partially mutate before returning `applied=false`
  (e.g. `InfluenceExalt` sets `generic_influence_bits` then reverts on failure,
  `engine/src/actions_basic.cpp:1122-1130`), so a blanket "apply in place" is unsafe. Safest
  intermediate: keep one rollback snapshot but remove the redundant copy-back.
- **Determinism:** Copies don't touch `rng`; correctness hinges on rollback still restoring the
  pre-action state on `!applied` so downstream `select_edge`/`evaluate_condition` see the same item.

### D2 — Shrink `pc_item_state` (drop/relocate cold fields)
- **Impact:** LOW · **Risk:** ABI · **Provenance:** [self-verified]
- **Location:** `engine/include/poecraft/item_state.h:58-94`.
- **What happens:** ~1.1 KB struct, **~94% cold** for this workload (`implicits[8]`,
  `enchantments[4]`, `rolls[8]` per slot, sockets — untouched by chaos/alch/reforge). Shrinking
  it (e.g. drop/relocate `rolls[]`, or split a lean hot state from a cold tail) roughly halves
  every item copy in P1/P2/D1.
- **Risk:** Changes the ABI / `pc_data_check_capacities` contract and any code reading `rolls`
  for display — re-validate those. No RNG impact (pure layout). Treat as a follow-up to P1/P2/D1.

---

## 4. Confirmed NON-issues — do not spend effort here

All independently verified as off the chaos/alch/reforge hot path or one-time cost.

| Suspected hotspot | Verdict | Why it's cold |
|---|---|---|
| Per-action weight-table build | negligible | `base_roll_weight` table built once/session; common path is an array index (`session_builder.cpp:778-786`, `:1067-1070`). `intern_item_tag_signature` short-circuits at `:921-923` with no allocation. |
| Fossil multiplier recompute per candidate | negligible | Only fires for `weight_kind==Fossil` (fossil reforges), not plain chaos/alch (`session_builder.cpp:1071-1086`). Determinism-safe to memoize if fossil reforges become hot. |
| `pick_weighted_id` / `active_spawn_weight` per-candidate `unordered_set` rebuild | negligible | Real inefficiency but **off this path** — only unveil/eldritch/bloodstained call it (`actions_basic.cpp:128-160`, callers at `:559/:816/:916`). For chaos/alch/reforge it runs **zero** times. |
| `do_harvest_resist` under "pick_weighted_id callers" | n/a (seed correction) | It uses the cached pool, not `pick_weighted_id` (`actions_basic.cpp:767-779`). |
| `signature_key` std::string map key / `first_matching` set lookups | negligible | Slow path of `intern_item_tag_signature` only; ≤256 lifetime, zero for non-influenced workload. |
| Frontend chunk loop / progress spam | negligible | ~13–15 chunks for 100k runs (adaptive, capped 10k); progress is rAF-throttled (`engine-worker.ts:199-252`, `pc-strategy-editor.ts:1296-1360`). |
| Per-chunk options re-parse/re-validate | negligible | Bounded by chunk count (~15), not run count. See P-Front. |
| Full `simulatorResult` marshalling | negligible | Called **once at end** (and on cancel), not per chunk (`engine-worker.ts:218-252`). Per-chunk payload is a ~60-byte progress object. |
| `ActionContextImpl` / pool caches per chunk | negligible (good — don't regress) | Created **once**, lazily, on first chunk; caches persist across chunks (`simulator.cpp:1007-1011`). |
| `bitset_and/or/andnot` scalar word loops | not the issue | Auto-vectorize fine; cost is the **number** of full-width passes (addressed by P5/P6), not per-pass. |
| `missing_keys` vector per Operation step | negligible | Empty `std::vector` = stack only, no heap alloc unless a price actually misses (`simulator.cpp:743-756`). |
| `pc_item_clear` memset + per-slot re-init | negligible | Runs once per sim at compile time, not per run (`item_state.cpp:45-56`; sim copies `start_item` instead). |
| `first_blocking_group` / `build_pool_debug_rows` per-call hash containers | negligible | Debug/inspection API only, not the sim loop (`session_builder.cpp:115-143`, `:1159-1162`). |

---

## 5. Provenance & recovery notes

- 7 analysis dimensions, 30 findings, all with `file:line` evidence. 28 of 37 agents completed;
  9 verifier agents were killed by a session limit (3 in the run-loop dimension, 6 in the
  data-layout dimension), which is why P1, P2, P4, D1, D2 are **[self-verified]** rather than
  **[verified]**. **No finding was rejected** by any verifier that ran (16 confirmed-with-caveats,
  5 confirmed).
- After hotness correction, the independent verdict impact distribution was: 1 high, 4 medium,
  2 low, 14 negligible (the 14 negligible are the §4 non-issues — the audit's main value there is
  telling you where NOT to look).
- Full verbatim dump (all 30 findings + 19 verdicts) was recovered to
  `C:\Users\Oliver\.claude\projects\RECOVERED_FINDINGS.txt`.

## 6. Suggested implementation order

1. **P1, P2** — guard the per-step trace copy and the example copy. Tiny, no determinism risk,
   removes hundreds of MB of memcpy. Best ROI.
2. **P3** — delete the dead zero-fills, add the `reserve`. Free.
3. **P4** — flatten `group_masks` + incremental block mask.
4. **P6** — structural pre-key so hits skip the candidate rebuild.
5. **P5** — incremental Fenwick refill. Highest leverage on the 4–6 mod question; largest change.
   Land the seeded golden-sequence test first; verify bit-identical output.
6. **D1, D2** — item-copy elision and struct slimming, after the above.

Measure with `perf_timing_enabled` + `apps/web/test/performance-benchmark.ts` before and after
each engine change.
