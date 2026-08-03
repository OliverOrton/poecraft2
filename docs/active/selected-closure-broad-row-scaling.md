# Selected-Closure Scaling And Exact Broad-Row Projection

**Status: Gates 0, 1, and 3 complete; Gate 2 skipped by its condition; Gate 4
exact projected broad-row prototyping is the active boundary.**

Owner: Oliver

Parent: [Active work](README.md)

Starting commit: `a6f7e13cf7d5cb202874c210992689d601c0e650`.

Branch: `codex/selected-closure-broad-row-scaling`.

Source milestone:
[Competitive Lazy Alternative Certification](../archive/2026-08-02-competitive-lazy-alternative-certification/README.md).

## Objective

Determine whether a measured reforge-work increase can safely restore the hard
two-goal case, then reduce the exact cost of broad selected-row construction.
The selected-policy-first quotient, complete admitted vocabulary,
proof-carrying unresolved alternatives, exactness boundary, and generic action
semantics remain intact.

No gate may narrow the action vocabulary, discard unresolved alternatives,
special-case a fixture or named action, weaken exactness, or move crafting-rule
authority out of the engine.

## Frozen evidence

The existing selected-first 20M result is immutable and must not be rerun:

- coarse work: 14,077,632;
- post-coarse allowance: 5,922,368;
- two selected rows completed using the entire allowance;
- 345,192 exact transitions, or 172,596 per completed selected row;
- approximately 2.96M work per completed selected row;
- zero alternative rows, obligations, partitions, or executable uppers; and
- 375,483,695 bytes of peak native-owned memory.

The archived 50M/100M slope diagnostic used the old eager-alternative
architecture. Its approximately linear row cost motivates attribution, but it
cannot answer current selected-closure size or first-upper affordability.

## Gate 0 — plan and measurement contract

**Status: complete.** Tracked evidence is
[`selected-closure-broad-row-scaling-gate0.json`](../../fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate0.json).

The current branch, commit, native benchmark and engine binaries, compiled
artifact, corpus and case, Mirage economy snapshot, 27-action ordered product
envelope, canonical options, exact-verification options, relevant source
authorities, and prior 20M evidence are frozen before source changes.

The one permitted Gate 1 diagnostic changes only a temporary copy of
`caps.max_reforge_work` from 20,000,000 to 100,000,000. It retains the 1 GiB
solver-owned cap, 900-second external watchdog, all other caps and options,
selected-first certification, full action envelope, exact-evaluation request,
and 10,000-run request if a policy exists. The canonical fixture and product
default remain unchanged.

Required measurements are selected rows begun/completed, selected and
alternative work/transitions, work and monotonic wall time to first partition
and upper, native/report wall, peak memory, maximum cooperative solve-step
duration, retained-policy status, complete action accounting, and obligation
lifecycle. No source test is required for this documentation-only gate.

Order-invariant identities include all inputs, caps except the explicitly
versioned diagnostic override, the ordered action envelope, coarse work and
semantic hashes, canonical projected probability maps, action cost/legality,
and final policy semantics. Source/binary/report hashes, raw transition order,
internal quotient/scheduling identities, timing, and compiled strategy bytes
are order-sensitive after implementation changes; they remain acceptable only
when the exact semantic authorities reconcile.

## Gate 1 — current selected-only 100M diagnostic

**Status: complete.** Tracked evidence is
[`selected-closure-broad-row-scaling-gate1.json`](../../fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate1.json).

The single run returned `refused_resource_cap` after 100M total work. The
coarse phase remained 14,077,632 work; 40 selected rows consumed the complete
85,922,368 exact allowance and emitted 6,903,840 transitions. No alternative
row, partition, obligation set, or executable upper appeared. Marginally from
20M to 100M, each of 38 additional selected rows cost 2.105M work and emitted
exactly 172,596 transitions. Peak native-owned memory stayed 375,483,695 bytes;
maximum cooperative solve step was 651.543 ms.

