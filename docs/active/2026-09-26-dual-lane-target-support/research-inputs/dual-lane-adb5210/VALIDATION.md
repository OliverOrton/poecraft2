# Qualification and comparison design

## Do not confuse five questions

- Native mechanics/graph evaluation correct?
- Graph actually solves its original requested target?
- Finder can discover a checked graph for that target?
- Current can discover a checked graph under a stated proof profile?
- Current can prove full-MDP optimality for that target/profile?

Each has an explicit status. A passing test in one row does not fill the others.

## Focused correctness cohort

Use the existing native harness and helpers. Currently available selectors include `--solver-eval-only`, `--solver-assertion-service-only`, `--solver-solve-only`, `--solver-proof-pattern-only`, `--solver-carrier-bounds-only`, `--solver-phase-lower-only`, `--solver-setup-service-only` and `--solver-compile-only ARTIFACT`. Read actual help/source for the exact runtime path and argument requirements. No invented command is a prerequisite. [R14]

Test categories:

1. Legacy-clean, normalized E, R and side-count assessments; all-required and any-k; wrong rarity/tier; disjointness; no-action completed root in both lanes.
2. Same generic graph executed with R success and request-checked separately against R/L; a nonterminal branch observes a family outside the goal; any-k is not converted to all observed slots.
3. Effective default priority, raw fake success, wrong root, wrong scope/economy, dependency occurrence tampering and interrupted validation.
4. Optimized/gated versus existing full native path where applicable; full mass, resources, success target and control/offer/checkpoint context.
5. Current target-neutral-zero producer/consumer isolation, stale-cache poison tests, pending/refused proof state, no exact-status escalation, ordinary clean exact control preservation.
6. Finder native retention's complete reached-entry checking; held goals remain actually preserved or losses are handled; R does not grant free macro interruption.
7. Lifecycle: cancellation before construction, during checking and transfer; Finish returns only a prior compatible checked winner; handle replacement; two simultaneous handles with different goals/profile remain isolated.
8. Memory: original problem and all private/evaluator graphs, mapping/provenance and transfer overlap stay in one aggregate budget; work refunds do not occur on rollback.

End-to-end checks exercise both callers, not just a shared helper. Run focused tests while iterating, then affected final families once. No broad suite at each milestone.

## Timed native batch (maximum eight initial cases)

Freeze exactly one implementation revision and native build. Record requested and resolved options, native data, corpus, ordered goal slots, terminal identity, primitive/programme scope, economy, root, stepping, work/memory caps and host. Existing corpus runner owns supervision; timed cases run serially.

| Arm | Cases | Purpose |
|---|---|---|
| Current ordinary L | A4,A5 | Preserve original positive proof and strong clean graphs |
| Current target-neutral-zero L | A4,A5 | Measure the cost of the common diagnostic profile itself |
| Current target-neutral-zero R | A4,A5 | Clean-versus-coverage contrast under the same profile |
| Finder conditional-retention L | A5 | Preserve current grammar/checking result |
| Finder conditional-retention R | A5 | Target contrast with the same finder vocabulary/ranking/budget |

E normalizes to L, so no extra timed E arm. If a true compatible saved baseline is reused, report it as reused and record the compatibility argument. Do not compare pre-change and post-change builds as though the shared checker cost disappeared.

Use `docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json` for CB01(A5)/CB02(A4), and the existing role corpus when a named role fixture is needed. Confirm actual source paths and IDs through their manifests before launch. The latest E/K records resolve the ordinary corpus and artifact hashes in `evidence/baseline.json`.

The proposed proof flag must first be implemented in native parser, isolated worker, corpus identity and help. The existing `--native-goal-terminal` values are already present. Reject conflicting explicit requests such as native-retention positive proof with target-neutral-zero; or resolve them through an explicit, reported common profile before the run. Never silently apply the old `reuse` activation in one contrast arm.

Default product boundaries remain those of the case: four-minute Finish, 300-second native watchdog, 315-second host deadline and one GiB aggregate for these cases. Verify all row/state/reforge limits from the resolver. A checker has no extra uncharged budget. Refrain from a broad soak if a semantic refusal occurs before work.

## Scorecard per lane and target

Record:
- actual root/goal/scope/economy/native/data/build/profile identities;
- capability flags: assess, compile, evaluate, search, positive-bound consumption, exact closure;
- active Current or Finder owner and actual stop reason;
- candidate generation, completed/priced rows, checked and accepted controllers, program support/refusals;
- first observed verified policy, strongest observed by available horizons, final best, no-policy duration;
- objective cost, expected primitive count and actual graph identity;
- published lower, upper, gap and their sources; `closure_unavailable_by_profile` versus `available_not_reached`;
- discovered/queued/started/expanded/completed populations, retained rows/transitions, transient construction and logical reforge work;
- active stage timers versus elapsed waits; maximum uninterrupted call; peak/live/transient ownership;
- omitted samples and unresolved/capped evidence.

Do not backdate sparse telemetry events. A5's reliability classification error is distinct from a good policy and must be projected without masking it. R's potentially smaller values are **different-goal economics**, never a same-clean improvement percentage.

## Analysis rules

Normal same-problem comparison continues to reject L/R. Extend the existing reporting owner with a labelled semantic-contrast projection if necessary, not a second runner or identity bypass.

Within Current, compare ordinaryL versus neutralL to expose the profile change, then neutralL versus neutralR for target effects. Within Finder, compare fixed grammar L/R. A faster Finder R does not predict Current proof closure. A capped neutral Current run does not prove that ordinary R with fully retargeted bounds is intrinsically difficult.

Single time/memory pairs are observations, not robust speed claims. Run reversed-order confirmation only if a material timing claim will be made and the causal question warrants the extra two cases. Do not automatically multiply the entire matrix.

## Product boundary

A shared evaluator/native change requires a source-matched release-WASM rebuild and relevant web/TypeScript tests. Exercise clean default Current and clean experimental Finder through the existing worker/Calculator path. Preserve compact stepping, frozen submissions and safe mode switching. Coverage remains private; no public default or UI promise is selected.

Use actual Calculator `default_finish` only for a product-quality claim; `finish` stops early on the first usable graph and answers a different question. Rendered review remains Oliver's. No fresh Simulator for unchanged graphs. If fresh execution qualification is needed for a new product claim, use the owner-approved 1,000 trials with censoring and budgets honestly reported.

## Acceptance and retention

Engineering success: both selected consumers actually run nontrivial supported native requests, return request-checked graphs and preserve defaults/lifecycles. A genuine shared helper with only one caller migrated is partial completion.

Scientific success: the semantic contrast is measured with its limitations, including profile effects and unsupported closure. No arbitrary percentage gain is needed for a target-support contract. If no competitive same-clean gain was attempted, say so explicitly.

On an implementation blocker, retain safe completed pieces and the exact native witness/callsite needed for the unfinished consumer. Do not mark that lane complete, silently switch scope, or describe “unqualified” as a permanent impossibility. Ordinary clean regressions block release even if the R diagnostic works.
