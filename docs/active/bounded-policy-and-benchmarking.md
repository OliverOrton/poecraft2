# Bounded Policy Results And Benchmarking

**Status: B1 complete and accepted. B2 product presentation is next.**

Parent: [Active work](README.md)

## Objective

Keep the exact mechanic model, admitted-action envelope, stochastic kernels,
and minimum-expected-cost objective, but make useful executable policies
first-class results before exact optimality closes. A bounded result carries

`L <= V*(start) <= J_pi(start) <= U`,

where `L` is the global admissible lower bound, `pi` is the returned executable
policy, `J_pi` is its evaluated expected cost, and `U` is the certified
incumbent upper bound. Report `U - L` and, for `L > 0`, `U / L - 1` as maximum
certified suboptimality, never as the policy's measured distance from the
unknown optimum.

After bounded results compile and reach the product, build a seeded natural-T1
benchmark system that generates, executes, resumes, and compares pinned cases
while recording bound progress, policy action use, and search cost. Benchmarks
guide investigation and optimization; they never authorize action pruning.

## Baseline and authority

- Start from clean `main` commit `60500ef` on
  `codex/bounded-policy-contract`.
- The structural two-T1 baseline for the `60500ef` lineage is 57,182 expanded
  states, 738,139 state-action rows, 1,165,840 transitions, and exact value
  `230.26738656962243`. The currently pinned 57,233 / 903,935 evidence was
  generated at `7b11b34`, before the constructive-search commits. Oliver
  approved re-pinning the active evidence to the `60500ef` lineage.
- Candidate A/C and the exact-search review remain evidence only on
  `codex/exact-search-design` at `273831f`; this plan does not adopt them.
- Archive the superseded exact constructive-policy plan and its final handoff.
- The motivating bracket is
  `261.05161071365512 <= V* <= 4104.7066630770487`. Its
  `U / L = 15.7237` certificate proves feasibility, not near-optimality.
- `HANDOFF.md` owns the exact resume point; this document owns sequence and
  acceptance.

## Fixed boundaries

- Preserve admitted actions, equal-price ties, costs, probabilities, mechanics,
  and production resource caps unless a later owner-approved plan changes one.
- No approximate state merging, sampled transition replacement, empirical
  action removal, or frontend pool/group/weight logic.
- Compile only executable native behavior; never compile a frontier heuristic.
- Retaining the best value-policy incumbent for output must not reuse a stale
  incumbent as search guidance, change occupancy focus, affect admission or
  pruning, or otherwise adopt Candidate C. A synthesized fallback witness may
  be retained across focused rounds strictly for executable upper-bound and
  output use.
- Generated goal modifiers are naturally rollable T1 modifiers only.
  Crafted/bench modifiers cannot be goal slots or satisfy natural-only slots.
- Bench actions stay admitted when legal and priced: metamods, temporary
  blockers, setup, cleanup, Multimod, and other bench-assisted routes remain.
- `goal_modifier_count` means required natural T1 slots, not total explicit
  affixes on intermediate or finished items.
- Resource caps determine comparable benchmark work; wall time is performance
  data and an external safety watchdog. Acceptance runs default to a hard 15
  minutes: launch long runs detached with a watchdog, kill the process tree on
  expiry, retain available telemetry, and treat expiry as a failed performance
  gate requiring diagnosis. Oliver authorized one B1-only 1,800-second
  superseding run after the 900-second deadline was shown to be miscalibrated;
  it completed successfully. The default remains 15 minutes for later runs.
- Run routine acceptance once at the end. Intermediate tests are narrowly
  diagnostic. Oliver owns rendered/visual review.

## B0 - Clean starting point

**Status: complete.**

1. Preserve the review branch as a local evidence commit.
2. Return to `main` without merging Candidate A/C.
3. Create `codex/bounded-policy-contract` from `60500ef`.
4. Confirm the branch was clean before activating this plan.

## B1 - Bounded executable-policy contract

