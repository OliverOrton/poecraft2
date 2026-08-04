# Certified Fallback Publication And Final-Depth Reforge Accumulation

**Status: archived. Track A Gate 2 is qualified; Track B Gate 6 structurally
rejects V3 and skips Gate 7. Gate 8 completed with disclosed rare-renewal and
five-goal qualification boundaries.**

Owner: Oliver

Parent: [Archive report](README.md)

Starting commit: `0b4ba38aa5754a39715640ad6d7a48cab2dc2b6c`.

Branch: `codex/fallback-and-terminal-reforge-factorization`.

Source milestone:
[Selected-Closure Scaling And Exact Broad-Row Projection](../2026-08-03-selected-closure-broad-row-scaling/README.md).

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
[`fallback-and-terminal-reforge-factorization-gate0.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate0.json).

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
[`fallback-and-terminal-reforge-factorization-gate1.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate1.json).

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

**Status: complete.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate2.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate2.json).

Run the canonical hard two-goal case exactly once with the unchanged 20M work
cap, 1 GiB solver-owned cap, complete 27-action vocabulary, and 900-second
watchdog. Require the full-envelope lower, the certified renewal fallback to
survive the preferred lift cap, bounded rather than exact status, a proper
compiled strategy, exact cost reconciliation, and 10,000 simulations with zero
off-policy failures.

If the existing renewal cannot be published, diagnose its exact provenance or
compiler failure. Do not synthesize a replacement or relax the contract.
Commit Track A qualification separately.

The one authorized canonical invocation completed at the unchanged 20M work
and 1 GiB owned-memory caps with all 27 candidate actions. Preferred exact
publication stopped on the remaining 5,922,368 reforge-work allowance, then
the retained primitive-renewal fallback published once with no invalidation,
compilation failure, or memory rejection. The result is bounded feasible:
the full-envelope lower is `752.90090756637869`, while the compiled fallback
upper and retained evaluated cost are `3984.9346987650665`.

The four-node/four-edge, 1,208-byte strategy reconciles to
`3984.9346987639233`; the absolute delta from the solver certificate is
`1.1432348401285708e-09`, success mass is
`0.99999999999923328`, and off-policy mass is zero. All 10,000 simulations
succeeded with zero off-policy failures. Total wall was 24.669 seconds, peak
native-owned memory was 375,494,320 bytes, all cap checks passed, stderr was
empty, and the benchmark's final report records all expectations met. The
fallback changed neither the focused lower nor unresolved-alternative
authority. The canonical hard case will not be rerun in later gates.

## Track B — exact final-depth factorization

### Gate 3 — explain the 638,365 terminal branches

**Status: complete.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate3.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate3.json).

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

Bounded deterministic telemetry now attributes the exact evaluator's final
depth without changing its decisions or the V1/V2 work ledgers. The binding
Zeal Essence row has 39,900 terminal predecessors and 638,365 real terminal
branches, but only 125,581 completed canonical successors. The remaining
512,784 contributions (80.33%) are algebraically accumulable; they carry
74.60% of terminal probability mass.

All duplicates cross predecessor identities. None occur within one
predecessor. Of the duplicates, 502,320 cross terminal bucket and exclusion
signatures, 512,330 cross predecessor availability, and 261,625 cross side.
Therefore availability and generation-group exclusions remain observable
until the final modifier has been applied, but predecessor, last-pick, and
roll-order identity cease to matter once the completed canonical successor is
formed. The existing unordered frontier already collapses the 24 possible
orders per predecessor; the diagnostic records 14,682,395 temporary terminal
order distinctions separately from the 638,365 charged V2 branches.

Gate 4 consequently prototypes a completed-successor unordered set/subset
recurrence with a factored last-pick transform shared across converging
predecessors. Exact integer/rational mass is accumulated before conversion to
floating point. The native build and the focused solver-calc suite pass
250,746 checks with zero failures, including forward/reversed aggregate
conservation. The canonical hard case was not rerun and the retained Track A
fallback receives no Track B credit.

