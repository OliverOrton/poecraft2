# Exact solver audit — Pro planning input (2026-09-29)

**Status: research input for a fresh Pro planning session. It selects no
programme and grants no implementation authority or timed-run allowance.**
Prepared by Claude (Opus 5.5) at Oliver's request after a read-only audit of the
Current (exact) solver. Data tables, build identity and reproduction live in
[evidence.md](evidence.md); witness inputs are in [witness/](witness).

Evidence tags: **[ran]** measured in this audit; **[code]** source reading at
`a1ba3fc`; **[record]** existing repository evidence; **[hyp]** hypothesis.

## 0. Brief for the Pro session

Write a long, staged plan for the **Current** lane (not Finder) that:

1. fixes the confirmed correctness items;
2. lets the exact solver use metamods and crafted cleanup where they are worth
   their price;
3. removes the admission/delivery wall that stops the product solve from
   reaching good policies on large-pool 4–5 T1 items;
4. fixes the admission contract for new actions and recipes before Dominance,
   Vaal, Awakener and double corruption enter the solver.

Use the planning contract in
[the handoff template](../../_templates/solver-research-handoff.md): evidence,
question and decision, owners, premises and counterexamples, a
Current/Finder/runtime applicability matrix, engineering deliverables separate
from economic targets, problem identity, cohorts, a total run allowance to be
granted by Oliver (the P0–P9 allowance is spent), conditional paths and stop
rules. Separate facts, arguments, hypotheses and recommendations.

Bootstrap: [AGENTS](../../../AGENTS.md),
[research standards](../../solver/research-standards.md),
[current status](../../solver/current-status.md), [HANDOFF](../../../HANDOFF.md),
this file, [evidence.md](evidence.md), then the cited source.

Source pin: `main` `a1ba3fc` (pushed). Measurements were taken on builds made
between `aa89762` and `b3d7884`; the ordinary solve path is unchanged through
`a1ba3fc` by diff review (details in evidence.md). The first milestone should
re-baseline at the plan's own pinned SHA.

A concurrent session is shipping Calculator, editor and currency work directly
on `main` (for example `a468feb` joint item goals; `b3d7884`/`332e8c1`
Calculator-only odds for Awakener, Dominance, Vaal and double corruption). Keep
the plan sequential on `main` and avoid colliding with that work. Root `0`
remains protected.

## 1. The question and the decision it informs

Oliver, 2026-09-29: *"we have really hit a wall with exact optimal solving … my
intuition says that we arent using metamods to their full extent … we have
recently added new actions to the engine that arent yet present in the solver
so before adding i thought would be a good time."* Follow-up: *"4 mod bow was
the one that wasnt showing it for me in my testing so any similar large mod
pool non armour 4-5 mod t1 item."*

Decision to inform: which Current-lane programmes run next and in what order,
and what contract new actions and recipes must meet to enter the solver.

## 2. Binding owner rulings

| Date | Ruling | Engine today | Recorded |
|---|---|---|---|
| 2026-09-29 | A Scour with exactly one side lock must **not** remove fractured mods on the unlocked side | **Wrong**: `do_scour` keeps only the locked side ([actions_basic.cpp:677-699](../../../engine/src/actions_basic.cpp)) | this pack only |
| 2026-09-29 | Cannot Roll filters a reroll only if the metamod survives it (e.g. on a locked side) | correct | this pack only |
| 2026-09-29 | A reroll may add new mods into open slots on a locked side | correct | this pack only |
| open | Double-lock Scour behaviour | keeps only fractured | [ordinary-currency.md](../../mechanics/ordinary-currency.md) |
| 2026-07-15 | `remove_crafted_modifiers` is a real primitive costing one Scour | implemented | mechanics docs |
| 2026-07-17 | Essence and Fossil ignore all metamods | implemented | mechanics docs |
| 2026-07-17 | Permanent finishes, temporary blockers, metamod setup, Multimod finishes and cleanup are distinct useful bench roles | recipe set | archived S8 plan |
| 2026-07-18 | Fracture early on cheap carriers; prepare several goal mods; led to S8.4R.3F promoting Fracture to an ordinary candidate | implemented | archived S8 plan |
| 2026-07-21 | A 3-T1 benchmark needs three rolled T1 mods; no bench-crafted goal slot | case rule | benchmark cases |
| 2026-09-28/29 | Vaal projection, Dominance elevation links, double-corruption odds rules | Calculator/Emulator | [currency record](../2026-09-28-currency-expansion/README.md) |