**Status: complete.** The final detached exact two-T1 oracle completed under
the one-time 1,800-second watchdog in 1,092,227.105 ms. It returned exact value
`230.26738656962243`, 57,182 expanded states, 738,139 state-action rows,
1,165,840 transitions, transition hash `ad4fc4865f2872e9`, and policy hash
`f797e61b00a127a7`. Retained-witness telemetry recorded 3 syntheses, 323 reuses,
and 0 refreshes. The implementation and acceptance evidence are committed;
B2 may proceed.

### B1.1 Result vocabulary and ABI

Keep termination and policy quality separate:

    termination = refused_resource_cap
    policy_status = bounded_feasible

    termination = target_gap
    policy_status = bounded_near_optimal

    termination = exact_closed
    policy_status = exact

Add policy statuses `none`, `bounded_feasible`, `bounded_near_optimal`, and
`exact`.

Append fields to `pc_solve_summary` for policy availability/status, lower and
upper bounds, evaluated policy cost, absolute/relative gaps, requested targets,
whether a target was met, and which target fired. Append live lower, upper, and
gap fields to `pc_solve_progress` for worker/UI progress and benchmark traces.
Keep variable-length incumbent detail in telemetry JSON.

Current output helpers overwrite the whole compile-time summary/progress struct
without honoring the caller's incoming `struct_size`. These changes are
therefore an ABI break, not append-safe v1 growth. Bump `PC_ABI_VERSION` from 1
to 2 and update header smoke, native callers, Python ctypes, WASM
marshalling/exports, TypeScript protocol, worker/client, fixtures, and stable
ABI docs. Do not claim backward binary compatibility.

Append optional `pc_solve_options` fields:

- `max_absolute_optimality_gap`;
- `max_relative_optimality_gap`.

Non-positive values disable a target; retain input field-presence checks.

### B1.2 Numerical versus product tolerance

- `epsilon` remains numerical Bellman/policy-evaluation tolerance.
- Name and document the exact-gap proof tolerance rather than conflating the
  existing `epsilon * 10` comparison with product semantics.
- Gap targets never affect Bellman comparisons, action ties, admission,
  pruning, transition generation, or exact closure.
- If both targets are enabled, satisfying either stops the solve. Record which
  fired. Relative gap serves expensive crafts; absolute gap serves cheap ones.
- Relative gap is usable only for `L > 0`.

### B1.3 Atomic incumbent bundle

Create one atomic incumbent containing:

- certified and evaluated values;
- per-state values and policy rows;
- `unveil_preferences` and `option_unveil_preferences` from the same round;
- frontier operator mapping and executable fallback witness;
- Restart/anchor identities;
- round and incumbent kind;
- goal, economy, action-vocabulary, and graph identity; and
- quotient/strict-state provenance for the final representative lift.

Support partial-upper-plus-fallback, direct executable row, and constructive
renewal/progressive-fracture incumbents. Updating minimum `U` atomically
replaces its policy, preferences, fallback, and provenance. Assert selected
rows still belong to their states and final quotient lifting preserves rows,
preferences, transitions, and value.

The atomically captured values, row IDs, frontier operators, fallback, and
provenance are the source of truth. Concrete policy references and Unveil
preference vectors may be materialized lazily from that immutable same-round
state, once when the incumbent is actually returned; do not rebuild those
full-state derived vectors on every improving upper round.

This bundle is output state, not guidance state. Search continues refreshing
and scheduling normally.

Retain a synthesized renewal/progressive-fracture fallback witness in the
atomic bundle across focused rounds. Reuse is permitted only while its goal,
economy, action-vocabulary prefix, referenced row/operator provenance, and
properness remain valid. Monotonic graph or lazy action-vocabulary extension
does not invalidate it: validate the complete operator prefix present at
synthesis plus referenced row owners/operators, then stamp the later output
bundle with the current graph and vocabulary identities without
re-synthesizing the witness. Refresh when no witness exists, an existing
executable dependency changes, or validation fails; a mere round advance,
graph/vocabulary extension, or lower-bound update does not refresh it. Direct
or partial executable upper policies may still replace the retained witness
atomically when they improve `U`. Retention must not influence Bellman
comparisons, focused scheduling, admission, pruning, or exact closure.

### B1.4 Stitch the complete bounded policy

