# Exact solver audit — evidence appendix

Companion to [the Pro input pack](README.md). Tags: **[ran]** measured in this
audit, **[code]** source reading at `a1ba3fc`, **[record]** existing repository
evidence, **[hyp]** hypothesis. All timings are single bounded runs on Oliver's
workstation; bounded solves are timing-dependent and are not matched comparisons.

## Build and data identity

- Pack written at `main` `a1ba3fc` (local = `origin/main`).
- Data artifact `data/compiled/current`: source `repoe-b458174874efbe64`, data
  hash `d511e23d…c853e`, game-data `604e36ae…`, strings `463affab…`; unchanged
  since `aa89762`. All witness manifests below validate at `a1ba3fc`.
- Economy for every run and evaluation: Allflame snapshot
  `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0` with the
  corpus override `base = 1.0` (same as the cross-base cases).
- Native builds were rebuilt by a concurrent session during the audit and the
  benchmark does not record an executable hash:
  - ring witnesses (runs 1–5, 08:59–09:10 local): `poecraft_solver_benchmark.exe`
    built 08:51 from source ≈ `aa89762` plus uncommitted currency work;
  - bow runs (09:36–09:43): benchmark rebuilt 09:33 (source between `aa89762`
    and `b3d7884`);
  - Python evaluations used the `poecraft_engine.dll` current at the time; the two
    authored ring witnesses were re-evaluated on the 14:41 DLL (after `a468feb`)
    with identical results.
- Solver-path diff from `8101879` to `a1ba3fc` was reviewed: Calculator-only
  additions (`calculator_currency.cpp`, `pc_calc_currency_outcomes_json`,
  Calculator-only item goals) and a `ConcreteRefill` path in `solver_reforge.cpp`
  guarded by `concrete != nullptr`. No change to ordinary Current solve semantics
  was found. **Re-baseline before using any number below as a comparison anchor.**

## E1. Ring witnesses (Amethyst Ring `Ring10`, ilvl 86, product profile and caps)

Inputs: [`witness/ring`](witness/ring), [`witness/ring-crafted-junk`](witness/ring-crafted-junk),
[`witness/ring-protected-only`](witness/ring-protected-only),
[`witness/ring-ungated-diagnostic`](witness/ring-ungated-diagnostic). Case
template: CB01 (`calculator_product_v1`, 1 GiB, 50 M reforge work, 200 k states).

| Case | Goal (all T1) | Start | Result [ran] |
|---|---|---|---|
| W1 | life + mana prefixes | both + FireResist7 + ColdResist7 | **No executable policy.** `refused_state_cap` after 62.6 s; cap `max_discovered_states`; graph 171 states (3 goal); L = 84.38. Zero lock-recipe candidates across 165 admitted carriers; temporary bench 1,048 (904 eligible), Cannot Roll 134 (120), Fracture 166 (27). Recipe admission reported 303,791 discovered states / 334,476 rows / 841,721 transitions. |
| W1b | W1 + FireResist8 suffix | as W1 | Bounded 69,715.41; L = 89.11; stop `max_owned_bytes` after 9.3 s at 160 expanded / 7,276 discovered. Policy Scours both T1 prefixes and restarts from Normal (transmute/alt/aug/regal). |
| W2 | life prefix + FireResist8 + ColdResist8 | life T1 + IncreasedMana12 + LightningResist7 | Bounded 70,935.49; L = 43.95; same pattern. |
| W4 | as W1 | both + crafted `HelenaMasterFireResist1` | Bounded 2,337.20; L = 84.16; 285 expanded. `remove_crafted_modifiers` absent from the priced candidate list. |

Authored policies, native strategy evaluator [ran], files in
[`witness/authored`](witness/authored):

| Start | Policy | Success | Expected cost |
|---|---|---|---|
| W1 | `bench:StrMasterItemGenerationCannotChangePrefixes` → `scour` | 1 | 424.3741 |
| W4 | `remove_crafted_modifiers` | 1 | 0.3741 |

Admission cost at a single carrier [ran] (`automatic_candidates.admission_phases`):

| Case | Carriers admitted | Discovered states | Transition entries | Logical reroll work |
|---|---:|---:|---:|---:|
| W1b | 1 | 904,368 | 104,146,655 | 50,790,412 |
| W2 | 1 | 1,619,409 | 1,390,180,656 | 173,138,758 |