The three 2026-09-29 rulings must be added to
[ordinary-currency.md](../../mechanics/ordinary-currency.md) and
[bench-and-metamods.md](../../mechanics/bench-and-metamods.md) when implemented.
Ruling 2 makes "Multimod + Suffixes Cannot Be Changed + Cannot Roll + reroll" a
legitimate technique, not an engine artifact.

## 3. How Current uses metamods today [code]

### 3.1 Roles in product solves

The Calculator always sends `action_mode: "goal_relevant"` with an explicit
priced action list ([solve-workspace.ts:125](../../../apps/web/src/app/solve-workspace.ts)).
The registry then classifies each primitive as Candidate, AutomaticDependency
or Filtered ([solver_registry.cpp:260-446](../../../engine/src/solver_registry.cpp)):

- All five metamods and `remove_crafted_modifiers` are **dependency-only**
  unless the metamod is itself a goal. A dependency cannot be requested
  ([solver_api.cpp:416](../../../engine/src/solver_api.cpp)) and never enters
  the Bellman candidate set ([solver_calc.cpp:485](../../../engine/src/solver_calc.cpp)).
  Metamods reach a policy only inside generated recipes.
- Veiled primitives are filtered; Eldritch primitives are dependency-only for
  side recipes; Fracture was promoted to an ordinary candidate by S8.4R.3F.
- Without `action_mode` every registry action is an unfiltered candidate, but
  no recipes are generated. That path is exhaustive and not used by the product.

### 3.2 Recipes that can contain a metamod

Synthesis happens per carrier in `synthesize_automatic_options`
([solver_options_helpers.hpp:399](../../../engine/src/solver_options_helpers.hpp)).

| Recipe | Program | Generated only when |
|---|---|---|
| Lock + Scour (`ProtectedSide`) | lock, Scour | lock not present and craftable now; a goal on the locked side is satisfied; **a goal on the other side is still missing** (loop at 1098-1124) |
| Lock + reroll (`ProtectedRepeat`) | lock, Alteration/Chaos/Harvest reforge, repeated | same, one recipe per missing other-side slot; **stops only on that slot** (1120) |
| Cannot Roll + add (`TemporaryBenchRepeat`) | Cannot Roll, one Aug/Regal/Exalt/Harvest augment/Influence Exalt, cleanup | target slot missing; no pre-existing unfractured crafted mod unless cleaned first |
| Multimod + Cannot Roll + Exalt | capacity variant (975) | rare, all slots required, exactly one missing, no junk, target side exactly one open, other side exactly two open |
| Multimod finish | Multimod + two goal crafts (712) | only if Multimod is itself a goal (correct under the clean terminal) |

Every recipe must end without its lock (`all_exits_without_flag`) and every
temporary recipe must end clean, so no policy can keep a metamod across
actions. Lock follow-ups are limited to `approved_renewal_roll`
(helpers:169) filtered by lock-respect metadata, with Scour special-cased; Annul
and Harvest augment never qualify. Metamod blockers other than Cannot Roll are
rejected ([solver_options_build.cpp:501](../../../engine/src/solver_options_build.cpp)).
The build path accepts lock + Annul (comment at 307), but only the private
dirty-guidance return bridge emits it
([solver_solve_return_bridge.cpp:1322](../../../engine/src/solver_solve_return_bridge.cpp)).

### 3.3 Other product restrictions (documented)

