# Execute J0–J4: dirty intermediate progress, unchanged clean goals

Repository: OliverOrton/poecraft2  
Reviewed main: `b4ea87d18e746dcfa753d753c83a93f8672e5789`  
Selected task: J0–J4 below, choosing **one** O/S/G implementation after the native evidence gate. This supersedes automatic continuation of the incomplete H2 finder expansion. It does not undo the useful H0–H4 code that has landed.

The user suspects that final junk-free goals have influenced intermediate search too strongly. Test that hypothesis now, but do not assume it is true. Keep the exact same root, requested goals/tiers/rarity/threshold, native clean terminal, economy, permitted actions/programs and computational limits. More permissive goal language is a separate future task.

## 0. Working rules and scope

Read current AGENTS/HANDOFF; reconcile newer main and relevant dirty work. Preserve unrelated work and protected root `0` without inspecting, staging, editing or deleting it. Work sequentially, without subagents, reset/cleanup, automatic restarts or unrequested push. Native C++/data own mechanics. Ask Oliver only when the native contracts genuinely cannot resolve a mechanic; do not look up or invent alternative rules.

Reuse the current corpus resolver, Lab/runner, benchmark, process supervisor and report comparison. Native timed runs are serial. Use supported blocking/long waits rather than model polling; keep native cancellation/watchdogs active. Use one living record and a concise HANDOFF at each checkpoint. No new generic tracer, scheduler, cache platform, training dataset or checker.

Read REVIEW.md and SOURCE_MAP.md, then PLAN.md. NATIVE_DESIGN.md supplies the actual diagnostic and intervention contracts; MATHEMATICS.md supplies their premises. VALIDATION.md, DATA_REQUEST.md and DOC_MAINTENANCE.md complete the packet. The text below is sufficient to start without prior chats.

## 1. Baseline: do not repeat completed work

H0 closed the default-success bypass, installs a guarded zero-cost finder graph for a completed root, and moves the accepted graph into its bundle. Keep those repairs.

The finder now has native finite control, a live deque/seen history, lineage and at most eight total attempts including refusals/censorings. Its conditional family uses slot 0 plus explicit count >=4 to choose Annul/Chaos; a deeper slot-1 branch uses native Scour→Alchemy. That family is NOT protected progress-retaining completion. Dependency-only programme admission also remains unimplemented.

The recorded same-build H3 conditional costs are A3 335995.68284382095, A4 9636485.241311843, A5 320800932.13201106; B3 has no policy and a 200000-state cap in one programme attempt. These are much worse than Current. They establish retained engineering capability, not economic superiority. Current stays default.

Preserve Current A5 C85558.70618560436, exact Regalia C65.60036144971359 and the stronger historical Current A4 C3746.1319409485764. F4 Current A4 C5218.040949685988 has equal case fields/game hash but different build/corpus identities; its cause is unresolved. Current A3 has a checked C2085.015 graph but fails bounded stop expectations on unnamed `other_resource_cap` / cap mask zero. Read raw records before classifying either issue; do not erase the errors or substitute weaker baselines.

Both reviewed-head CI workflows completed successfully. Recheck if source changes; this is not permission to call all native behavior qualified.

## 2. Source facts that constrain the hypothesis

- `CalcContext::is_goal_state` requires rarity, enough satisfied native slots, and explicit count == satisfied count. It explicitly permits junk/temporary crafts as ordinary NONTERMINAL state. Freeze this.
- `joint_policy_terminal_debt` returns zero for true goals; otherwise `max(1, max(required-satisfied,0) + explicits-satisfied + rarity_mismatch)`. Its familiar `m+k-2g` simplification requires requested rarity and `g<=m` with distinct satisfying slots.
- The debt progress test is gated by high-impact uppers, incremental generation, and **no output_incumbent object**. That differs from “no independently verified policy.” Later seed selection uses goal truth or increased satisfied count. Its full key also considers pending successor routes, cost/progress, goal mass and stable row identity; only completed priced rows are selected.
- Carrier order groups by FULL goal mask, orders subsets by satisfied count, and round-robins them. IncrementalLegacy penalises unrelated occupancy only WITHIN a mask after fracture/protection. FocusedLegacy skips this within-mask sort. A dirty 3-goal and empty 0-goal state are not globally tied merely because their D matches.
- A separate obstruction-aware cooperative profile exists but `cooperative_high_progress_ordering_enabled()` returns false after failed historical controls. Do not enable that entire profile.
- Canonical search mathematics already states the debt tie and a synthetic dirty-route counterexample. Native causality is missing, not the algebra.
- Finder does not call that debt function. Its current reset/annul branch is a grammar choice. Changing it is a different treatment from reordering Current.