Recipe admission work is exempt from `max_reforge_work` by design
([solver_model.hpp:253](../../../engine/src/solver_model.hpp)). Peak tracked
owned bytes were only ≈285–292 MB when the 1 GiB `max_owned_bytes` stop fired;
the binding quantity is probably transient admission context memory [hyp].

Protected-only diagnostic (case field `planner_envelope_diagnostic_v1` with
`automatic_candidate_kinds = ["protected_metamod"]`) [ran]:

- W1b-pm / W2-pm: exactly one lock-recipe record in the whole solve, at a later
  state (`option:protected_repeat:prefix:alteration:until:1:2`, rejected as illegal
  on a rare), plus one generation record deferred with
  `price_independent_kernel_generation_max_owned_bytes`. **No admission record
  at the root**, although its synthesis conditions are met there.
- W1-pm: `internal_error: bounded strict finish lost its verified artifact` at
  45.15 s; **reproduced** at 45.19 s (run 5).
- With `goal_progress_gated_reforges: false` added (ungated diagnostic):
  `internal_error: solver exceeded max_reforge_work (50000000)` after ≈2 s in
  both cases instead of a named stop.

## E2. Bow (Spine Bow `Bow20`, ilvl 86) — CB04 and last-mile variants

Inputs: [`witness/bow`](witness/bow) (CB04 case, product settings; last-mile
variants use a 120 s requested finish). Goal: `LocalIncreaseSocketedGemLevel1`,
`LocalAddedPhysicalDamageTwoHand9`, `LocalAddedColdDamageTwoHand10` (prefixes),
`ManaGainedFromEnemyDeath6` (suffix), all T1, clean. Pool: 65 natural prefixes,
77 natural suffixes; all five metamods craftable. No Eldritch.

| Start | Published [ran] | Expected consumption per run [ran] |
|---|---|---|
| Empty rare (CB04) | 223,349.00; L = 212.39; requested finish at 240 s | alteration 908,324; augment 460,571; exalt 18,832; regal 7,605; scour 7,602; annul 1,547; transmute 4.4; fracture 4.0; restart 3.0; **metamods 0** |
| 3 T1 prefixes, no suffixes | 50,798.15; L = 212.01; stop `other_resource_cap` after 72 s | exalt 120.2; scour 119.2; `StrMasterItemGenerationCannotChangePrefixes` 119.2 |
| 3 T1 prefixes + Dexterity7 + LocalIncreasedAttackSpeed3 | 223,064.93; L = 212.39; requested finish at 120 s | same alt/aug route as the empty start |
| Same, composed: lock → Scour → solver's 50,798 policy | — (authored) | success 1, **51,222.52** |

From-scratch CB04 internals [ran]:

- Graph: 927 states, frontier 0, 220 policy-reachable, 5 goal states.
- Recipes: lock 7,476 candidates / 1,873 eligible / 5,008 setup rejections;
  temporary bench 5,768 / 3,304; Cannot Roll 594 / 315; Fracture 920 / 217;
  919 admitted carriers; admission discovered 4,150,029 states, 2,997,597 rows,
  1,816,929,912 transition entries, 925,285,872 logical reroll work.
- Time: publication pipeline (`timings_ns.extraction`, measured from
  [solver_solve_finish.cpp:427](../../../engine/src/solver_solve_finish.cpp))
  166.9 s; transition calculation 49.5 s; lock-recipe evaluation 57.7 s.
- The core Bellman policy was estimated at 1,055,896.07. Direct certification
  classified it `improper_policy` / `route_coverage_failure`: success 0.0255,
  `action_not_applied` 0.9745 (exact cost of that improper graph 25,637.78).
  Status `direct_core_policy_retained_after_strict_lift`; the verified fallback
  (`anytime_reachable_proper_policy`) at 223,349.00 was published.

Recorded CB04 history [record]:

| Run | Flags | Cost |
|---|---|---|
| `out/progress-delivery/M10-bow` | `--native-retention-diagnostic reuse --native-dirty-guidance execution-count --native-execution-action-price 0.159024756944986`, corpus `docs/active/2026-09-13-execution-aware-proposals/native-600-final840-400m` | 223,349.0 |
| `M11-final-bow`, `verified-delivery/A7-bow` | same flags, later executables | 12,770.8; graph contains Multimod + Cannot Roll Attack + remove crafted |
| CB05 `execution-aware-proposals/final-original-core` | ordinary | 87,200,457.4, `refused_resource_cap` |

The only sub-100k CB04 policy on record therefore comes from the private
dirty-guidance mode, not the product path.