### Gate 4 — exact V3 terminal accumulator prototype

**Status: complete.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate4.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate4.json).

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

The diagnostic V3 path now indexes the complete live final-depth predecessor
sets, generates each completed unordered roll set from one canonical live
removable last pick, and reconstructs all incoming mass with a destination-
driven subset recurrence. Already-terminal goal subsets do not claim canonical
ownership. Zero-progress non-goal picks are summed as one exact retry numerator
per predecessor. Complete availability and observation identity is checked by
reconstructing every admitted predecessor-plus-last-pick state.

Bucket numerators and denominators remain checked integers until the final
ratio. The active V3 ledger separately charges predecessor indexing,
denominator traversal, canonical-subset checks, candidates, every recurrence
term, completed publications, retry aggregates, and any raw-identity
expansion. All terminal maps and vectors participate in owned-memory
preflight. A one-less-than-completed-work focused cap refuses the row.

The flag occupies a native-benchmark-only diagnostic bit; the C ABI, WASM,
product defaults, V1 oracle, and V2 diagnostic remain unchanged. Focused
ordinary, Essence, Harvest, and Fossil coverage, including forward/reversed
enumeration, passes 251,729 checks with zero failures. Broad mechanic and
Bellman equivalence belongs to Gate 5; the canonical hard case was not rerun.

### Gate 5 — exact equivalence

**Status: complete.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate5.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate5.json).

Compare V3 with raw across ordinary reforges, forced Essence, targeted and
exceptional-support Harvest, all relevant Fossil weight/addition/forced
channels, empty and fractured real items, prefix/suffix/mixed goals,
present-below-tier goals, and forward/reversed enumeration.

Require identical legality, cost, total and per-canonical-target mass,
goal/below mass, exclusion behavior, Bellman value, semantic action, and
collision-checked deterministic cache identity. Adversarial junk modifiers
that exclude different future groups must remain distinct.

The raw oracle and V3 now agree across the focused synthetic ordinary,
Essence, targeted Harvest, and Fossil matrix. Fossil coverage includes positive
weighting, a zeroed pool, a direct addition, and a forced modifier. Prefix-
only, suffix-only, mixed, and present-below-tier goal projections match in both
directions. Goal-level-equivalent junk modifiers with distinct future group
exclusions remain different classes and produce zero V3 identity mismatches.

Real-artifact Vaal Regalia comparisons cover empty and fractured carriers for
Chaos, one forced Essence, one viable targeted Harvest reforge, and one
single-Fossil action. Legality, total mass, every canonical `AbstractState`
mass, goal and below-tier mass, and group-exclusion behavior agree. Abstract
state comparison deliberately preserves the virtual gated retry-basin bit;
concrete materialization is not used as a lossy oracle for that state.

Raw and V3 gated Bellman solves both publish a policy with the same evaluated
cost, operator kind, and semantic action id. Forward and reversed V3 kernels
have identical deterministic bits. The native build and focused solver-calc
suite pass 252,035 checks with zero failures. Gate 5 changes tests only, does
not rerun the canonical hard case, and authorizes the one Gate 6 binding
diagnostic.

### Gate 6 — binding performance decision

