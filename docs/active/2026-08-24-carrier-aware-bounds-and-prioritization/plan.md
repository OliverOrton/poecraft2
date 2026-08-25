# Carrier-Aware Bounds And Prioritization

**Status: stopped at Gate 2 on 2026-08-24.** Gate 0 attribution and Gate 1's
authority/type/source refactor remain. The Gate 2 ordering candidate improved
the five-T1 upper but was restored because the retained-control authority was
invalid: the Warlord and earlier non-armour artifacts predate exact terminal
semantics, the later non-armour result used an uncommitted stop-window
override, and the tri-elemental input was not tracked. Gates 3-5 were not
entered. See the [Gate 2 stop result](evidence/gate2/result.md) and
[control reproduction audit](evidence/control-reproduction/result.md).

Proposed from checkpoint: `04cab15`

Activated from checkpoint: `526ff6f`

Parent: [Active work](../README.md)

## Objective

Improve exact multi-goal strategy quality and search efficiency through two
separate authorities:

1. carrier/action ordering that reaches useful executable policies sooner but
   never prunes work or strengthens a proof; and
2. an admissible carrier-aware completion lower that retains enough side,
   junk, blocker, protection, and preservation state to strengthen operator
   pruning and certified gaps.

A gated third step may represent unmaterialized carrier/action obligations by
proved lower-only descriptors. That is the only proposed mechanism that can
make the certified lower continue to rise while the incremental action
envelope remains open.

## Audit of the investigation notes

### Retained findings

- An open incremental action envelope correctly prevents the restricted
  Bellman optimum from becoming the public lower. The public value falls back
  to the independent goal-cover floor.
- The accepted five-T1 artifact has 116 bound samples. Its lower moves from
  zero to `36.4286171890906` once and is then bit-identical, while the upper
  changes repeatedly. It records 14,372 discovered states, 8,650 frontier
  states, 1,868 policy-reachable states, zero goal states, and 125,173 open
  action obligations.
- The current exact-semantics four-of-five workhorse shows the same lower
  pathology more cheaply: `0.01165` lower, `2698.87479601436` independently
  evaluated upper, 6,820 discovered / 6,812 expanded states, one goal state,
  and 108,970 open action obligations in about 21 seconds.
- The universal cover is deliberately probability-free. The clean carrier
  MDP already models probability, rarity, goal subset, side counts, exact
  junk-free success, destructive replacement, weights, tags, and capacity.
  This plan must extend it rather than rebuild those features.
- A non-clean carrier whose required goal mask is already satisfied currently
  receives zero completion cost even when exact terminal semantics prove that
  junk or a temporary craft still has to be removed or replaced.
- Active protection, fractured state, or changed influence/implicit identity
  can remove a carrier from clean-cover eligibility. The strict clean
  refinement still excludes Eldritch sessions and rare source carriers.
- Operator-lower pruning is live in expansion but did not fire in the accepted
  five-T1 evidence. Focused scheduling consumes per-state gaps, so a flat
  lower also weakens work direction even before publication.
- Focused and incremental carrier ladders are asymmetric. Both round-robin by
  exact goal subset, but only the incremental path has a structural
  within-subset order. `blocked_mask`, side counts, fracture, protection, and
  junk counts already exist in native state.
- The archived mandatory-setup-price descriptor was sound. It was rejected
  because work moved immediately into a 200,000-state broad reforge, not
  because its proof failed. Current goal-progress gating and carrier graphs
  are different enough that this blocker deserves one fresh measurement.

### Findings that remain hypotheses

- Zero discovered goal states is a useful symptom, not proof that Bellman
  contributes nothing. Frontier seeds can still anchor an admissible lower.
- The source of `36.4286171890906`—universal cover versus clean MDP maximum—is
  not exposed by current telemetry.
- The accepted strategy traverses protected and dirty carriers, but the share
  of live lower evaluations that fall through the non-clean zero branch has
  not been measured.
- A real-carrier pool score should help ordering, but its benefit and cache
  cost are unmeasured.

### Ideas not promoted into this plan

- Do not publish the July probability MDP directly, average slot orders, or
  remove the probability clamp. Those directions either lost necessary
  blocker state or violate the probability-upper requirement.
