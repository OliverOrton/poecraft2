# Lower-Bound And Carrier-Prioritization Findings

**Status: non-authoritative investigation notes.** This is raw input for a
future plan. It selects no boundary, authorizes no implementation, and owns no
contract. Implemented behavior lives in [Solver](README.md); deferred designs
live in the [solver roadmap](../future/solver-roadmap.md).

Parent: [Solver Notes](NOTES.md)

## Provenance And Limits

Assembled 2026-08-24 from source reading at `1f68497` plus the committed run
artifacts under `docs/active/`. Nothing was executed: `data/sqlite/` and
`data/compiled/` are gitignored and empty in the working checkout, so the
solver could not be run and no measurement here is new. Every numeric claim is
either quoted from a committed diagnostic or derived from source.

Items marked **[unconfirmed]** rest on a single artifact and should be
reproduced before anything is built on them.

## A. What The Published Lower Currently Is

- `certified_global_lower_bound()` returns the independent goal-cover floor
  and discards the search's own value whenever the incremental action envelope
  is open — `solver_solve_constructive.cpp:2742-2745`, inside
  `globally_certified_action_envelope_lower_bound`.
- The reasoning is sound: minimizing over a subset of actions can only raise
  the optimum, so a restricted optimum is not a lower bound for the requested
  envelope. The consequence is that the published lower is a value computed
  before search begins.
- In the accepted five-T1 run the envelope never closed:
  `incremental_action_envelope: {closed: false, remaining_action_envelope: 125173}`.
- The lower-bound authority classifier is
  `classify_public_lower_bound_authority` — `solver_solve_constructive.cpp:2754-2806`.
  The published provenance in this run was `independent_global_floor`.

## B. Why The Value Does Not Move

- Partial-graph harvest exists and runs every round. Two writers:
  `solver_solve_focused.cpp:756-757` and `solver_solve_focused.cpp:1320-1323`.
  The second is the one live under
  `incremental_action_generation && !incremental_envelope_closed`. Additional
  writers at `:1012`, `:1045`, `:1077` and `solver_solve_finish.cpp:1285-1288`
  are closure/proof paths that set lower equal to upper.
- Cross-round value retention takes a maximum for known states —
  `solver_solve_focused.cpp:45-56` (and the behavioral-representative branch at
  `:58-81`), so known states cannot regress.
- Empirically the harvested value is static. The committed bound trace has
  **116 samples**; the lower rose once at sample 0 → 1 and was bit-identical
  `36.4286171890906` for the remaining 114. The upper moved repeatedly across
  the same window (`470485205.19` → `427203631.39` → `87361.17`).
- Telemetry reports `restricted_action_envelope_lower_bound` equal to
  `independent_goal_cover_lower_bound` to all digits. The former is
  `diagnostics.focused_lower_bound` — `solver_solve_telemetry.cpp:3768-3769`.
- **[unconfirmed]** `states.goal = 0` in that run: the 14,372-state graph
  contained no terminal states. The counter is a real scan over discovered
  states (`solver_solve_telemetry.cpp:460-468`) and reports non-zero elsewhere
  (`docs/active/2026-08-22-compact-policy-certification/*.json`: 105, 57,
  15984, 1), so the field works. If it is accurate, the lower Bellman has no
  terminal anchor and every value traces to a frontier heuristic seed, which
  would explain bit-exact staticity rather than slow creep.
- Frontier is 8,650 of 14,372 discovered; `policy_reachable` is 1,868. The
  published upper comes from the constructive/fallback policy machinery, not
  from search reaching a goal.

## C. Where The Cover Is Weak

- **Universal cover is a deterministic set cover.** `relax_cover(goal_cover_cost, false)`
  passes `probability_aware = false`, so probability is hardcoded `1.0` —
  `solver_solve_heuristics.cpp:650-653`. Each action grants any reachable
  subset for its list price.
- **The zero return.** `optimistic_completion_cost` at
  `solver_solve_heuristics.cpp:1722-1724`:
  `if (!clean_carrier && satisfied_count >= required) return 0.0;` — no
  affix-count condition.
