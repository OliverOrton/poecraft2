# Recommended successor: complete conditional native controller search (H0–H4)

Repository: OliverOrton/poecraft2  
Reviewed main: `a7ea53c3ea4ea230c4eb099bb8800a9d5c86e0ec`  
Pre-finder reference: `28a955056a41699d1fa9996ee345b70678937f21`

This is a proposed next implementation, to execute only after Oliver selects it. It preserves the independent finder already on main. It does not restart F0–F4 or authorize learning/GPU/proof integration. This prompt is self-contained; the packet supplies detailed sources, contracts and a native regression snippet.

## 1. Decision and existing result

Current main has the peer `PolicyFinderWork`, native/API lifecycle, request-bound preparation, independent evaluation and Calculator experimental mode. Keep those. Current remains default. Eight commits since `28a9550` are already on main; HANDOFF's instruction to commit dirty F4 work is stale.

The implemented v1 finder produces only five one/two-primitive controllers on A3/B3/A4/A5 and stops in roughly 1–4.9 seconds. It has no general partial-progress branches or native macro block adapter. The beam is an append-only capped candidate vector; it does not expand checked controllers into deeper control structures. Scoring uses prices, goal-slot count, renewal/recovery booleans—not the intended heterogeneous role/context features.

Recorded primary Current/finder costs:
A3 2085.015 / 5387327.209;
B3 79273.770 / no policy;
A4 5218.041 / 13111122.495;
A5 85558.706 / 455464336.951.
Uninformed ordering improves finder A4/A5 to 1516093.480 / 28555986.962 but still loses badly. Ranking also changes which initial seeds fit the cutoff; these are not proven fixed-candidate-set comparisons.

The architectural split is real. The intended conditional/macro search is only partially implemented. The next objective is to complete that missing capability, not tune a model to rank five poor loops.

## 2. Working rules and first reads

Read current AGENTS and relevant local changes before editing. Preserve root `0` without inspecting, staging, changing or deleting it. No destructive resets, subagents, new supervisor, unrequested push or background process relaunch. Native code/data owns mechanics; ask Oliver about genuine ambiguity. Use existing project-bound Python, corpus runner, long waits and serial timing.

Read:
- docs/active/2026-09-24-strategy-finder/README.md and the actual recent commits.
- engine/src/solver_finder.hpp/.cpp in full; solver_compile.cpp's finder helpers.
- engine/src/simulator.cpp default-edge parsing and route semantics.
- engine/tests/test_solver_compile.cpp finder tests; relevant API/worker tests.
- existing native option/program emission and scope contracts needed by ONE chosen macro family.
- packet REVIEW.md, DESIGN_AND_MATHEMATICS.md and VALIDATION_AND_DATA.md.

The old single-Chaos improper component was repaired by keeping recurrent entry snapshots distinct from full occupancy. Preserve unresolved mass and the original rejection tests; do not weaken the evaluator or reclassify a capped result as expensive crafting.

## H0 — Mandatory correctness/ownership gate

### H0a: effective success routing

Source-confirmed mismatch: `prepare_finder_candidate` checks each raw success edge's condition equals `compile_finder_goal_condition`, but ignores `is_default`. The native parser replaces the condition of a default edge with Always. Thus a raw trusted-goal decoration can pass preparation even though execution is unconditional.

Integrate tests/finder_default_success_regression.inc into the existing compile-test owner. Use a real Start node, empty non-goal exact root, success terminal and one default edge decorated with the trusted goal. Establish the native preparation/compiled semantics and, where supported, full evaluation. The current trusted emitter does not generate this graph; do not claim prior F4 outputs are invalid without evidence.

Fix the finder acceptance boundary against effective compiled routes or a compiler-owned trusted terminal contract. A minimal conservative repair forbids default success ingress and validates the actual nondefault guard. Keep ordinary Strategy Builder default semantics unchanged. Do not merely strip `is_default` from arbitrary inputs and call the original graph checked. Add wrong goal/tier/rarity/root/scope regressions and a valid counterpart.

### H0b: completed root and artifact lifetime

Test the actual finder/public mode on an already-complete original root with no priced action needed. It must return a properly guarded checked zero-cost graph, not execute an unnecessary action or report no seed. The existing handmade zero-action gate test is not this runtime test.

At winning-candidate adoption, preserve graph, request identity and evaluation ownership together. Current code copies `checking_graph_` while old best/parsed graph/checker remain live, then audits after releasing them. Reserve the true overlap or move ownership safely; count growing compiler/parser scratch when relevant. No added resource allowance and no blind sum of unrelated historical peaks. Test cap/cancel/failure with an old checked winner still owned.

### H0c: baseline and status

Read saved F4 ledgers under out/strategy-finder/F4/{current,finder-v2,ablation-heuristic,ablation-uninformed}. Resolve per-case paths from the ledgers. Do not overwrite/relabel the superseded `finder` directory.

Current A3's checked graph has a bounded-proof-contract error on an unnamed other_resource_cap. Current A4 returns 5218.041 despite a prior 3746.1319409485764 preservation reference. Extract exact request, data/build/activation/cap identity and graph/stop evidence before attributing either. One focused reproduction is allowed for a genuinely missing cause, not an entire campaign by habit. Never normalize the stronger reference downward or loosen a check to make the comparison pass.

Exit H0 only when the request-binding defect is fixed/tested and ownership is safe. Unresolved native correctness blocks broader search. A separately documented profile/performance discrepancy may remain open with matched controls. Update stale HANDOFF instructions to actual source status.

## H1 — A minimal real conditional controller