- Do not remove the strict Eldritch guard or broaden clean eligibility by
  assertion. Each new carrier shape needs action-coverage and Bellman proof.
- Do not replace subset round-robin with one greedy score.
- Do not filter junk carriers or clean them merely because junk exists. A
  protected reforge may profitably replace an opposite dirty side.
- Do not change the exact Bellman row-index tiebreak unless measurement shows
  meaningful exact-value ties. It is downstream of the admission bottleneck.
- Repository-wide archive and mechanics-document cleanup is valid separate
  debt, but it is not part of this solver behavior boundary.

## Fixed boundaries

- Exact terminal semantics remain unchanged: every explicit affix at success
  is a satisfied requested goal slot; empty slots and implicits remain allowed.
- No fixed subgoal order. Direct multi-slot jumps remain legal and subset
  round-robin remains the diversity owner.
- Ordering scores have no lower-bound, dominance, pruning, closure, or
  publication authority.
- Proof-bearing bounds must cover every admitted and unmaterialized action
  alternative in scope or fall back locally to the existing universal value.
- No mechanics, action admission, prices, goal scope, caps, strategy
  vocabulary, or frontend rule authority changes.
- Do not rerun the full acceptance pipeline during intermediate gates. Run
  one proportional changed-layer acceptance at the end.

## Gate 0 — Current-tree baseline and attribution

Add full-evidence-only, behavior-neutral telemetry before changing search:

- decompose the start and sampled state lower into universal, clean-MDP,
  strict-clean, and selected maximum components;
- count clean-cover eligibility and every rejection reason separately;
- count satisfied-mask-but-nonterminal states, including how many return zero;
- retain goal-state, frontier, policy-reachable, and per-goal-subset counts;
- record operator-lower evaluations, finite values, incumbent margins, and
  `state_incumbent_operator_lower` prunes by action family;
- record carrier and carrier/action scheduling admissions by subset, side
  capacity, blocked mask, protection, fracture, and unrelated occupancy; and
- record time/work to first finite and first independently verified upper.

Use the current exact-semantics four-of-five Conquest Lamellar as the fast
workhorse. Reproduce the five-T1 case once after the telemetry is neutral.
Retain the non-Eldritch partial-five Bow, the tri-elemental Bow executable-
policy regression, Warlord exact closure, and automatic Eldritch controls.

Telemetry is neutral only if values, bounds, status, termination, selected
actions, policy/transition hashes, and graph census are unchanged. Full-
evidence median wall must remain within 5% on the fast workhorse; otherwise
sample or aggregate more cheaply before proceeding.

Gate 0 must answer before implementation:

1. Which cover component owns each start lower?
2. What fraction of expanded and policy-reachable states lose clean coverage?
3. How often does the satisfied-but-dirty zero branch occur?
4. Is lower-bound weakness actually preventing operator pruning?
5. Does the five-T1 case still discover zero goal states at the new baseline?

## Gate 1 — Separate proof and ordering ownership

Refactor only as needed to make authority difficult to misuse:

- move goal-cover/pattern-database construction out of the already-large
  solve heuristic translation unit;
- centralize focused and incremental carrier scheduling in a shared native
  carrier-priority helper; and
- expose distinct proof-lower and ordering-score APIs/types. An ordering value
  must not be accepted by a pruning or publication call site.

Require bit-for-bit numerical and policy parity after this refactor. This gate
is technical-debt reduction, not permission to change behavior.

## Gate 2 — Ordering-only carrier and action priority

Preserve urgent missing-frontier carriers first, exact goal-subset
round-robin, the focused half-batch reservation, and per-class caps. Change
only order within each subset bucket and within a carrier's pending action
work.

Start with a cheap structural key derived from existing state:

1. missing prefix/suffix goals versus free capacity, distinguishing carriers
   that require a removal/replacement before any additive finish;
2. `popcount(blocked_mask & missing_goal_mask)`;
3. preserved/fractured goal progress and useful active protection;
4. action-specific ability to destroy a mandatory blocker or preserve the
   useful side, using `CarrierEffectSummary`; and
