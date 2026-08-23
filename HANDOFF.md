# Handoff

**Status: the
[Exact-Goal Carrier Ladder](docs/active/2026-08-22-exact-goal-carrier-ladder/plan.md)
boundary is [complete](docs/active/2026-08-22-exact-goal-carrier-ladder/result.md).
No implementation boundary is active.** Oliver must select a successor before
implementation resumes.

## Current checkpoint

- Branch: `main`.
- Local `origin/main` remains at `40eeb87`; no push was issued.
- The accepted implementation and evidence are in the local checkpoint that
  contains this handoff.
- Result:
  [docs/active/2026-08-22-exact-goal-carrier-ladder/result.md](docs/active/2026-08-22-exact-goal-carrier-ladder/result.md)
- Full repository pipeline and rendered UI review were deliberately not run.

## Post-boundary Scour applicability repair

After the accepted ladder checkpoint, Oliver reported a release-WASM strategy
sample in which node `s1` attempted Scour once in every run and all 1,000 runs
ended with `action was not applicable`. The checked accepted clean-five and
partial-five artifacts do not contain that failing node, and the live Strategy
Board document was not available for direct replay. Source inspection still
found and closed a concrete solver/runtime mismatch that can produce exactly
that class of graph.

The native Scour action now counts a rarity-only rare-to-magic transition as
applied even when it removes zero modifiers. The exact calculator no longer
models an illegal or genuinely not-applied Scour as a supported deterministic
self-loop. This is especially important for compound planner programs: an
unusable Scour can no longer be silently skipped during planning and then
emitted as a primitive operation that the strategy runtime rejects. A compiled
candidate that somehow reaches this boundary fails its independent exact graph
evaluation instead of publishing.

The repair is deliberately Scour-specific. The shared RNG-free calculator
branch also discards `ActionOutcome.applied` for Bench and Remove Crafted
Modifiers. A broad change was tested and correctly exposed existing fractured-
crafted cleanup assumptions plus changed several established option contracts;
it was not retained in this focused repair. That separate deterministic-action
applicability audit remains worthwhile, but it needs its own witnesses and
publication review rather than an incidental behavior change here.

Post-repair checks:

- Native build: pass.
- Artifact-backed core action/simulator suite: 2,861,290 checks, zero failures.
- Solver Calc: 436,636 checks, zero failures.
- Solver exact evaluator: 18,065 checks, zero failures.
- Release WASM rebuild: pass.
- Release-WASM engine smoke, including the new magic fractured-carrier Scour
  refusal: 28/28 pass.
- `npx tsc --noEmit`: pass.
- `git diff --check`: pass.
- The focused solver API executable still has the same 13 stale expectation
  failures on both this tree and a separately built untouched `2b8d5ac`
  worktree; none were introduced by this repair.

## Completed exact-goal carrier ladder boundary

Every solver terminal now has exact explicit-affix semantics: all occupied
prefixes and suffixes must be satisfied requested goal slots, while empty
slots and implicits remain allowed. Intermediate junk, temporary blockers, and
metamods remain legal. Native goal classification, compiled success routing,
reforge terminal handling, fixed options, constructive policies, and clean-
carrier lower models use the same boundary. Protected-side/tagged-side
regressions prove disposable junk is not cleaned merely because it exists;
paired obstruction cases retain useful cleanup.

Focused and incremental carrier scheduling now round-robins achieved goal
subsets and uses fracture, protection, occupancy, and unrelated-affix shape to
order work. This preserves direct multi-slot jumps and delays rather than
rejects dirty carriers. Completed cooperative rows can be joined to an already
certified executable frontier, missing carriers feed grow-in-place refinement,
and exact Howard improvement chooses among all completed rows before an
immutable upper candidate is retained. None of this gains lower-bound or
closure authority.

On the clean Allflame five-T1 Conquest Lamellar, the 60-second requested
bounded finish publishes an independently exact-evaluated
`87361.1690420501`-Chaos policy instead of the roughly 470.5-million-Chaos
renewal fallback. The 607-node / 1,460-edge graph has success probability 1,
zero off-policy mass, and complete cost accounting. It completes 10,000 /
10,000 simulations with zero failures or run limits; sampled mean cost is
`86677.1523926521`. The selected policy uses Chaos, Eldritch Annul/Chaos/Exalt,
Exalt, temporary blockers/cleanup, and small Harvest Augment/Reforge branches.
Imprint remains enabled but is not selected.

