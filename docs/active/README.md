# Active Work

**Status: no active implementation boundary.** Oliver must select the next
chunk before implementation resumes.

Parent: [Documentation map](../README.md)

The most recently completed milestone is
[R4 browser transfer and solver lifetime](../archive/2026-07-26-browser-transfer-lifetime-r4/README.md).
It replaced nested compiled-strategy JSON with transferable bytes, removed
ownerless full-graph clones, released the scoped native Solve closure after
handoff, and rebuilds on a later Solve or reprice. The release-WASM build,
complete web suite, TypeScript check, and focused Node-worker lifecycle
evidence passed. It changed no mechanics, solver algorithm, action scope,
public cap, or strategy vocabulary.

The three immediately preceding research milestones are also closed:

- [Broad-action separation and renewal](../archive/2026-07-25-broad-action-separation-research/README.md)
  rejected production integration because cheap exact separation excluded no
  broad actions and the smallest exact fixed-policy compaction still exceeded
  current work limits on both three-mod hard cases.
- [Exact automatic-action constraint generation](../archive/2026-07-25-exact-automatic-action-constraint-generation/README.md)
  rejected production integration because automatic-only deferral moved the
  first-carrier 200,000-state failure to an ordinary broad reforge.
- [Gap-directed natural-T1 research](../archive/2026-07-25-gap-directed-natural-t1-research/README.md)
  supplied the diagnosis and exact-separation gates for those follow-ups.

Those research branches contributed documentation and evidence only. Their
diagnostic prototypes were restored before closure.

Unselected possibilities remain in [Future work](../future/README.md).
`future/` and archived plans do not establish sequence.
