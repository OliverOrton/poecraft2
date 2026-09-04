# Native evidence for free-value lower research

Research-only, 2026-09-04. Main worktree read-only, pinned HEAD `c9530ac941b95e68a1e3d890caf00d53c9e6d774`. Protected untracked `0` was never read or touched. No solve, solver step, test suite, simulator, browser, build in the main tree, or public action was run. External scratch helpers compile against the existing canonical native static library; these are research evidence, not current production qualification.

## Identities and scope

`native-provenance.json` hashes the consumed artifacts, native library and scratch outputs. Static library: `build/engine/libpoecraft_engine.a`, 12,853,024 bytes, SHA-256 `a00317cb1cfd9d1fdeb94ab4252b591e17c62e33be53c2f669a0e94ac951b5fe`. Canonical build object's relevant timestamps are Sep 1 03:37–03:38 UTC, consistent with the current source checkpoint. No embedded cryptographic source identity or clean reproducible rebuild was established; do not claim binary/source reproducibility from timestamps. Other local MinGW/MSVC/incremental build trees contain July binaries and were rejected.

Current compiled game-data SHA `af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5`, strings SHA `ba2110894e94b533d42e0440b83fab468d848e438ff5e6d6ed976108ac0d507f`; manifest file SHA `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`. Artifact source-data identity `76375e02fc21b0bc0d5709ab589aede8b1967b9a2d53b25aaf517a206f592000`.

The clean-five probe uses the r9 request's exact 28 explicit primitive IDs and economy snapshot `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0` with base override 5. Session Conquest Lamellar level 86, empty rare without implicits, same five T1 goal slots, no economic restart, no automatic Imprint, goal-progress gated reforges. Native primitive `action_legal` does not itself implement the caller's no-restart restriction: the exported `restart` entry is registry/price/primitive-legality evidence and is excluded from caller proof obligations by the request. Automatic families remain open in this setup-only probe; their safe common scalar placeholder is the independent source lower.

`native-identities.json` obtains full canonical item vectors directly from native `exact_item_state_key`, with zero proof or transition work. Empty rare digest `7965050586570456120`; Normal digest `10262648309111924158`. Their 15-word keys differ only in rarity 2 versus 0. These digests match r9 source and latest Scour successor evidence. Archived full vectors are not serialized, so digest agreement is a lookup corroboration; no collision-safe full-vector comparison against the archive is claimed. The request itself fully specifies both concrete empty items.

## Exact microproblem selected and queried

Saved exact artifact: `build/performance/native-solver-final-acceptance-cb26c29-v1.json`, case `oracle-real-one-mod` (whole report SHA `9f346bd26dc158fd808a3d3d75804ded4b0971922f9883fa9f9a96168f3a9c1e`). This is a genuinely exact small oracle, unlike the two-state Scour slice.

- Vaal Regalia level 86, empty Normal, no implicits; sole magic T1 `LocalIncreasedEnergyShield11`, exact terminal explicit affixes.
- Explicit action set `{transmute, alteration, restart}`; frozen prices `.05`, `.1`, base `5`; economic restart allowed, no automatic programs, no goal-progress gating.
- Archived exact lower = upper = independently evaluated cost `23.78999999999971`; success one, off-policy mass zero, cost complete/reconciled.
- Archived 3 quotient / 8 strict states, 4 sparse rows, 3 sparse transition entries, 56 outcome entries; two algebraically removed self loops. Native live/peak ownership 2,648,965 / 4,257,768 bytes.
- Archived independent envelope lower `2.240771812080537` is not the final exact lower; gap to exact oracle `21.549228187919173`. Its root scratch Transmute composition is `2.2227390251952914`. Carrier lower is `2.15`.

The current scratch helper `native_micro.cpp` (output `native-micro.json`) instantiates the same C-API parsed request and proof manager, but never calls solve `step` or `finish`. It queries the finite native calculator model to closure, capped at 8 states / 24 caller-action obligations / 56 legal transition entries. It exports full native canonical representative keys, goal bits, existing state lowers, complete native rows with stored-double probabilities, cost and explicit inapplicability records. The native finite representation may merge physically irrelevant items under these reset-only actions; these eight states are calculator-model states, not a claimed enumeration of every physical modifier item.