Goal-progress-gated rerolls send zero-progress outcomes to a retry basin that
cannot select Annul, Exalt, Bench or protection. Economic restart and automatic
Imprint programs are off. See
[request-action-scope.md](../../solver/request-action-scope.md) and
[architecture-history.md](../../solver/architecture-history.md#goal-progress-gated-reforge-mode).

### 3.4 Economics that shape metamod use

Locks and Multimod cost 424c and Cannot Roll 212c in the pinned Allflame
economy; blockers cost 0.05–2c, Eldritch side actions about 40c, Annul 9.69c,
Scour 0.37c (evidence E5). Each metamod except Suffixes Cannot Be Changed is a
crafted suffix (E6). A lock sits on the side it does not protect, so a reroll
wipes it and "lock + reroll until hit" pays 424c per attempt.

## 4. Findings

**F1 — Current's exactness is over a restricted grammar.** [code][ran]
The exact optimum ranges over priced candidates plus generated recipes, not
over policies that use metamods or cleanup freely. The research ledger lists
dependency-only programme scope as open only in the Finder context
([research.md](../../solver/research.md), "finder review"), and
[current-status](../../solver/current-status.md) does not state the
consequence for Current. See F12 for labels.

**F2 — Lock + Scour is never offered for cleanup.** [code][ran] Because the
recipe needs a missing goal on the other side, goals on one side only (or
cleanup of a finished side) cannot use it. The kernel's relevance test already
accepts "removes unwanted mods" ([solver_options.cpp:1133](../../../engine/src/solver_options.cpp)),
and a registry comment intends this use (solver_registry.cpp:324). Witness W1
(ring, T1 life + mana prefixes held, two junk suffixes): the authored two-step
policy costs **424.3741c** with success 1; Current published **no policy**,
stopping on `max_discovered_states` after 62.6 s with zero lock recipes
generated across 165 carriers.

**F3 — Crafted cleanup is not selectable.** [code][ran] Witness W4 (same goal,
crafted junk suffix): one `remove_crafted_modifiers` costs **0.3741c** with
success 1; Current published **2,337.20c**.

**F4 — Lock + reroll discards other goal hits.** [code] The exit predicate
counts only the recipe's one slot ([option_exit_matches](../../../engine/src/solver_options_helpers.hpp)
1236). Outcomes whose next attempt would look identical (same locked side, room
for the lock) are rerolled ([solver_options.cpp:495, 760](../../../engine/src/solver_options.cpp)),
so hitting goal B while rolling for A throws B away. No "any of A, B" recipe
exists, although the kernel supports multi-slot exits. Not runtime-witnessed:
root admission never ran in the relevant witnesses (F6).

**F5 — Missing recipe shapes the engine already supports.** [code]

- lock + Annul (exits with or without the surviving lock need a contract);
- lock + Harvest augment (the removal step respects locks);
- Multimod + Suffixes Cannot Be Changed + Cannot Roll + reroll (valid by ruling 2);
- keeping a lock across several actions;
- Multimod beyond the two cases in 3.2;
- stop on any, or all, missing other-side slots;
- one-step crafted cleanup.

Harvest augment has no `mana` tag, so lock + Harvest augment would not help
CB04's last suffix but could help CB05's projectile speed via the `speed` tag.

**F6 — Recipe admission is the wall.** [ran][code]

- In gated (product) mode, per-carrier recipes are generated only after the
  plain-currency graph ([solver_solve_expand.cpp:587](../../../engine/src/solver_solve_expand.cpp)).
- Admission work is exempt from `max_reforge_work`
  ([solver_model.hpp:253](../../../engine/src/solver_model.hpp)).
- Ring W1b/W2: only one carrier was admitted before the owned-bytes stop, and
  that one admission produced 104 M / 1.39 B transition entries and
  50.8 M / 173 M logical reroll work. In the protected-only rerun that carrier
  was not the root, which holds the T1 progress; the root had no admission
  record at all.
- Bow CB04 from scratch: 919 carriers, 1.82 B transition entries, 925 M reroll
  work; lock recipes alone took 57.7 s.
- A4/A5 records: 28,906 / 26,885 lock recipes generated, 83 % / 81 % rejected
  at the lock-setup check (lock not craftable there, or already present).