Relevant owners: `solver_calc.cpp`, `solver_solve_constructive.cpp` (`joint_policy_terminal_debt`, `select_joint_policy_seed_row`), `solver_solve_priority.cpp`, `solver_solve_incremental.cpp`, `solver_solve_focused.cpp`, `solver_finder.cpp`, `solver_compile.cpp`, `solver_compile_contracts.hpp`, native options/refinement/evaluator owners. Finish the local call map before changing any consumer.

## 3. J0 — original problem, actual activation, saved records

Use the existing resolver/ledgers to freeze source/data/module, initial item, requested goal/tier/threshold, strict terminal identity, prices, action/program/dependency scope, activation, caps and clocks. Native fixed-eight and actual adaptive compact worker are distinct treatments.

Start with `out/strategy-finder/H3/capped-primitive/ledger.json`, `capped-conditional/ledger.json`, `capped-comparison.json` and `out/strategy-finder/F4/current/ledger.json`. Resolve actual report/strategy paths from those ledgers; missing files are not empty results. Reuse compatible already-checked donors; do not infer chronological service from final graphs.

Complete SOURCE_MAP.md: semantic authority, lower/proof domain, candidate initiation, work order, first-policy construction, continuation service, compiler/evaluator or diagnostic. Record which consumers actually run in the selected A5 profile. Resolve only the control anomalies relevant to a safe comparison; an unreconciled correctness issue blocks that case rather than being silently ignored.

Exit: precise live consumers and clean immutable comparison input. No broad state-space or function-size census.

## 4. J1 — two distinct evidence channels

### A. Search service

Read saved carrier/row reports first. If first-use/service data is missing, permit ONE full-duration Current A5 diagnostic under its actual product profile: requested Finish 240s, native watchdog 300s, host cleanup 315s, aggregate 1 GiB, remaining native caps from the case. Use current compact transport for any worker observation; never write full telemetry every step.

Instrument existing native attribution with bounded aggregates and initially at most 128 stratified carrier samples / 32 seed comparisons. Record exact source namespace/generation; goal mask; native per-side occupancy/capacity; below-tier/junk/temp-craft distinctions; blockers/persistent context; actual debt gate; output and independent-verification stages; actual original ordering key; first queue/service/completion work and times; missing-route state; actual cleanup evidence grade. Keep omitted and never-serviced-at-Finish samples censored.

Do not build cleanup kernels on a status read. A MAY-survive/refinement effect is not a proof of preservation. A state can be harmless for one proposed next program and obstruct another.

Compute counterfactual ordering only on the SAME eligible frozen set. First O counterfactual removes only the IncrementalLegacy raw unrelated-occupancy tie; keep earlier keys, mask round robin, quotas and final state tie. Measure real inversions and whether useful completed continuation work is affected.

For S, compare the full seed-row key only while the real first_policy gate applies. A debt tie is insufficient: establish a different complete selected row and its exact missing-route burden.

### B. Crafting economics

Define diagnostic requested coverage as original rarity plus original goal threshold, ignoring only unmatched explicit occupancy. It is not terminal success and cannot publish an upper for the clean request.

Prefer a bounded diagnostic stopped copy of an already checked donor: identical original root, native operations, costs and routing until first coverage at a compiler-declared complete-decision boundary, then a private STOP. Preserve hidden control and mandatory programme interiors. Keep clean/dirty stop labels where supported. Evaluate it through the generic diagnostic evaluator, NOT finder acceptance. Never retain it as a candidate or user strategy.

With faithful prefix coupling and complete evaluations:

    expected post-first-coverage bill = original J - stopped-prefix J.

That bill includes later goal loss and reacquisition. It is NOT avoidable savings or necessarily only cleanup actions. A low bill does not disprove expensive destruction of partial progress before all goals coexist. Recurrent visits times remaining value double-counts; use first-hit semantics.

The predicate must bind ORIGINAL requested slots, not all auxiliary slots derived from a graph's intermediate conditions. If only complete-decision boundaries are observed, label `first_decision_boundary_coverage`; do not call it earliest primitive acquisition. A macro that crosses the event needs native reward/control accounting, not guessed division.

At most TWO saved-donor stopped checks initially. First verify never-cut equality, root-cut zero, and a small recurrent-loss fixture against independently computed first-hit equations. `J - prefix - (J-prefix) == 0` is a tautology, not validation of the transform. Check preserved prefix semantics and actual absorption/STOP provenance. If exact observation or evaluation is capped, report unknown; do not replace it with success-only sampling.

Exit: one actual supported causal witness or a precise negative. Do not expand tracing indefinitely to get the expected answer.