5. unrelated occupancy only as a late tiebreak, never as a generic cleanup
   mandate.

Apply the same vocabulary to action-pair order. Prefer an action when it can
reach missing slots, preserves the useful carrier facts, or removes a proved
capacity/blocking obstruction. An action that preserves disposable junk is
not penalized if its own transition replaces that junk before terminal
success.

Only if the cheap key leaves material ambiguity, add a cached ordering-only
score from the actual carrier's weighted pool. Key the cache by carrier pool
signature, action, missing goal subset, and relevant side. Use real
`target_weight / total_weight` or an equivalent existing native pool query;
do not repeat the expensive optimistic blocker scan per state. Prices may
scale this ranking, but the score remains inadmissible and must never filter a
row.

Qualify with fixed-work/fixed-time A/Bs. Record first verified upper, best
upper at 10/20/60 seconds, upper trajectory, expanded/discovered states,
carrier/action rows, reforge work, memory, largest cooperative step, and
selected action families. Retain an ordering layer only if it improves first-
upper latency or fixed-time upper quality without a greater than 5% regression
on any retained control. Final exact results and action coverage must remain
unchanged when an envelope closes.

## Gate 3 — Non-goal-aware admissible carrier pattern database

Build a proof-bearing abstraction only after Gate 0 identifies the dominant
zero/fallback shapes. Extend the existing clean MDP rather than replacing it.
The smallest candidate state should retain:

- rarity and exact satisfied-goal mask;
- prefix/suffix counts and missing-slot sides;
- side-local junk/blocked-goal occupancy;
- crafted/metamod protection mode and its occupied slot;
- fractured goal preservation where it can make the real problem easier; and
- the influence/Eldritch identity needed to choose a valid action relaxation.

The abstraction may grant favorable identity, blocker removal, target side,
goal preservation, or setup for free. It may not omit a cheaper executable
action, charge a setup that the real path can avoid, reduce a real success
probability, or destroy progress that a real outcome can retain.

Cover ordinary primitives, temporary blockers and cleanup, protected Harvest,
automatic Eldritch side programs, Fracture/recovery, and every other admitted
operator family through existing native descriptors or explicitly more-
capable macros. Unknown/unpriced/uncovered shapes fall back locally to the
existing universal cover. The strict Eldritch maximum stays disabled until
its complete option coverage is separately proved.

Add focused proof controls:

- exact goal is zero; goal-plus-junk and goal-plus-metamod are nonterminal;
- positive prices produce a finite positive cleanup debt where every legal
  path must act;
- the protected-prefix/tagged-suffix example does not charge a preliminary
  cleanup that the protected reforge avoids;
- a paired obstruction case charges useful removal/replacement work;
- rare-carrier and Eldritch/none-Eldritch controls;
- adding an action cannot raise the relaxed optimum; and
- for every materialized row in small complete fixtures,
  `h(s) <= immediate(a) + E[h(S')]` within exact numeric tolerance.

Integrate the new value as `max(existing_admissible_components, new_component)`
only after those proofs pass. Then measure whether it creates finite operator
separation and nonzero `state_incumbent_operator_lower` pruning. Do not retain
a large pattern database solely because it makes the displayed lower less
embarrassing: it must either materially strengthen the certified gap or reduce
work on the fast witness without harming controls.

## Gate 4 — Re-evaluate lower-only unresolved action descriptors

Enter this gate only if Gate 3 supplies useful continuation lower values.
First reproduce the archived mandatory-setup-price prototype as a measurement,
not production behavior, against today's four-of-five and five-T1 graphs.

For every unmaterialized carrier/operator obligation, derive a descriptor
lower of the form:

`ell(s,a) = guaranteed immediate price + optimistic successor continuation`.

Use the new carrier pattern database for the continuation only where every
possible exact successor is covered by a no-stronger optimistic shape.
Otherwise retain the archived immediate-price-only value. Descriptor values
are price-scoped and invalidated on repricing.

Lower-only descriptors must:

- participate in the minimization that defines a global lower;
- remain ineligible for executable, constructive, or compiled upper policy;
- be replaced monotonically by an exact row or a stronger proved descriptor;
- remain explicit proof obligations until exact evaluation or strict lower-
  versus-incumbent separation; and