Final current query: 8 states, 21 caller-action obligations (all 3 actions at each of 7 nonterminal states), 7 native `action_legal` inapplicability records, 14 legal exact rows, 56 legal transitions, no choices, zero solver steps. Row-query time 374,700 ns; total load/setup/query 265,427,800 ns. Accounted owned bytes before/after rows 644,195 / 651,843 (+7,648). These small timings are observations, not a chosen performance threshold or rigorous peak RSS measurements. Existing root lower `2.15`; failure states have `2.2` except state 7 at `.1`. Parent's independent rational reference (`native-model-comparison.json`) finds this donor vector Bellman-consistent. The six failure-state action rows agree under the exported behavioral quotient. Raw serialized binary row mass exceeds 1 by `21/2^60`, so the exact stored-double coefficient model and explicitly normalized rational reference must remain separately labeled; neither provides directed-rounding native numeric promotion.

Parent's normalized rational reference gives root `2.2235586973264385` for the root-only exact model with outside existing lowers, `2.2415219923456653` after adding the goal-plus-junk recovery state, and `23.790000000000003` for the complete free-value model. The last matches the archived oracle within its disclosed arithmetic difference. The raw stored-double coefficient algebra gives `23.790000000000106`. Fresh `2.15` versus historical envelope `2.240771812080537` are different work stages, not matched baseline gains.

The initially guessed outcome bound was hit before export. Narrow diagnosis found a real API seam: `CalcContext::evaluate` in `solver_calc.cpp:2610–2640` represents illegal ordinary actions as supported, default-applicable no-op rows (Scour is a deliberate exception). Testing `OutcomeDistribution.applicable` alone does not establish strict admission. The harness was corrected to query native `action_legal` first and emit exact inapplicability without querying the illegal action. The original 8/24/56 bounds then passed unchanged. The retained initial refusal is `native-micro-initial-refusal.stderr`; unsuccessful harness attempts are not qualification or completed-model evidence.

## Medium two-sided live witness

Selected saved report: `build/performance/native-solver-gate8-operator-proof-attribution-case-conquest-lamellar-allflame-partial-five-last-mile-current-v1.json`, SHA `f64c488514dedc9c626987b5d84f555d010746bcb26f0c92e70d2a0157b896bd`.

Same Conquest Lamellar / five-goal / Allflame request shape as clean-five, from three requested T1 prefixes plus fractured requested T1 spell suppression; exact source mask 23, fractured mask 16, occupancy 3 prefixes / 1 suffix, missing requested Physical Damage Reduction. This is not terminal PDR, and its proof consumer population is nonempty.

- Lower `36.42861718910441`; independently evaluated proper upper `2698.8747960143623`; reported gap `2662.446178825258`. True optimum and true oracle gap unknown.
- 6,820 discovered states, 6,812 expanded, 8 frontier, 1 goal, 526 policy reachable.
- 40,551 rows, 119,625 sparse transitions, 74,514,364 raw outcome entries, 186,866 reported reforge work. Exact graph 215 nodes / 563 edges / 410,793 strategy bytes.
- Native live/peak ownership 62,809,343 / 99,834,010 bytes.
- Typed ledger 193,360 entries; 22,976 unresolved named-stop obligations, 79,938 incumbent-dominated, 34,518 exact complete and 55,928 exact inapplicability. This is a live bounded witness with prior real lower consumers, not a claim of statewise alternative-policy domain completeness.
- Existing fixed-identity clean MDP owns `36.42861718910441`, minimizing Harvest Physical. Source envelope trace: 10 admitted root rows, all finite; exact Physical lower `38.068615060362319`, Scour lower `36.809254018621417`. Reported lowest materialized root operator is `option:eldritch_side_intent:suffix:eldritch_exalt:eldritch_ichor:1`.

Important reporting trap: `refresh_envelope_bellman_pattern` computes `best=max(common, exact_operator_envelope)` yet reports `minimizing_operator` even when the common lower dominates. Thus the operator's displayed identity does not prove its exact RHS is `36.42861718910441`. The report does not expose that RHS or the complete ranked root alternative vector. Do not infer those values.

The selected program is source-defined as mandatory paid `eldritch_ichor:1` then `eldritch_exalt` on the suffix side, total `.08234 + 3.59 = 3.67234`. Current source ownership is `solver_options_build.cpp:208–246`, `solver_options_helpers.hpp:500–710`, and `solver_options.cpp:882–1000`. The native kernel checks legality and dominance at every step, charges both operations, multiplies the exact phase probabilities, and returns only complete decision-to-decision exits. No free setup is present in the executable macro. The artifact reports one root row / 3 root outcomes for this program; globally 69 rows / 209 outcomes / zero reforge work, 150,100 ns attributed wall time.