**Status: complete: V3 structurally rejected.** Tracked evidence is
[`fallback-and-terminal-reforge-factorization-gate6-structural-rejection.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate6-structural-rejection.json).

Run the same hard-row diagnostic used for V1/V2. V3 qualifies only if:

- comparable work is at most 300,000 and row wall is no worse than 0.809
  seconds; or
- selected refinement reaches a partition and its own proper executable upper
  within the original 20M cap, without credit from Track A fallback.

Report fallback and selected-refinement publication separately. If V3 misses,
retain Track A and all attribution/rejection evidence, and leave V3 diagnostic
or restore it if unsound. Commit the decision.

The one accepted temporary 18M carrier added only the V3 native diagnostic
flag to Gate 3's semantics; the canonical hard case was not rerun. The binding
Zeal Essence row remains exact and completes in 0.395 seconds, below the 0.809
second wall threshold, but honest V3 work is 2,514,591: 8.38 times the 300,000
limit, 172.40% above V2's 923,141, and 19.89% above raw V1's 2,097,355.

V3 publishes 136,045 completed unordered sets rather than 638,365 terminal
branches, but it must still perform 638,365 exact denominator-edge visits,
641,095 live-canonical-subset checks, and 638,365 incoming last-pick recurrence
terms. Together with predecessor indexing, candidate generation, completed
publication, and the 284,776 nonterminal base, those operations exactly sum to
the reported V3 ledger. No identity mismatch occurred.

Selected refinement completes one row and consumes the temporary post-coarse
3,922,368 allowance without reaching a partition or its own executable upper.
The retained `3984.9346987650665` fallback is reported separately and gives no
Track B credit. V3 therefore misses both Gate 6 criteria. It remains a sound
diagnostic, is not integrated into production, and Gate 7 is skipped.

### Gate 7 — conditional production integration

**Condition: only if Gate 6 qualifies V3.**

**Status: skipped because Gate 6 rejected V3.**

Integrate V3 into selected and competitively scheduled alternative rows while
preserving proof-store, partition, invalidation, Bellman, properness, compiler,
and obligation authorities. Interrupted aggregation is resumable and never
partially certified. Price-only changes may reuse completed probability work;
pool, artifact, action, goal, observation, and vocabulary changes invalidate
it. Raw remains a test oracle. Commit production integration separately.

## Gate 8 — native and release acceptance

**Status: complete with disclosed qualification failures.** Tracked evidence
is
[`fallback-and-terminal-reforge-factorization-gate8-release-acceptance.json`](../../../fixtures/solver-reliability/v1/evidence/fallback-and-terminal-reforge-factorization-gate8-release-acceptance.json).

Run this gate for the retained Track A product behavior whether or not V3
qualifies. Cover the hard two-goal, Fracture full-four, natural representative
four-goal, five-goal scale, 27-case smoke, 49-case reliability, and relevant
ring/armour cases. Use 10,000 simulations for every compiled strategy.

If V3 qualifies, include paired V1/V3 work, wall, memory, policy cost, and hash
comparisons where practical. Then rebuild release WASM, verify 61 exports and
ABI 2, run WASM reliability and cancellation/responsiveness checks, `npm test`,
`npx tsc --noEmit`, and `scripts/test.ps1` once. Oliver owns visual review; do
not perform it unless requested.

The final logical native portfolio passes 47 of 49 cases. It includes 40
compiled strategies and 400,000 requested simulations with zero off-policy
failures, while reusing rather than rerunning Gate 2's one authorized hard
case. The canonical bounded fallback remains qualified. Two extremely rare
renewals miss the frozen `1e-9` relative exact-cost tolerance and mostly exceed
the 100,000-action simulation horizon; the full-four Fracture probe has the
same boundary and is not qualified. The five-goal process was terminated at
the required roughly 900-second watchdog without a finalized strategy.

The release WASM exposes all 61 expected callable exports at ABI 2. Its
ring/body-armour checks each complete 10,000 successful simulations with zero
off-policy failures. The 27-check worker smoke, `npm test`, and TypeScript all
pass. The full repository pipeline was invoked once and passes 3,001,413
engine checks with zero failures plus artifact, binding, solver-benchmark, and
web/WASM acceptance. No visual review was performed.

## Final report and handoff

Archive separate conclusions for fallback publication, selected-policy
refinement, and terminal factorization. Report policy availability and costs,
lower/upper provenance, terminal causal breakdown, V1/V2/V3 work and wall,
equivalence, native/WASM results, 10,000-run verification, and the next
structural boundary. Leave the branch clean and local; do not push or merge.