Replace the action-list-only proposal representation with a finite native-bound control view inside the existing finder/compiler interface:

    Test(native predicate, true node, false node)
    Run(native primitive or supported program, explicit continuation ports)
    GoalTerminal (assembler guarded by original target)
    FailureTerminal (positive reached mass rejects acceptance)
    Hole (incomplete search only)

No new public JSON language or interpreter. Emit ordinary strategy graphs. No fake SolveResult/proof fields. Persist stage/control context in nodes; do not price a recurring controller as a finite prefix.

Use real native predicates for partial goal-family satisfaction, capacity/rarity and supported persistent context. A required fixture has two reachable NONTERMINAL progress situations that select different next operations. “Full goal succeeded versus retry” alone does not meet H1.

Add ONE existing native program family through its authoritative compiler/options owner. Preserve initiation, setup, mandatory cleanup, observed choices and actual resource costs. The scope gate must allow its dependency-only operations only inside a validated program occurrence. Do not globally whitelist dependencies or blindly flatten primitive lists. An unsupported macro stays unsupported.

A second fixture must require more than the old two-operation structure. At least one fixture uses unequal native goal bindings; each retains its own numerical laws and may choose a different branch. Approximate role features are only for proposals and ranking. They never become native cache/state/certificate identities.

Exit: supported Test/Run control is compiled, independently checked and consumed by the finder; negative branch/setup/dependency mutations fail as intended. No unused descriptor-only framework.

## H2 — Give the grammar an actual generative search

Keep separate live frontier, bounded seen identity/history, completed verification outcomes and best checked artifact. A live beam limit is not a lifetime limit on every already-processed candidate. An eight-check starting calibration is separate from started attempts and cumulative work.

Generate full candidate identities from original problem plus control topology, guard/native program bindings and scope. Role-feature equality is not candidate equality. Incomplete proposals may be ranked/pruned heuristically but cannot enter acceptance or become a success-tail.

Implement one native-described progress-retaining family:

    acquire useful goal progress
        → inspect actual partial result
        → use a supported completion/protection programme
        → handle loss/extra affixes through paid recovery
        → repeat or finish through the original goal guard

The concrete native actions and guards must follow available descriptors/contracts, not base-name branches or invented mechanics. All original-root acquisition and all positive failure outcomes remain included. Current clean final-goal semantics remain unchanged even though intermediate items may be dirty.

Demonstrate a parent→child→grandchild construction lineage and a declared feedback consumer that creates/selects a later meaningful expansion after a check. Do not just enumerate the same fixed five graphs again. Do not require every incomplete local edit to beat the current root cost; coordinated completion may matter. Retain a complete old winner while exploring.

Ensure the selected new conditional family receives a declared checking opportunity rather than letting all cheap old seeds consume the checking budget. That reservation is within, not in addition to, the original total budget.

Handcrafted deterministic ranking only. Start with small existing-width/check controls, and permit at most one documented calibration based on an actual richer-grammar bottleneck. No broad sweep, learned ranker, GPU or concurrency. Extending numeric caps alone is not H2.

Exit: independently generated original-root conditional controllers on two heterogeneous development contexts, without hidden legacy SolveWork or archived strategy seeding. One must use real nonterminal progress to change action choice. Fixed saved graphs can be expressiveness/checker oracles, never discovery inputs.

## H3 — Fair capability qualification

Before another ranking claim, produce bounded per-candidate records: identity/parent, structure/native bindings, score features and availability, generated/started/completed state, typed failure/censor, checked cost only when valid, native work/time/memory and graph reference. Use the existing reporter/supervisor, not a dataset platform.

Compare old primitive grammar versus new conditional/native-program grammar with ranking held fixed. A rank-only ablation later uses exactly the same candidate multiset. An end-to-end search-order treatment may change selection, but label it accordingly.

Use A3/B3 first for actual coverage, then A4/A5 and exact Regalia as affected qualification. Preserve the strongest compatible Current reference and its real profile; do not treat native fixed-eight results as adaptive Calculator results. Charge seed/failed attempts/features/compile/check/output and all simultaneous ownership inside original limits. One live checker initially.

Report best CHECKED original-root cost at real 30/60/240-second observations when applicable, no-policy periods, expected primitives, support, work/caps and why search stopped. Do not idle an exhausted grammar to reach a timestamp. Large improvements over a bad finder baseline can still be economically poor versus Current. A 20% reduction below the strongest compatible Current control is a material research target, not a correctness gate or prediction. Any smaller verified gain receives its actual time/cost.

Rebuild source-matched WASM for actual native changes. Preserve the already-working Calculator mode, compact stepping, frozen inputs, Finish/cancel and result authority. No new selector or default change is needed. Run native positive/negative tests before expensive root campaigns. No Simulator for unchanged graphs; use approved 1,000 trials only where new execution qualification is genuinely required. Rendered UI review remains Oliver's.

## H4 — Documentation and stop

Use DOC_MAINTENANCE.md. Canonical publication/upper/resource owners document effective success semantics and complete bundle transfer. Policy/representation/search chapters document finite control, program scope, partial features versus exact identities and grammar exhaustion versus proof. Preserve the original F4 failures and closed-component repair history.

Retain a safe usable experimental increment at a meaningful checkpoint; no automatic default promotion. Stop broader research after two substantive failed economic variants without a new native premise, not during ordinary debugging. Do not continue into ML, proof import, action-count objectives, GPU, threading, a universal DSL or another micro-cache census.

Final handoff: actual source/dirty state; supported grammar/programs; safety witness and repair; Current A3/A4 disposition; native/abstract/worker tests distinctly labelled; concrete candidates, costs and paths; canonical sections updated; next measured capability boundary. No push without authorization.
