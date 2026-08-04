# Certified Fallback Publication And Final-Depth Reforge Accumulation

**Status: Track A Gate 1 is complete; Gate 2 hard-case qualification is the
active boundary.**

Owner: Oliver

Parent: [Active work](README.md)

Starting commit: `0b4ba38aa5754a39715640ad6d7a48cab2dc2b6c`.

Branch: `codex/fallback-and-terminal-reforge-factorization`.

Source milestone:
[Selected-Closure Scaling And Exact Broad-Row Projection](../archive/2026-08-03-selected-closure-broad-row-scaling/README.md).

## Objective

Complete two independently scored tracks:

1. retain and publish the best still-current independently certified
   executable fallback when a cheaper preferred candidate cannot finish exact
   refinement; and
2. explain and, if possible, factor the 638,365 exact final-depth branches in
   the binding selected reforge row.

Track A must remain useful if Track B is rejected. Track B continues after
Track A unless a correctness blocker is found. Neither track may raise the 20M
product cap, narrow the action vocabulary, change mechanics, weaken exactness,
special-case a named action or fixture, or move crafting-rule authority out of
the native engine.

## Gate 0 — frozen boundaries

**Status: complete.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate0.json`](../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate0.json).

The branch begins from clean commit `0b4ba38`. The source tree, release native
binaries, compiled artifact, Mirage economy snapshot, canonical hard fixture,
27-action order, canonical options, hard 20M evidence, selected-only 100M
evidence, and exact V2 prototype evidence are frozen without rerunning a
benchmark.

The terminal-row boundary is:

- 2,097,355 raw V1 work and 923,141 V2 work;
- 51,155 frontier states and 688,739 terminal arrivals;
- 172,596 unique projected outcomes;
- 638,365 eligible final-depth branches absent from the earlier sparse proxy;
  and
- approximately 0.809 seconds raw versus 1.004 seconds V2 row wall.

The independently certified renewal boundary in the existing hard report is:

- primitive action: Chaos;
- exact success probability: `0.00025094514103578676`;
- certified and bootstrap-evaluated upper: `3984.9346987650665`;
- 24 validated non-goal carriers; and
- witness hash `5e2bd0c222942bda`.

The cheaper preferred coarse/Q-directed incumbent is
`3323.6694369790375`, but its selected Essence policy cannot finish strict
refinement within the unchanged 20M cap. Gate 0 freezes those as distinct
facts: the cheaper candidate is preferred but not publishable; the renewal is
more expensive but independently executable.

No source, fixture, generated artifact, product default, public contract, or
test changed at this gate.

## Track A — certified fallback incumbent portfolio

### Gate 1 — preserve independently publishable witnesses

**Status: complete.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate1.json`](../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate1.json).

Replace single-cheapest-incumbent ownership with a small deterministic
portfolio. A retained candidate owns its certified upper, independently
evaluated policy cost, complete policy or renewal witness, goal/economy/graph/
vocabulary/artifact identities, source and target generations, compilation
provenance, properness and executability result, invalidation dependencies,
and solver-owned memory.

Keep four concepts explicit and independent:

- cheapest optimistic/coarse candidate;
- selected candidate awaiting strict refinement;
- independently certified executable fallback; and
- retained compiled executable artifact.

A cheaper unpublishable candidate cannot evict a certified fallback. A
successful cheaper lift may replace it. A lift cap publishes the best current
fallback. Staleness or impropriety invalidates only the affected witness.
Compilation or reconciliation failure forbids publishing that candidate but
does not destroy unrelated certified witnesses. Fallback uppers never enter the
lower bound and never resolve alternatives.

Reuse the generic primitive-renewal, bounded-incumbent, compiler, exact-policy
evaluation, properness, and invalidation authorities. Do not branch on Chaos,
Restart, Essence, or a fixture identity.

Focused tests must cover a cheaper uncertified candidate, successful cheaper
replacement, lift-cap retention, isolated compile failure, price/goal/graph/
vocabulary/generation invalidation, multiple deterministic and equal-cost
fallbacks, improper cycles, memory accounting, and refusal of a scalar upper
without executable provenance.

The retained implementation keeps a deterministic four-entry portfolio. Each
generic primitive-renewal entry owns its complete captured policy and witness,
an independently recomputed cost, compiled strategy artifact and compiler
counts, properness/executability result, goal/economy/vocabulary/artifact/
graph identities, source and target generations, graph-prefix dependencies,
and capacity-derived memory. Ordering is a strict total semantic order; graph
and vocabulary append-only growth may preserve a witness only while its frozen
prefix still matches.

Before an incremental or direct preferred policy mutates the selected
incumbent, an independently executable renewal is copied into the portfolio.
Preferred strict-lift and ordinary exact-publication failures both try the best
still-current fallback without clearing the preferred failure diagnostic. A
successful cheaper preferred assertion discards the unused portfolio. The
fallback restores only an executable upper; the full-envelope focused lower
and unresolved alternative semantics are unchanged.

