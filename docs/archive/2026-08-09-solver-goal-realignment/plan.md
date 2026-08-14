# Solver Goal Realignment And End-To-End Capability Recovery

**Status: completed on 2026-08-13.** See the [completion report](report.md).

Owner: Oliver

Parent: [Documentation archive](../README.md)

Starting commit: `c00c18133f88151dc971955c161d01e2178aef4b`.

Branch: `codex/solver-goal-realignment`.

## Objective

Given any supported item, arbitrary legal current state, achievable goal, and
pinned current economy, discover the minimum-expected-chaos-cost executable
crafting strategy across every relevant supported crafting family, compile it
faithfully, verify the exact emitted graph independently, and distinguish an
exact optimum from the best verified policy found under resource limits.

The solver must remain anytime: install a valid executable incumbent early,
improve it while relevant work continues, and pursue exact closure. Irrelevant
actions stay narrowly filtered, while a relevant action may not disappear at
registry, dependency, pricing, carrier generation, scheduling, abstraction,
resource-accounting, publication, compilation, evaluation, simulation, or
presentation boundaries.

Target behavior:

- representative one- through three-mod goals normally finish within one
  minute;
- the primary four-mod Allflame case reaches exact lower/upper equality and a
  verified executable policy within five minutes; and
- representative five-mod goals may run for ten minutes, returning exact when
  possible and otherwise the cheapest independently verified executable policy
  with honest bounds and unresolved obligations.

A Chaos-only policy is exact only when every relevant alternative has a
completed Q value or a valid non-improvement certificate. A cap-stopped Chaos
fallback is an incumbent, never proof that Chaos is optimal.

## Owner decisions and preserved boundaries

- Preserve `goal_progress_gated_reforges`. Zero-progress destructive outcomes
  may use the existing renewal basin; every partial-progress state retains its
  complete relevant salvage envelope.
- Natural and crafted/bench modifier identities remain distinct. A bench goal
  names and requires the actual crafted modifier.
- Reject mechanically impossible goals as impossible. Do not turn an
  achievable goal on an unusual base into `no policy` through an item-class
  assumption.
- Support the existing basic-currency, narrow relevant Essence, narrow Harvest,
  bounded positively relevant Fossil, permanent/temporary bench, metamod,
  Multimod, Eldritch, Fracture, Imprint, Influence Exalt, and Veiled primitive
  vocabulary through the product path as specified below. Do not enumerate the
  unrestricted Fossil universe or broaden the current Harvest/Essence filters.
- Craicic Croaker is the required magic-item Imprint beast. Replace Craicic
  Chimeral identity across every layer. Croaker uses its live Allflame Beast
  quote. Each of the three generic rare beasts costs one chaos through an
  explicit, overridable owner default with non-market provenance.
- The primary fixture supplies a five-chaos base/Restart price. Base prices
  remain fixture/user inputs, not a universal five-chaos default.
- Eldritch Exalt is generated automatically only when preserving the opposite
  side is useful.
- Automatic Veiled planning may use one relevant blocker, observe the actual
  three offers, choose the best legal goal-satisfying Unveil after observation,
  clean up, and continue. It is relevant initially only when an unveiled
  modifier directly satisfies an unmet requested goal.
- Do not infer another Path of Exile rule. Stop and ask Oliver if implementation
  encounters a mechanic ambiguity not resolved by these decisions or the
  stable mechanics library.
- SQLite game data and the compiled artifact remain generated through their
  owning tools. Economy publication uses a fresh isolated database and is
  local-only. Nothing is uploaded or externally published.
- Commits are local-only and carry the required Codex co-author line. Oliver
  owns rendered/visual review; this milestone runs non-visual acceptance only.

## Gate 0 - boundary and fresh baseline

Frozen measurements and raw-artifact identities are in
[Gate 0 baseline](evidence/gate0-baseline.md).

1. Verify the clean starting commit and branch, read the required authority
   documents and the two latest milestone reports, activate this plan in the
   documentation indexes and `HANDOFF.md`, and commit the boundary before
   production changes.