- distinguish materialized closure from proof closure in status and telemetry.

If every unresolved alternative has a valid descriptor, the lower may move as
competitive descriptors are refined even while exact rows remain
unmaterialized. A descriptor whose lower strictly exceeds an independently
verified incumbent can be classified non-improving without building its full
kernel. Exact closure requires a complete row or separation proof for every
alternative; a scheduling decision is never closure.

Stop and restore this gate if the old failure repeats: cheap bench-first
descriptors pin the minimum near zero, the first broad reforge again hits a
state/work cap, no descriptor is separated, or published lower/work is not
materially improved. Do not ship descriptor machinery merely because the old
proof remains sound.

## Gate 5 — Bound consumers and proportional acceptance

After the lower is frozen and proved, route that same authority consistently:

- expansion operator pruning through `optimistic_operator_lower`;
- preservation pruning, replacing immediate-price-only lower only where the
  stronger operator lower has the same proof coverage;
- focused fringe gaps and fixed-time scheduling;
- public lower and gap-target termination only when full action-envelope
  coverage/provenance is present; and
- telemetry that keeps independent pattern, restricted search, unresolved-
  descriptor, and exact-closure values separate.

No consumer may silently upgrade an ordering score, restricted optimum, or
partially covered pattern value.

Run final acceptance once:

| Case | Required property |
| --- | --- |
| Current exact-semantics four-of-five Conquest Lamellar | Faster workhorse; retain a proper exact-evaluated upper and show the bound/pruning mechanism actually fires. |
| Clean five-T1 Conquest Lamellar | Compare lower and upper trajectories, first verified upper, 60-second quality, work, memory, and responsiveness to the accepted artifact. |
| Non-Eldritch partial-five Bow | Ensure carrier ranking and rare-source bounds generalize beyond Eldritch armour. |
| Tri-elemental clean Bow | Preserve the new executable-policy recovery; never regress to `no_executable_policy`. |
| Warlord exact control | Preserve exact value, closure, policy hash, and compiled execution. |
| Automatic Eldritch and Imprint controls | Preserve action coverage, properness, and family-local resource behavior. |
| Protected/dirty paired fixtures | Preserve no-waste cleanup and required-cleanup behavior. |

Every changed compiled strategy receives exact evaluation and 10,000 simulator
runs. Record native and release-WASM parity, bounds/provenance, policy cost,
graph census, action composition, open obligations, state/row/transition and
reforge work, selected memory, wall, largest cooperative step, and precise stop
owner.

Run the native build and affected solver Calc/Solve/compiler/evaluator/policy-
refinement/automatic-action suites, rebuild release WASM, run the applicable
non-visual web tests and `npx tsc --noEmit`, and finish with `git diff --check`.
Do not run the full repository pipeline unless Oliver separately selects it as
a merge gate.

## Success and stop conditions

The plan succeeds only if:

- ordering reaches an equal or cheaper verified upper sooner on at least one
  hard witness without material control regressions;
- the new proof component passes exhaustive focused Bellman inequalities;
- operator pruning or descriptor separation becomes measurably nonzero;
- public provenance remains honest when the action envelope is open; and
- no exact control, action family, compiled policy, or terminal mechanic
  regresses.

Stop precisely at the responsible gate if:

- actual current-tree attribution contradicts the proposed owner;
- a mechanic ambiguity requires a new Oliver ruling;
- any proposed lower exceeds an exact row or complete-fixture optimum;
- useful action coverage is lost or a scheduler starts acting as a filter;
- the descriptor prototype repeats its archived broad-reforge failure; or
- improvement exists only in display values while upper quality, pruning, and
  work remain unchanged.

## Documentation and commits if selected

On activation, move this plan under `docs/active/`, record the chosen boundary
in the active index and `HANDOFF.md`, and preserve measured evidence beside the
plan. On completion, update the solver documentation for the final authority
split and record rejected branches rather than leaving experiments implicit.

Commits remain local unless Oliver says to push and end with:

`Co-authored-by: Codex <codex@openai.com>`