## E3. Recorded A4/A5 recipe telemetry [record]

From `out/sustained-dual-solver/P3/a{4,5}-ordinary-L.json` (P3 ordinary arm):

| | A4 (CB02) | A5 (CB01) |
|---|---:|---:|
| Lock recipes: candidates / eligible / setup rejections | 28,906 / 3,074 / 23,966 | 26,885 / 3,851 / 21,645 |
| Cannot Roll: candidates / eligible | 1,651 / 162 | 1,048 / 281 |
| Temporary bench: candidates / eligible | 31,851 / 12,274 | 13,316 / 4,290 |
| Eldritch side: candidates / eligible | 26,883 / 26,033 | 14,268 / 14,122 |
| Admitted carriers | 7,053 | 3,269 |
| Admission transition entries | 404,443,822 | 276,257,468 |
| Admission reroll work | 280,682,062 | 158,776,072 |

A4's saved checked policy re-evaluated on the current engine [ran]: 3,746.13194
(matches the recorded anchor). Its suffix-lock → Scour nodes (`s552`, `s684`)
have **0 expected visits**; its Fracture node is visited ≈4.0 times. A5's policy
contains no Scour and no metamod.

## E4. Metamod usage census [ran]

547 saved Current strategy graphs under `out/` were scanned; 157 contain a
metamod craft (45 distinct case files). 19 latest-per-case graphs still load on
current data; 26 reference bench mods removed by the RePoE refresh
(`EinharMasterFireDamage1`, `JunMaster2ManaAndManaCostPercent1`).

| Graph (latest) | Start | Cost | Metamod crafts per run |
|---|---|---:|---:|
| CB02 / A4 (`sustained-dual-solver/P7`) | empty | 3,746.1 | 0 |
| `conquest-four`, `conquest-native` | empty | 3,746.1 | 0 |
| `conquest-capacity-blocker-full` | empty | 3,946.5 | 0.632 (Cannot Roll Attack + Multimod) |
| CB03 (`execution-aware-proposals`) | 3 T1 prefixes | 794,067.5 | 2.221 prefix lock |
| `partial-five-last-mile` (archive) | near-complete | 2,698.9 | 5.946 prefix lock (≈93 % of cost) |
| `partial-five-capped-diagnostic` (archive) | near-complete | 2,087.1 | 1.474 prefix lock |
| `clean-5-goal-product8` (C9, Sept 9) | empty | 30,267,250.8 | 3,113 (2,757 prefix + 355 suffix locks) |
| `exact-zero-to-five` (archive) | empty | 14,454,067.4 | 0 |

Script: `out/2026-09-29-exact-solver-audit/eval_metamods.py` (local, ignored).

## E5. Prices (Allflame `de282eec`)

| Key | Chaos |
|---|---:|
| Prefixes/Suffixes Cannot Be Changed, Multimod | 424.0 each |
| Cannot Roll Attack / Caster | 212.0 each |
| Scour (also `remove_crafted_modifiers`) | 0.3741 |
| Annul | 9.69 |
| Chaos / Exalt | 1.0 / 1.77 |
| Alteration / Augment / Regal | 0.153 / 0.066 / 0.1563 |
| Eldritch Chaos / Annul / Exalt | 39.21 / 40.53 / 3.59 |
| Fracture | 405.1 |
| Harvest augment (by tag) | 329.5–1,042.0; no `mana` tag |
| Veiled Chaos / Veiled Exalt | 50.52 / 2,581 |
| Typical temporary blocker crafts | 0.05–2 |

Every bundled economy prices all five metamods (locks 220–1,632 depending on league).

## E6. Native metamod semantics [code] (`engine/src/actions_basic.cpp` at `a1ba3fc`)

Data sides (SQLite): Multimod, Cannot Roll Attack, Cannot Roll Caster and
Prefixes Cannot Be Changed are crafted **suffixes**; Suffixes Cannot Be Changed
is a crafted **prefix**.