2. Freeze source, native executable, release-WASM wrapper/module, compiled game
   data, and active Allflame economy identities. Preserve the historical
   Allflame and Mirage evidence without rerunning either complete portfolio.
3. Capture focused product-path baselines for:
   - the primary four-mod Conquest Lamellar case;
   - one simple exact one-mod control;
   - one existing partial-item control;
   - one Ring and one Amulet case;
   - one automatic Eldritch forced winner;
   - one Imprint eligibility case; and
   - one explicit-policy Veiled case.
4. For every baseline retain termination reason, declared scope, lower/upper
   bounds, published cost, family candidate/dependency/filter counts, missing
   prices, generated/eligible automatic options, completed/unresolved rows,
   meaningful resource counters, transition/policy hashes, and compile/evaluate
   outcome.

### Primary frozen acceptance case

Add a new Allflame fixture without modifying the historical Mirage fixture:

- Conquest Lamellar, item level 86;
- empty rare, uninfluenced, unfractured, no crafted modifiers or Eldritch
  implicits;
- explicit `base = 5` chaos;
- four required natural T1 goals: increased Armour and Evasion; hybrid
  Armour/Evasion plus Stun and Block Recovery; flat Armour plus Evasion; and
  additional Physical Damage Reduction; and
- normal Calculator goal-relevant action envelope with zero-progress-gated
  reforges.

The root and high-impact partial-state evidence must account for Chaos,
retained relevant Fossils, Harvest Defence, any relevant Essence, prefix
protection, temporary blockers, applicable Cannot Roll routes, Exalt/Annul
continuations, Eldritch suffix salvage, useful Eldritch Exalt, and legal priced
Fracture/Restart. No family is forced to win.

## Gate 1 - fresh-eyes end-to-end audit

The executable audit contract and current evidence are in the
[Gate 1 action-family matrix](evidence/gate1-action-family-matrix.md). The
focused family-contract gate covers every primitive action type, every native
automatic kind, and the separate Bestiary operations with typed price,
carrier, scheduling, row/Q, selection, compiler, evaluator, Simulator, C-API,
and release-WASM paths; it passes 182,656 checks. The matrix keeps outstanding
selected-policy and final release-WASM proofs explicit rather than treating
vocabulary presence as end-to-end acceptance.

Audit the live chain:

```text
goal UI and serialization
  -> feasibility and modifier identity
  -> catalog relevance and dependency retention
  -> price identity and completeness
  -> carrier-local option generation and scheduling
  -> exact transitions and state refinement
  -> Bellman values, bounds, termination, and policy retention
  -> compilation and exact graph evaluation
  -> Simulator and frontend presentation
```

Create an engine-owned family matrix covering primitive implementation and
legality, candidate/dependency/filter reason, price keys/provenance, carrier
eligibility, automatic generation, scheduler admission, row completion,
Bellman-Q availability, selected-policy coverage, compiler/evaluator/Simulator
vocabulary, C ABI, and release-WASM coverage.

Specifically search for unrestricted-registry tests hiding product filtering,
test-only prices, synthetic winners without public-path coverage, evaluated
but unpublished delayed rows, values without executable provenance, compiler
or evaluator vocabulary gaps, misleading frontend labels, resource counters
that combine unrelated work, and any family with zero candidates across the
corpus. Assign each confirmed defect to its owning boundary before broad
optimization.

## Gate 2 - action and price availability

The completed economy/action evidence is in
[Gate 2 action and price availability](evidence/gate2-action-price-availability.md).

1. Replace Craicic Chimeral with Craicic Croaker in the Bestiary manifest,
   stable price catalog/provider mapping, generated/static bundles, native/web
   consumers, solver cost vectors, compiler/Simulator accounting,
   documentation, and fixtures.
2. Enable the required Beast ingest for Croaker without exposing every Beast
   as a solver action.
3. Represent `beast:rare = 1` as explicit overridable owner-default provenance,
   never as a poe.ninja quote.
4. From a fresh isolated economy database, generate a new immutable Allflame
   snapshot with Croaker market price, the rare-beast owner default, unchanged
   Currency/Fossil/Resonator/Essence and canonical Harvest coverage, and
   explicit missing-price reporting.