- [hyp] Retry-equivalence certification computes a full lock + reroll kernel for
  every distinct outcome state ([solver_options.cpp:429](../../../engine/src/solver_options.cpp)),
  and distinct locked-side contents each need a new distribution. Not profiled;
  the `protected_detail` telemetry fields support this measurement.

**F7 — The product solve often ships a trivial fallback.** [ran] On CB04 from
scratch, the core Bellman policy (estimated 1,055,896c) failed direct
certification as `route_coverage_failure`: 97.45 % of executions hit states
with no route. The publication pipeline then used 166.9 s of the 240 s. The
early verified fallback, **223,349c** of about 906 k Alterations and 460 k
Augments per run, was published. The only sub-100k CB04 policy on record,
12,770.8c with Multimod + Cannot Roll Attack in its graph, came from the private
dirty-guidance mode with retention reuse and longer budgets [record].

**F8 — Cheap compositions of verified pieces are missed.** [ran] Bow start with
three T1 prefixes and two junk suffixes: Current published **223,064.93c** (the
from-scratch route). Prepending "Prefixes Cannot Be Changed → Scour" to the
solver's own verified **50,798.15c** policy for the empty-suffix start gives
**51,222.52c**, success 1. The first step is in the recipe grammar; it was never
admitted at that root in time.

**F9 — Scour fracture bug (ruling 1).** [code] Fix sites:

- engine: [actions_basic.cpp:688-699](../../../engine/src/actions_basic.cpp);
- solver mirrors: the Scour refinement contract
  ([solver_registry.cpp:1414-1436](../../../engine/src/solver_registry.cpp)) and
  the abstract applicability check
  ([solver_abstract.cpp:1102-1131](../../../engine/src/solver_abstract.cpp));
  junk classes already carry `gen_type`;
- test: [test_solver_abstract.cpp:458-459](../../../engine/tests/test_solver_abstract.cpp)
  asserts the old rule;
- docs, then a WASM rebuild.

Exact rows call the native action and follow automatically. The A4 policy
re-evaluates to 3,746.13194 with its lock → Scour nodes never visited, and A5's
policy never Scours, so headline anchors do not move [ran]. No lower producer
was found that the fix invalidates:

- the phase lower is built on acquisition cost;
- recipe costs come from native kernels;
- the clean-cover projection excludes lock and fracture states
  ([solver_solve_carrier_pattern.cpp:8-53](../../../engine/src/solver_solve_carrier_pattern.cpp)).

The plan should still check each producer.

**F10 — Internal errors instead of named outcomes.** [ran]

- `bounded strict finish lost its verified artifact`
  ([solver_solve_finish.cpp:5384-5390](../../../engine/src/solver_solve_finish.cpp)):
  observed twice (45.15 s and 45.19 s) on W1 with product settings and a
  protected-only diagnostic mask. A requested finish arriving during strict lift
  with no verified fallback throws. Product reachability is unproven.
- In an ungated diagnostic, `max_reforge_work` escaped as `internal_error`
  instead of a named stop.

**F11 — Advertised action metadata is wrong.** [code] `action_transition_facts`
([engine_internal.hpp:798](../../../engine/src/engine_internal.hpp)) marks
Annul, Scour and both Harvest removal actions as ignoring locks, and
Exalt/Aug/Regal as ignoring Cannot Roll; the engine honours both. This feeds
`pc_solver_get_action_info`, telemetry and the lock-recipe filter. Exact
transitions are unaffected. Fix the solver derivation (solver_registry.cpp:1607)
rather than the shared engine table unless every consumer is audited.

**F12 — Scope disclosure.** [code] The product label names the gating, restart
and Imprint restrictions, not the recipe grammar. A native goal-relevant solve
with default options is labelled `globally_optimal_unrestricted`
([solver_solve.cpp:135-148](../../../engine/src/solver_solve.cpp)). The
Calculator summary lists operation types, so a lock appears only as "Bench".

**F13 — New-action integration pattern.** [code]

- Any in-scope Foulborn action switches the whole solve to `TargetNeutralZero`:
  zero lower, no exact closure ([solver_solve.cpp:39-55](../../../engine/src/solver_solve.cpp);
  [solver_api.cpp:686-697](../../../engine/src/solver_api.cpp)).
