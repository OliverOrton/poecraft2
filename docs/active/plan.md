# Q-Directed Deep Solving And Automatic Eldritch Side Actions

**Status: active (2026-07-28).**

Owner: Oliver

Branch: `codex/q-directed-eldritch-side-actions`

Starting source: `85e9ac4`

## Objective

Build on the retained Chaos-anchored incremental action scheduler so completed
delayed actions drive exact refinement of the successor states that dominate
their unresolved Q intervals. Then add exactly four automatic goal-relevant
Eldritch side-intent options on engine-certified eligible armour:

- Eldritch Annul Prefix;
- Eldritch Annul Suffix;
- Eldritch Chaos Prefix; and
- Eldritch Chaos Suffix.

The value machinery must retain every completed exact distribution, recompute
only its Q values after continuation values change, and preserve certified
lower/executable-upper bounds for all omitted fringe mass. The automatic
Eldritch options must use existing real Ember/Ichor setup, dominance,
transition, resource, and compilation authority. No hidden dominance flag,
dense shared DAG, goal-count state merge, Veiled automation, Influence Exalt
automation, or product-default cap change belongs to this boundary.

## Exactness And Status Contract

For each completed unresolved row, successor uncertainty is attributed by its
exact probability contribution to the row's Q interval. Ordinary transitions,
self-loops, retry carriers, choice groups, carrier-relative self states, action
cost, and exceptional support must be represented soundly.

A refinement batch may prioritize states but never deletes a state or
probability mass. Every unexpanded state retains an admissible lower value and
an executable upper value. After each batch the solver propagates both value
sides, refreshes completed-row Q intervals without rebuilding their
distributions, admits only proved improvements, rejects only proved
non-improvements, and leaves overlap unresolved.

An open action envelope, unvalued delta state, overlapping Q interval, or
resource boundary prevents exact closure while permitting an independently
executable bounded policy.

## Gate 0 — High-Cap Control

Before changing solver behavior, run the two frozen four-mod cases from
`85e9ac4` using diagnostic-only caps:

- 100,000,000 reforge work;
- 10,000,000 retained transitions;
- 512 MiB solver-owned memory;
- 200,000 discovered states;
- 25,000 expanded states;
- the existing row limit;
- cooperative one-item stepping and partial snapshots; and
- a bounded native watchdog.

Record action lifecycle counts, remaining envelope, per-family/action exact
work, kernel evaluations/reuse, support deltas, Q intervals, Bellman
reoptimizations, graph size, wall, memory, and all cap hits. A diagnostic cap
increase is run-local and evidence-driven; it is not a product solution.

## Gate 1 — Exact Q-Directed Refinement

Implement probability-weighted Q-uncertainty attribution over retained exact
rows. Select bounded batches of successor states by their ability to decide an
unresolved comparison, expand them through the existing per-state envelope,
propagate valid lower and executable upper values, refresh every completed
row's Q interval, and repeat.

Required properties:

1. completed probability rows survive every Bellman/refinement cycle;
2. no completed distribution is recomputed merely because values changed;
3. combined uncertainty across unresolved rows determines state priority;
4. choice/self/retry semantics contribute exactly;
5. support-delta states are expanded before classification;
6. admitted alternatives can change later action decisions; and
7. deterministic family interleaving prevents capped family starvation
   without order-dependent pruning.

If directed expansion alone leaves the frozen intervals effectively
unchanged, strengthen only sound value machinery: executable goal-preserving
continuations, admissible blocker/side/capacity/preservation-aware lowers, and
focused propagation over high-impact states. A restricted-action value is
never presented as a global lower bound.

## Gate 2 — Q-Directed Exact Oracles

Add native controls proving:

1. maximum certified uncertainty contribution wins refinement priority;
2. successor refinement tightens the parent Q interval;
3. refinement admits a proved better row;
4. refinement proves a worse row non-improving;
5. admission triggers Bellman reoptimization;
6. multiple admitted rows compose into an improved policy;
7. omitted fringe mass remains bounded;
8. exceptional support expands before classification;
9. completed probability work is not repeated after value changes; and
10. repeated transition/policy hashes are deterministic.

## Gate 3 — Four Automatic Eldritch Side Options

Generate the four named high-level options lazily only for rare carriers in
engine-certified Eldritch-eligible helmet, body-armour, glove, and boot
sessions, and only when the targeted/preserved explicit side can matter to
the remaining goal.

Each option inspects actual dominance. When correct dominance already exists,
it executes the requested real Eldritch currency directly. Otherwise it uses
the existing side-intent machinery to choose and charge the cheapest legal
real Ember/Ichor setup, then executes the currency. Resulting implicit tiers
and complete item state remain authoritative.

Compilation emits the real setup operation when needed and the real final
Eldritch Chaos or Annul operation. No automatic standalone implicit currency,
Eldritch Exalt, or arbitrary implicit rolling is exposed. Eldritch Chaos is
never a retry-basin action because the preserved explicit side observes
discarded affixes.

## Gate 4 — Eldritch Exact Oracles

Prove:

1. ineligible bases receive no automatic side option;
2. only the four certified armour classes receive them;
3. prefix/suffix intent creates correct dominance;
4. existing dominance avoids setup;
5. missing dominance emits and charges exact setup;
6. Eldritch Chaos preserves the opposite explicit side;
7. Eldritch Annul targets the requested side;
8. compiled strategies contain real setup/final operations;
9. Eldritch delta states expand before classification; and
10. different partial carriers may select different side options.

Preserve the implemented target-side metamod behavior and raw Annul rarity
legality. Manual Calculator/Emulator Eldritch actions remain unchanged.

## Gate 5 — Frozen And Artifact Qualification

Rerun both frozen cases and require materially tighter completed Lucent/Jagged
and Harvest Q intervals, useful refinement beyond the initial 32-state
handoff, completed-row reuse across value changes, honest envelope/bound
status, exact state/kernel reuse, deterministic repetition, and complete
work/wall/memory/cap reporting.

Do not force a classification. Continue through successive evidence-backed
value refinements if the first policy is weak.

Add at least one artifact-backed Eldritch-eligible armour case with explicit
goals split across prefix and suffix sides. It must permit Bellman to discover
useful side preservation without a prescribed sequence.

If a qualified strategy is new, compile it, run exact strategy evaluation
where applicable, and run 10,000 simulator executions.

## Gate 6 — Integration And Closure

Preserve unrestricted behavior, small exact values/policies, existing
filtering outside the four automatic options, economy accounting, compiled
strategy semantics, manual Eldritch actions, and product defaults.

Run the complete affected native acceptance once after implementation,
rebuild release WASM, and run non-visual web tests plus TypeScript no-emit.
Oliver owns rendered review.

Update stable solver documentation, the Eldritch mechanic reference if its
automatic-solver scope changes, tracked evidence, archive indexes, final
report, and HANDOFF. Commit locally with:

`Co-authored-by: Codex <codex@openai.com>`

Do not push.

## Stop Conditions

Stop only for:

- a genuine Path of Exile mechanic ambiguity not already ruled by Oliver;
- a demonstrated architectural impasse after multiple evidence-backed
  refinement attempts; or
- a consequential action outside this boundary.

If one refinement is falsified, restore only that candidate, retain its
measurement, and continue to the next justified approach.
