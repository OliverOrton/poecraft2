# Session Handoff

**Status: an implementation boundary is active.**

Plan:
[Harvest Natural Pools And Shared Exact Reforge Frontier](docs/active/plan.md)

Branch: `codex/harvest-shared-reforge-frontier`

Starting source: `94fb013` (`main`)

## Current Boundary

Proceed through the active plan without another confirmation between phases.

Phase 1 implements Oliver's 2026-07-28 ruling: Harvest reforge, augment, and
resistance conversion use the ordinary naturally rollable target-tagged pool,
requiring positive spawn and generation weights and retaining the ordinary
final roll weight. One shared pool authority must cover sampled, exact,
solver-helper, and debug paths. A positive-spawn/zero-generation modifier must
be excluded everywhere.

Only after focused sampled/exact parity passes, Phase 2 prototypes one shared
exact structural reforge frontier for compatible Chaos, filtered Fossils, and
retained Harvest reforges. Each action retains its own probability lane, cost,
distribution, and Bellman choice. Fossil added mods are topology deltas;
forced mods are deterministic seed deltas. The sequential evaluator remains
the fallback.

Keep initial caps unchanged. Qualify on the frozen full-four and deep-four
cases plus retained goal-relevant Harvest reforges. If exact parity fails,
probability propagation remains effectively additive, or memory is
unacceptable, restore frontier production changes, retain the Harvest
correction/evidence, and stop.

Commits remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
