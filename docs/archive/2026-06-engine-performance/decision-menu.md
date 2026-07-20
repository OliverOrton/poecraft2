# Engine Speedups & Cheats — decision menu

> **Archived 2026-07-17.** Historical decision menu from 2026-06-30. It does
> not own current sequencing.

**Date:** 2026-06-30 · companion to [`review.md`](review.md) (evidence,
`file:line`, measured baseline). This is the short menu for codex to pick from.

## Anchor (measured, native g++ -O2, BodyInt17 iLvl86)
- **346 ns/reforge · 289k runs/s · 100k×10-chaos runs in 346 ms · 98.9% pool-cache hit.**
- ~80% of per-action cost is the **weighted sampler** (`lower_bound` over `prefix_sums`, ~56 ns/call).
- Single-core floor ≈ 70–95 ns/reforge ⇒ ~4–4.5× headroom. Parallelism is an orthogonal ~W× on top.

**Two ways to go faster:** compute the *same events* faster (Optimizations), or *don't compute
most of the events* (Cheats). Cheats are legal because exact per-seed replay is **not** required —
only the output **distribution** (success rate, cost/outcome dists, examples) must match.

---

## Optimizations — same result, faster compute

| # | Idea | Payoff | Effort | Risk / gate |
|---|---|---|---|---|
| **O1** | **Alias-table sampling** (Walker/Vose) cached on `WeightedPool`; O(1) draw instead of O(log n) binary search. The single biggest lever. | **~3×** (most of the headroom) | Med | Changes draw order → land golden + statistical-equivalence test first (F1). See §G. |
| **O2** | Slim `pc_item_state` / carry projection-only state (groups+counts+rarity) when no condition reads rolls/identity. | ~10–15% **after O1**; noise before it | Med | ABI / `pc_data_check_capacities`. See F6/D2. |
| **O3** | **Parallel workers** — runs are independent, data/session immutable, one `ActionContext` per thread (already supported). | **~W×** wall-clock (8 cores ⇒ 100k in ~10–15 ms) | Low–Med | None, if per-run seeds derived deterministically. |
| **O4** | Delete the dead metamod re-check in `add_random_mod_from_superset` (superset already applied it). | Free | Trivial | Bit-identical — safe now. See F3. |
| **O5** | Maintain group blocking as a small occupied-group set instead of a full-width bitmask OR per pick. | Small | Low | Must match current blocking exactly. |

## Cheats — same distribution, skip the events

| # | Idea | Payoff | Effort | Exact? / catch |
|---|---|---|---|---|
| **C1** | **Geometric run-length collapse** — for "spam action until condition", draw attempt count from Geometric(p) in one shot; materialize only the success-conditioned final item. | **~100×** on spam strategies | Med | **Exact** if attempts are memoryless (same item+action each try). Breaks if the strategy keeps partial progress → use C3. Gives full cost distribution free. |
| **C2** | **Analytic branch probabilities** — a reforge condition (`has_group`/`has_family`/`open_*`) has a closed form / tiny DP over the 4–6 picks. Compute the branch split instead of sampling the reforge. | Removes the sampler on that branch | Med | **Exact** (validate the DP). Conjunctions need a small multi-group DP; arbitrary nesting → fall back to MC. Building block for C1 & C3. |
| **C3** | **"Exact odds" mode** — quotient items by the conditions they satisfy, compile the strategy to a Markov chain over condition-classes (transitions from C2), solve the absorbing chain. Zero runs. | **Instant, zero-variance** | High | **Exact** only when the projection is a sufficient statistic & the class graph stays small; **detect & fall back to MC** otherwise. Still MC a few runs for example items. |
| **C4** | **Importance sampling** for rare targets (p≈1e-5) — bias draws toward the target, reweight to stay unbiased. | **100–1000×** fewer runs for rare events | Med | Unbiased **only if reweighting is exact** — most dangerous cheat; validate the headline number hard. |
| **C5** | **Variance reduction** — stratify the count draw (4/5/6) & first mod, antithetic / common random numbers, or QMC (Sobol) on low-dim draws. | **~2–10×** fewer runs for same CI | Low | Low risk; drop-in. QMC edge degrades on discontinuous estimands — measure. |
| **C6** | Micro: fuse unconditional action chains at compile time; memoize convergent (projection-state, node) outcome distributions. | Modest | Low | Exact. |

---

## Recommended order
1. **C5 + O4** — free / low-risk, do now.
2. **O1 (alias)** — land F1 tests first; captures ~80% of the single-core headroom.
3. **C1 + C2** — big, *exact* wins on the strategy shapes people actually build.
4. **C4** — the moment any target outcome is rare.
5. **C3** — the real prize; separate mode, MC fallback.
6. **O2 / O3** — O3 (parallelism) anytime for wall-clock; O2 once O1 makes the item copy relatively visible.

## Validation discipline (non-negotiable)
Every cheat is gated behind a check vs. brute-force MC on fixtures. For an odds tool, **wrong-but-fast
is a bug, not an optimization** — a cheat that silently biases P(success) must fail CI. Same gate as F1
(golden seeded sequence + statistical equivalence).