The result remains honestly bounded. Native Solve takes 71.880 seconds,
discovers 14,372 states, expands 5,722, and retains a certified global lower
of only `36.4286171890906`. The 87,324.74 absolute gap and roughly 8,303
expected primitive actions identify the next quality boundary; no optimality
or exact-closure claim was made. Selected-allocation telemetry reports
209,362,905 live solver-owned bytes, 387,387,874 total solver-owned bytes, and
a 3.226-second largest native cooperative step.

## Completed five-T1 recovery boundary

Completed delayed rows now receive geometric joint-policy checkpoints. Each
checkpoint temporarily considers only fully materialized rows, constructs a
start-reachable proper policy, retains an improved immutable executable
incumbent, and restores prior row admission. It is upper-only evidence and
does not close an action family or strengthen the lower bound.

Stepped Solve now has an append-only `request_bounded_finish` lifecycle across
native C, WASM, worker, and TypeScript surfaces. A request stops discovery at a
cooperative boundary and runs the ordinary compile/certify/exact-evaluate
finalizer. It reports `requested_bounded_finish`, never a fabricated cap or
exact closure. Abort remains abandon. Calculator requests bounded finish after
four minutes so a long five-mod solve no longer expands indefinitely.

The exact owner-supplied four-of-five Conquest Lamellar publishes an
independently evaluated `2083.88214353439`-Chaos policy. Native Solve takes
21.289 seconds and release WASM 21.852 seconds; total qualification wall is
23.183 and 26.519 seconds respectively. Both emit the same 23-node / 36-edge
graph and complete 10,000 / 10,000 simulations with 100% success and zero
off-policy failures. The strategy uses Exalt, a temporary blocker and cleanup,
Prefixes Cannot Be Changed plus Harvest Reforge Physical, and Eldritch Ichor
plus Eldritch Chaos. It is not Chaos spam.

At bounded finish the native graph has 3,047 discovered / 898 expanded states,
22,856 rows, 79,612 transitions, and 188,182 reforge work. Selected-owned
memory is 53,307,653 live / 124,297,621 peak bytes. The public lower is
`0.01165` and 17,636 action obligations remain open, so the result is useful
but not exact. Candidate scope has zero missing prices: 10 currency, 13
Fossil, four Harvest, and one Fracture candidates, plus 158 carrier-local
automatic dependencies including Bench and Eldritch programs.

Warlord remains exact at `224.123858897249`; Magic Imprint retry remains exact
at `252.653520212745`; both pass 10,000 simulations. The four-T1 control retains
its accepted bounded Imprint-work refusal at `3759.97631221018` and also passes
10,000 simulations. A new non-armour partial-five Bow control publishes an
honest exact-evaluated bounded fallback but shows that an attractive early
partial-frontier row policy can remain structurally unmaterializable; that is
separate cross-base quality debt.

Release WASM keeps the 278,396,928-byte reported linear-memory high-water and
has a 6.942-second largest worker slice on the primary. The headless Node RSS
measurement grows to 3.278 GB, so browser/device memory and responsiveness
remain open measurement owners rather than inferred guarantees.

## Completed compact-certification boundary

Ordinary primitive policy regions now share by their complete emitted runtime
behavior: operation JSON, accounting roles, and common master-policy
continuation. Gated-capable reforges with zero retry mass participate as
ordinary regions. Product Fracture, positive-mass gated repeats,
observation-owned choices, state-local choices, and compound fixed options
retain their stronger or local sharing authority.

On the Imprint-disabled four-T1 witness this reduces the direct graph from
3,208 nodes / 7,139 edges / 1,356 policy regions to 570 nodes / 1,690 edges /
33 regions. Exact evaluation still discovers 162,829 real carrier pairs and
refines them to 43,833 behavioral pairs, so Calculator's optional direct-proof
allowance is raised from 5,000 to the measured 200,000 boundary.

The independently evaluated direct policy costs `2823.050846721888`, improving
the prior `3759.9822404728984` fallback by 24.9%. It is proper, completely
priced, and zero-off-policy. It remains bounded because the coarse estimate /
former lower is `2889.7687877196995`; the 66.718 mismatch invalidates that
coarse lower as publication authority, so the public lower is zero. Under a
nonzero product optional-proof allowance, this cheaper verified candidate now
publishes without a redundant strict lift. Zero-allowance native callers keep
historical exhaustive behavior.