Evidence-selected phase refinement: the source `identity_clean_goal_progress_eligible` (`solver_solve_carrier_pattern.cpp:120–145`) requires unchanged Exarch and Eater tiers as well as exact fracture/protection/influence identity. Mandatory Ichor changes Eater tier 0→1, removing that donor even while the target prefixes and fractured suppression persist. A bounded current probe, `native_medium.cpp` → `native-medium.json`, now measures this exact donor-admission hole using the existing fixed-identity lower owner anchored at the canonical post-Ichor carrier. It preserves the original paid two-step program, target prefixes, suffix occupancy and fracture identity. It does not globally remove the guard.

Current medium local result (stored-double arithmetic, zero global authority):

- Original source lower `36.428617189104408`.
- Post-Ichor original-anchor lower `3.4724500000000003`, fixed-identity owner unavailable. Reanchored existing owner: `36.428617189104408`, fixed-identity owner available.
- Three final modeled outcome classes: probabilities `.035906642728904849`, `.0089766606822262122` (terminal exact success), `.95511669658886855`. Nonterminal classes retain requested mask 23, fractured mask 16, occupancy 3 prefixes / 2 suffixes. Their lower rises from `3.47245` to `36.428617189104408`.
- Paid projected macro RHS baseline `7.1136189946140025`; reanchored RHS `39.773949853475088`; projected RHS gain `32.660330858861085`, exceeding the source's current common floor by `3.34533266437068`. This becomes an action-lower gain only after the uniform lower/pushforward contract is proved. It is not a global lower improvement, retirement or fixed-policy deviation Q. Other legal constraints remain covered only by their previous floors/placeholders.
- Two exact native primitive queries, four modeled transition entries (deterministic setup plus three draw classes), states 1→5; query 43,400 ns. Accounted owner bytes before/after rows 1,238,571 / 1,239,787 (+1,216).
- Reanchoring the proof manager costs 4,346,723,800 ns; total load/two proof setups/queries 8,960,023,400 ns. Reanchored owner reports 1,239,778 bytes including the shared calculator. Do not sum both owner totals without subtracting shared memory; no peak transient or process peak was measured.

The three exits are complete native calculator-model outcome classes, not a literal enumeration of all physical modifier items. `solver_calc.cpp:2744–2764` implements deterministic tier setup then `evaluate_pool_add` with the native dominant-side filter. The queried lower uses recorded fields shared by these representative classes; its uniformity over all physical members still needs proof. This is useful projected expectation research evidence. Any authority promotion needs the strict-oracle pushforward/coverage contract, complete canonical source/row identities and controlled numerical bounds. No incumbent router or arbitrary-entry continuation is used.

The measurement makes persistent proof-table/context reuse a real design question: 4.347 seconds of rebuilding an existing donor versus 43.4 microseconds of kernel work. It does not yet prove baseline and reanchored proof tables are byte-identical or safely reusable. A broad strict reconstruction or another global strategy-domain expansion is unnecessary to expose this gap.

If that compositional query still pins cheaply, the concrete optimistic model grants to challenge are visible in `solver_solve_bounds.cpp:2280–2455`: normalizing failure occupancy to the minimum goal-affix occupancy; preserving progress; a union upper probability granting all missing target-side goals on a relaxed success; free dominance setup. A selected model counterexample should choose one of these, rather than an unmeasured generic retention architecture. The exact mandatory program already accounts for setup, so avoid double charging it.

## Clean-five setup-only scalar and deterministic query

Output `native-probe.json`, helper `native_probe.cpp`. Zero solver steps, current full price/request setup. A 30-second process watchdog and 256 MiB proof-owned cap guarded the probe. No broad action outcomes, automatic kernel admission, exact evaluator, or alternative census was invoked. A single deterministic Scour row was queried after inspecting the scalar minima.

Fresh source scalar comparisons before root exact rows:

| Action | Typed operator floor | Clean root action floor |
| --- | ---: | ---: |
| Harvest Physical | 2.494 | 36.4286171890906 |
| Scour | .44573 | 36.48853172876641 |
| Chaos | 1 | 38.76116050095102 |
| Harvest Defences | 2.59275 | 40.49615117725027 |
| Exalt | 1.77 | 56.59487697720022 |
| Dense Fossil | 12.5015 | 60.497989994619786 |

Fresh independent root maximum is `36.4286171890906`; the primitive minimum is Harvest Physical, not Scour. The old r9 root trace had already refined Harvest Physical to `38.909043109189682` and Eldritch Exalt to `40.018617189091387`, leaving Scour's `36.488531728766411` as its root envelope minimum. This is a concrete scope/order explanation, not a new useful lower gain.

