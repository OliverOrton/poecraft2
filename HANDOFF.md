# Session Handoff

**Status: Gate 0 active for goal-progress-gated reforge mode.**

Plan: [Goal-Progress-Gated Reforge Mode](docs/active/plan.md)

Branch: `codex/goal-progress-gated-reforge`

Starting source: `f843a9d` (`main`)

## Current Boundary

Implement an opt-in solver mode that groups terminal reforge mass, sends
zero-satisfied-goal outcomes to a preserved-boundary retry basin, and retains
every partial-progress outcome exactly. Basin actions are restricted to legal
primitive destructive reforges whose next kernels ignore the discarded
affixes.

The default unrestricted exact solver must remain behaviorally unchanged.
Gated results must be labelled exact only within the zero-progress-reroll
restriction.

## Immediate Next Step

Land the internal basin identity and grouped exact reforge distribution, then
prove probability conservation and grouped parity against an unrestricted
small oracle before changing Bellman expansion.

## Standing Boundaries

- Do not merge partial-progress states by goal count.
- Do not drop or renormalize terminal/retry probability mass.
- Do not admit salvage actions from a retry basin.
- Do not raise product caps or change mechanics.
- Existing unrestricted behavior and hashes are a hard control.

Commits remain local unless Oliver asks to push and must end with:

`Co-authored-by: Codex <codex@openai.com>`