5. Add a product-path Imprint case proving all four recipe components are
   priced and the option is not silently skipped. Audit every supported family
   for explicit price completeness/exclusion.

The reusable cross-runtime Imprint fixture now closes exactly at
`252.65352021274481`, compiles one create and one restore operation, and passes
the exact 3:1 rare-beast/Croaker consumption contract with complete pricing.
Its final native/release-WASM 10,000-run qualification remains part of the
end-of-plan acceptance pass.

## Gate 3 - bounded automatic Veiled crafting

Gate 3 is complete. Oliver ruled that the Simulator's acquisition-time offers
are authoritative and that an offer set with no goal modifier takes the best
legal cleanup/retry continuation. The bounded automatic grammar therefore
permits relevant cleanup only before acquisition, then requires immediate
Veiled Chaos or Veiled Exalt -> observed Unveil with no intervening mutation.
The exact solver and sampled engine are distribution-equivalent on that
grammar. The ruling and proof boundary are recorded in
[Veiled crafting](../../mechanics/veiled-crafting.md), and the focused result
is recorded in
[Gate 3 automatic Veiled evidence](evidence/gate3-automatic-veiled.md).

Reuse the existing exact Veiled primitives, offer identity, compiler routing,
evaluator, and Simulator. Add carrier-local bounded programs with optional
relevant pre-cleanup, Veiled Chaos or Veiled Exalt, observation of the exact
offer set, best legal goal-satisfying choice, optional cleanup, and
continuation.

Generate only when an eligible unveiled modifier can directly satisfy an unmet
goal. Never resample an observed offer. No post-acquisition blocker enters;
pre-cleanup remains dependency-only. Keep Veiled primitives out of the broad
standalone candidate set. Add public-path eligible, ineligible, missing-price,
and forced-winner cases. Compile and exact-evaluate at least one selected route,
then complete 10,000 Simulator runs.

## Gate 4 - bounded Eldritch Exalt planning

Extend the carrier-local Eldritch-side planner with Eldritch Exalt only when
the session and influence/implicit state are legal, the target side has
capacity, an unmet goal can roll there, preserving the opposite side is useful,
and setup/action prices are complete. It remains dependency-only and must not
widen unrelated parent layouts.

Cover prefix target, suffix target, full side, missing price, influenced state,
ineligible base, and one public-path forced winner that compiles and verifies.

## Gate 5 - multi-action solver completeness

Focused counter, watchdog, and leaf-ownership evidence is retained in
[Gate 5 solver-completeness recovery](evidence/gate5-solver-completeness-recovery.md).

The first repaired primary rerun proved that the former transition-cap stop
was false: all 3,621 parent states were expanded while only 35,866 sparse rows
and 52,758 transitions were retained. It then expired inside one unyielded
solver step. Carrier-local automatic admission is now a transactional,
single-flight cooperative continuation. Focused cancellation, deterministic
resume/no-replay, rollback, owned-byte reconciliation, and final-checkpoint
cap tests pass; six resumes and five suspensions measured a maximum
uninterrupted leaf of 0.210 ms against the 20-second worker slice.

Automatic child discovery is now separate from the retained parent-state cap.
The Warlord-only control consequently completes that former boundary, exposing
644,462 transient automatic child states against only 698 retained parent
states. Its next honest stop is the shared `max_reforge_work` accounting limit.
At that stop the solver previously discarded a completed start-reachable
Warlord/Restart route because an irrelevant expanded component made the global
proper-policy bootstrap fail.

The resource-stop constructor now works over the start-reachable admitted-row
attractor, independently certifies the resulting proper policy, and exposes an
unsampled authoritative signal when the cheapest evaluated fallback candidate
is selected. Its focused open-envelope regression publishes a reconciled
74.625-chaos Transmute/Restart incumbent with lower bound zero while irrelevant
expanded states remain outside the policy. Automatic-admission reforge work is
now separate from retained parent work as well. A debugger capture assigned the
subsequent unreturned work item to `discover_automatic_imprint_options`, and
that discovery/attempt traversal is now cooperative and transactional. Its
focused gate preserves goal-capable longer prefixes such as Augment followed by
Regal. Price-bounded finite closure is now implemented: it uses only a
carrier-local certified incumbent upper, downward-rounded mandatory prices, an
outward-rounded useful-depth ceiling, exact terminal-kernel equality, and
componentwise primitive-resource dominance. Zero, missing, or nonfinite prices,
an absent incumbent upper, and the independent work cap remain honest open
boundaries. The final focused native gates pass 98,783 Solve checks and 60
dedicated Imprint compile/evaluate checks.