- The Calculator's visible fallback price fills unquoted action keys while
  pinning ([economy-service.ts:748, 806](../../../apps/web/src/app/workspace/economy-service.ts)).
  A user fallback would therefore silently admit Foulborn and demote every
  applicable solve.
- The new currencies have Calculator-only one-action odds whose successor IDs
  are local, not strategy state IDs
  ([calculator_currency.hpp](../../../engine/src/calculator_currency.hpp)).
  Implicit, influence and corruption goal requirements refuse strategy solving.
- Guards that already fail closed:
  - held currency IDs are rejected ([solver_api.cpp:409](../../../engine/src/solver_api.cpp));
  - new action types need a refinement contract ([solver_registry.cpp:1597](../../../engine/src/solver_registry.cpp));
  - the phase lower refuses unknown recipe kinds
    ([solver_phase_lower.cpp:141-160](../../../engine/src/solver_phase_lower.cpp)).

**F14 — Metamod usage census.** [ran] 157 of 547 saved Current graphs contain a
metamod craft. Of the 19 that still load:

- A4 and the other empty-start Conquest runs craft **none**;
- CB03 (three T1 prefixes held) crafts 2.2 locks per run;
- a near-complete last-mile case crafts 5.9 per run, about 93 % of its cost;
- a September 9 5-goal incumbent crafted about 3,113 per run at a total cost of
  30.3 M.

Metamods win once valuable progress is held and are rarely reached from scratch.

## 5. Why Oliver sees no metamods

