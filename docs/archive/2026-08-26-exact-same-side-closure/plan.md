# Exact Same-Side Closure Plan

**Status: complete.** Selected by Oliver on 2026-08-26 from source checkpoint
`9ae5a1d` and completed on 2026-08-27. See the [result](result.md).

Parent: [Exact Same-Side Closure](README.md)

## Objective

Make a clean rare item with exactly three requested prefixes, and a symmetric
clean rare item with exactly three requested suffixes, close exactly under the
current Calculator product profile. Exact means all of the following:

- public policy status `exact` and termination `exact_closed`;
- the requested priced action envelope is closed, with no unresolved action,
  carrier, or operator obligations silently discarded;
- certified lower equals the independently evaluated executable upper;
- the compiled strategy is proper, cost-complete, succeeds with probability
  one, has zero off-policy mass, and contains no junk explicit affixes at
  success;
- native repetition is deterministic on values, policy identity, and graph;
- final verification uses exactly 10,000 Simulator runs when the strategy's
  action horizon makes sampling applicable.

The historical August 14 three-prefix `exact_closed` result is prior art, not
acceptance authority: it predates the August 23 exact-explicit-affix terminal
contract and could terminate with unrequested affixes. The current clean
three-prefix fixture instead publishes a bounded policy near 2,722 Chaos over
a 79.4 lower. This milestone closes the current contract rather than restoring
an obsolete success predicate.

## Constraints

- Do not weaken exact-item success, hide junk, close an open envelope by
  classification, or turn an executable upper into proof authority.
- Do not hard-code Conquest Lamellar, particular mod families, prefix/suffix
  direction, or an item-specific crafting sequence.
- Same-side structure may be used only through generic facts already present
  in the goal and carrier state: side capacity, satisfied slots, occupied
  slots, preservation/destruction contracts, and admitted action outcomes.
- Search ordering may use heuristic carrier/subgoal preferences. Pruning,
  lower bounds, dominance, and exact closure require an explicit admissibility
  or completeness argument and a regression witness.
- Generated automatic Imprint programs remain off through
  `calculator_product_v1`; Imprint-specific behavior is not part of this
  milestone.
- Start with focused native witnesses. Do not run a corpus, a broad benchmark
  matrix, the full repository pipeline, or five-goal qualification while the
  three-on-one-side boundary is open.
- Preserve the stable C ABI. Rebuild release WASM after native behavior is
  accepted. Oliver retains visual/browser review.

## Gate 0 — Stabilize Frontier Incumbent Construction

Repair the reproduced PDR four-goal `vector::_M_range_check` failure. The
failure occurs when `try_install_reachable_incumbent` asks the sparse
transition cache for a newly discovered frontier state whose row segment has
not been materialized. Preserve the existing certified-boundary and
grow-in-place semantics: absence of a completed row is an ordinary frontier
condition, not an exception and not a free terminal.

Acceptance:

- a focused native regression reaches this state/cache skew without throwing;
- it either consumes a certified boundary or reports/queues the existing
  missing-row repair path truthfully;
- no fabricated row, value, or closure authority is introduced;
- the focused native solver build/tests pass.

## Gate 1 — Freeze Current Same-Side Witnesses

Use the current exact-item Conquest clean-three-prefix fixture as the prefix
witness. Derive a clean-three-suffix witness from canonical engine data using
three compatible natural suffix goal families on a legal base; record its
identity and why the families are compatible. Do not infer a new mechanic.

Run only these two native cases far enough to attribute their first exactness
blocker. Record action-envelope state, terminal/frontier counts, proof lower,
evaluated upper, delayed lanes, stop cause, resource use, and whether a goal
state is actually reached. Separate search failing to find terminals from
proof failing to close after finding a policy.

## Gate 2 — Repair The First Generic Closure Blocker

Trace the earliest open obligation that prevents exact closure for both
witnesses. Repair its owner generically. Likely owners include action-envelope
retirement, destructive-renewal carrier enumeration, exact-junk cleanup,
same-side capacity representation, or selected-policy proof lift; the runtime
evidence chooses the implementation.

Any finite-state closure or obligation-retirement change must prove that all
priced requested alternatives are either materialized or retired by a typed
complete argument. Any lower-bound improvement must remain optimistic for all
real outcomes. Any scheduling improvement must retain goal-mask and carrier
diversity rather than greedily starving another exact route.

Gate 2 is complete only when the change reaches an exactness consumer—not
merely when a telemetry number moves—and a focused counterexample fails before
the repair and passes after it.

## Gate 3 — Exact Three-Prefix And Three-Suffix Closure

Run both same-side witnesses to genuine completion. If they expose different
generic blockers, continue within Gate 2/3 until both close; do not declare a
single-direction win sufficient.

Acceptance:

- both cases satisfy every exact criterion in the Objective;
- two native repetitions retain values, hashes, graph size, termination, and
  obligation counts;
- exact graph evaluation matches the published upper within the named
  tolerances;
- compiled-strategy terminal assertions reject injected junk-success and
  inapplicable-action variants;
- 10,000-run sampling passes when applicable, otherwise the exact evaluator
  is named as authority and the horizon reason is recorded.

## Gate 4 — Probe The Next Boundary

Only after Gate 3 is stable, run one current four-mod cross-side witness and
the PDR crash witness. This is a bounded diagnostic, not a new requirement to
solve every four- or five-goal case exactly. Determine whether the same repair
advances four-mod closure and whether a next blocker is representational,
scheduling, resource, or proof work.

Do not start broad five-T1 tuning. Record a precise successor handoff if the
four-mod boundary does not close cheaply.

## Gate 5 — Final Acceptance And Handoff

- fresh native build;
- focused solver solve/eval/API tests, including the stale-frontier regression
  and both exact same-side witnesses;
- focused deterministic repeats and exact graph evaluation;
- exactly 10,000 Simulator runs for each applicable selected strategy;
- release WASM rebuild and focused nonvisual worker/engine controls;
- complete nonvisual web tests and `npx tsc --noEmit` once, at the end;
- `git diff --check` and coherent local checkpoint commits, each ending with
  `Co-authored-by: Codex <codex@openai.com>`.

Do not run the full repository acceptance pipeline unless a cross-layer change
outside the stable solver/WASM contract makes it necessary. Archive this plan,
update the stable solver/evidence documentation that changed, and leave
`HANDOFF.md` with measured results and the next exactness owner.