The post-proof Warlord rerun did not reach the former depth witness. Its
60.354-second watchdog expired without a survivor after 8,943 returned solve
slices. All 698 parent states were expanded and the frontier was empty, but the
solver remained in iteration round zero with zero completed sweeps, no
incumbent, and no finite upper; 5,081 rows, 3,719 transitions, and 2,555,093
retained reforge-work units were unchanged at the final sample. Because this
carrier never acquired a certified upper, the price certificate was correctly
dormant and the no-incumbent Imprint fallback did not finish its first Bellman
sweep within the watchdog. This is the new precise Gate 5 boundary. Characterize
and close that fallback, then rerun Warlord before the primary. Do not start the
mixed-action seven-step follow-up or the primary from this handoff.

Run the primary four-mod case and trace every relevant family to materialized,
pending, completed, proved non-improving, selected, or deterministically
rejected. Repair confirmed scheduling, delayed-row lifecycle, partial-state
expansion, upper propagation, lower closure, resource accounting, family
interleaving, policy retention, and exact-termination defects.

Retain only state identity observed by fractures, crafted modifiers/metamods,
affix groups, influence, Veiled offers, Eldritch dominance, tag blocking, or a
future legal distribution. Preserve the existing exact junk abstraction where
future actions do not observe identity. Split/supplement resource counters when
materially different work shares `max_reforge_work`; a limit blocks only on the
resource that threatens time or memory.

Install a verified executable incumbent early, improve it as rows complete,
and publish the cheapest verified policy at a resource stop. Exactness requires
closure of every improvement obligation. The primary case is accepted only at
exact lower/upper equality with a compiled independently verified policy.

## Gate 6 - all-base correctness and evaluator recovery

Gate 6 is complete. The Ring/Amulet publication and independent-evaluator
repair is recorded in
[Gate 6 evaluator recovery](evidence/gate6-ring-amulet-evaluator-recovery.md).
Both cases now publish honest bounded executable policies whose emitted
three-node/three-edge graphs exact-evaluate with success probability one and
zero off-policy or unresolved mass. Amulet completes in 1.634 seconds at
`144.88045459605041`; exact exchangeable-family compression reduced its
previously interrupted row from 20,000,025 to 1,043,760 logical work without
raising a cap or weakening probability semantics. The direct every-base matrix
passes 2,811,093 checks over all 979 ordinary bases. The isolated current-
product Prefixes Cannot Be Changed start and full-junk-prefix suffix-salvage
controls both close exactly, compile, independently evaluate, and complete
10,000/10,000 native Simulator runs. Late-discovered incremental carriers now
have a complete state/operator-pair ledger, and constructive renewal can
compose a stationary strict carrier through the real fresh-base setup and an
arbitrary priced-Restart start.

Run lightweight generated checks over every compiled mechanically achievable
base for session creation, feasibility, rarity, modifier availability, action
roles, price completeness, one-action transitions, and compiler/evaluator
vocabulary. Use deeper representatives for Eldritch and non-Eldritch armour,
one-/two-handed weapons, bows/wands, shields/quivers, Rings/Amulets/Belts,
unusual affix-count bases, supported magic/non-rare starts, influence,
fracture, crafted/metamod states, valuable partial states, and full-junk-side
salvage.

Repair Ring and Amulet exact-evaluation/publication failures at the sparse or
shared attribution and strict-partition ownership boundary. Do not weaken
partition correctness, discard probability mass, or bypass independent
verification.

Focused acceptance: [Ring/Amulet evaluator recovery](evidence/gate6-ring-amulet-evaluator-recovery.md).

## Gate 7 - compiler and presentation integrity