- **Clean eligibility is narrow.** `clean_goal_cover_eligible`
  (`solver_solve_heuristics.cpp:1752-1775`) rejects any carrier with
  `(flags & kProtectionFlags) != 0`, any fractured goal/metamod/junk state, and
  any influence or Eldritch tier differing from the start.
  `kProtectionFlags = kFlagMultimod | kFlagNoAttack | kFlagNoCaster | kFlagPrefixesLocked | kFlagSuffixesLocked`
  — `solver_solve_types.hpp:240-242`.
- **Combined effect worth checking first.** Once any metamod is active the
  carrier leaves the clean cover, and once its mask is satisfied its lower
  becomes `0` regardless of remaining junk and regardless of being provably
  nonterminal. This is on the accepted policy's own path: that strategy spends
  243.98 temporary-blocker cleanup cycles per success and selects protected
  Harvest Reforge.
- **Probability slack (not a bug — direction is correct).** Cost is `c / p`, so
  admissibility requires an *upper* bound on `p`; the current constructions are
  in the right direction but loose:
  - `probability *= std::min(1.0, placements * best_order)` where `placements`
    is the falling factorial applied as if draws were independent —
    `solver_solve_heuristics.cpp:544-550`. Multi-slot hits can saturate to
    certainty.
  - `best_order` is a maximum over slot permutations — `:517-543`.
  - `optimistic_goal_draw_probability` clears both carrier sides before
    building the pool (`solver_options_temporary.cpp:257-258`) and grants each
    blocker the strongest group-exclusion effect in the session, summed even
    when effects overlap (`:345-395`).
- **Strict clean cover is off for Eldritch sessions** —
  `solver_solve_heuristics.cpp:1799-1804`. `HANDOFF.md:277-278` states the
  prerequisite: "until it models automatic side options."
- **Compound operators are atomized into individually-purchasable primitives.**
  `prepare_goal_cover_cost` includes each `TemporaryBenchEffectClass`
  `followup_action` and every member of `blocker_actions`
  (`solver_solve_heuristics.cpp:580-586`), plus `setup_action`,
  `cleanup_action`, `conditional_action`, and every `primitive_program` member
  (`:551-575`). Deliberate and sound — the in-source comment says granting one
  primitive the whole reachable subset for its own price is "cheaper and more
  capable than every real setup-bearing compound." It is nonetheless where a
  setup-bearing macro loses its setup price.

## D. What The Cover Already Does Correctly

Recorded so a plan does not redo it:

- **The clean MDP already requires junk gone at its terminal.**
  `is_abstract_goal` is
  `rarity == goal && popcount(mask) >= required && prefixes + suffixes == popcount(mask)`
  — `solver_solve_heuristics.cpp:720-727`. Junk is charged, by count.
- **The clean MDP is side- and capacity-aware.** Indexed by
  `(rarity, mask, prefix_count, suffix_count)` —
  `solver_solve_heuristics.cpp:698-706`, sized
  `kRarityCount * mask_count * 4 * 4` = 1,536 doubles for a five-slot goal.
- **It is weight- and tag-aware.** `subset_probability` consumes
  `optimistic_goal_draw_probability` (`solver_options_temporary.cpp:242-395`),
  which builds the real weighted pool and handles fossil weight kinds, Harvest
  `TargetedNatural` target tags, group exclusion against already-satisfied
  slots, and per-side filtering.
- **It models destructive replacement.** The clean branch is a finite MDP where
  destructive rolls replace the prior goal subset, not an acyclic set cover.
- **Bench blockers are not lost in the search abstraction.** Junk classes are
  keyed by `{gen_type, tag_bits, goal_block_mask, special_role, metamod_role,
  required_level, exclusion_effect_mask, count_observation_bits, …}` —
  `solver_abstract.cpp:803-841`. Crafted, natural and fractured junk are
  counted separately (`solver_model.hpp:1240-1245`), and `blocked_mask` records
  which goal slots a non-member explicit blocks (`solver_abstract.cpp:920-940`).
  What the *cover* loses is junk identity, not junk presence.

## E. What Consumes Lower Bounds

Relevant because the effect of a better lower is mostly not in the reported
gap. Items 1–3 consume internal per-state and per-operator lowers, not the
published certified bound, so they improve regardless of the certification gate
in section A.

