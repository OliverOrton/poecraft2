# Session Handoff

**Status: active implementation boundary.**

Plan:
[High-Impact Partial-State Executable Upper Policies](docs/active/high-impact-executable-uppers.md)

Branch: `codex/high-impact-executable-uppers`

Starting source: `3a6d191668f837090717295c8650ef7bbb5e0c0e`

## Active Boundary

Extend the existing focused-upper and gated incremental lifecycle to discover
proper, progress-preserving multi-action continuations from the partial states
that contribute most to completed frozen Fossil and Harvest upper Q values.
Reuse the shared exact graph, retained delayed rows, exact kernels,
exceptional-support handling, Eldritch delta states, and existing
policy-evaluation/properness machinery. Do not create a local-policy solver,
dense structural DAG, goal-count merge, or isolated scalar-upper authority.

Gate 1 first adds bounded deterministic provenance telemetry and must reproduce
the complete `3a6d191` primary baseline with high-impact behavior disabled.
Only after observational parity may behavior change.

Qualification is frozen: the primary full-four case must either strictly admit
a completed Fossil/Harvest row by executable upper-Q separation or reduce one
frozen completed Fossil/Harvest upper Q by at least 10%, through a verified
proper progress-preserving multi-action continuation. Completed probability
rows must remain unrecomputed. Smaller movement is diagnostic and does not
qualify.

The active plan owns the exact baselines, percentage formula, resource limits,
21 native gates, controls, commands, evidence fields, and rollback rules.
Product defaults and unrestricted behavior remain unchanged. The deep-four
case runs once, bounded, only after primary qualification or final measured
rejection.

This milestone produces exactly one remaining local commit: the final retained
implementation or restored rejection, acceptance evidence, archived plan,
report, indexes, and no-active-boundary HANDOFF. Do not push. The commit must
end with:

`Co-authored-by: Codex <codex@openai.com>`