Native acceptance solves in 174.221 seconds, independently matches the exact
compiled cost, and completes 10,000 / 10,000 simulator runs with zero
off-policy failures; sampled mean is `2818.5691565639445`. Direct certification
peaks at 583,591,051 owned bytes.

The corrected release-WASM diagnostic publishes the same bounded result in
267.181 seconds with the same graph. Its duplicate post-solve exact evaluation
crosses the shared 300-second benchmark watchdog. WASM heap grows from about
278 MB to 917 MB, and the maximum worker slice is 47.45 seconds. These remain
explicit speed, memory, and cancellation-responsiveness debts. The WASM
benchmark runner now forwards `consider_imprint_programs`; its prior omission
had accidentally labeled an enabled control as the disabled diagnostic.

## Retained bounded-publication boundary

Incremental executable-upper passes perform exactly one proper fixed-policy
proof on an open graph; they no longer continue Howard improvement merely to
optimize that witness. Automatic carriers and delayed primitives are batched
at frozen frontier epochs. Exact envelope closure still admits every completed
legal alternative together and runs normal Bellman selection.

The first stable materializable selected policy is retained as an immutable
anytime candidate and independently compiled/exact-evaluated in finalization.
Appending state-local operators does not invalidate an unchanged captured
vocabulary prefix. Non-finite compiler `expected_cost` metadata is omitted,
so compiled JSON stays valid.

Calculator passes `max_policy_refinement_states: 200000`. It bounds only
optional direct certification/strict lift after fallback verification; native
callers omitting the append-only ABI field retain the historical allowance.
The new value is the measured compact direct-graph closure boundary. When
direct certification exhausts it, the verified fallback remains available.

An Imprint work/depth refusal is now family-local. Its transaction rolls back,
the family stays unresolved and blocks exactness, and the same carrier is
replayed without Imprint so all unrelated automatic/delayed work can finish.
The solve does not spend the exhausted Imprint-family budget again on later
carriers. Mechanical applicability remains the native exact carrier check;
no rarity-, goal-count-, or scalar-price dominance heuristic was added.

## Retained predecessor measurement

The enabled four-T1 primary solves in 14.204 seconds, retains the certified
`21.772459401332767` lower, and publishes the independently evaluated
`3759.9763122101763` upper in an 87-node / 241-edge strategy. It leaves Imprint
open at `max_imprint_program_work`, but reaches zero unevaluated actions and
continues Harvest, Fossil, Fracture, Eldritch, Bench, and other automatic work.
The strategy passed exact evaluation plus 10,000 simulations with 100% success
and zero off-policy failures; total solve/compile/verification wall was 74.072
seconds.

With only Imprint disabled, the same request closes all 119,838 alternatives,
raises the lower to `2889.7687877196995`, and publishes the independently
evaluated `3759.9822404728984` policy in 146.378 seconds. The optional direct
attempt reaches the 5,000-state refinement budget in 2.168 seconds and strict
lift does not run.

The current rerun keeps the Warlord control exact at `224.123858897249` and the
dedicated Magic Imprint retry exact at `252.653520212745`; both complete 10,000
simulations at 100% success and zero off-policy. The enabled primary retains
its `3759.9763122101763` bounded upper and 10,000 / 10,000 successful trials;
compiler compaction reduces its graph from 87 to 79 nodes without changing
cost.

## Acceptance

- `powershell -File scripts/build.ps1`: pass.
- Focused native solver tests: 96,550 checks, zero failures.
- Focused compiler tests: 863 checks, zero failures.
- Release `powershell -File scripts/build-wasm.ps1`: pass.
- Complete `npm test`, including 28/28 release-WASM smoke checks: pass.
- `npx tsc --noEmit`: pass.
- `git diff --check`: pass.
- Full repository pipeline: deliberately not run.
- Rendered/visual review: deliberately not run; it remains Oliver's.

## Prior stopped Imprint proof boundary