- **Armour:** Eldritch Chaos/Annul give one-sided rerolls and removals for
  about 40c, and 0.05–2c ordinary blockers do most Exalt steering more cheaply
  than 212c Cannot Roll. Lock recipes are generated in the tens of thousands,
  but 80–83 % fail the lock-setup check where generated, and the rest are not
  on the executed path (A4's lock → Scour steps are never visited).
- **Bows and other large-pool non-armour items:** there is no Eldritch
  substitute, so metamods should matter more. But the from-scratch product solve
  never reaches a trustworthy sophisticated policy:
  - admission cost and scheduling consume the budget (F6);
  - the core policy fails certification (F7);
  - the early alt-spam fallback ships.
- **Last-mile starts:** metamods appear at once when the solve starts from held
  progress, but any junk sends it back to the fallback (F8).
- **Missing shapes:** the techniques where 424c metamods usually pay are exactly
  the recipes missing (F2–F5).

## 6. Checked and consistent (do not reopen without a new premise)

- Exact rows for Scour, Bench and cleanup call native `apply_action`
  ([solver_calc.cpp:2685](../../../engine/src/solver_calc.cpp)); pool adds use
  the shared pool builder, which honours Cannot Roll. Reroll refill honours
  only a surviving Cannot Roll (ruling 2) and may fill a locked side (ruling 3).
- Restricting Multimod finish to Multimod-in-goal is correct under the clean
  terminal.
- Eldritch and Veiled recipes do not have the F4 discard problem; Eldritch side
  recipes are one-shot.
- The A4 anchor is reproducible (3,746.13194); A5 has no Scour or metamod.
- All five metamods are priced in every bundled economy; missing prices are
  not the cause.
- The registry and API fail closed for unknown or held actions (F13).

## 7. Related prior work and negatives

- [Finder H0–H4](../2026-09-24-strategy-finder/README.md): the protected,
  progress-retaining family was not implemented; dependency-only programme
  admission is open.
- [S8 plan, S8.4R.3F](../../archive/2026-07-19-bestiary-solver-s8/plan.md)
  (around line 745): Fracture promoted from dependency-only to an ordinary
  candidate. This is the precedent for narrowing the rule.
- [Practical four-goal research](../../archive/2026-07-29-practical-four-goal-solving-research/README.md):
  Fracture forces the exact group layout (root Chaos support 217 → 134,477
  strict carriers), and dependency-only cleanup "would have the same effect".
  This is the main cost any plan to make cleanup or metamods selectable must face.
- [Native metamod first-exit v1](../../archive/2026-09-05-native-metamod-first-exit-v1/README.md):
  lower-side Cannot Roll filter domains; legal unmodelled locks remain
  conservative escapes.
- [Eldritch side recipe decision](../../decisions.md): the product recipe
  precedent.
- [Current status](../../solver/current-status.md) "Do not restart without a
  new premise" and the currency record's Pro handoff list.

## 8. Candidate workstreams (inputs, not a selected plan)

| Id | Workstream | Size | Main obligations and risks |
|---|---|---|---|
| A | Correctness and honesty: Scour fix (F9), internal-error degradation (F10), metadata (F11), labels and Calculator naming (F12) | small | mechanic change → re-baseline; WASM rebuild; update mechanics docs with rulings |
| B | Grammar inside the recipe machinery: lock+Scour for cleanup, one-step crafted cleanup, multi-slot lock exits, lock+Annul, lock+Harvest augment, Multimod+lock+Cannot Roll reroll | small code each, medium validation | every recipe adds admission cost (conflicts with D); enlarges the policy class, so exactness is restated over a new scope; lower producers must cover new kinds (phase-lower `grammar_coverage`, operator-proof lowers, residual family constraints in `solver_action_coverage.hpp`); Finder, the selective service and the return bridge share the same admission code |
| C | Structural: metamods and cleanup as ordinary Bellman candidates, possibly limited to carriers holding satisfied goals | large; needs a measuring spike first | subsumes most B shapes and lets Bellman pick stop points; risks layout refinement (four-goal research), more rows per state and lower coverage; compare against B on the witness set before committing |
| D | Admission cost and scheduling | large, research | admit cheap deterministic recipes and progress-holding carriers first; bound per-carrier admission; pre-check locked-side identity before computing kernels [hyp]; reuse lock+reroll kernels across carriers; profile first |
| E | Delivery and certification | medium–large | diagnose route-coverage failure of core policies; cap strict-lift time that does not improve the incumbent; build verified compositions (prefix + verified continuation, F8) as an upper service |
| F | New-action contract | medium–large per family | per-family proof capability instead of whole-solve demotion; guard fallback pricing; solver-namespace kernels, state carriers (corruption, destroyed lifecycle, donor inventory), refinement contracts and lower coverage for Dominance, Vaal, Awakener, double corruption; follow the currency record's list |
| G | Evidence plan | — | witness corpus (ring W1/W1b/W2/W4, bow CB04 plus last-mile, CB05, A4/A5 anchors); metrics: checked cost, time to first policy, admission work, recipe counts, metamod consumption via the evaluator; scope changes are not same-problem improvements; 3-T1 rolled, no bench goal slots; request an allowance from Oliver |

Claude's recommended order (not selected): A; a D measurement spike; the
cheapest B shapes (lock+Scour cleanup, one-step cleanup, multi-slot exits)
together with the D scheduling fix; E; the C decision after its spike; F per
family.

## 9. Open questions

For Oliver:

1. Double-lock Scour.
2. Is enlarging the policy class acceptable if exactness is restated over the
   new scope?
3. Run allowance for timed qualification.
4. Should the Calculator show metamod names instead of "Bench"?
5. Priority between better checked policies (uppers) and tighter certification
   (lowers) for product goals.

For research:

1. Root cause and profile of admission cost.
2. Route-coverage failure mechanism.
3. Lower validity for each new recipe kind.
4. Why W1 hit `max_discovered_states` with only 171 graph states (probably
   admission-interned states [hyp]).
5. Why W1b/W2 stopped on `max_owned_bytes` at ~290 MB tracked against a 1 GiB
   limit (probably transient admission contexts [hyp]).

## 10. Artifacts and audit footprint

- This folder: README, [evidence.md](evidence.md), validated witness
  manifests and cases, and the two authored ring policies.
- `out/2026-09-29-exact-solver-audit/`: raw results, strategies and scripts
  (git-ignored, local only).
- The audit ran 13 bounded native case executions and several evaluator runs as
  diagnostics at Oliver's request, outside any programme allowance. It changed no
  solver source, mechanic, canonical data or product default.