Gate 7 is complete. Focused native Solve/compiler/evaluator suites and the
non-visual frontend presentation, workspace, strategy-model, Calculator-mode,
goal-item, corpus-contract, and TypeScript checks pass. Exact, bounded,
cap-stopped, and no-policy authority remain distinct; an open action envelope
or absent executable policy blocks exact presentation even if numeric bounds
are equal. Strategy Builder round-trips the engine-authored policy and router
provenance. Evidence is recorded in
[Gate 7 compiler and presentation integrity](evidence/gate7-compiler-presentation-integrity.md).

For every headline strategy, compile the actual selected policy, retain action
and policy provenance, reconcile compiled expected cost with solver cost,
require eventual success probability one, and require zero failure,
unresolved, and off-policy mass. Routers distinguish only observations that
change continuation and collapse equivalent branches safely. Calculator,
Strategy Builder, exact evaluation, and Simulator must accept the same document.

Frontend output clearly distinguishes exact versus bounded, lower/upper bounds,
stopping resource, product scope, zero-progress restriction, admitted families,
missing prices, unresolved obligations, and why a Chaos-only result is exact or
only incumbent. Equal displayed bounds without an executable policy is a hard
defect.

## Gate 8 - performance and final acceptance

Gate 8 is complete. The native/release-WASM primary closes exactly at
`3745.7309340083884` inside five minutes, the representative five-mod case
returns the required independently evaluated bounded policy with a truthful
gap, all 979 ordinary bases pass, selected 10,000-run checks pass, and the
single native/release-WASM 49-case portfolio comparison passes 4,072 semantic
checks. Evidence is recorded in
[Gate 8 performance and final acceptance](evidence/gate8-final-acceptance.md).

Measure deterministic native behavior first and release WASM second. Record
native/WASM live and peak owned bytes, state payload, transitions, evaluator
attribution, strategy size, and browser heap growth. Raise a memory cap only
with measured browser evidence and explicit disclosure; do not hide duplicate
state or broken lifecycle accounting. Add checkpoint/replay only if repeated
construction materially blocks iteration and the format rejects incompatible
engine/data/action/economy identities.

During implementation run only narrow diagnostics. At final acceptance, once:

1. rebuild native;
2. rebuild release WASM;
3. run the complete repository acceptance pipeline;
4. run the primary four-mod case in native and release WASM;
5. run one representative five-mod ten-minute case;
6. run the lightweight every-base matrix;
7. run selected 10,000-run Veiled, Eldritch/Eldritch Exalt, Imprint, and
   primary-policy verifications; and
8. compare native and WASM solve, policy, compilation, and evaluation semantics.

Run the complete 49-case portfolio comparison only once, and only when the
focused gates are green.

## Completion contract

The milestone succeeds only when the Conquest Lamellar case is exact within
five minutes with a verified compiled policy; every relevant family has an
engine-owned inclusion/exclusion/dependency reason; Veiled, Eldritch Exalt, and
priced Croaker Imprint work end to end; narrow Harvest/Essence/Fossil filtering,
bench/natural goal identity, arbitrary starts, and every achievable supported
base remain correct; Ring and Amulet policies independently evaluate and
publish; the ten-minute five-mod run returns its best verified policy with an
honest gap; native/WASM/compiler/evaluator/Simulator/frontend agree; and final
acceptance passes without an unexplained material regression.

If the primary case does not close exactly, do not call the milestone
successful. Retain the best executable policy and complete root-cause evidence,
then leave a precise active boundary instead of weakening claims or raising an
arbitrary cap.

## Checkpoint commits and documentation

Use logical local checkpoints for:

1. this active plan and handoff boundary;
2. fresh audit/evidence and isolated economy/price repair;
3. major action/solver implementation;
4. substantial cross-base/evaluator/compiler recovery; and
5. final tests, generated WASM, report, archive, and completed handoff.

At completion, promote durable contracts to the stable solver, product,
economy, mechanics, foundation, decisions, and evidence owners; archive this
plan with its report/raw evidence; restore the active indexes and `HANDOFF.md`
to no active boundary; and leave a clean worktree.