1. **Operator pruning at expansion.** `solver_solve_expand.cpp:962-985` removes
   operators whose `optimistic_operator_lower(state, op)` exceeds the incumbent
   upper. That function is immediate price plus the same goal cover —
   `solver_solve_heuristics.cpp:2515-2600`. Documented at
   `docs/solver/README.md:849-852`.
   - In the accepted run it **never fired**. The only recorded pruning reasons
     are `not_permitted_by_explicit_goal_envelope` (15,420) and
     `outside_product_goal_relevance` (98). This is the cheapest available
     leading indicator for any cover change.
2. **Gap-directed fringe priority.** `collect_focused_fringe` weights frontier
   states by `path_mass × (upper − lower)` —
   `solver_solve_focused.cpp:168-199`. With a near-constant lower the gap term
   approaches the upper and scheduling silently degenerates to mass-directed.
3. **Preservation pruning.** `preservation_decision` uses
   `candidate_lower_bound = priced.cost` — immediate price with no continuation
   term (`solver_solve_expand.cpp:1213`, decision logic `:1169-1250`).
4. **Gap-target termination.** `satisfied_gap_target`
   (`solver_solve_constructive.cpp:2816+`) cannot fire for any product
   "stop at X% gap" while the lower is this small.

## F. Carrier And Action Prioritization

The originating questions, kept separate from the lower-bound work because they
are ordering-only and carry no admissibility obligation.

- **Ladder ordering is progress-count only.** The focused ladder buckets by
  satisfied-goal mask and orders buckets by `popcount(mask)` desc, tie-broken
  by the front state's priority — `solver_solve_focused.cpp:493-562`,
  comparator `:511-526`. There is no within-bucket ordering; buckets inherit
  fringe order.
- **The incremental ladder does have a within-bucket order** — fractured desc,
  protection desc, then `unrelated = explicit_count − satisfied_count` asc —
  `solver_solve_incremental.cpp:143-164`. The two paths are asymmetric.
- **Side occupancy is a diversity key, not a preference.** `prefix_count` and
  `suffix_count` appear in `focused_schedule_signature`
  (`solver_solve_quotient.cpp:494-496`), which caps representatives per class
  but expresses no ordering.
- **A side-aware ranking already exists uncalled.** `optimistic_completion_cost_for_state`
  (`solver_solve_heuristics.cpp:1777-1815`) is a memoized table lookup keyed by
  rarity, mask and both side counts. It seeds lower values
  (`solver_solve_focused.cpp:17-21`) but is not used as a ladder ranking key.
- **Per-slot side is already computed.** `slot_side[]` —
  `solver_solve_heuristics.cpp:405-414`. Missing-slots-on-side against free
  capacity is a mandatory-removal predicate, not merely a bias, and it is
  per-state rather than per-mask.
- **`blocked_mask` is observed but unranked.** It is in the schedule signature
  (`solver_solve_quotient.cpp:491`) and in no ordering. Ranking by
  `popcount(blocked_mask)` ascending needs no new computation.
- **Row selection ties break by row index.** `sparse_policy_row_precedes` —
  `solver_sparse_policy.cpp:382-395`. Rows already carry
  `preservation_effect` with `preserved_properties` / `destroyed_properties`
  including `kCarrierJunkBlockers` (`solver_solve_types.hpp:116-131`), so a
  junk-debt tiebreak is computable from existing data.

## G. Prior Art That Constrains The Design

- **Setup-price envelope, proved but shelved.**
  `docs/archive/2026-07-25-exact-automatic-action-constraint-generation/report.md:66`
  defines `ell(d, s) = price(mandatory setup primitive of d)` and the milestone
  also proved complete temporary-bench deferral. Per
  `docs/future/solver-roadmap.md:44-49` production integration was rejected
  because all four hard cases "transferred the 200,000-state start-carrier
  failure to their first ordinary broad reforge" — a state-cap reason, not a
  soundness reason. Whether that blocker still binds is worth establishing
  early; it may supply a ready construction.
