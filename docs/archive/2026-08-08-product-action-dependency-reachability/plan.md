# Product Action Dependency Reachability

**Status: completed.**

Owner: Oliver

Parent: [milestone entry](README.md)

Starting commit: `c95359663088d515982ba33e83fe2d15f89438ee`.

Branch: `codex/verified-best-policy-publication`.

## Objective

Preserve the product's existing family-specific relevance rules while
repairing false negatives caused by removing automatic-option primitives
before option construction. Every product action must have one explicit role:

1. **Relevant candidate** — may independently compete in Bellman solving.
2. **Automatic-option dependency** — may be used only by a bounded,
   engine-owned automatic option.
3. **Filtered action** — excluded under a stable deterministic reason.

Automatic builders declare their primitive dependencies through one reusable
engine-owned contract. A dependency does not become independently selectable,
and only relevant candidates plus dependencies needed by materialized options
may affect the state layout.

## Preserved boundaries

- Keep the current Harvest target-tag rule, Essence guaranteed-goal-mod rule,
  and bounded positive-relevance Fossil beam and emitted limits.
- Do not add Fossil-effect analysis or enumerate the complete Fossil universe.
- Preserve action prices, missing-price exclusions, and
  `goal_progress_gated_reforges`.
- Keep Veiled automatic crafting deferred.
- Change no crafting mechanic, Bellman comparison, state abstraction,
  resource cap, simulator limit, or strategy/evaluator vocabulary.
- Do not include Ring/Amulet evaluator-attribution or strict-partition repair.
- Never expose dependency-only Eldritch, metamod, blocker, or cleanup
  primitives as ordinary Bellman actions.
- Run routine acceptance once after the whole implementation, rebuild release
  WASM first, and use 10,000 simulator runs for selected compiled policies.
- Run no rendered/visual review; Oliver owns it.
- Make exactly two local commits and do not push.

## Gate 0 — freeze the boundary

- Confirm the clean starting commit and record the current branch.
- Preserve the completed 49-case native/release-WASM publication evidence
  without rerunning it.
- Run focused public-C-ABI and unrestricted automatic-Eldritch controls.
- Freeze a deterministic before matrix covering Eldritch eligibility,
  permanent and temporary bench crafts, side locks, Cannot Roll Attack/Caster,
  Multimod, cleanup, Essence, Harvest, Fossil, Fracture, and Imprint.
- Record retained registry actions, selectable candidates, dependency-only
  actions, automatic materialization, price exclusions, layout effects, and
  deterministic rejection or failure reasons to the degree exposed before the
  milestone.
- Activate this plan in `HANDOFF.md`, `docs/active/README.md`, and the main
  documentation execution state, then make the boundary commit before source
  changes.

## Phase 1 — explicit registry roles and dependency declarations

1. Replace the current boolean/pruned-count ambiguity with explicit candidate,
   dependency-only, and filtered roles plus stable reason codes.
2. Give every automatic family a reusable engine-owned dependency declaration
   evaluated before goal-relevant filtering.
3. Preserve explicit authored fixed-option dependency retention separately
   from automatic discovery.
4. Ensure the product's priced second-stage action list contains candidates
   only; dependency-only entries remain native registry inputs and cannot be
   named as independent Calculator actions.
5. Build the abstract layout from admitted candidates plus the dependency
   union for automatic options the product can materialize, never the complete
   primitive registry.
6. Add deterministic counts and identities for candidates, dependency-only
   entries, materialized dependencies, filtered entries, family counts,
   missing-price exclusions, and reason codes through native/WASM telemetry.

## Phase 2 — automatic Eldritch reachability

- Retain Ember tiers, Ichor tiers, Eldritch Chaos, and Eldritch Annul as
  dependency-only primitives only for eligible rare helmets, body armours,
  gloves, and boots.
- Preserve all influence, implicit, dominance, setup, action-pricing, and
  partial-goal usefulness checks already owned by the native builder.