| Action | Side locks | Cannot Roll |
|---|---|---|
| Transmute, Alteration, Alchemy, Chaos, Harvest reforge, Veiled Chaos (`reforge`, `collect_preserved` 537) | keep locked side and fractured; refill may add to open slots on a locked side (ruling 3) | applied only if the metamod survives the wipe (ruling 2) |
| Essence, Fossil | ignored (2026-07-17 ruling) | ignored |
| Annul (`do_annul` 637) | excludes locked side | n/a |
| Scour (`do_scour` 677) | one lock: keeps locked side only (**drops fractured on the other side — ruling 1 says this is wrong**); two locks: keeps only fractured (open question) | n/a |
| Harvest augment (`do_harvest_augment` 940) | add, then remove a random mod excluding the locked side | add honours Cannot Roll |
| Harvest resist | source excludes locked side | honours |
| Augment, Regal, Exalt, Foulborn, Influence Exalt, Eldritch Exalt | n/a | honour Cannot Roll present on the item |
| Eldritch Chaos/Annul with dominance | ignore locks; act on dominant side | Chaos refill honours |
| Bench (`do_bench` 858) | one crafted mod unless Multimod (max three) | — |

The solver's exact rows use native `apply_action` for Scour, Bench and cleanup
([solver_calc.cpp:2685](../../../engine/src/solver_calc.cpp)) and the shared
pool builder for adds, so they inherit these semantics. The registry's
advertised flags do not (E7, F11).

## E7. Code reference index (`a1ba3fc`)

| Topic | Location |
|---|---|
| Product request always goal-relevant | `apps/web/src/app/solve-workspace.ts:125`; pinning `pc-calculator.tsx:767` |
| Registry role classification | `engine/src/solver_registry.cpp:260-446` (Bench 292-362, Veiled filtered 372, cleanup 411-445, default Candidate 446) |
| Dependency-only rejection / exclusion | `solver_api.cpp:416`; `solver_calc.cpp:485` |
| Held currencies rejected | `solver_api.cpp:409` |
| Recipe synthesis | `solver_options_helpers.hpp:399` (Veiled, Eldritch, Multimod 712, temporary/Cannot Roll ~770-1020, capacity variant 975, lock recipes 1026-1124, single-slot exit 1120) |
| Allowed lock follow-ups | `approved_renewal_roll` `solver_options_helpers.hpp:169`; temporary follow-ups `:274` |
| Recipe kernels | `solver_options.cpp` (lock+reroll certificate 429, retry equivalence 495, exit normalization 760, lock+Scour relevance 1133) |
| Recipe construction checks | `solver_options_build.cpp` (protected Annul comment 307; blocker restriction 501) |
| Exit predicate | `option_exit_matches` `solver_options_helpers.hpp:1236` |
| Incremental deferral of recipes | `solver_solve_expand.cpp:587` |
| Admission exempt from reforge cap | `solver_model.hpp:253` |
| Private lock+Scour/Annul (dirty guidance only) | `solver_solve_return_bridge.cpp:1322` |
| Internal error site | `solver_solve_finish.cpp:5384-5390` |
| Scope labels | `solver_solve.cpp:135-165`; `solver_solve_finish.cpp:5317-5345` |
| Foulborn demotion | `solver_solve.cpp:39-55`; `solver_api.cpp:686-697` |
| Fallback price | `apps/web/src/app/workspace/economy-service.ts:733-752, 800-810` |
| Metadata derivation | `engine_internal.hpp:798`; `solver_registry.cpp:1607-1627` |
| Scour mirrors of the engine rule | `solver_registry.cpp:1414-1436`; `solver_abstract.cpp:1102-1131`; test `engine/tests/test_solver_abstract.cpp:458-459` |
| Clean-cover domain (no locks/fractures) | `solver_solve_carrier_pattern.cpp:8-53` |
| Phase-lower grammar coverage | `solver_phase_lower.cpp:141` |
| New-action fail-closed contract | `solver_registry.cpp:1597` |

## E8. Reproduction

From the repository root (native build and the ring/bow manifests are ABI 3):

```powershell
$m = "docs/active/2026-09-29-exact-solver-audit/witness/ring/manifest.json"
build/engine/poecraft_solver_benchmark.exe --artifact data/compiled/current `
  --corpus $m --output out/2026-09-29-exact-solver-audit/repro/result.json `
  --strategy-output out/2026-09-29-exact-solver-audit/repro/strategies
```

Use `--case <id>` for one case and `--validate-only` to check a manifest.
Authored policies: `Session.compile_strategy(json).evaluate(economy=...)` from
`bindings/python` with the economy above and `base = 1.0`. The composed bow
policy is the solver's `bow4-last-mile-empty-suffixes` strategy with
`base_state` replaced by the two-junk start and two nodes
(`bench:StrMasterItemGenerationCannotChangePrefixes`, `scour`) inserted on the
start edge. Raw outputs, strategies and scripts from this audit are in
`out/2026-09-29-exact-solver-audit/` (git-ignored, local only).