- **The junk-bench failure mechanism.**
  `docs/archive/2026-07-27-pre-expansion-probability-lower-audit/report.md:38-42`:
  for a junk bench that changed no represented goal-progress state the MDP had
  "no conditioned transition", and the sound fallback "charged the exact
  first-action price and granted a **free terminal continuation**". Measured
  competing-class lowers were `0.005872` and `0.014770` against best-class
  lowers of `15.98` to `431.4` (`report.md:68-71`). Note this was a separate
  graph-free artifact keyed on rarity, satisfied-goal mask and affix count; its
  failure mode does **not** transfer verbatim to today's clean cover, which has
  the count-based terminal in section D.
- **What changed since.** `is_goal_state` now requires
  `explicit_affixes == satisfied` — `solver_calc.cpp:1312-1315`, landed
  2026-08-22. Junk must be gone at success. The roadmap's standing note that
  "the goal-mask abstraction discards non-goal blocker effects"
  (`docs/future/solver-roadmap.md:124-136`) predates that change.
- **The roadmap's stated requirement for any future lower**: it "must retain
  enough non-goal first-action, blocker, and preservation state to charge
  downstream work" — `docs/future/solver-roadmap.md:132-136`.

## H. Recorded As Closed Or Constrained

Not prohibitions from this note — these are the repository's own prior
findings, gathered so a plan does not rediscover them.

- Do not flip `best_order` to an average or drop the `min(1.0, …)` clamp. A
  cost lower bound needs a probability upper bound; the current direction is
  required for admissibility.
- Do not publish the July probability MDP directly. "Merely making `L` and `U`
  finite, or publishing the existing probability cover, is closed" —
  `docs/future/solver-roadmap.md:132-136`.
- Do not merge partial states by satisfied-goal count. The deferred bounded
  Pareto admission design carries five proof obligations —
  `docs/future/solver-roadmap.md`, "Deferred bounded Pareto admission design".
- Metamod handling is more constrained than junk. Metamod state is already
  represented (`flags`, `kProtectionFlags`, `fractured_metamod_flags` —
  `solver_model.hpp:1230-1235`), the ladder result retains regressions in both
  directions on cleanup eagerness, and `HANDOFF.md` records an open
  deterministic-action applicability audit covering Bench and
  `RemoveCraftedModifiers`.
- Broad-kernel folding, shared reforge frontiers, and action-local side
  factorization are separately falsified in the roadmap.

## I. Open Questions Needing A Run

- Reproduce `states.goal = 0` on the five-T1 case. If accurate it means the
  Bellman half contributes nothing until search reaches terminals, which
  changes where leverage sits.
- Determine whether `36.4286171890906` comes from the universal cover or the
  clean MDP. A one-off print settles it; the whole of section C is currently
  inference on this point.
- Measure what fraction of expanded states fail `clean_goal_cover_eligible`,
  and how many of those return `0.0` from the section-C zero return.
- Establish whether the archived setup-price envelope's state-cap blocker still
  applies at current caps.
- For any cover change: does `state_incumbent_operator_lower` become non-zero?
  If not, the change did not reach the search.
- Suggested first A/B target is the four-of-five Conquest Lamellar rather than
  the five-T1: published lower `0.01165`, 17,636 open action obligations,
  roughly 22 solve seconds, same pathology, much faster iteration.

## J. Repository And Documentation Staleness

Observed while assembling the above; unrelated to the lower bound.

- `docs/archive/` stops at `2026-08-14`. All 16 directories under
  `docs/active/` are complete or stopped, while `docs/active/README.md` states
  no boundary is active.
- `docs/README.md` "Execution State" is accreting completed-boundary
  narratives rather than distilling into the area indexes.
- `docs/mechanics/README.md` is stamped "Verified against code: 2026-07-19 @
  d5e38e3" — the oldest stamp, and `CLAUDE.md` names it the mechanic authority.
  It predates the Harvest targeted-natural correction, temporary-blocker tier
  canonicalization, and `1f68497` "Align Scour planning with runtime
  applicability".
- `docs/engine/README.md` is stamped `2026-07-22 @ 042a281`.
- `docs/solver/README.md` is a 2026-07-30 base plus eight or more addenda; some
  addenda supersede base statements in place (Restart scope is one).
- `HANDOFF.md:57` records 13 stale expectation failures in the focused solver
  API executable, present on both the current tree and an untouched `2b8d5ac`
  worktree.