## 5. J2 — select ONE O/S/G implementation

### O: raw-occupancy tie suppresses useful same-mask service

Implement the neutral-extra comparator as a diagnostic/native treatment at the actually active IncrementalLegacy consumer. No changed goal-mask order, lane quotas, focused classes, global scheduler flag, native rows, action scope or proof rules. Frozen-list tests establish that only the intended tie changes. One matched root comparison establishes economics. If a second obstruction-aware variant is justified by new native evidence, document that premise; it is not an automatic coefficient sweep.

### S: initial debt hides a useful acquisition row

In the gated progress calculation, count `old debt decrease OR increased native satisfied-goal count`. Keep true goals and all old cleanup progress events. Do not alter completed-row eligibility, pending-route ordering, chance support, choice timing, actual prices, lower vectors or post-output behavior. Full composed-policy checking still decides properness and cost. This can fail economically; no guarantee is presumed.

### G: the completion is absent from the generator, not starved by Current

Use the existing finite-control/compiler/native-option owners for ONE genuine progress-retaining family. It must acquire the source from the original root, branch on requested coverage and native obstruction, preserve valuable holdings where the actual program permits, finish missing goals, and remove only what the strict target requires. Every failure/recovery branch remains complete and paid.

A new coverage test remains NONTERMINAL. Do not mutate GoalSpec/CalcContext terminal semantics or use it as a success guard. Native goal roles must be bound generically rather than always slot 0/1; do not use raw explicit count >=4 as a universal cleanup trigger.

If a necessary program dependency is not standalone selectable, admit it only inside a compiler/native-checked program occurrence. Do not silently widen the action envelope or substitute Scour→Alchemy and describe it as preservation. If the actual request excludes the necessary program, record that scope limit instead of changing the benchmark.

At most three native source/binding opportunities and six complete candidate checks inside existing total budgets, with the finder attempt ceiling still enforced. Include unequal or differently blocked bindings and an ineligible contrast. Proper complete root graphs alone receive acceptance. Unknown tails do not receive zero or another policy's unrelated value.

A G win supports the domain insight and candidate-coverage hypothesis. It does NOT prove that Current's scheduler caused the earlier failure.

## 6. J3 — qualify the result, not the score

Run focused native witnesses before long solves. Preserve default-success rejection, completed-root zero, old winner under cap/cancel, memory overlap, initial/candidate/checked identities and one terminal response. Preserve all positive-probability outcomes and current numerical acceptance.

For the selected branch use one predeclared baseline/treatment comparison under unchanged original limits, then a reversed confirmation only when warranted. A4 remains a preservation target, A5 primary, with exact Regalia and a genuinely affected non-armour/blocker contrast. Full Bow/Ring campaigns are not automatic. Existing wide Ring-four evidence is not ordinary Ring-two qualification.

Report actual first checked policy, best by 30/60/240s where observed, final independent cost/count, no-policy time, service delay/work, completed useful rows/programs, memory, native logical work and typed stops. Search effort and future primitive crafting counts are different units. Same proof AUTHORITY does not require the same numerical lower after different legitimate work.

Any strict verified original-root gain is real. Twenty percent is the material target, not a correctness gate. Earlier same-quality delivery is separate. A large reduction from the weak finder alone is not a win against Current. Neither a relaxed target, privileged intermediate start, raised cap nor prettier heuristic statistic qualifies.

Source-matched WASM/actual Calculator qualification is required for a product claim when native behavior/vocabulary changes. Keep existing setup/cancel/Finish and accepted latency scope honest. No Simulator for unchanged graphs; changed graphs needing corroboration use only current owner-approved 1,000 trials with censoring retained.

## 7. J4 — preserve reasoning and stop

Integrate accepted first-hit/ordering/coverage arguments in existing policy/search/representation mathematics. Update actual scheduling, resource, telemetry and compiler owners only where changed. Keep historical tests, failures and broad open claims. The uploaded hypothesis stays a hypothesis unless native evidence supports its specific mechanism. No theorem IDs for standard equations or ordinary helpers; no hand-edited generated research-state files.

Record findings as incorporated/already-present/refuted-on-scope/unresolved/deferred, with real paths, hashes, commands and failed/unrun checks. Use supported knowledge lint and edited-link/diff review; lint is not proof. Documentation-only changes need no new native campaign.

Stop after two substantive failed variants without a new native premise, or immediately for unresolved mechanics, mass/goal/provenance, or ownership failure. This does not limit ordinary test-driven repair of a selected interface. Deliver a concise self-contained handoff and stop. No automatic goal-language migration, model training, new solver lane, global pruning or subsequent research campaign; no push authorised.