Materialize:

    partial upper-policy row where defined
    otherwise Restart -> fallback anchor -> renewal/progressive route

Guarantee every reachable non-goal has an action, the fallback is proper,
heuristic tips are not actions, unmatched compiled states have an explicit safe
default, preferences match the incumbent, ordinary strategy vocabulary is
used, and compiler caps remain enforced.

Distinguish:

- valid native bounded policy but inexpressible strategy fallback:
  `bounded_feasible` plus compile refusal;
- fallback properness cannot be established: `no_executable_policy` with no
  finite `U` claim.

### B1.5 Early stop

After each complete focused lower/upper round, once an executable incumbent is
installed, stop on the first enabled condition satisfied:

    U - L <= requested_absolute_gap
    or
    U <= (1 + requested_relative_gap) * L

This is a product-performance feature, not a relabel: easy cases should skip
expensive exact closure after an acceptable certificate exists. A cap reached
before the target remains `bounded_feasible` with its cap diagnostic.

### B1.6 Evaluation and occupancy foundation

Evaluate native and compiled policies exactly. Require `L <= J_pi <= U` within
documented tolerance and native/compiled parity. Retain per-state
occupancy/reward data internally now; expose action accounting in B4.

Simulation is independent corroboration. Use a documented one-sided confidence
check because a finite sample mean can exceed a valid expected upper through
noise. Exact compiled evaluation is authoritative.

### B1 acceptance

Oliver approved re-pinning the two-T1 structural evidence from the older
`7b11b34` result to the `60500ef` lineage baseline. Existing slow B1 reports
are the before-timing evidence and must not be rerun merely to reconfirm the
regression.

- Exact cases keep values, actions, and compiled policies.
- Two-T1 remains `230.26738656962243` within accepted tolerance.
- A cap stop returns the matching upper policy, never lower-mode state.
- A target-gap case stops at the first qualifying completed round.
- Targets do not change admission, ties, Bellman semantics, or eventual exact
  results.
- `U`, rows, preferences, fallback, and compilation share one incumbent.
- Every policy-reachable state has an action.
- Native/compiled evaluation agree and satisfy the bracket.
- Cap termination composes with bounded status.
- Unmet targets never report near-optimal.
- Inexpressible and non-proper fallbacks exercise distinct statuses.

## B2 - Product presentation

Compile whenever `policy_available`, not only when `converged`.

Show returned-policy expected cost, optimal-cost lower bound, absolute gap,
certified multiplicative factor, policy quality, termination/cap reason,
requested target/firing criterion, economy, and admitted-action identity.

Approved wording:

    Certified within 1.10x of optimal.
    At most 10% more expensive than optimal.

Never say the policy *is* 10% suboptimal or that `U` is the optimum. A weak
lower bound may make the certificate pessimistic. Bounded policies may open in
Strategy Board and use existing evaluation/simulation surfaces. Preserve cap
warnings and exclusions.

## B3 - Seeded natural-T1 corpus generator

### B3.1 Inputs and goal contract

Support base path/list/class/pool, item level, start item, natural T1 goal
count, prefix/suffix composition, natural family/tag filters, seed, cases per
stratum, resource caps, and watchdog.

Every goal resolves to a generation-type prefix/suffix naturally spawnable at
T1 on that base and item level. Exclude crafted/bench, essence-only, and other
guaranteed-only modifiers unless a later plan defines another corpus. A bench
mod cannot satisfy a natural-only goal because it shares a family/group.

Bench operations remain in the priced action envelope. Crafted blockers,
metamods, setup, and cleanup can increase intermediate explicit affix count,
but not `goal_modifier_count`; record them separately in action telemetry.

### B3.2 Engine-owned feasibility

SQLite may enumerate candidates, but a dedicated native query decides
`feasible`, `infeasible` plus reason, or `unknown`. Reuse goal resolution,
base/item-level eligibility, spawn weights, group conflicts, slot capacity,
and goal-relevant admission. Do not reproduce those rules in Python/TS. A
capped solve is never proof of infeasibility; only engine-confirmed feasible
cases enter the corpus.