The synthetic two-goal regression proves all three publication dispositions:
a 1,000-work preferred assertion cap publishes the more expensive compiled
renewal, a 2,000-work assertion publishes the cheaper preferred exact policy,
and a preferred JSON compilation cap leaves the unrelated compact fallback
publishable. Pure contract tests cover every declared identity/generation
invalidation, multiple and equal-cost ordering, improper and provenance-free
rejection, and exact byte-cap arithmetic. The focused solve suite passes
98,217 checks with zero failures. Gate 1 does not qualify the canonical hard
case; that one permitted invocation belongs to Gate 2.

Commit Gate 1 separately.

### Gate 2 — hard-case fallback qualification

**Status: active.**

Run the canonical hard two-goal case exactly once with the unchanged 20M work
cap, 1 GiB solver-owned cap, complete 27-action vocabulary, and 900-second
watchdog. Require the full-envelope lower, the certified renewal fallback to
survive the preferred lift cap, bounded rather than exact status, a proper
compiled strategy, exact cost reconciliation, and 10,000 simulations with zero
off-policy failures.

If the existing renewal cannot be published, diagnose its exact provenance or
compiler failure. Do not synthesize a replacement or relax the contract.
Commit Track A qualification separately.

## Track B — exact final-depth factorization

### Gate 3 — explain the 638,365 terminal branches

Add bounded deterministic telemetry classifying final-depth contributions by
target count, side, predecessor, terminal bucket, canonical successor,
duplicate status, goal/below/junk/exclusion observation, predecessor
multiplicity, order multiplicity, and probability mass.

Measure how much duplication comes from final-modifier choice, roll order,
predecessor convergence, four/five/six-affix target channels, and distinctions
that cease to be observable after termination. Report an evidence-backed lower
bound separating unavoidable successor representation from algebraically
accumulable contributions and temporary roll-order distinctions. Branch work
remains real work even when the implementation avoids materializing each
branch. Commit attribution separately.

### Gate 4 — exact V3 terminal accumulator prototype

Choose the representation from Gate 3 evidence. Candidate forms include an
unordered-set DP, factored last-pick transform, subset recurrence, cached
terminal maps, per-side bucket sums, or exact aggregation shared by equivalent
predecessors.

Every key must retain remaining availability and generation-group exclusions,
side capacity, goal and below-tier status, forced/guaranteed channels, pool and
Fossil weight effects, crafted/fractured/protection state, observation and
exclusion signatures, action program, and artifact generation. Accumulate
exact integer/rational mass before floating-point conversion.

Retain parallel V1 raw, V2 sparse, and V3 factored ledgers. The active evaluator
is charged honestly to `max_reforge_work`; V1 stays the independent oracle and
V2/V3 remain diagnostic pending qualification.

### Gate 5 — exact equivalence

Compare V3 with raw across ordinary reforges, forced Essence, targeted and
exceptional-support Harvest, all relevant Fossil weight/addition/forced
channels, empty and fractured real items, prefix/suffix/mixed goals,
present-below-tier goals, and forward/reversed enumeration.

Require identical legality, cost, total and per-canonical-target mass,
goal/below mass, exclusion behavior, Bellman value, semantic action, and
collision-checked deterministic cache identity. Adversarial junk modifiers
that exclude different future groups must remain distinct.

### Gate 6 — binding performance decision

Run the same hard-row diagnostic used for V1/V2. V3 qualifies only if:

- comparable work is at most 300,000 and row wall is no worse than 0.809
  seconds; or
- selected refinement reaches a partition and its own proper executable upper
  within the original 20M cap, without credit from Track A fallback.

Report fallback and selected-refinement publication separately. If V3 misses,
retain Track A and all attribution/rejection evidence, and leave V3 diagnostic
or restore it if unsound. Commit the decision.

### Gate 7 — conditional production integration

**Condition: only if Gate 6 qualifies V3.**

Integrate V3 into selected and competitively scheduled alternative rows while
preserving proof-store, partition, invalidation, Bellman, properness, compiler,
and obligation authorities. Interrupted aggregation is resumable and never
partially certified. Price-only changes may reuse completed probability work;
pool, artifact, action, goal, observation, and vocabulary changes invalidate
it. Raw remains a test oracle. Commit production integration separately.

## Gate 8 — native and release acceptance

Run this gate for the retained Track A product behavior whether or not V3
qualifies. Cover the hard two-goal, Fracture full-four, natural representative
four-goal, five-goal scale, 27-case smoke, 49-case reliability, and relevant
ring/armour cases. Use 10,000 simulations for every compiled strategy.

If V3 qualifies, include paired V1/V3 work, wall, memory, policy cost, and hash
comparisons where practical. Then rebuild release WASM, verify 61 exports and
ABI 2, run WASM reliability and cancellation/responsiveness checks, `npm test`,
`npx tsc --noEmit`, and `scripts/test.ps1` once. Oliver owns visual review; do
not perform it unless requested.

## Final report and handoff

Archive separate conclusions for fallback publication, selected-policy
refinement, and terminal factorization. Report policy availability and costs,
lower/upper provenance, terminal causal breakdown, V1/V2/V3 work and wall,
equivalence, native/WASM results, 10,000-run verification, and the next
structural boundary. Leave the branch clean and local; do not push or merge.