Normal successor existing lower `36.11443172876641`; Scour cost `.3741`; exact composition `36.48853172876641`. One deterministic transition, no choices, states 2→2; query 22,100 ns; calculator-owned bytes 852,773→853,389 (+616); total proof live ownership 1,250,224 bytes. Total load/setup/query 4,709,901,400 ns. Post-Normal base query shares the same proof owner but is not a demonstrated second-shape exact-row reuse consumer.

Conservative unmaterialized automatic-family placeholders at the independent root common lower immediately cap a newly introduced fully covered model at `36.4286171890906`. Refining Scour alone therefore gives zero gain in that fresh model; importing the r9 refined Physical row without importing the family's proof contract and all remaining constraints is not an authority-preserving shortcut. Complete canonical family coverage and stronger compositional placeholders are the missing experiment inputs. Current primitive ID export is exact to the parsed request; open automatic coverage is deliberately not certified by a count.

## Retained clean-five facts and limits

r9 report SHA `4d47329437b5bf1d3fb6c633313553e974eea1200aa0e04ccc0f2bb45a5c24a6`; latest Scour report SHA `86f50c586aa5355876310db75a3471710bb5bd031d3722706aacadf0917f0156`; common strategy SHA `EA59F23F770FF2A969774266FA34C58FB23CB16A6B2E46C25B6CAFA1B3C1FF1A`.

- Exact proper fixed policy upper `85408.64362148782`, lower `36.48853172876641`, 27,021 reached exact policy entries, full retained census 671,410 alternatives, zero existing-lower retirements. No full census repeated.
- Latest Scour canonical successor cannot continue through existing compiled global router: `failure_reachable`. No finite `J_policy`, candidate, SCC, exact deviation Q, or proof of unsolvability follows. One refused kernel, zero complete, not a 256-entry cap stop.
- r9 bounded sample: 16 entries / 492 applicable alternatives; 12 generic exact rows / 26,064 transitions; 30 constraints including nine Fracture rows and nine selected Annul equalities; 10 internal / 20 outside; 21 unresolved. Bellman retained/transient 24,584,584 / 89,465,656 bytes, 54 strict states attributable, 16.0083403 s build. Native peak 476,337,952 bytes.
- Fracture's nine source/action witnesses belong to one semantic shape; every six-outcome row leaves the fixed policy-entry domain. Refined lower `405.41175`, not an exact fixed-policy Q. Existing Fracture proof-only gain `.31175`, zero retirements. No new reuse across a second shape was measured.
- The reported internal temporary-bench/cannot-roll row has five positive contributions but only three distinct *reported digest pairs*. Three contributions share exact item digest `3024104616361989188` and policy entry digest `14266200034966059843`. Full successor canonical vectors are not serialized, so this is not a collision-safe proof of exactly three cells. Its source digest is `16796284535041289418`, action digest `2670173055422159793`, complete local RHS `85622.5980428518` versus source J `85408.64362148767`. This local inequality passes but supplies neither all-action nor selected-dependency SCC closure. It remains opportunistic control evidence.

## Owners and smallest missing contracts

Canonical exact lower queries need not enter an incumbent router: `CalcContext::intern_item` plus native legal-action admission and `outcomes` already own them, as used in the existing envelope scratch context. C API benchmark exposes no focused row-query command; a research-only friend harness was sufficient here.

`SolveWorkTestAccess::Impl` (existing test friend) exposes the existing proof manager without modifying production source. The scratch helpers reuse `prepare_goal_cover_cost`, `completion_proof_lower_value`, `operator_proof_lower_value`, floor arrays, registry and calculator kernel owners. No second lower algorithm was implemented natively.

Count comparisons are insufficient: root `admitted_start_rows == finite_start_rows` in `solver_solve_envelope_proof.cpp:326–330` certifies finite cached rows only; the function itself does not compare a canonical legal caller action set against them. A consumer must bind canonical legal operator/program/observed-choice identities, inapplicability proofs, open-family optimistic constraints and descriptor dependencies. Medium report and full census counts cannot substitute for set-level coverage evidence.

Matched native fixed-J/free-value/candidate-repair/capacity values are only meaningful for the same exact exported row domain and outside boundary. The parent rational oracle handles the small current micro. For the medium and large request, exact optimum, full free-value gain, retirement gain, reactivation cost, transient peak bound and cheap cross-identity table reuse remain unmeasured. The medium probe supplies local macro successor/owner evidence across paid dominance identity change. The concrete next missing contract is safe persistent reuse of that existing lower owner with complete action/family coverage and controlled numerics, rather than redoing all tables per reached identity.