The exact selected row/kernel population and transition count equal the old
eager-architecture 100M prefix. The expensive first 40 rows are therefore
selected closure, not post-selected alternatives. There is no credible upper
projection below 200M, so the decision contract forbids a 200M run and rejects
a higher default. Compilation, reconciliation, and simulation are not
applicable because no policy exists.

The process completed in 28.634 seconds with a valid report, empty stderr, and
no survivor. The tool-level shell timeout detached the intended PowerShell
watcher at 10 seconds; continuous external watchdog attachment is not claimed,
and the run was not repeated.

Run `natural-t1-breadth-two-4e65dda9c53b` exactly once using a temporary
diagnostic fixture whose only semantic difference is
`caps.max_reforge_work = 100000000`. Request exact strategy evaluation and
10,000 simulations if a policy is returned. Use the existing selected-first
binary, 1 GiB cap, full 27-action vocabulary, goal-progress-gated reforge mode,
progress output, partial report, and a process-tree 900-second watchdog.

Record partition/upper reachability, selected closure, selected versus
alternative work, marginal row work and transitions, first-upper cost and
provenance, post-upper work, wall and maximum step time, memory, action and
obligation accounting, compilation/reconciliation/simulation status, and
whether the current selected path remains approximately linear.

Decision:

- upper by 100M: Gate 2 may treat the measured cap as a viable interim range;
- partition plus credible upper projection below 200M: one 200M diagnostic is
  permitted;
- no partition, or flat/rising rows without a credible near-term upper: do not
  run 200M and do not raise the default; proceed directly to Gate 3; and
- broad-row research continues in every outcome.

Commit the diagnostic evidence before source changes.

## Gate 2 — conditional safe higher-cap product behavior

**Condition: only if Gate 1 produces an upper in a measured practical range.**

**Status: skipped.** Gate 1 produced neither a partition nor an upper, so no
higher-cap behavior or product-default change is permitted.

Expose a higher limit as explained solver/product configuration rather than an
unlabelled constant. Separate pre-upper work from optional post-upper
improvement work. Retain the first certified upper immediately and return it
across later caps while continuing to represent unresolved alternatives in
the lower. Never label the result exact until all potentially improving actions
are resolved.

Do not automatically spend every remaining unit after a useful upper. Preserve
cooperative stepping, cancellation, progress, worker responsiveness, and the
complete action envelope. Map every solver-option or ABI-visible change through
[`change-impact.md`](../foundation/change-impact.md). A higher product default
cannot ship without release-WASM memory, cancellation, maximum-step, and policy
return evidence. Commit this behavior separately.

## Gate 3 — attribute broad selected rows

**Status: complete.** Tracked evidence is
[`selected-closure-broad-row-scaling-gate3.json`](../../fixtures/solver-reliability/v1/evidence/selected-closure-broad-row-scaling-gate3.json).

A source-authoritative 18M attribution probe completed one selected row after
the invariant 14,077,632-work coarse phase. It did not repeat the frozen 20M
or one-shot 100M runs and did not change any canonical cap. The selected row
is the one-step Zeal Essence operator on strict/coarse root `0`, with one
forced modifier, a post-forced natural pool of 259 modifiers and weight
210,650, and 172,596 raw transitions.

The causal owner is the current-roll frontier, not literal junk identity. The
row visits 51,155 frontier states and scans all 40 buckets at each state,
charging exactly 2,097,355 work. Raw-choice tables and identity-tree work are
both zero. Of 2,046,200 bucket probes, only 185,825 are positive. The sparse
node-plus-positive-edge proxy is therefore 236,980 work, below Gate 5's 300k
threshold before prototype overhead.

Terminal enumeration commits 688,739 arrivals into 172,596 strict abstract
outcomes. The 516,143 duplicate arrivals carry 0.25435223351246694 probability
mass after their first insertion. Frontier construction takes 704.269 ms of
808.997 ms measured raw build time; final projection/interning takes another
100.054 ms. The existing isolated-family compression has no opportunity: 38
eligible physical junk families produce 38 distinct projected classes.

