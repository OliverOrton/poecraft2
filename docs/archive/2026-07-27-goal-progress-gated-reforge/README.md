# Goal-Progress-Gated Reforge Mode

**Status: complete (2026-07-27), implementation retained.**

Parent: [Archive](../README.md)

- [Final report](report.md)
- [Completed plan](plan.md)
- [Tracked evidence summary](../../../fixtures/solver-natural-t1/v1/evidence/goal-progress-gated-reforge-summary.json)

The unrestricted exact solver remains the default. The retained opt-in mode
solves an exact restricted MDP: goal outcomes share one terminal exit,
zero-goal-progress outcomes share a destructive-reforge-only retry basin for
their preserved boundary, and every partial-progress outcome stays exact.

Both frozen four-mod first Chaos rows now fit below 200,000 states. Full-four
retains 134,475 partial states and deep-four retains 123,695. The next exact
reforge request reaches the unchanged 3,000,000 reforge-work cap before
Bellman optimization, so no frozen policy or global-optimality claim is made.
The future bounded Pareto admission design remains unimplemented.