- Cover public goal JSON and the exact Calculator registry path for prefix and
  suffix salvage, ineligible class, influenced/illegal base, missing setup or
  final price, and a strictly cheaper forced winner.
- Compile and independently evaluate the winner, then complete 10,000 sampled
  runs with eventual success one and zero off-policy failures.

## Phase 3 — bounded bench, metamod, and cleanup audit

Audit each real product path and classify it as irrelevant, generated but
cost-dominated, legally/resource rejected, or incorrectly unavailable:

- permanent goal bench crafts;
- ordinary temporary blockers;
- Prefixes Cannot Be Changed;
- Suffixes Cannot Be Changed;
- Cannot Roll Attack Modifiers;
- Cannot Roll Caster Modifiers;
- Multimod finishing; and
- Remove Crafted Modifiers.

Repair only false negatives. Cannot Roll Attack/Caster may become bounded
automatic options only when the native metamod changes a supported follow-up's
legal distribution in a way that can improve the requested goal and every
setup, continuation, and cleanup step is legal and priced. Cleanup may be a
dependency only when a relevant route must remove an obsolete crafted mod
before installing a goal craft, blocker, or metamod. Add public-path forced
winners where practical and stop for Oliver if any route requires a new
mechanic ruling.

## Phase 4 — filtering preservation and native Essence authority

- Prove relevant Harvest targets survive and unrelated target tags remain
  filtered; add a priced public-path Harvest winner.
- Prove ordinary goal-guaranteeing Essences survive and unrelated Essences
  remain filtered.
- Load `is_corruption_only` from the existing compiled artifact and reject
  corruption-only Essences natively from ordinary solving with an explicit
  reason. Do not implement corruption Essence mechanics.
- Preserve the bounded Fossil beam, scoring, and emitted limits; prove emitted
  loadouts remain positively relevant and add a priced public-path Fossil
  winner.
- Freeze before/after family counts, dependency counts, layout width, abstract
  states, transitions, work, memory, and deterministic control hashes. Reject
  material unrelated state-space broadening.

## Phase 5 — compact product scope disclosure

Near policy quality, show that the solve used the product goal-relevant action
scope and zero-progress reroll restriction, together with admitted family
counts, missing-price exclusions, deferred Veiled scope, and unresolved
actions when a resource cap prevents closure. Keep identities in existing
details rather than dumping the complete action list into the primary result.

## Product-path matrix and acceptance

Create an engine-owned matrix following:

```text
goal JSON
  -> relevance filtering
  -> dependency retention
  -> native candidates
  -> price completeness
  -> automatic options
  -> solve
  -> compile
  -> exact evaluation
```

For every supported automatic family in scope, include an eligible case, a
correct rejection, and a priced winner where practical. Assert that
dependency-only actions never become candidates; unrelated bases retain their
layout dimensions; Harvest, Essence, and Fossil discovery does not broaden;
corruption-only Essences cannot enter ordinary Solve; missing prices remain
explicit; Fracture, Imprint, and ordinary bench behavior remains intact; and
unchanged controls preserve deterministic transition and policy hashes.

After implementation, run once:

1. Native release build and the focused engine-owned matrix.
2. Complete native CTest, binding tests, artifact validation, and corpus
   acceptance selected by `scripts/test.ps1`.
3. Compilation and exact whole-graph evaluation for selected winners.
4. 10,000-run simulation for required compiled-strategy qualifications.
5. Release WASM rebuild.
6. Web tests and `npx tsc --noEmit`.
7. Native/release-WASM semantic comparison, including preservation of the
   completed 49-case publication portfolio.

Archive the implementation report, before/after matrix, telemetry, layout and
resource comparison, strategy/evaluation/simulation evidence, final native /
WASM comparison, and deferred work. Promote the durable dependency/filtering
contract into the stable solver, product, engine, and mechanic documentation,
restore the handoff to no active boundary, and leave a clean worktree.