Two sound pre-evaluation terminal-renewal dominance proofs increased exact
program pruning from 169 to 385 and then 461, but the checked carrier still
spent all 256 program units. Surviving kernels grew from 182,778 to 2,072,977
action-state evaluations and from 2.57 million to 2.14 billion merged outcomes;
solve wall regressed from 47.789 to 107.747 seconds. The final open witness is
the mechanically distinct `Fossil > Harvest Reforge Defences > Fracture`, not
another repeated renewal. Engine and test source was restored byte-for-byte to
`f8c5932`.

Do not repeat local renewal predicates, scalar-price ordering, or a cap-only
increase. The retained work counter also fails to describe the several-orders-
of-magnitude support-processing spread between programs.

## Prior recovery-scoped Restart boundary

Calculator-default Solve scope now excludes voluntary discard-and-buy-new-base.
The UI has an unchecked explicit opt-in, while native/explicit callers retain
the historical unrestricted default. No ordinary Bellman state receives a
Restart row in restricted mode, and unmatched bounded compilation fails
closed.

Product-local Fracture replacement is independent of that action scope. A
miss still pays `base`, reaches a fresh Normal carrier, and compiles the
dedicated `product_fracture_restart` retry operation. Do not generalize this
authority to Influence Exalt or another miss: the Warlord control continues on
the influenced carrier with Harvest Reforge Fire and requires no replacement.

Operator lowers now carry only source goal slots retained by at least one
proved runtime execution path, then add all possibly reached slots. Sequential
refinement contracts are the authority. Exact reset removes progress;
incomplete semantics and may-destroy selectors retain it. The universal and
proved shape-aware state covers combine by maximum, never replacement. The
strict rare-carrier cover remains excluded on Eldritch-eligible sessions until
it models automatic side options. The reported concrete-rare ordering bug was
stale and current source already counts both sides correctly.

Restricted proper-policy initialization now permits stochastic retry SCCs when
no executable incumbent exists. The deterministic seed is evaluated by the
existing exact SCC solver and never gains publication authority by itself.
Historical unrestricted initialization remains unchanged.

## Possible successors

No successor is selected. The closest evidence-backed choices are:

1. **Rare-carrier successor-aware lower bounds.** The exact five-T1 policy is
   executable at 87,361.17 Chaos, but the certified global lower is only
   36.43. Extend the existing admissible cover with proved post-action carrier
   shapes and rare successors so focused expansion has a useful gradient and
   branch-and-bound can reject obviously weaker continuations.
2. **Carrier/action quality scheduling.** Use the recovered goal-subset ladder
   as an incumbent generator: prioritize actions that can move between useful
   exact carrier shapes, including direct multi-slot jumps, without turning a
   preferred subgoal order into pruning or exactness authority. The current
   policy still averages roughly 8,303 primitive actions.
3. **WASM/cooperative responsiveness.** The accepted native witness has a
   3.226-second largest solve step, while predecessor exact evaluation reached
   47.45 seconds and about 917 MB in release WASM. Split measured row/kernel
   and exact-pair stages cooperatively or reduce retained carrier payload
   without weakening exact evaluation.
4. **Carrier-equivalent Imprint recovery dominance (exact research owner).**
   Compare checkpoint restore against a proper non-Imprint recovery policy to
   the same observable Magic carrier class from every relevant failure class,
   including distributions and continuation value. Primitive-price or
   arbitrary-Magic comparisons are insufficient.
5. **First-class exact Imprint kernel/label search.** Retain this only if exact
   closure over the included Imprint family remains the selected goal after
   the now-healthy product bounded-publication path.

## Retained architecture

Production strict refinement owns one durable quotient session across
competitive frontier growth. The strict oracle, split-only partition, Bellman
graph, proof store, alternative obligations, published rows, and independently
evaluated incumbent grow in place. Source/target splits invalidate dependent
proof authority through stable generations and reverse indexes. Do not replace
this with the rejected cross-generation carrier-row cache or another unbounded
replay cache.

The retained implementation spans the local checkpoints `18e4640` through
`bb29378`, with release qualification through `9d447f5`. The prior accepted
persistent-quotient result remains at
[docs/active/2026-08-21-persistent-quotient-session/result.md](docs/active/2026-08-21-persistent-quotient-session/result.md).

Exact-goal quality proof, broader compiler/router work, mechanics, prices, and
action admission remain separate unless Oliver explicitly selects them.