Record attempts, accepts, duplicates, ineligible bases, zero-weight families,
group/slot conflicts, infeasible/unknown results, and exhausted strata so
rejection bias remains visible.

### B3.3 Provenance

Persist resolved explicit cases and pin base/item/start, T1 goal families,
action envelope, economy, data hashes, generator version/seed, and generation
engine commit/dirty flag/binary hash/ABI/compiler. Each run separately records
its solver build so A/B versions use the identical corpus.

### B3.4 Initial corpus

- **Smoke:** 12-16 cases, mainly one/two natural T1 goals across armour, weapon,
  and jewellery, plus the bounded three-T1 Dire Pelt.
- **Full short-budget:** 100-150 cases with one to four natural T1 goals,
  weighted toward two/three, varied side mixes, and four to six bases.
- **Deep:** 10-20 cases with three/four natural T1 goals, low spawn probability
  or dense pools, and mixed prefix/suffix layouts.

Tier is not a dimension: every goal is T1. Stratify difficulty by goal count,
side mix, pool density, and native spawn probability.

## B4 - Large-run orchestration and action telemetry

### B4.1 Runner

Add arbitrary corpus/output paths, per-case process isolation, deterministic
order, watchdog kill, survivor check, resumable ledger, skip-completed, and
optional memory-budgeted concurrency; default hard-case concurrency is one.
Resource caps drive comparison, wall time supplies performance and safety data.

### B4.2 Bound trace

At round changes and bounded wall intervals record time, round, `L`, `U`,
gaps, incumbent kind, states/frontier, rows/transitions/reforge work,
live/peak memory, and cap proximity. Derive time to first incumbent and
standard gap thresholds, and whether progress raises `L` or lowers `U`.

### B4.3 Policy action utility

From exact evaluator occupancy aggregate reachable states/regions per action,
exact expected uses, probability of use where computable, expected spend/cost
share, use by progress/rarity/blockers/crafted count/fractured subset, and
lower-versus-upper policy differences. Retain simulator distributions as
empirical corroboration.

### B4.4 Search cost by action

Attribute rows, raw outcomes, retained transitions, reforge work, cache
requests/hits, wall time, and retained memory by action. Non-use is an
optimization lead, never a pruning proof.

## B5 - Stratified reports and iteration loop

Keep analytics separate from the native/WASM correctness gate. Group by
base/class, natural T1 count, side mix, pool density/probability, incumbent,
policy status, and termination.

Report exact/near-optimal/feasible/no-policy/refusal rates, threshold reach,
median/p90/p99 time and memory, time to incumbent/target, `L` versus `U`
progress, work distributions, graph sizes, action utility/search cost, paired
A/B deltas, regressions, and outliers.

Funnel:

1. smoke during development;
2. full short-budget corpus per candidate;
3. deep representative/hard cases;
4. exact evaluation of compiled bounded policies;
5. required 10,000-run simulation only for the acceptance subset.

## B6 - Final acceptance and documentation

After B1-B5, run one downstream acceptance pass:

1. native build and affected complete solver suite;
2. exact two-T1 oracle;
3. capped bounded-policy case;
4. early-stop near-optimal case;
5. native/compiled exact-value parity;
6. required 10,000-run verification;
7. generator determinism/feasibility tests;
8. runner watchdog/resume/survivor tests;
9. aggregation/paired-comparison tests;
10. mandatory WASM rebuild;
11. `npm test` and `npx tsc --noEmit`;
12. appropriate complete cross-layer pipeline once.

Update stable solver flow/contract, Calculator, WASM reference if exports
change, decisions/evidence, active/archive indexes, and final `HANDOFF.md`. No
rendered UI review unless Oliver requests it.

## Completion criteria

Complete only when a non-exact stop returns/compiles the incumbent matching
`U`; status composes with termination; gap targets stop qualifying cases
without semantic drift; native/compiled evaluation validates bounded results;
the UI states the certificate honestly; a pinned natural-T1 corpus is
reproducible through engine feasibility; runs are resumable/watchdog-safe with
no survivors; reports expose policy action utility and action search cost; and
exact, WASM, web, verification, and documentation gates pass.