The immutable Gate 1 report did not retain per-row action/base/source
identities, so rows 2–40 cannot be retroactively named without the forbidden
100M rerun. Their exactly equal 172,596-transition populations remain valid;
only row 1's complete attribution is source-authoritative. New bounded
telemetry records up to 64 later rows with explicit omissions and does not
participate in solving or proof.

Identify the exact selected programs producing the 172,596-transition rows.
For every row record source carrier/observation requirements, parameterized
action family, forced/guaranteed modifiers, natural pools and weights, slot
process, group/tag exclusions, raw identity-tree nodes, terminal exact states,
projected outcomes, phase work/time, duplicate projected targets, and combined
duplicate mass.

Attribute cost among complete affix enumeration, junk identity, future group
exclusions, repeated pools, transitions later merged by projection, and shared
carrier/reforge-family work. Junk identity may be removed only when no reachable
action, legality rule, exclusion rule, compiler route, or proof obligation can
observe it. Commit causal evidence.

## Gate 4 — exact projected broad-row prototype

**Status: active.**

Build an action-parameterized exact prototype that computes probability over
future-relevant projected states without first retaining every complete item
outcome. Keys must retain every distinction used by future crafting behavior,
including goal/below-tier state, side counts and remaining slots, crafted and
fractured state, protection/metamod flags, group/exclusion signatures, required
tag observations, blocked goals, forced/guaranteed state, and compiler or
simulator routing.

Collapse junk only for identical current observation, remaining exclusions,
future legality, and future transition distribution. Accumulate integer weights
or equivalent exact mass before floating conversion. Support ordinary reforge,
forced Essence, guaranteed-target Harvest including exceptional support, and
Fossil multipliers/zeroed pools/additions without assuming their supports are
interchangeable. Reuse subproblems only under exact input identity.

The existing raw enumerator remains an independent oracle and must not mask
projected-path errors.

## Gate 5 — exact equivalence and performance

Across focused fixtures and sampled real bases require identical total mass,
proof-relevant projected maps, action cost/legality, goal/below-tier behavior,
forced/guaranteed behavior, group exclusions, reversed-order determinism,
Bellman values, and selected policy semantics. Raw order hashes may change only
when canonical projected maps and final semantics still match.

The structural gate qualifies only if the frozen selected row drops from about
2.96M to at most 300k work or the complete hard case reaches a first proper
upper inside the original 20M cap. If projected-work semantics differ, version
telemetry and preserve parallel counters. Commit a qualified prototype or a
measured rejection.

## Gate 6 — conditional production integration

**Condition: only if Gate 5 qualifies.**

Integrate projected construction into selected and competitively scheduled
certification while preserving the proof store, partition authority, reverse
invalidation, properness, compiler routing, complete action envelope, and
bounded/exact status. Cache only under full collision-checked identities;
price-only changes may reuse probability work, while requirement, pool,
vocabulary, artifact, and mechanic generations invalidate it. Partial work is
resumable and never certified. Commit production integration.

## Gate 7 — conditional hard and native portfolio qualification

Run the hard case under the original 20M cap and any Gate 2 qualified higher
cap. Require proper compilation, exact reconciliation, and 10,000 simulations
for every returned policy. Then run the qualified Fracture full-four, natural
representative four-goal, five-goal scale, 27-case smoke, 49-case reliability,
and relevant ring/armour representatives. Attribute gains to projection,
higher cap, or both.

## Gate 8 — conditional release acceptance

Only after native qualification, rebuild release WASM, verify 61 exports and
ABI 2, run the applicable WASM reliability workflows, verify cancellation and
maximum worker-step duration, run `npm test`, `npx tsc --noEmit`, and finally
`scripts/test.ps1` once. Oliver owns visual review; do not perform it unless
requested.

## Final report and handoff

Archive the selected-first 100M result, partition/upper milestones, cap
recommendation, broad-row attribution, projected/raw equivalence, work
reduction, native/WASM timing and memory, bounded/exact status, portfolio and
10,000-run evidence, and any rejection or remaining structural boundary.
Update `HANDOFF.md`, leave a clean tree, use local commits with the required
co-author trailer, and do not push or merge.
